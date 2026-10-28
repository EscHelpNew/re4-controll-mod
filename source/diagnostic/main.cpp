#include <switch.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#include "re4/input.hpp"
#include "re4/settings.hpp"
#include "re4/settings_file.hpp"
extern "C" {
#include "remote_reload.h"
#include "reload_main.h"
#include "reload_project.h"
#include "reload_user_channel.h"
#include "reload_core.h"
}

namespace {
constexpr float tau = 6.28318530718f;
void configure_reload(int argc, char** argv) {
    for (int i = 0; i < argc; ++i) {
        if (!std::strncmp(argv[i], "--token=", 8)) {
            game_reload_configure(argc, argv); return;
        }
    }
    // The local SD token enables first launch from this project's forwarder.
    // Pass it explicitly to the unmodified kit, never embed it in an image.
    char token[67]{};
    char argument[73]{};
    char* args[64]{};
    FILE* file = std::fopen(NXDK_SD_ROOT "/token.txt", "rb");
    size_t count = file ? std::fread(token, 1, 66, file) : 0;
    const bool complete = file && std::fgetc(file) == EOF && !std::ferror(file);
    if (file) std::fclose(file);
    while (count && (token[count - 1] == '\n' || token[count - 1] == '\r')) --count;
    token[count] = 0;
    if (complete && count == 64 && reload_token_valid(token) && argc >= 0 && argc < 63) {
        for (int i = 0; i < argc; ++i) args[i] = argv[i];
        std::snprintf(argument, sizeof(argument), "--token=%s", token);
        args[argc] = argument;
        game_reload_configure(argc + 1, args);
    } else game_reload_configure(argc, argv);
    std::memset(token, 0, sizeof(token));
    std::memset(argument, 0, sizeof(argument));
}
struct Sensors {
    HidSixAxisSensorHandle handles[2]{};
    bool running[2]{};
    unsigned count = 0;
    u32 style = 0;
    u32 attributes = 0;
    Result result = 0;
    void stop() {
        for (unsigned i = 0; i < 2; ++i) {
            if (running[i]) hidStopSixAxisSensor(handles[i]);
            running[i] = false;
        }
        count = 0;
    }
    bool select(const PadState& pad) {
        const auto next = padGetStyleSet(&pad);
        const auto connected = padGetAttributes(&pad) &
            (HidNpadAttribute_IsConnected | HidNpadAttribute_IsLeftConnected | HidNpadAttribute_IsRightConnected);
        if (next == style && connected == attributes) return false;
        stop(); style = next; attributes = connected; result = 0;
        HidNpadStyleTag tag;
        auto id = HidNpadIdType_No1;
        int wanted = 1;
        if (!padIsConnected(&pad)) return true;
        if (padIsHandheld(&pad)) { id = HidNpadIdType_Handheld; tag = HidNpadStyleTag_NpadHandheld; }
        else if (style & HidNpadStyleTag_NpadFullKey) tag = HidNpadStyleTag_NpadFullKey;
        else if (style & HidNpadStyleTag_NpadJoyDual) { tag = HidNpadStyleTag_NpadJoyDual; wanted = 2; }
        else { result = MAKERESULT(Module_Libnx, LibnxError_NotFound); return true; }
        result = hidGetSixAxisSensorHandles(handles, wanted, id, tag);
        if (R_FAILED(result)) return true;
        for (int i = 0; i < wanted; ++i) {
            const auto started = hidStartSixAxisSensor(handles[i]);
            if (R_FAILED(started)) { result = started; continue; }
            running[i] = true;
            ++count;
        }
        return true;
    }
    size_t read(HidSixAxisSensorState* samples) {
        if (!count) return 0;
        // Prefer the right Joy-Con for split controllers, then the left.
        unsigned index = (style & HidNpadStyleTag_NpadJoyDual)
            && (attributes & HidNpadAttribute_IsRightConnected) && running[1] ? 1 : 0;
        if (!running[index]) index ^= 1;
        return hidGetSixAxisSensorStates(handles[index], samples, 16);
    }
};


}

