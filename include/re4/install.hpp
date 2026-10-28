#pragma once
namespace re4 {
// Installs only the three fixed project artifacts. A different pre-existing
// file without our matching receipt is a conflict, not permission to replace it.
const char* install_game_files(const char* payload_root, const char* sd_root);
const char* disable_game_files(const char* sd_root);
const char* install_overlay_loader(const char* payload_root, const char* sd_root);
const char* disable_overlay_loader(const char* sd_root);
const char* install_overlay_only(const char* payload_root, const char* sd_root);
}
