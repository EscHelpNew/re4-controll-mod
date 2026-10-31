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
std::uint32_t checksum(const std::uint8_t* p, std::size_t size) {
    std::uint32_t hash = 2166136261u;
    for (std::size_t i = 0; i < size; ++i) { hash ^= p[i]; hash *= 16777619u; }
    return hash;
}
}
bool valid_settings(const Settings& s) {
    if (!valid_graphics({s.texture_filtering,s.shadow_mode,s.anti_aliasing,s.draw_distance_scale})
        || unsigned(s.language)>1 || unsigned(s.stick_curve)>3 || unsigned(s.reticle)>2 || unsigned(s.reticle_size)>2 || !valid_button_map(s.button_map)
        || !std::isfinite(s.camera_return_speed) || s.camera_return_speed<0 || s.camera_return_speed>1
        || !std::isfinite(s.third_person_distance) || s.third_person_distance<1 || s.third_person_distance>2) return false;
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
    put32(out + 4, 13);
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
    std::memcpy(out + 60, &s.camera_return_speed, 4);
    out[64] = std::uint8_t(s.free_camera);
    out[65] = std::uint8_t(s.remap_enabled);
    std::memcpy(out + 68, s.button_map.data(), kButtonCount);
    out[88] = std::uint8_t(s.texture_filtering);
    out[89] = std::uint8_t(s.shadow_mode);
    out[90] = std::uint8_t(s.anti_aliasing);
    std::uint32_t distance; std::memcpy(&distance, &s.draw_distance_scale, 4);
    put32(out + 92, distance);
    out[96]=std::uint8_t(s.reticle);
    out[97]=std::uint8_t(unsigned(s.first_person) | unsigned(s.move_while_aiming)<<1 | unsigned(s.move_while_knife)<<2
        | unsigned(s.unified_controls)<<3 | unsigned(s.re6_response)<<4 | unsigned(s.move_while_reloading)<<5);
    out[98]=std::uint8_t(s.reticle_size);
    std::memcpy(out+100,&s.third_person_distance,4);
    put32(out + 124, checksum(out, 124));
    return true;
}
bool decode_settings(const std::uint8_t* data, std::size_t size, Settings& out) {
    if (!data || (size != kSettingsBytes && size != kLegacySettingsBytes)
        || std::memcmp(data, "R4CT", 4)) return false;
    const auto version = get32(data + 4);
    if ((size == kLegacySettingsBytes ? version < 1 || version > 6 : version < 7 || version > 13)
        || (get32(data + 8) & (version<4 ? ~15u : version<6 ? ~127u : ~0x007f7f7fu))
        || (version == 1 && get32(data + 56))
        || checksum(data, size - 4) != get32(data + size - 4)) return false;
    Settings s;
    if (version >= 7) {
        if (data[64] > 1 || data[65] > 1 || data[66] || data[67]) return false;
        if (version == 7) {
            for (unsigned i = 88; i < 124; ++i) if (data[i]) return false;
        } else {
            if (data[91]) return false;
            if(version<9){for (unsigned i = 96; i < 124; ++i) if (data[i]) return false;}
            else{
                if(data[97]&~(version>=13?63u:version>=12?31u:version>=11?15u:7u) || data[99])return false;
                for(unsigned i=version>=10?104:100;i<124;++i)if(data[i])return false;
                if(version>=10)std::memcpy(&s.third_person_distance,data+100,4);
                s.reticle=static_cast<ReticleStyle>(data[96]);s.reticle_size=static_cast<ReticleSize>(data[98]);
                s.first_person=data[97]&1;s.move_while_aiming=data[97]&2;s.move_while_knife=data[97]&4;
                if(version>=11)s.unified_controls=data[97]&8;
                if(version>=12)s.re6_response=data[97]&16;
                if(version>=13)s.move_while_reloading=data[97]&32;
            }
            s.texture_filtering = static_cast<TextureFilter>(data[88]);
            s.shadow_mode = static_cast<ShadowMode>(data[89]);
            s.anti_aliasing = static_cast<AntiAliasing>(data[90]);
            const auto distance = get32(data + 92);
            std::memcpy(&s.draw_distance_scale, &distance, 4);
        }
        std::memcpy(&s.camera_return_speed, data + 60, 4);
        s.free_camera = data[64] != 0;
        s.remap_enabled = data[65] != 0;
        std::memcpy(s.button_map.data(), data + 68, kButtonCount);
    }
    const auto flags = get32(data + 8);
    s.analog_enabled = flags & 1; s.gyro_enabled = flags & 2;
    s.invert_x = flags & 4; s.invert_y = flags & 8;
    if (version>=4) {
        s.stick_curve=static_cast<StickCurve>((flags>>4)&3);
        s.language=static_cast<Language>((flags>>6)&1);
    }
    if(version>=6){s.right_aim_gain_x=float((flags>>8)&127)/100;s.right_aim_gain_y=float((flags>>16)&127)/100;}
    if (version>=5) std::memcpy(&s.gyro_deadzone_dps,data+12,4);
    else s.calibration_request = get32(data + 12);
    const unsigned count = version >= 5 ? 11 : 10;
    for (unsigned i = 0; i < count; ++i) {
        const auto value = get32(data + 16 + i * 4);
        std::memcpy(&(s.*fields[i]), &value, 4);
    }
    if (version < 3) {
        constexpr float Settings::* capped[] = {&Settings::turn_gain, &Settings::camera_gain,
            &Settings::aim_gain_x, &Settings::aim_gain_y};
        for (auto member : capped) {
            const float gain = s.*member;
            if (!std::isfinite(gain) || gain < 0 || gain > 4) return false;
            s.*member = std::fmin(gain, 1.f);
        }
    }
    if (version>=2 && version<=4) {
        float legacy_walk;std::memcpy(&legacy_walk,data+56,4);
        if (!std::isfinite(legacy_walk) || legacy_walk<0 || legacy_walk>(version<3?4:1)) return false;
    }
    if (version<4) {
        if (std::fabs(s.left_deadzone-0.15f)<0.00001f) s.left_deadzone=0.05f;
        if (std::fabs(s.right_deadzone-0.15f)<0.00001f) s.right_deadzone=0.05f;
    }
    if (!valid_settings(s)) return false;
    out = s;
    return true;
}
}
