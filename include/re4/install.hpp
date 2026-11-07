#pragma once
#include "re4/game_target.hpp"
#include <cstddef>
#include <cstdint>
namespace re4 {
struct KnownInstallFile {GameTarget game;unsigned index;std::uint64_t bytes,digest;};
struct InstallCatalog {const KnownInstallFile* files=nullptr;std::size_t count=0;};
// Trusted exact artifact identities compiled into this app, never read from SD.
InstallCatalog legacy_install_catalog();
// Installs only the three fixed project artifacts. A different pre-existing
// file needs a matching receipt. Zero ownership left by overlay-only installation
// may migrate only an exact known legacy artifact from the trusted app catalog.
const char* install_game_files(const char* payload_root, const char* sd_root, GameTarget game=GameTarget::RE4,
    InstallCatalog known=legacy_install_catalog());
const char* disable_game_files(const char* sd_root, GameTarget game=GameTarget::RE4);
const char* install_overlay_loader(const char* payload_root, const char* sd_root);
const char* disable_overlay_loader(const char* sd_root);
const char* install_overlay_only(const char* payload_root, const char* sd_root, GameTarget game=GameTarget::RE4);
}
