#pragma once
#include "settings.hpp"
namespace re4 {
bool load_settings_file(const char* path, Settings& out);
bool save_settings_file(const char* path, const Settings& settings);
}
