#pragma once
#include <cstddef>
#include <cstdint>

namespace re4 {
constexpr std::size_t kSettingsBytes = 64;
constexpr const char* kSettingsPath = "sdmc:/config/re4-controls/settings.bin";
struct Settings {
    bool analog_enabled = true;
    bool gyro_enabled = false;
    bool invert_x = true;
    bool invert_y = true;
    std::uint32_t calibration_request = 0;
    float turn_gain = 1;
    float camera_gain = 1;
    float aim_gain_x = 1;
    float aim_gain_y = 1;
    float left_deadzone = 0.15f;
    float right_deadzone = 0.15f;
    float gyro_gain_x = 1;
    float gyro_gain_y = 1;
    float gyro_bias_x = 0;
    float gyro_bias_y = 0;
    float movement_gain = 1;
};
bool valid_settings(const Settings& settings);
bool encode_settings(const Settings& settings, std::uint8_t* out, std::size_t size);
bool decode_settings(const std::uint8_t* data, std::size_t size, Settings& out);
}
