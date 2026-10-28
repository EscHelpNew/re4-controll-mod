#include "re4/settings.hpp"
#include <cmath>
#include <cstring>

namespace re4 {
namespace {
constexpr float Settings::* fields[] = {
    &Settings::turn_gain, &Settings::camera_gain, &Settings::aim_gain_x,
    &Settings::aim_gain_y, &Settings::left_deadzone, &Settings::right_deadzone,
    &Settings::gyro_gain_x, &Settings::gyro_gain_y, &Settings::gyro_bias_x,
    &Settings::gyro_bias_y
};
void put32(std::uint8_t* p, std::uint32_t v) {
    for (unsigned i = 0; i < 4; ++i) p[i] = std::uint8_t(v >> (8 * i));
}
std::uint32_t get32(const std::uint8_t* p) {
    return std::uint32_t(p[0]) | std::uint32_t(p[1]) << 8
        | std::uint32_t(p[2]) << 16 | std::uint32_t(p[3]) << 24;
}
std::uint32_t checksum(const std::uint8_t* p) {
    std::uint32_t hash = 2166136261u;
    for (unsigned i = 0; i < 60; ++i) { hash ^= p[i]; hash *= 16777619u; }
    return hash;
}
}
bool valid_settings(const Settings& s) {
    for (auto member : fields) if (!std::isfinite(s.*member)) return false;
    for (unsigned i = 0; i < 4; ++i) if (s.*fields[i] < 0 || s.*fields[i] > 4) return false;
    for (unsigned i = 4; i < 6; ++i) if (s.*fields[i] < 0 || s.*fields[i] > 0.95f) return false;
    for (unsigned i = 6; i < 8; ++i) if (s.*fields[i] < 0 || s.*fields[i] > 10) return false;
    return std::fabs(s.gyro_bias_x) <= 10 && std::fabs(s.gyro_bias_y) <= 10;
}
bool encode_settings(const Settings& s, std::uint8_t* out, std::size_t size) {
    static_assert(sizeof(float) == 4);
    if (!out || size != kSettingsBytes || !valid_settings(s)) return false;
    std::memset(out, 0, size);
    std::memcpy(out, "R4CT", 4);
    put32(out + 4, 1);
    put32(out + 8, unsigned(s.analog_enabled) | unsigned(s.gyro_enabled) << 1
          | unsigned(s.invert_x) << 2 | unsigned(s.invert_y) << 3);
    put32(out + 12, s.calibration_request);
    for (unsigned i = 0; i < 10; ++i) {
        std::uint32_t value;
        const float f = s.*fields[i]; std::memcpy(&value, &f, 4);
        put32(out + 16 + i * 4, value);
    }
    put32(out + 60, checksum(out));
    return true;
}
bool decode_settings(const std::uint8_t* data, std::size_t size, Settings& out) {
    if (!data || size != kSettingsBytes || std::memcmp(data, "R4CT", 4)
        || get32(data + 4) != 1 || (get32(data + 8) & ~15u)
        || get32(data + 56) || checksum(data) != get32(data + 60)) return false;
    Settings s;
    const auto flags = get32(data + 8);
    s.analog_enabled = flags & 1; s.gyro_enabled = flags & 2;
    s.invert_x = flags & 4; s.invert_y = flags & 8;
    s.calibration_request = get32(data + 12);
    for (unsigned i = 0; i < 10; ++i) {
        const auto value = get32(data + 16 + i * 4);
        std::memcpy(&(s.*fields[i]), &value, 4);
    }
    if (!valid_settings(s)) return false;
    out = s;
    return true;
}
}
