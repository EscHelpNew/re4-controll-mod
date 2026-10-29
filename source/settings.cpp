#include "re4/settings.hpp"
#include <cmath>
#include <cstring>

namespace re4 {
namespace {
constexpr float Settings::* fields[] = {
    &Settings::turn_gain, &Settings::camera_gain, &Settings::aim_gain_x,
    &Settings::aim_gain_y, &Settings::left_deadzone, &Settings::right_deadzone,
    &Settings::gyro_gain_x, &Settings::gyro_gain_y, &Settings::gyro_bias_x,
    &Settings::gyro_bias_y, &Settings::gyro_speed
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
    if (unsigned(s.language)>1 || unsigned(s.stick_curve)>3) return false;
    const float right_gains[]={s.right_aim_gain_x,s.right_aim_gain_y};
    for(float gain:right_gains)
        if(!std::isfinite(gain) || gain<0 || gain>1)return false;
    for (auto member : fields) if (!std::isfinite(s.*member)) return false;
    for (unsigned i = 0; i < 4; ++i) if (s.*fields[i] < 0 || s.*fields[i] > 1) return false;
    for (unsigned i = 4; i < 6; ++i) if (s.*fields[i] < 0 || s.*fields[i] > 0.95f) return false;
    for (unsigned i = 6; i < 8; ++i) if (s.*fields[i] < 0 || s.*fields[i] > 10) return false;
    return std::isfinite(s.gyro_deadzone_dps) && s.gyro_deadzone_dps>=0 && s.gyro_deadzone_dps<=20
        && s.gyro_speed>=0 && s.gyro_speed<=10
        && std::fabs(s.gyro_bias_x) <= 10 && std::fabs(s.gyro_bias_y) <= 10
        && s.movement_gain == 1;
}
bool encode_settings(const Settings& s, std::uint8_t* out, std::size_t size) {
    static_assert(sizeof(float) == 4);
    if (!out || size != kSettingsBytes || !valid_settings(s)) return false;
    std::memset(out, 0, size);
    std::memcpy(out, "R4CT", 4);
    put32(out + 4, 6);
    put32(out + 8, unsigned(s.analog_enabled) | unsigned(s.gyro_enabled) << 1
          | unsigned(s.invert_x) << 2 | unsigned(s.invert_y) << 3
          | unsigned(s.stick_curve) << 4 | unsigned(s.language) << 6
          | std::uint32_t(s.right_aim_gain_x*100+0.5f)<<8
          | std::uint32_t(s.right_aim_gain_y*100+0.5f)<<16);
    std::memcpy(out+12,&s.gyro_deadzone_dps,4);
    for (unsigned i = 0; i < 11; ++i) {
        std::uint32_t value;
        const float f = s.*fields[i]; std::memcpy(&value, &f, 4);
        put32(out + 16 + i * 4, value);
    }
    put32(out + 60, checksum(out));
    return true;
}
bool decode_settings(const std::uint8_t* data, std::size_t size, Settings& out) {
    if (!data || size != kSettingsBytes || std::memcmp(data, "R4CT", 4)
        || get32(data + 4) < 1 || get32(data + 4) > 6
        || (get32(data + 8) & (get32(data + 4)<4 ? ~15u : get32(data+4)<6 ? ~127u : ~0x007f7f7fu))
        || (get32(data + 4) == 1 && get32(data + 56))
        || checksum(data) != get32(data + 60)) return false;
    Settings s;
    const auto flags = get32(data + 8);
    s.analog_enabled = flags & 1; s.gyro_enabled = flags & 2;
    s.invert_x = flags & 4; s.invert_y = flags & 8;
    if (get32(data+4)>=4) {
        s.stick_curve=static_cast<StickCurve>((flags>>4)&3);
        s.language=static_cast<Language>((flags>>6)&1);
    }
    if(get32(data+4)==6){s.right_aim_gain_x=float((flags>>8)&127)/100;s.right_aim_gain_y=float((flags>>16)&127)/100;}
    if (get32(data+4)>=5) std::memcpy(&s.gyro_deadzone_dps,data+12,4);
    else s.calibration_request = get32(data + 12);
    const unsigned count = get32(data + 4) >= 5 ? 11 : 10;
    for (unsigned i = 0; i < count; ++i) {
        const auto value = get32(data + 16 + i * 4);
        std::memcpy(&(s.*fields[i]), &value, 4);
    }
    if (get32(data + 4) < 3) {
        constexpr float Settings::* capped[] = {&Settings::turn_gain, &Settings::camera_gain,
            &Settings::aim_gain_x, &Settings::aim_gain_y};
        for (auto member : capped) {
            const float gain = s.*member;
            if (!std::isfinite(gain) || gain < 0 || gain > 4) return false;
            s.*member = std::fmin(gain, 1.f);
        }
    }
    if (get32(data+4)>=2 && get32(data+4)<=4) {
        float legacy_walk;std::memcpy(&legacy_walk,data+56,4);
        if (!std::isfinite(legacy_walk) || legacy_walk<0 || legacy_walk>(get32(data+4)<3?4:1)) return false;
    }
    if (get32(data+4)<4) {
        if (std::fabs(s.left_deadzone-0.15f)<0.00001f) s.left_deadzone=0.05f;
        if (std::fabs(s.right_deadzone-0.15f)<0.00001f) s.right_deadzone=0.05f;
    }
    if (!valid_settings(s)) return false;
    out = s;
    return true;
}
}
