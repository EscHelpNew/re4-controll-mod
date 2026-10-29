// Build-specific controls adapter. Runtime guards precede every hook install.
#include "lib.hpp"
#include "nn/fs.hpp"
#include "re4/input.hpp"
#include "re4/profile.hpp"
#include "re4/settings.hpp"
#include "re4/shared.hpp"
#include "re4/turn.hpp"
#include "re4/hook_context.hpp"
#include "re4/response.hpp"
#include "re4/sensor.hpp"
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
extern "C" Result sensor_handles(std::uint32_t*, int*, int, std::uint32_t, std::uint32_t)
    asm("_ZN2nn3hid6detail22GetSixAxisSensorHandleEPNS0_19SixAxisSensorHandleEPiijNS_4util10BitFlagSetILi32ENS0_12NpadStyleTagEEE");
extern "C" Result sensor_start(const std::uint32_t&)
    asm("_ZN2nn3hid6detail18StartSixAxisSensorERKNS0_19SixAxisSensorHandleE");
extern "C" Result sensor_stop(const std::uint32_t&)
    asm("_ZN2nn3hid6detail17StopSixAxisSensorERKNS0_19SixAxisSensorHandleE");
extern "C" Result sensor_read(int*, re4::SensorState*, int, const std::uint32_t&)
    asm("_ZN2nn3hid6detail22GetSixAxisSensorStatesEPiPNS0_18SixAxisSensorStateEiRKNS0_19SixAxisSensorHandleE");
struct SensorBackend {
    static auto handles(std::uint32_t* h, int* n, int capacity, std::uint32_t id, std::uint32_t style) {
        return sensor_handles(h, n, capacity, id, style);
    }
    static auto start(const std::uint32_t& handle) { return sensor_start(handle); }
    static auto stop(const std::uint32_t& handle) { return sensor_stop(handle); }
    static auto read(int* n, re4::SensorState* states, int capacity, const std::uint32_t& handle) {
        return sensor_read(n, states, capacity, handle);
    }
};

__attribute__((used, section(".data.re4control")))
volatile re4::SharedControl control = {
    "RE4CTL_SHARED_6", re4::kSharedAbi, 0, 0, re4::kCapabilityTurning, {}, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};
re4::Settings settings;
float raw_left_x = 0;
bool dpad_turn = false;
bool dpad_move = false;
void* movement_actor = nullptr;
std::uint32_t last_sequence = 0;
bool file_read = false;
std::uintptr_t main_base = 0;
std::uint32_t sensor_pad = 0, sensor_style = 0, sensor_attributes = 0;
re4::SensorReader<SensorBackend> sensor;
re4::GyroIntegrator gyro;
re4::AimDelta gyro_pending;
std::uint64_t last_sensor_tick = 0;
int (*aiming_predicate)(void*) = nullptr;
void* gyro_input_actor = nullptr;

bool firearm_aim(void* actor) {
    auto* bytes = static_cast<std::uint8_t*>(actor);
    if (!actor || actor != *reinterpret_cast<void**>(main_base + re4::profile::player_slot)
        || bytes[0x114] != 0 || (bytes[0x115] != 6 && bytes[0x115] != 11 && bytes[0x115] != 15)
        || !(*reinterpret_cast<std::uint64_t*>(main_base + re4::profile::input_flags) & 0x10)) return false;
    return aiming_predicate && aiming_predicate(actor) != 0;
}
void poll_sensors() {
    gyro_pending = {};
    if (sensor.select(sensor_pad, sensor_style, sensor_attributes, settings.gyro_enabled)) {
        gyro.reset(); last_sensor_tick = 0;
    }
    control.sensor_result = sensor.result;
    control.sensor_running = sensor.running();
    if (!sensor.running()) return;
    const auto now = svcGetSystemTick();
    // Tegra's system counter runs at 19.2 MHz. Ignore the first batch and any
    // batch following a >100 ms gap (HOME, overlay pause, controller switch).
    const bool continuous = last_sensor_tick && now >= last_sensor_tick && now - last_sensor_tick <= 1'920'000;
    last_sensor_tick = now;
    void* actor = *reinterpret_cast<void**>(main_base + re4::profile::player_slot);
    const bool active = continuous && firearm_aim(actor);
    re4::SensorState samples[16]{};
    const int count = sensor.read(samples, 16);
    control.sensor_result = sensor.result;
    control.sensor_running = sensor.running();
    constexpr float tau = 6.28318530718f;
    for (int i = count - 1; i >= 0; --i) {
        const auto& s = samples[i];
        const bool connected = s.attributes & 1;
        const float yaw = -s.angular_velocity[1] * tau - settings.gyro_bias_x;
        const float pitch = -s.angular_velocity[0] * tau - settings.gyro_bias_y;
        const auto d = gyro.update({s.number, s.interval_ns, yaw, pitch}, active && connected,
            settings.gyro_gain_x * (settings.invert_x ? -1 : 1),
            settings.gyro_gain_y * (settings.invert_y ? -1 : 1));
        gyro_pending.yaw += d.yaw; gyro_pending.pitch += d.pitch;
    }
    control.sensor_samples = control.sensor_samples + count;
}