int main(int argc, char** argv) {
    if (game_reload_early_bridge()) return 0;
    AppletStorage ordinary{};
    if (game_reload_take_user_channel(&ordinary)) appletStorageClose(&ordinary);
    configure_reload(argc, argv);
    consoleInit(nullptr);
    mkdir("sdmc:/config", 0777); mkdir("sdmc:/config/re4-controls", 0777);
    mkdir("sdmc:/switch", 0777); mkdir(NXDK_SD_ROOT, 0777);
    const Result sockets = socketInitializeDefault();
    if (R_SUCCEEDED(sockets)) {
        nxlinkStdioForDebug();
        game_reload_start(NXDK_APP_NAME, "0.1.0");
    }
    PadState pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);
    Sensors sensors;
    re4::Settings settings;
    re4::load_settings_file(re4::kSettingsPath, settings);
    re4::GyroIntegrator integrator;
    float aim_x = 0, aim_y = 0;
    u64 last_calibration_sample = 0;
    unsigned calibration_left = 0;
    float sum_x = 0, sum_y = 0;
    unsigned row = 0, frame = 0;
    const char* status = "Ready. This diagnostic does not modify RE4.";
    constexpr const char* names[] = {"Turn speed", "Camera speed", "Aim X", "Aim Y",
        "Left deadzone", "Right deadzone", "Gyro X", "Gyro Y"};
    float* values[] = {&settings.turn_gain, &settings.camera_gain, &settings.aim_gain_x,
        &settings.aim_gain_y, &settings.left_deadzone, &settings.right_deadzone,
        &settings.gyro_gain_x, &settings.gyro_gain_y};
    while (appletMainLoop()) {
        game_reload_main_poll();
        padUpdate(&pad);
        const auto down = padGetButtonsDown(&pad);
        if (down & HidNpadButton_Plus) break;
        if (sensors.select(pad)) {
            integrator.reset(); calibration_left = 0; aim_x = aim_y = 0;
        }
        if (down & HidNpadButton_Up) row = (row + 7) % 8;
        if (down & HidNpadButton_Down) row = (row + 1) % 8;
        if (down & (HidNpadButton_Left | HidNpadButton_Right)) {
            const float step = row == 4 || row == 5 ? 0.01f : 0.05f;
            const float maximum = row < 4 ? 4.f : row < 6 ? 0.95f : 10.f;
            *values[row] = std::clamp(*values[row] + ((down & HidNpadButton_Right) ? step : -step), 0.f, maximum);
        }
        if (down & HidNpadButton_A) settings.gyro_enabled = !settings.gyro_enabled;
        if (down & HidNpadButton_X) { aim_x = aim_y = 0; }
        if (down & HidNpadButton_B) status = re4::save_settings_file(re4::kSettingsPath, settings) ? "Settings saved." : "Settings save failed.";
        if ((down & HidNpadButton_Y) && sensors.count) {
            calibration_left = 200; sum_x = sum_y = 0; last_calibration_sample = 0;
            status = "Keep controller STILL for calibration.";
        }
        HidSixAxisSensorState samples[16]{};
        const size_t sample_count = sensors.read(samples);
        re4::AimDelta movement;
        for (size_t i = sample_count; i > 0; --i) {
            const auto& sample = samples[i - 1];
            const float yaw = -sample.angular_velocity.y * tau;
            const float pitch = -sample.angular_velocity.x * tau;
            if (calibration_left && sample.sampling_number > last_calibration_sample) {
                last_calibration_sample = sample.sampling_number;
                sum_x += yaw; sum_y += pitch;
                if (!--calibration_left) {
                    settings.gyro_bias_x = sum_x / 200;
                    settings.gyro_bias_y = sum_y / 200;
                    status = "Calibration complete. B saves it.";
                }
            }
            const bool active = settings.gyro_enabled && !calibration_left &&
                (padGetButtons(&pad) & HidNpadButton_ZL);
            const auto delta = integrator.update({sample.sampling_number, sample.delta_time,
                yaw - settings.gyro_bias_x, pitch - settings.gyro_bias_y}, active,
                settings.gyro_gain_x * (settings.invert_x ? -1 : 1),
                settings.gyro_gain_y * (settings.invert_y ? -1 : 1));
            movement.yaw += delta.yaw; movement.pitch += delta.pitch;
        }
        aim_x += movement.yaw; aim_y += movement.pitch;
        const auto left = padGetStickPos(&pad, 0), right = padGetStickPos(&pad, 1);
        const float lx = re4::shape_axis(left.x / 32767.f, settings.left_deadzone, settings.turn_gain);
        const float rx = re4::shape_axis(right.x / 32767.f, settings.right_deadzone, settings.aim_gain_x);
        const float ry = re4::shape_axis(right.y / 32767.f, settings.right_deadzone, settings.aim_gain_y);
        std::printf("\x1b[HRE4 CONTROLS - diagnostic 0.1.0\x1b[K\n"
            "Pad connected: %s  style: %08lX\x1b[K\n"
            "Raw LEFT  X:%7ld Y:%7ld\x1b[K\nRaw RIGHT X:%7ld Y:%7ld\x1b[K\n"
            "Processed turn:% .3f  aim:% .3f % .3f\x1b[K\n"
            "Gyro:%s  sensors:%u  result:%08lX\x1b[K\n"
            "Samples:%2u dt:%llu ns rate:% .4f % .4f % .4f\x1b[K\n"
            "Hold ZL: gyro preview (radians) X:% .3f Y:% .3f\x1b[K\n\x1b[K\n",
            padIsConnected(&pad) ? "yes" : "no", (unsigned long)sensors.style,
            (long)left.x, (long)left.y, (long)right.x, (long)right.y, lx, rx, ry,
            settings.gyro_enabled ? "ON" : "OFF", sensors.count, (unsigned long)sensors.result,
            unsigned(sample_count), (unsigned long long)samples[0].delta_time,
            samples[0].angular_velocity.x, samples[0].angular_velocity.y,
            samples[0].angular_velocity.z, aim_x, aim_y);
        for (unsigned i = 0; i < 8; ++i)
            std::printf("%c %-17s %5.2f\x1b[K\n", i == row ? '>' : ' ', names[i], *values[i]);
        std::printf("\nUP/DOWN select  LEFT/RIGHT change\x1b[K\n"
            "A gyro on/off  Y calibrate  X reset preview\x1b[K\n"
            "B save  PLUS exit\x1b[K\n%s\x1b[K\n"
            "NRO receiver: %s  title:%016llX\x1b[K\n"
            "Automatic RUN needs own installed forwarder.\x1b[K\n",
            status, R_SUCCEEDED(sockets) ? "enabled with token" : "network unavailable",
            (unsigned long long)NXDK_TITLE_ID);
        if (++frame % 120 == 0) std::fprintf(stderr,
            "[input] style=%08lX raw=%ld,%ld,%ld,%ld gyro_samples=%u sensor_rc=%08lX\n",
            (unsigned long)sensors.style, (long)left.x, (long)left.y, (long)right.x,
            (long)right.y, unsigned(sample_count), (unsigned long)sensors.result);
        consoleUpdate(nullptr);
    }
    sensors.stop();
    game_reload_main_finish();
    if (R_SUCCEEDED(sockets)) socketExit();
    consoleExit(nullptr);
    return 0;
}
