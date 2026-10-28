// Experimental movement patch. Gyro/aim hooks are deliberately not advertised
// as supported until their game-state semantics are verified on the console.
#include "lib.hpp"
#include "nn/fs.hpp"
#include "re4/input.hpp"
#include "re4/profile.hpp"
#include "re4/settings.hpp"
#include "re4/shared.hpp"
#include "re4/turn.hpp"
#include <atomic>

namespace {
struct NpadState {
    std::uint64_t sample, buttons;
    std::int32_t lx, ly, rx, ry;
    std::uint32_t attributes, reserved;
};
static_assert(sizeof(NpadState) == 40);
extern "C" void get_full(NpadState*, const std::uint32_t&)
    asm("_ZN2nn3hid12GetNpadStateEPNS0_16NpadFullKeyStateERKj");
extern "C" void get_handheld(NpadState*, const std::uint32_t&)
    asm("_ZN2nn3hid12GetNpadStateEPNS0_17NpadHandheldStateERKj");
extern "C" void get_dual(NpadState*, const std::uint32_t&)
    asm("_ZN2nn3hid12GetNpadStateEPNS0_16NpadJoyDualStateERKj");

__attribute__((used, section(".data.re4control")))
volatile re4::SharedControl control = {
    "RE4CTL_SHARED_1", 1, 0, 0, re4::kCapabilityTurning, {}, 0, 0, 0, 0, 0
};
re4::Settings settings;
float raw_left_x = 0;
bool dpad_turn = false;
std::uint32_t last_sequence = 0;
bool file_read = false;

void publish_settings() {
    std::uint8_t bytes[re4::kSettingsBytes];
    if (!re4::encode_settings(settings, bytes, sizeof(bytes))) return;
    control.sequence = control.sequence | 1;
    std::atomic_thread_fence(std::memory_order_seq_cst);
    for (unsigned i = 0; i < sizeof(bytes); ++i) control.settings[i] = bytes[i];
    std::atomic_thread_fence(std::memory_order_seq_cst);
    control.sequence = (control.sequence + 1) & ~1u;
    last_sequence = control.sequence;
}
bool read_settings(const char* path) {
    nn::fs::FileHandle handle{};
    if (R_FAILED(nn::fs::OpenFile(&handle, path, 1))) return false;
    long size = 0;
    std::uint8_t bytes[re4::kSettingsBytes];
    const bool valid = R_SUCCEEDED(nn::fs::GetFileSize(&size, handle)) && size == sizeof(bytes)
        && R_SUCCEEDED(nn::fs::ReadFile(handle, 0, bytes, sizeof(bytes)))
        && re4::decode_settings(bytes, sizeof(bytes), settings);
    nn::fs::CloseFile(handle);
    return valid;
}
void read_initial_settings() {
    if (file_read) return;
    file_read = true;
    // One bounded startup read; live changes use the shared block, with no
    // filesystem polling in the per-frame path.
    if (R_FAILED(nn::fs::MountSdCardForDebug("r4ctl"))) return;
    if (read_settings("r4ctl:/config/re4-controls/settings.bin")
        || read_settings("r4ctl:/config/re4-controls/settings.bin.previous")) publish_settings();
}
void poll_settings() {
    const auto before = control.sequence;
    if ((before & 1) || before == last_sequence) return;
    std::atomic_thread_fence(std::memory_order_acquire);
    std::uint8_t bytes[re4::kSettingsBytes];
    for (unsigned i = 0; i < sizeof(bytes); ++i) bytes[i] = control.settings[i];
    std::atomic_thread_fence(std::memory_order_seq_cst);
    if (control.sequence != before || !re4::decode_settings(bytes, sizeof(bytes), settings)) return;
    last_sequence = before;
}
void capture(const NpadState& pad, std::uint32_t id) {
    if (id != 0 && id != 0x20) return;
    const bool connected = pad.attributes & 1;
    raw_left_x = connected ? pad.lx / 32767.f : 0;
    dpad_turn = connected && (pad.buttons & ((1ull << 12) | (1ull << 14)));
    control.raw_lx = raw_left_x;
    control.raw_ly = connected ? pad.ly / 32767.f : 0;
    control.raw_rx = connected ? pad.rx / 32767.f : 0;
    control.raw_ry = connected ? pad.ry / 32767.f : 0;
}

HOOK_DEFINE_TRAMPOLINE(FullPad) {
    static void Callback(NpadState* out, const std::uint32_t& id) { Orig(out, id); capture(*out, id); }
};
HOOK_DEFINE_TRAMPOLINE(HandheldPad) {
    static void Callback(NpadState* out, const std::uint32_t& id) { Orig(out, id); capture(*out, id); }
};
HOOK_DEFINE_TRAMPOLINE(DualPad) {
    static void Callback(NpadState* out, const std::uint32_t& id) { Orig(out, id); capture(*out, id); }
};
HOOK_DEFINE_TRAMPOLINE(PadNormalize) {
    static int Callback(void* state) {
        const int result = Orig(state);
        read_initial_settings(); poll_settings();
        control.frames = control.frames + 1;
        return result;
    }
};
HOOK_DEFINE_INLINE(TurnDirection) {
    static void Callback(exl::hook::InlineCtx* context) {
        context->W[8] = re4::turn_flags(context->W[8], raw_left_x, dpad_turn,
                                       settings.analog_enabled, settings.left_deadzone);
    }
};
HOOK_DEFINE_INLINE(TurnSpeed) {
    static void Callback(exl::hook::InlineFloatCtx* context) {
        if (settings.analog_enabled)
            context->S[0] *= re4::turn_factor(raw_left_x, dpad_turn,
                                             settings.left_deadzone, settings.turn_gain);
    }
};
}

extern "C" void exl_main(void*, void*) {
    u64 program_id = 0;
    if (R_FAILED(svcGetInfo(&program_id, InfoType_ProgramId, CUR_PROCESS_HANDLE, 0))
        || program_id != re4::profile::title_id) { control.status = 2; return; }
    const auto& main = exl::util::GetMainModuleInfo();
    const auto* memory = reinterpret_cast<const std::uint8_t*>(main.m_Total.m_Start);
    if (!re4::profile::matches(memory, main.m_Total.m_Size)) {
        control.status = 2; // Unsupported executable: no game/SDK hooks installed.
        return;
    }
    publish_settings();
    exl::hook::Initialize();
    FullPad::InstallAtFuncPtr(get_full);
    HandheldPad::InstallAtFuncPtr(get_handheld);
    DualPad::InstallAtFuncPtr(get_dual);
    PadNormalize::InstallAtOffset(0x25F680);
    for (const auto offset : {0x30D84C, 0x30D9DC, 0x30DD8C}) TurnDirection::InstallAtOffset(offset);
    for (const auto offset : {0x30D870, 0x30D8F0, 0x30D9EC, 0x30DA10, 0x30DDCC})
        TurnSpeed::InstallAtOffset(offset);
    control.status = 1;
}
extern "C" NORETURN void exl_exception_entry() { EXL_ABORT("RE4 controls exception"); }
