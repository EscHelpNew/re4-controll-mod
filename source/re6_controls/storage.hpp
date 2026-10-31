#pragma once
namespace re6_overlay_storage {
constexpr const char* settings_path = "r6ovl:/config/re4-controls/re6-settings.bin";
bool mount();
bool ready();
void close();
}
