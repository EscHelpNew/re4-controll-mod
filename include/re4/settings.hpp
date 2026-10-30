#pragma once
#include <cstddef>
#include <cstdint>
#include "re4/button_map.hpp"

namespace re4 {
constexpr std::size_t kLegacySettingsBytes = 64;
constexpr std::size_t kSettingsBytes = 128;
constexpr const char* kSettingsPath = "sdmc:/config/re4-controls/settings.bin";
enum class Language : std::uint8_t { English, Russian };
enum class StickCurve : std::uint8_t { Linear, Exponential, Logarithmic, Hybrid };
struct Settings {
    Language language = Language::English;
    StickCurve stick_curve = StickCurve::Linear;
    bool analog_enabled = true;
    bool gyro_enabled = false;
    bool invert_x = true;
    bool invert_y = true;
    std::uint32_t calibration_request = 0;
    float turn_gain = 1;
    float camera_gain = 1;
    float aim_gain_x = 1;
    float aim_gain_y = 1;
    float right_aim_gain_x = 1;
    float right_aim_gain_y = 1;
    float left_deadzone = 0.05f;
    float right_deadzone = 0.05f;
    float gyro_gain_x = 1;
    float gyro_gain_y = 1;
    float gyro_speed = 0.8f;
    float gyro_deadzone_dps = 0.5f;
    float gyro_bias_x = 0;
    float gyro_bias_y = 0;
    float movement_gain = 1; // Legacy wire field; native walking maximum in v4.
    bool remap_enabled = true;
    ButtonMap button_map = identity_button_map();
    bool free_camera = false;
    float camera_return_speed = 1;
};
bool valid_settings(const Settings& settings);
bool encode_settings(const Settings& settings, std::uint8_t* out, std::size_t size);
bool decode_settings(const std::uint8_t* data, std::size_t size, Settings& out);
}
