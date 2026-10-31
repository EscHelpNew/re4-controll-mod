#include "re4/profiles.hpp"
#include "re4/settings_file.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

namespace {
namespace fs = std::filesystem;
using namespace re4;
int failures = 0;
#define CHECK(condition) do { if (!(condition)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #condition); ++failures; } } while (false)
const fs::path root = "build/profiles-test-work";
std::string path(const char* name) { return (root / name).string(); }
std::vector<char> bytes(const std::string& name) {
    std::ifstream in(name, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
void write(const std::string& name, const std::vector<char>& data) {
    std::ofstream out(name, std::ios::binary);
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    if (!out) { std::fprintf(stderr, "fixture write failed: %s\n", name.c_str()); std::exit(2); }
}
void corrupt(const std::string& name) { write(name, {'b','a','d'}); }
bool encoded_equal(const Settings& a, const Settings& b) {
    std::array<std::uint8_t, kSettingsBytes> first{}, second{};
    return encode_settings(a, first.data(), first.size()) && encode_settings(b, second.data(), second.size()) && first == second;
}
Settings custom() {
    Settings s;
    s.turn_gain = 0.43f; s.camera_gain = 0.72f; s.free_camera = true;
    s.right_aim_gain_x = 0.62f; s.gyro_speed = 2.3f; s.button_map[0] = kUnboundButton;
    s.texture_filtering = TextureFilter::Aniso4; s.draw_distance_scale = 1.5f;
    s.language = Language::Russian; s.calibration_request = 17; s.gyro_bias_x = 1.2f; s.gyro_bias_y = -2.1f;
    return s;
}
void domain_copy_and_identity() {
    Settings active = custom(), defaults=default_controls_profile();
    copy_profile(active, defaults, ProfileDomain::Controls);
    CHECK(active.turn_gain == 1 && !active.free_camera && active.button_map == identity_button_map());
    CHECK(active.texture_filtering == TextureFilter::Aniso4 && active.draw_distance_scale == 1.5f);
    CHECK(active.language == Language::Russian && active.calibration_request == 17);
    CHECK(active.gyro_bias_x == 1.2f && active.gyro_bias_y == -2.1f);
    CHECK(profile_kind(active, ProfileDomain::Controls) == ProfileKind::Default);
    CHECK(profile_kind(active, ProfileDomain::Graphics) == ProfileKind::Custom);
    CHECK(same_profile(active, defaults, ProfileDomain::Controls));
    CHECK(!same_profile(active, defaults, ProfileDomain::Graphics));
    active = custom();
    copy_profile(active, defaults, ProfileDomain::Graphics);
    CHECK(active.texture_filtering == TextureFilter::Native && active.draw_distance_scale == 1);
    CHECK(active.turn_gain == 0.43f && active.free_camera && active.button_map[0] == kUnboundButton);
    CHECK(active.language == Language::Russian && active.calibration_request == 17 && active.gyro_bias_y == -2.1f);
    CHECK(profile_kind(active, ProfileDomain::Controls) == ProfileKind::Custom);
    CHECK(profile_kind(active, ProfileDomain::Graphics) == ProfileKind::Default);
    Settings preferences=default_controls_profile();
    preferences.language = Language::Russian; preferences.calibration_request = 123;
    preferences.gyro_bias_x = 2; preferences.gyro_bias_y = -3;
    CHECK(profile_kind(preferences, ProfileDomain::Controls) == ProfileKind::Default);
    CHECK(profile_kind(preferences, ProfileDomain::Graphics) == ProfileKind::Default);
    Settings quantized = defaults; quantized.right_aim_gain_x = 0.999f;
    CHECK(same_profile(quantized, defaults, ProfileDomain::Controls));
    quantized.right_aim_gain_x = 0.994f;
    CHECK(!same_profile(quantized, defaults, ProfileDomain::Controls));
    copy_profile(active, active, ProfileDomain::Controls);
    CHECK(active.turn_gain == 0.43f && active.language == Language::Russian);
    Settings invalid; invalid.turn_gain = std::numeric_limits<float>::quiet_NaN();
    CHECK(!same_profile(invalid, defaults, ProfileDomain::Controls));
    CHECK(same_profile(invalid, defaults, ProfileDomain::Graphics));
}
void migration_and_restore() {
    const auto file = path("migration.bin");
    auto active = custom();
    CHECK(save_settings_file(file.c_str(), active)); // Existing installation has no sidecars.
    CHECK(switch_profile(file.c_str(), active, ProfileDomain::Controls, ProfileKind::Default) == ProfileResult::Applied);
    CHECK(active.turn_gain == 1 && active.texture_filtering == TextureFilter::Aniso4);
    CHECK(active.language == Language::Russian && active.calibration_request == 17 && active.gyro_bias_x == 1.2f);
    CHECK(switch_profile(file.c_str(), active, ProfileDomain::Graphics, ProfileKind::Default) == ProfileResult::Applied);
    CHECK(active.draw_distance_scale == 1 && active.turn_gain == 1);
    CHECK(switch_profile(file.c_str(), active, ProfileDomain::Controls, ProfileKind::Custom) == ProfileResult::Applied);
    CHECK(active.turn_gain == 0.43f && active.draw_distance_scale == 1);
    CHECK(switch_profile(file.c_str(), active, ProfileDomain::Graphics, ProfileKind::Custom) == ProfileResult::Applied);
    CHECK(active.draw_distance_scale == 1.5f && active.turn_gain == 0.43f);
    CHECK(active.calibration_request == 17 && active.gyro_bias_y == -2.1f);
    const auto before = bytes(file);
    CHECK(switch_profile(file.c_str(), active, ProfileDomain::Controls, ProfileKind::Custom) == ProfileResult::Unchanged);
    CHECK(bytes(file) == before);
}
void snapshots_survive_default_preferences() {
    const auto file = path("preferences.bin");
    auto active = custom(); CHECK(save_profiled_settings(file.c_str(), active));
    Settings defaults=default_controls_profile(); copy_profile(active, defaults, ProfileDomain::Controls); copy_profile(active, defaults, ProfileDomain::Graphics);
    CHECK(save_profiled_settings(file.c_str(), active));
    const auto controls = bytes(file + ".controls-custom"), graphics = bytes(file + ".graphics-custom");
    active.language = Language::English; active.gyro_bias_x = -4; active.calibration_request = 44;
    CHECK(save_profiled_settings(file.c_str(), active));
    CHECK(bytes(file + ".controls-custom") == controls && bytes(file + ".graphics-custom") == graphics);
    CHECK(load_custom_profile(file.c_str(), ProfileDomain::Controls, active));
    CHECK(active.turn_gain == 0.43f && active.draw_distance_scale == 1);
    CHECK(active.language == Language::English && active.gyro_bias_x == -4 && active.calibration_request == 44);
}
void invalid_custom_is_unchanged() {
    const auto file = path("invalid.bin");
    Settings active = custom(), before = active;
    CHECK(!load_custom_profile(file.c_str(), ProfileDomain::Controls, active)); CHECK(encoded_equal(active, before));
    CHECK(switch_profile(file.c_str(), active, ProfileDomain::Graphics, ProfileKind::Custom) == ProfileResult::MissingCustom);
    CHECK(encoded_equal(active, before) && active.calibration_request == 17);
    const auto sidecar = file + ".controls-custom";
    corrupt(sidecar);
    CHECK(!load_custom_profile(file.c_str(), ProfileDomain::Controls, active)); CHECK(encoded_equal(active, before));
    Settings defaults=default_controls_profile();
    CHECK(save_settings_file(sidecar.c_str(), defaults));
    CHECK(!load_custom_profile(file.c_str(), ProfileDomain::Controls, active)); CHECK(encoded_equal(active, before));
    fs::remove(sidecar + ".previous");
    CHECK(save_settings_file(sidecar.c_str(), custom())); fs::remove(sidecar + ".previous");
    auto oversized = bytes(sidecar); oversized.push_back('x'); write(sidecar, oversized);
    CHECK(!load_custom_profile(file.c_str(), ProfileDomain::Controls, active)); CHECK(encoded_equal(active, before));
    // Sidecars use the same validated codec and migrate legitimate older settings.
    auto legacy = bytes(sidecar); legacy.resize(kLegacySettingsBytes); legacy[4] = 6;
    std::uint32_t hash = 2166136261u;
    for (unsigned i = 0; i < 60; ++i) { hash ^= static_cast<unsigned char>(legacy[i]); hash *= 16777619u; }
    for (unsigned i = 0; i < 4; ++i) legacy[60+i] = static_cast<char>(hash >> (8*i));
    write(sidecar, legacy);
    active.turn_gain = 0.9f;
    CHECK(load_custom_profile(file.c_str(), ProfileDomain::Controls, active));
    CHECK(active.turn_gain == 0.43f && active.draw_distance_scale == 1.5f && active.calibration_request == 17);
    CHECK(!load_custom_profile(nullptr, ProfileDomain::Controls, active));
    CHECK(!load_custom_profile(file.c_str(), static_cast<ProfileDomain>(9), active));
    CHECK(switch_profile(file.c_str(), active, static_cast<ProfileDomain>(9), ProfileKind::Default) == ProfileResult::SaveFailed);
    CHECK(switch_profile(file.c_str(), active, ProfileDomain::Controls, static_cast<ProfileKind>(9)) == ProfileResult::SaveFailed);
}
void recovery() {
    const auto file = path("recovery.bin");
    auto active = custom(); CHECK(save_profiled_settings(file.c_str(), active));
    active.turn_gain = 0.25f; CHECK(save_profiled_settings(file.c_str(), active));
    corrupt(file + ".controls-custom");
    Settings out; CHECK(load_custom_profile(file.c_str(), ProfileDomain::Controls, out)); CHECK(out.turn_gain == 0.43f);
    // Recovering the old active configuration must preserve it when selecting Default.
    corrupt(file); Settings defaults=default_controls_profile();
    CHECK(save_profiled_settings(file.c_str(), defaults));
    CHECK(load_custom_profile(file.c_str(), ProfileDomain::Controls, out)); CHECK(out.turn_gain == 0.43f);
    corrupt(file + ".graphics-custom");
    CHECK(load_custom_profile(file.c_str(), ProfileDomain::Graphics, out)); CHECK(out.draw_distance_scale == 1.5f);
}
void failed_snapshot_preserves_active() {
    const auto file = path("failure.bin");
    auto active = custom(); CHECK(save_settings_file(file.c_str(), active));
    const auto original = bytes(file);
    fs::create_directory(file + ".controls-custom.incoming");
    Settings defaults=default_controls_profile();
    CHECK(!save_profiled_settings(file.c_str(), defaults)); CHECK(bytes(file) == original);
    CHECK(switch_profile(file.c_str(), active, ProfileDomain::Controls, ProfileKind::Default) == ProfileResult::SaveFailed);
    CHECK(active.turn_gain == 0.43f && active.language == Language::Russian && active.calibration_request == 17);
    CHECK(bytes(file) == original);
    fs::remove(file + ".controls-custom.incoming");
    fs::create_directory(file + ".graphics-custom.incoming");
    CHECK(!save_profiled_settings(file.c_str(), defaults)); CHECK(bytes(file) == original);
    fs::remove(file + ".graphics-custom.incoming");
    fs::create_directory(file + ".incoming");
    CHECK(!save_profiled_settings(file.c_str(), defaults)); CHECK(bytes(file) == original);
    Settings invalid; invalid.draw_distance_scale = std::numeric_limits<float>::infinity();
    CHECK(!save_profiled_settings(file.c_str(), invalid)); CHECK(bytes(file) == original);
    CHECK(!save_profiled_settings(nullptr, active)); CHECK(!save_profiled_settings("", active));
}
void builtin_saves_preserve_existing_custom() {
    const auto file = path("preserve.bin");
    auto active = custom(); CHECK(save_profiled_settings(file.c_str(), active));
    const auto controls = bytes(file + ".controls-custom"), graphics = bytes(file + ".graphics-custom");
    // A legacy/external active file must not replace an already saved Custom.
    active.turn_gain = 0.91f; active.draw_distance_scale = 1.75f;
    CHECK(save_settings_file(file.c_str(), active));
    Settings defaults=default_controls_profile();
    CHECK(save_profiled_settings(file.c_str(), defaults));
    CHECK(bytes(file + ".controls-custom") == controls);
    CHECK(bytes(file + ".graphics-custom") == graphics);
    CHECK(load_custom_profile(file.c_str(), ProfileDomain::Controls, defaults));
    CHECK(defaults.turn_gain == 0.43f);
    CHECK(switch_profile(file.c_str(), active, ProfileDomain::Controls, ProfileKind::RE6) == ProfileResult::Applied);
    CHECK(bytes(file + ".controls-custom") == controls);
    active.aim_gain_x = 0.25f;
    CHECK(profile_kind(active, ProfileDomain::Controls) == ProfileKind::Custom);
    CHECK(save_profiled_settings(file.c_str(), active));
    CHECK(switch_profile(file.c_str(), active, ProfileDomain::Controls, ProfileKind::Default) == ProfileResult::Applied);
    CHECK(switch_profile(file.c_str(), active, ProfileDomain::Controls, ProfileKind::Custom) == ProfileResult::Applied);
    CHECK(active.aim_gain_x == 0.25f && active.gyro_enabled && active.free_camera);
}
void native_default_and_legacy_migration() {
    auto native=default_controls_profile();
    CHECK(valid_settings(native));
    CHECK(!native.free_camera && !native.move_while_aiming && !native.move_while_knife && !native.move_while_reloading && !native.first_person);
    CHECK(native.reticle==ReticleStyle::Off && native.button_map==identity_button_map());
    CHECK(native.camera_gain==1 && native.camera_return_speed==1 && native.third_person_distance==1);
    CHECK(native.turn_gain==1 && native.aim_gain_x==1 && native.aim_gain_y==1 && native.right_aim_gain_x==1 && native.right_aim_gain_y==1);
    CHECK(profile_kind(native,ProfileDomain::Controls)==ProfileKind::Default);
    auto legacy=re6_profile();legacy.re6_response=false;legacy.aim_gain_x=legacy.aim_gain_y=0;legacy.move_while_reloading=false;
    CHECK(profile_kind(legacy,ProfileDomain::Controls)==ProfileKind::Custom);
    const auto file=path("legacy-re9.bin");
    CHECK(save_settings_file(file.c_str(),legacy));
    CHECK(switch_profile(file.c_str(),legacy,ProfileDomain::Controls,ProfileKind::Default)==ProfileResult::Applied);
    CHECK(!legacy.analog_enabled && !legacy.gyro_enabled && !legacy.remap_enabled && !legacy.unified_controls);
    CHECK(switch_profile(file.c_str(),legacy,ProfileDomain::Controls,ProfileKind::Custom)==ProfileResult::Applied);
    CHECK(legacy.unified_controls && legacy.aim_gain_x==0 && !legacy.re6_response);
    // Numeric value 2 belonged to the removed RE9 selection and cannot be applied.
    const auto before=legacy;
    CHECK(switch_profile(file.c_str(),legacy,ProfileDomain::Controls,static_cast<ProfileKind>(2))==ProfileResult::SaveFailed);
    CHECK(encoded_equal(legacy,before));
    const auto saved_custom=path("legacy-preserve.bin");
    CHECK(save_profiled_settings(saved_custom.c_str(),custom()));
    const auto snapshot=bytes(saved_custom+".controls-custom");
    CHECK(save_settings_file(saved_custom.c_str(),legacy));
    CHECK(switch_profile(saved_custom.c_str(),legacy,ProfileDomain::Controls,ProfileKind::Default)==ProfileResult::Applied);
    CHECK(bytes(saved_custom+".controls-custom")==snapshot);
    CHECK(switch_profile(saved_custom.c_str(),legacy,ProfileDomain::Controls,ProfileKind::Custom)==ProfileResult::Applied);
    CHECK(legacy.turn_gain==0.43f);
    for(const auto current:{ProfileKind::Default,ProfileKind::RE6,ProfileKind::Custom}) {
        CHECK(next_control_profile(next_control_profile(current,1),-1)==current);
        CHECK(next_control_profile(next_control_profile(next_control_profile(current,1),1),1)==current);
    }
}
void re6_recipe_and_custom_restore() {
    const auto re6 = ProfileKind::RE6;
    CHECK(static_cast<unsigned>(re6)==3);
    CHECK(!Settings{}.re6_response && re6_profile().re6_response);
    std::array<std::uint8_t,kSettingsBytes> old_wire{};
    CHECK(encode_settings(re6_profile(),old_wire.data(),old_wire.size()));
    old_wire[4]=12;old_wire[97]&=~32u;
    std::uint32_t old_hash=2166136261u;
    for(unsigned i=0;i<124;++i){old_hash^=old_wire[i];old_hash*=16777619u;}
    for(unsigned i=0;i<4;++i)old_wire[124+i]=std::uint8_t(old_hash>>(8*i));
    Settings old_re6;
    CHECK(decode_settings(old_wire.data(),old_wire.size(),old_re6));
    CHECK(old_re6.re6_response && !old_re6.move_while_reloading);
    CHECK(profile_kind(old_re6,ProfileDomain::Controls)==ProfileKind::Custom);
    auto scoped = re6_profile();
    copy_profile(scoped, custom(), ProfileDomain::Graphics);
    CHECK(scoped.re6_response && profile_kind(scoped, ProfileDomain::Controls)==re6);
    const auto file = path("re6.bin");
    auto active = custom();
    CHECK(save_settings_file(file.c_str(), active)); // Migrate a pre-preset install.
    CHECK(switch_profile(file.c_str(), active, ProfileDomain::Controls, re6) == ProfileResult::Applied);
    CHECK(profile_kind(active, ProfileDomain::Controls) == re6);
    CHECK(active.analog_enabled && active.gyro_enabled && active.free_camera && active.unified_controls);
    CHECK(active.re6_response);
    CHECK(active.move_while_aiming && active.move_while_knife && active.move_while_reloading && !active.first_person);
    CHECK(active.camera_return_speed == 0 && active.third_person_distance == 1.15f);
    CHECK(active.reticle == ReticleStyle::Crosshair && active.reticle_size == ReticleSize::Small);
    CHECK(active.turn_gain == 1 && active.camera_gain == 1 && active.aim_gain_x == 1 && active.aim_gain_y == 1);
    CHECK(active.right_aim_gain_x == 1 && active.right_aim_gain_y == 1 && active.gyro_gain_x == 1 && active.gyro_gain_y == 1);
    CHECK(active.gyro_speed == 0.8f && active.invert_x && active.invert_y);
    CHECK(active.left_deadzone == 0.05f && active.right_deadzone == 0.05f);
    const ButtonMap expected{0,1,2,3,1,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19};
    CHECK(active.remap_enabled && active.button_map == expected);
    CHECK(active.texture_filtering == TextureFilter::Aniso4 && active.draw_distance_scale == 1.5f);
    CHECK(active.language == Language::Russian && active.calibration_request == 17);
    CHECK(active.gyro_bias_x == 1.2f && active.gyro_bias_y == -2.1f);
    std::array<std::uint8_t, kSettingsBytes> wire{};
    CHECK(encode_settings(active, wire.data(), wire.size()) && wire[97] == 62);
    const auto controls = bytes(file + ".controls-custom");
    Settings restarted;
    CHECK(load_settings_file(file.c_str(), restarted));
    CHECK(restarted.move_while_reloading);
    CHECK(profile_kind(restarted, ProfileDomain::Controls) == re6);
    CHECK(switch_profile(file.c_str(), restarted, ProfileDomain::Controls, re6) == ProfileResult::Unchanged);
    CHECK(switch_profile(file.c_str(), restarted, ProfileDomain::Controls, ProfileKind::Default) == ProfileResult::Applied);
    CHECK(switch_profile(file.c_str(), restarted, ProfileDomain::Controls, re6) == ProfileResult::Applied);
    CHECK(bytes(file + ".controls-custom") == controls);
    CHECK(switch_profile(file.c_str(), restarted, ProfileDomain::Controls, ProfileKind::Custom) == ProfileResult::Applied);
    CHECK(restarted.turn_gain == 0.43f && restarted.button_map[0] == kUnboundButton);
    CHECK(switch_profile(file.c_str(), restarted, ProfileDomain::Controls, re6) == ProfileResult::Applied);
    restarted.camera_gain = 0.64f; restarted.right_aim_gain_x = 0.32f;
    CHECK(profile_kind(restarted, ProfileDomain::Controls) == ProfileKind::Custom);
    CHECK(save_profiled_settings(file.c_str(), restarted));
    CHECK(switch_profile(file.c_str(), restarted, ProfileDomain::Controls, ProfileKind::Default) == ProfileResult::Applied);
    CHECK(switch_profile(file.c_str(), restarted, ProfileDomain::Controls, ProfileKind::Custom) == ProfileResult::Applied);
    CHECK(restarted.camera_gain == 0.64f && restarted.right_aim_gain_x == 0.32f);
    CHECK(encode_settings(restarted, wire.data(), wire.size()) && (wire[97] & 16));
    const auto before = restarted;
    const auto saved = bytes(file);
    CHECK(switch_profile(file.c_str(), restarted, ProfileDomain::Graphics, re6) == ProfileResult::SaveFailed);
    CHECK(encoded_equal(restarted, before) && bytes(file) == saved);
    const auto builtin_file = path("re6-only.bin");
    CHECK(save_profiled_settings(builtin_file.c_str(), re6_profile()));
    CHECK(!fs::exists(builtin_file + ".controls-custom"));
    CHECK(save_settings_file((builtin_file + ".controls-custom").c_str(), re6_profile()));
    CHECK(!load_custom_profile(builtin_file.c_str(), ProfileDomain::Controls, restarted));
    CHECK(encoded_equal(restarted, before));
}
}
int main() {
    fs::remove_all(root); fs::create_directories(root);
    domain_copy_and_identity(); migration_and_restore(); snapshots_survive_default_preferences();
    invalid_custom_is_unchanged(); recovery(); failed_snapshot_preserves_active();
    builtin_saves_preserve_existing_custom(); native_default_and_legacy_migration();
    re6_recipe_and_custom_restore();
    fs::remove_all(root);
    if (failures) { std::fprintf(stderr, "%d profile checks failed\n", failures); return 1; }
    std::puts("profiles: independent domains, canonical identity, migration, recovery and failed writes PASS");
}

