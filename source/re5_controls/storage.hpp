#pragma once
namespace re5_overlay_storage {
constexpr const char* settings_path = "r5ovl:/config/re4-controls/re5-settings.bin";
bool mount();
bool ready();
void close();
}
