#pragma once
namespace overlay_storage {
constexpr const char* settings_path = "r4ovl:/config/re4-controls/settings.bin";
bool mount();
bool ready();
void close();
}
