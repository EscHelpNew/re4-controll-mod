#include "re4/settings_file.hpp"
#include <cassert>
#include <cstdio>
#include <limits>
#include <initializer_list>

int main() {
    constexpr const char* path = "build/settings-test.bin";
    for (auto p : {path, "build/settings-test.bin.previous", "build/settings-test.bin.incoming"}) std::remove(p);
    re4::Settings first, second, loaded;
    second.turn_gain = 2;
    assert(re4::save_settings_file(path, first));
    assert(re4::save_settings_file(path, second));
    assert(re4::load_settings_file(path, loaded) && loaded.turn_gain == 2);
    re4::Settings invalid;
    invalid.turn_gain = std::numeric_limits<float>::quiet_NaN();
    assert(!re4::save_settings_file(path, invalid));
    assert(re4::load_settings_file(path, loaded) && loaded.turn_gain == 2);
    FILE* corrupt = std::fopen(path, "wb"); assert(corrupt);
    std::fputs("interrupted/corrupt", corrupt); std::fclose(corrupt);
    assert(re4::load_settings_file(path, loaded) && loaded.turn_gain == 1);
    std::remove("build/settings-test.bin.previous");
    loaded.turn_gain = 3;
    assert(!re4::load_settings_file(path, loaded) && loaded.turn_gain == 3);
    std::remove(path);
    std::puts("settings file: replacement, backup recovery, invalid snapshot PASS");
}
