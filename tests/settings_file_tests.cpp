#include "re4/settings_file.hpp"
#include <cassert>
#include <cstdio>
#include <limits>
#include <initializer_list>
#include <cstring>

int main() {
    constexpr const char* path = "build/settings-test.bin";
    for (auto p : {path, "build/settings-test.bin.previous", "build/settings-test.bin.incoming"}) std::remove(p);
    re4::Settings first, second, loaded;
    second.turn_gain = 0.5f;
    assert(re4::save_settings_file(path, first));
    assert(re4::save_settings_file(path, second));
    assert(re4::load_settings_file(path, loaded) && loaded.turn_gain == 0.5f);
    re4::Settings invalid;
    invalid.turn_gain = std::numeric_limits<float>::quiet_NaN();
    assert(!re4::save_settings_file(path, invalid));
    assert(re4::load_settings_file(path, loaded) && loaded.turn_gain == 0.5f);
    FILE* corrupt = std::fopen(path, "wb"); assert(corrupt);
    std::fputs("interrupted/corrupt", corrupt); std::fclose(corrupt);
    assert(re4::load_settings_file(path, loaded) && loaded.turn_gain == 1);
    // Saving after recovery must not rotate the corrupt primary over the
    // only valid backup. Losing the newly published primary models an
    // interruption before publication of the incoming replacement.
    assert(re4::save_settings_file(path, second));
    assert(std::remove(path) == 0);
    assert(re4::load_settings_file(path, loaded) && loaded.turn_gain == 1);
    std::remove("build/settings-test.bin.previous");
    loaded.turn_gain = 3;
    assert(!re4::load_settings_file(path, loaded) && loaded.turn_gain == 3);
    std::remove(path);
    // A real 64-byte v6 primary migrates on save, and remains a valid recovery
    // copy if publication of its 128-byte replacement is interrupted.
    std::uint8_t bytes[re4::kSettingsBytes];
    first.gyro_speed=2.5f;
    assert(re4::encode_settings(first,bytes,sizeof(bytes)));
    bytes[4]=6;
    std::uint32_t hash=2166136261u;
    for(unsigned i=0;i<60;++i){hash^=bytes[i];hash*=16777619u;}
    for(unsigned i=0;i<4;++i)bytes[60+i]=std::uint8_t(hash>>(i*8));
    FILE* legacy=std::fopen(path,"wb");assert(legacy);
    assert(std::fwrite(bytes,1,re4::kLegacySettingsBytes,legacy)==re4::kLegacySettingsBytes);
    assert(std::fclose(legacy)==0);
    assert(re4::load_settings_file(path,loaded) && loaded.gyro_speed==2.5f);
    assert(loaded.button_map==re4::identity_button_map() && !loaded.free_camera);
    loaded.free_camera=true;loaded.button_map[0]=re4::kUnboundButton;
    assert(re4::save_settings_file(path,loaded));
    FILE* current=std::fopen(path,"rb");assert(current);
    assert(std::fread(bytes,1,sizeof(bytes),current)==re4::kSettingsBytes);
    assert(std::fgetc(current)==EOF && std::fclose(current)==0);
    assert(bytes[4]==11);
    re4::Settings migrated;
    assert(re4::load_settings_file(path,migrated) && migrated.free_camera && migrated.button_map[0]==re4::kUnboundButton);
    current=std::fopen(path,"wb");assert(current);
    assert(std::fwrite(bytes,1,64,current)==64 && std::fclose(current)==0);
    assert(re4::load_settings_file(path,migrated) && !migrated.free_camera && migrated.gyro_speed==2.5f);
    assert(migrated.button_map==re4::identity_button_map());
    std::remove(path);std::remove("build/settings-test.bin.previous");
    std::puts("settings file: replacement, backup recovery, invalid snapshot PASS");
}