void publish_settings() {
    std::uint8_t bytes[re4::kSettingsBytes];
    if (!re4::encode_settings(settings, bytes, sizeof(bytes))) return;
    control.sequence = control.sequence | 1;
    std::atomic_thread_fence(std::memory_order_seq_cst);
    for (unsigned i = 0; i < sizeof(bytes); ++i) control.settings[i] = bytes[i];
    std::atomic_thread_fence(std::memory_order_seq_cst);
    control.sequence = (control.sequence + 1) & ~1u;
    last_sequence = control.sequence;
    re4::acknowledge_settings(control, settings, last_sequence);
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
    re4::acknowledge_settings(control, settings, before);
}
void capture(const NpadState& pad, std::uint32_t id, std::uint32_t style) {
    control.pad_samples = control.pad_samples + 1;
    control.last_pad_id = id;
    if (id != 0 && id != 0x20) return;
    const bool connected = pad.attributes & 1;
    if (connected) {
        sensor_pad = id; sensor_style = style; sensor_attributes = pad.attributes & 13u;
    } else if (id == sensor_pad && style == sensor_style) sensor_attributes = 0;
    raw_left_x = connected ? pad.lx / 32767.f : 0;
    if (std::fabs(raw_left_x) > control.maximum_raw_lx) control.maximum_raw_lx = std::fabs(raw_left_x);
    dpad_turn = connected && (pad.buttons & ((1ull << 12) | (1ull << 14)));
    dpad_move = connected && (pad.buttons & ((1ull << 13) | (1ull << 15)));
    control.raw_lx = raw_left_x;
    control.raw_ly = connected ? pad.ly / 32767.f : 0;
    control.raw_rx = connected ? pad.rx / 32767.f : 0;
    control.raw_ry = connected ? pad.ry / 32767.f : 0;
}

