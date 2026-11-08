#pragma once
#include <cstdint>
namespace re4 {
// Frontend-only preference: never part of the game Settings ABI or wire packet.
enum class LaunchMode : std::uint8_t {Normal,Diagnostics};
constexpr bool valid_launch_mode(LaunchMode mode){return mode==LaunchMode::Normal||mode==LaunchMode::Diagnostics;}
constexpr bool automatic_diagnostics(LaunchMode mode){return mode==LaunchMode::Diagnostics;}
inline constexpr const char* kLaunchModePath="sdmc:/config/re4-controls/launch-mode.bin";
inline constexpr const char* kOverlayLaunchModePath="r4ovl:/config/re4-controls/launch-mode.bin";
// Missing, corrupt or future formats always select Normal, ignoring backups.
bool load_launch_mode(const char* path,LaunchMode& out);
bool save_launch_mode(const char* path,LaunchMode mode);
}
