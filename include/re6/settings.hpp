#pragma once
#include <cstddef>
#include <cstdint>

namespace re6 {
constexpr std::size_t kSettingsBytes = 64;
constexpr const char* kSettingsPath = "sdmc:/config/re4-controls/re6-settings.bin";
enum class Language : std::uint8_t { English, Russian };
enum class TextureFilter : std::uint8_t { Native, Af2x, Af4x, Af8x, Af16x };
enum class AntiAliasing : std::uint8_t { Native, Off, Fxaa, Fxaa3, Fxaa3High };
struct Settings {
    Language language = Language::English;
    TextureFilter texture_filtering = TextureFilter::Native;
    AntiAliasing anti_aliasing = AntiAliasing::Native;
};
bool valid_settings(const Settings& settings);
bool encode_settings(const Settings& settings, std::uint8_t* out, std::size_t size);
bool decode_settings(const std::uint8_t* data, std::size_t size, Settings& out);
bool load_settings_file(const char* path, Settings& out);
bool save_settings_file(const char* path, const Settings& settings);
}