HOOK_DEFINE_TRAMPOLINE(FullPad) {
    static void Callback(NpadState* out, const std::uint32_t& id) { Orig(out, id); capture(*out, id, 1); }
};
HOOK_DEFINE_TRAMPOLINE(HandheldPad) {
    static void Callback(NpadState* out, const std::uint32_t& id) { Orig(out, id); capture(*out, id, 2); }
};
HOOK_DEFINE_TRAMPOLINE(DualPad) {
    static void Callback(NpadState* out, const std::uint32_t& id) { Orig(out, id); capture(*out, id, 4); }
};
HOOK_DEFINE_TRAMPOLINE(PadNormalize) {
    static int Callback(void* state) {
        const int result = Orig(state);
        read_initial_settings(); poll_settings();
        poll_sensors();
        control.frames = control.frames + 1;
        return result;
    }
};
HOOK_DEFINE_TRAMPOLINE(MovementTurn) {
    static void Callback(void* actor) {
        auto* bytes = static_cast<std::uint8_t*>(actor);
        const bool ours = actor && actor == *reinterpret_cast<void**>(main_base + re4::profile::player_slot);
        const bool normal = ours && bytes[0x114] == 0 && bytes[0x115] <= 4;
        auto* flags = reinterpret_cast<volatile std::uint64_t*>(main_base + re4::profile::input_flags);
        const std::uint64_t old_direction = normal ? *flags & 15u : 0;
        const bool remap = normal && settings.analog_enabled;
        const float before = normal ? *reinterpret_cast<float*>(bytes + 0xB4) : 0;
        if (remap) {
            *flags = (*flags & ~std::uint64_t(12)) | (re4::turn_flags(0, raw_left_x,
                dpad_turn, true, settings.left_deadzone) & 12u);
            // Preserve digital pad direction when the user uses the D-pad.
            if (dpad_turn) *flags = (*flags & ~std::uint64_t(12)) | (old_direction & 12u);
            if (!dpad_move) {
                const float axis = re4::shape_axis(control.raw_ly, settings.left_deadzone, 1);
                *flags = (*flags & ~std::uint64_t(3)) | (axis > 0 ? 1u : axis < 0 ? 2u : 0u);
            }
        }
        const bool manual = remap && (*flags & 12u);
        void* previous_actor = movement_actor;
        movement_actor = remap ? actor : nullptr;
        Orig(actor);
        movement_actor = previous_actor;
        if (remap) *flags = (*flags & ~std::uint64_t(15)) | old_direction;
        if (normal) control.direction_calls = control.direction_calls + 1;
        if (!manual || bytes[0x114] != 0 || bytes[0x115] > 4) return;
        const float after = *reinterpret_cast<float*>(bytes + 0xB4);
        const float factor = re4::turn_factor(raw_left_x, dpad_turn, settings.left_deadzone, settings.turn_gain);
        const float corrected = re4::corrected_turn_yaw(before, after, factor);
        if (!std::isfinite(before) || !std::isfinite(after) || !std::isfinite(corrected)) return;
        control.speed_calls = control.speed_calls + 1;
        control.last_turn_factor = factor;
        control.original_yaw_delta = re4::yaw_delta(before, after);
        control.corrected_yaw_delta = re4::yaw_delta(before, corrected);
        // Original walk/run handlers also write actor yaw AFTER root-motion
        // update. Normal game rendering rebuilds its matrices later in frame.
        *reinterpret_cast<float*>(bytes + 0xB4) = corrected;
    }
};
// The motion frame increment drives both the gait and next frame's root
// displacement. Collision/ground handling still receives the game's motion.
HOOK_DEFINE_TRAMPOLINE(MotionAdvance) {
    static unsigned Callback(void* motion) {
        auto* actor = static_cast<std::uint8_t*>(movement_actor);
        const bool walking = actor && actor[0x114] == 0 && actor[0x115] >= 1 && actor[0x115] <= 3;
        const bool ours = walking && (motion == actor + 0x228 || motion == *reinterpret_cast<void**>(actor + 0x320));
        if (!ours || dpad_move) return Orig(motion);
        auto* speed = reinterpret_cast<float*>(static_cast<std::uint8_t*>(motion) + 0xDC);
        const float original = *speed;
        const float factor = std::fabs(re4::shape_axis(control.raw_ly, settings.left_deadzone, 1)) * settings.movement_gain;
        if (!std::isfinite(original) || std::fabs(original) > 16) return Orig(motion);
        *speed = original * factor;
        control.movement_calls = control.movement_calls + 1;
        control.movement_factor = factor;
        const auto result = Orig(motion);
        *speed = original;
        return result;
    }
};
HOOK_DEFINE_TRAMPOLINE(CameraInput) {
    static void Callback(void* camera) {
        auto* actor = *reinterpret_cast<std::uint8_t**>(main_base + re4::profile::player_slot);
        if (!settings.analog_enabled || !actor || actor[0x114] != 0 || actor[0x115] > 4) { Orig(camera); return; }
        const bool alternate = *reinterpret_cast<int*>(main_base + 0x11C7D68) == 1;
        auto* input = reinterpret_cast<std::int8_t*>(main_base + 0x124EC38 + (alternate ? 0xC0 : 8));
        const auto x = input[0], y = input[1];
        control.camera_calls = control.camera_calls + 1;
        input[0] = re4::camera_axis(control.raw_rx, settings.right_deadzone, settings.camera_gain);
        input[1] = re4::camera_axis(control.raw_ry, settings.right_deadzone, settings.camera_gain);
        Orig(camera);
        input[0] = x; input[1] = y;
    }
};
HOOK_DEFINE_TRAMPOLINE(AimSensitivity) {
    static int Callback(void* actor, int mode) {
        auto* bytes = static_cast<std::uint8_t*>(actor);
        const bool ours = actor && actor == *reinterpret_cast<void**>(main_base + re4::profile::player_slot);
        const bool active = ours && (settings.analog_enabled || settings.gyro_enabled) && bytes[0x114] == 0
            && (bytes[0x115] == 6 || bytes[0x115] == 11 || bytes[0x115] == 15)
            && (*reinterpret_cast<std::uint64_t*>(main_base + re4::profile::input_flags) & 0x10);
        if (!active) return Orig(actor, mode);
        auto* pitch = reinterpret_cast<float*>(main_base + 0x1265A08);
        auto* yaw = reinterpret_cast<float*>(bytes + 0xB4);
        void* previous_input_actor = gyro_input_actor;
        gyro_input_actor = nullptr;
        // Apply once before native stick/weapon processing, retaining both
        // inputs and letting the caller update aim matrices normally.
        if (settings.gyro_enabled && firearm_aim(actor)) {
            const auto d = gyro_pending;
            gyro_pending = {};
            const float old_yaw = *yaw, old_pitch = pitch[1];
            *yaw = re4::gyro_yaw(old_yaw, d.yaw);
            pitch[0] = re4::gyro_pitch(pitch[0], d.pitch);
            pitch[1] = re4::gyro_pitch(old_pitch, d.pitch);
            if (*yaw != old_yaw || pitch[1] != old_pitch) gyro_input_actor = actor;
            control.gyro_yaw = d.yaw; control.gyro_pitch = d.pitch;
        }
        const float p0 = pitch[0], p1 = pitch[1];
        auto* offset = reinterpret_cast<float*>(bytes + 0x524);
        const float y0 = *yaw, o0 = *offset;
        const auto routine = bytes[0x115];
        const int result = Orig(actor, mode);
        gyro_input_actor = previous_input_actor;
        if (bytes[0x114] != 0 || bytes[0x115] != routine) return result;
        control.aim_calls = control.aim_calls + 1;
        if (!settings.analog_enabled) return result;
        // Preserve the configured game's choice of aiming stick and direction;
        // use its output delta and the physical deflection magnitude.
        const float x = std::fmax(std::fabs(re4::shape_axis(control.raw_lx, settings.left_deadzone, 1)),
                                  std::fabs(re4::shape_axis(control.raw_rx, settings.right_deadzone, 1)));
        const float y = std::fmax(std::fabs(re4::shape_axis(control.raw_ly, settings.left_deadzone, 1)),
                                  std::fabs(re4::shape_axis(control.raw_ry, settings.right_deadzone, 1)));
        const float fx = (dpad_turn ? 1 : x) * settings.aim_gain_x;
        const float fy = (dpad_move ? 1 : y) * settings.aim_gain_y;
        if (dpad_turn || std::fabs(control.raw_lx) > 0.05f || std::fabs(control.raw_rx) > 0.05f) {
            *yaw = re4::corrected_turn_yaw(y0, *yaw, fx);
            *offset = re4::scaled_axis_state(o0, *offset, fx, 3.14159265f, 0.35f);
        }
        if (dpad_move || std::fabs(control.raw_ly) > 0.05f || std::fabs(control.raw_ry) > 0.05f) {
            pitch[0] = re4::scaled_axis_state(p0, pitch[0], fy, 1, 0.25f);
            pitch[1] = re4::scaled_axis_state(p1, pitch[1], fy, 1, 0.25f);
        }
        return result;
    }
};
HOOK_DEFINE_TRAMPOLINE(AimSway) {
    static void Callback(void* actor, unsigned flags, float* pitch, float* offset) {
        // Native sway clamps to the previous manual-input baseline unless bit0
        // is set. Gyro is manual input too; retain native sway everywhere else.
        if (actor && actor == gyro_input_actor) flags |= 1u;
        Orig(actor, flags, pitch, offset);
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
    if (main.m_Total.m_Size < 0x1265A10) { control.status = 2; return; }
    main_base = reinterpret_cast<std::uintptr_t>(memory);
    aiming_predicate = reinterpret_cast<int (*)(void*)>(main_base + 0x317630);
    publish_settings();
    exl::hook::Initialize();
    FullPad::InstallAtFuncPtr(get_full);
    HandheldPad::InstallAtFuncPtr(get_handheld);
    DualPad::InstallAtFuncPtr(get_dual);
    PadNormalize::InstallAtOffset(0x25F680);
    MovementTurn::InstallAtOffset(re4::profile::movement_dispatch);
    MotionAdvance::InstallAtOffset(0x2B8F10);
    CameraInput::InstallAtOffset(0x19B20);
    AimSensitivity::InstallAtOffset(0x33DF80);
    AimSway::InstallAtOffset(0x33EC50);
    control.status = 1;
}
extern "C" NORETURN void exl_exception_entry() { EXL_ABORT("RE4 controls exception"); }
