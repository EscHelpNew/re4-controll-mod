#include "re6/settings.hpp"
#include <cstring>

namespace re6 {
namespace {
void put32(std::uint8_t* p, std::uint32_t v) {
    for (unsigned i = 0; i < 4; ++i) p[i] = std::uint8_t(v >> (8 * i));
}
std::uint32_t get32(const std::uint8_t* p) {
    return std::uint32_t(p[0]) | std::uint32_t(p[1]) << 8
        | std::uint32_t(p[2]) << 16 | std::uint32_t(p[3]) << 24;
}
std::uint32_t checksum(const std::uint8_t* bytes, std::size_t size) {
    std::uint32_t hash = 2166136261u;
    for (std::size_t i = 0; i < size; ++i) { hash ^= bytes[i]; hash *= 16777619u; }
    return hash;
}
}
bool valid_settings(const Settings& settings) {
    return unsigned(settings.language) <= unsigned(Language::Russian)
        && unsigned(settings.texture_filtering) <= unsigned(TextureFilter::Af16x)
        && unsigned(settings.anti_aliasing) <= unsigned(AntiAliasing::Fxaa3High);
}
bool encode_settings(const Settings& settings, std::uint8_t* out, std::size_t size) {
    if (!out || size != kSettingsBytes || !valid_settings(settings)) return false;
    std::memset(out, 0, size);
    std::memcpy(out, "R6CT", 4);
    put32(out + 4, 1);
    put32(out + 8, unsigned(settings.language));
    out[12] = std::uint8_t(settings.texture_filtering);
    out[13] = std::uint8_t(settings.anti_aliasing);
    put32(out + 60, checksum(out, 60));
    return true;
}
bool decode_settings(const std::uint8_t* data, std::size_t size, Settings& out) {
    if (!data || size != kSettingsBytes || std::memcmp(data, "R6CT", 4)
        || get32(data + 4) != 1 || (get32(data + 8) & ~1u)
        || checksum(data, 60) != get32(data + 60)) return false;
    for (unsigned i = 14; i < 60; ++i) if (data[i]) return false;
    Settings settings;
    const auto flags = get32(data + 8);
    settings.language = static_cast<Language>(flags & 1);
    settings.texture_filtering = static_cast<TextureFilter>(data[12]);
    settings.anti_aliasing = static_cast<AntiAliasing>(data[13]);
    if (!valid_settings(settings)) return false;
    out = settings;
    return true;
}
}
