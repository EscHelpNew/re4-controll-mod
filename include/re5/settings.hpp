#pragma once
#include "re4/settings.hpp"
#include "re5/graphics.hpp"
#include <array>
namespace re5 {
inline constexpr std::size_t kSettingsBytes=320;
inline constexpr const char* kSettingsPath="sdmc:/config/re4-controls/re5-settings.bin";
inline constexpr re4::Settings native_controls(){
 re4::Settings s;s.analog_enabled=false;s.remap_enabled=false;return s;
}
// Version1: 64-byte RE5 envelope plus two strict v19 RE4 control payloads.
// Language and graphics live in the envelope. Player graphics remain Native;
// the UI composes shared graphics into its view, never into saved player controls.
struct Settings {
 std::array<re4::Settings,2> players{{native_controls(),native_controls()}};
 GraphicsOptions graphics;
 re4::Language language=re4::Language::English;
};
bool valid_settings(const Settings&);
bool encode_settings(const Settings&,std::uint8_t*,std::size_t);
bool decode_settings(const std::uint8_t*,std::size_t,Settings&);
bool load_settings_file(const char*,Settings&);
bool save_settings_file(const char*,const Settings&);
}
