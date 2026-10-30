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
    Settings active = custom(), defaults;
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
    Settings preferences;
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
    Settings defaults; copy_profile(active, defaults, ProfileDomain::Controls); copy_profile(active, defaults, ProfileDomain::Graphics);
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
    Settings defaults;
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
    corrupt(file); Settings defaults;
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
    Settings defaults;
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
void re9_recipe_switch_and_restart() {
    const auto file = path("re9.bin");
    const auto re9 = ProfileKind::RE9;
    auto active = custom();
    CHECK(save_settings_file(file.c_str(), active));
    CHECK(switch_profile(file.c_str(), active, ProfileDomain::Controls, re9) == ProfileResult::Applied);
    CHECK(active.analog_enabled && active.gyro_enabled && active.free_camera);
    CHECK(active.camera_return_speed == 0 && !active.first_person);
    CHECK(active.third_person_distance == 1.35f);
    CHECK(active.move_while_aiming && active.move_while_knife);
    CHECK(active.reticle == ReticleStyle::Crosshair && active.reticle_size == ReticleSize::Small);
    CHECK(active.aim_gain_x == 0 && active.aim_gain_y == 0);
    CHECK(active.right_aim_gain_x == 1 && active.right_aim_gain_y == 1);
    CHECK(active.gyro_speed == 0.8f && active.gyro_gain_x == 1 && active.gyro_gain_y == 1);
    CHECK(active.turn_gain == 1 && active.camera_gain == 1 && active.movement_gain == 1);
    CHECK(active.left_deadzone == 0.05f && active.right_deadzone == 0.05f && active.gyro_deadzone_dps == 0.5f);
    CHECK(active.invert_x && active.invert_y && active.remap_enabled && active.stick_curve == StickCurve::Linear);
    const ButtonMap expected{1,3,10,1,1,5,6,7,8,9,11,2,12,13,14,15,16,17,18,19};
    CHECK(active.button_map == expected);
    CHECK(active.texture_filtering == TextureFilter::Aniso4 && active.draw_distance_scale == 1.5f);
    CHECK(active.language == Language::Russian && active.calibration_request == 17);
    CHECK(active.gyro_bias_x == 1.2f && active.gyro_bias_y == -2.1f);
    CHECK(profile_kind(active, ProfileDomain::Controls) == re9);
    CHECK(profile_kind(active, ProfileDomain::Graphics) == ProfileKind::Custom);
    Settings restarted;
    CHECK(load_settings_file(file.c_str(), restarted));
    CHECK(profile_kind(restarted, ProfileDomain::Controls) == re9);
    CHECK(restarted.third_person_distance == 1.35f);
    const auto saved = bytes(file), controls = bytes(file + ".controls-custom");
    CHECK(switch_profile(file.c_str(), restarted, ProfileDomain::Controls, re9) == ProfileResult::Unchanged);
    CHECK(bytes(file) == saved && bytes(file + ".controls-custom") == controls);
    CHECK(switch_profile(file.c_str(), restarted, ProfileDomain::Controls, ProfileKind::Default) == ProfileResult::Applied);
    CHECK(switch_profile(file.c_str(), restarted, ProfileDomain::Controls, re9) == ProfileResult::Applied);
    CHECK(bytes(file + ".controls-custom") == controls);
    CHECK(switch_profile(file.c_str(), restarted, ProfileDomain::Controls, ProfileKind::Custom) == ProfileResult::Applied);
    CHECK(restarted.turn_gain == 0.43f && restarted.button_map[0] == kUnboundButton);
    CHECK(restarted.texture_filtering == TextureFilter::Aniso4 && restarted.language == Language::Russian);
}
void builtin_saves_preserve_existing_custom() {
    const auto file = path("preserve.bin");
    auto active = custom(); CHECK(save_profiled_settings(file.c_str(), active));
    const auto controls = bytes(file + ".controls-custom"), graphics = bytes(file + ".graphics-custom");
    // A legacy/external active file must not replace an already saved Custom.
    active.turn_gain = 0.91f; active.draw_distance_scale = 1.75f;
    CHECK(save_settings_file(file.c_str(), active));
    Settings defaults;
    CHECK(save_profiled_settings(file.c_str(), defaults));
    CHECK(bytes(file + ".controls-custom") == controls);
    CHECK(bytes(file + ".graphics-custom") == graphics);
    CHECK(load_custom_profile(file.c_str(), ProfileDomain::Controls, defaults));
    CHECK(defaults.turn_gain == 0.43f);
    CHECK(switch_profile(file.c_str(), active, ProfileDomain::Controls, ProfileKind::RE9) == ProfileResult::Applied);
    CHECK(bytes(file + ".controls-custom") == controls);
    active.aim_gain_x = 0.25f;
    CHECK(profile_kind(active, ProfileDomain::Controls) == ProfileKind::Custom);
    CHECK(save_profiled_settings(file.c_str(), active));
    CHECK(switch_profile(file.c_str(), active, ProfileDomain::Controls, ProfileKind::Default) == ProfileResult::Applied);
    CHECK(switch_profile(file.c_str(), active, ProfileDomain::Controls, ProfileKind::Custom) == ProfileResult::Applied);
    CHECK(active.aim_gain_x == 0.25f && active.gyro_enabled && active.free_camera);
}
void re9_rejection_and_failed_save() {
    const auto file = path("re9-failure.bin");
    auto active = custom(); CHECK(save_settings_file(file.c_str(), active));
    const auto original = bytes(file);
    const auto before = active;
    CHECK(switch_profile(file.c_str(), active, ProfileDomain::Graphics, ProfileKind::RE9) == ProfileResult::SaveFailed);
    CHECK(encoded_equal(active, before) && bytes(file) == original);
    CHECK(!fs::exists(file + ".controls-custom") && !fs::exists(file + ".graphics-custom"));
    fs::create_directory(file + ".incoming");
    CHECK(switch_profile(file.c_str(), active, ProfileDomain::Controls, ProfileKind::RE9) == ProfileResult::SaveFailed);
    CHECK(encoded_equal(active, before) && active.calibration_request == before.calibration_request && bytes(file) == original);
    fs::remove(file + ".incoming");
    CHECK(switch_profile(file.c_str(), active, ProfileDomain::Controls, ProfileKind::RE9) == ProfileResult::Applied);
    CHECK(profile_kind(active, ProfileDomain::Controls) == ProfileKind::RE9);
}
void re9_canonical_identity() {
    Settings active = re9_profile();
    CHECK(valid_settings(active));
    CHECK(profile_kind(active, ProfileDomain::Controls) == ProfileKind::RE9);
    CHECK(profile_kind(active, ProfileDomain::Graphics) == ProfileKind::Default);
    active.language = Language::Russian; active.calibration_request = 99;
    active.gyro_bias_x = 3; active.gyro_bias_y = -4;
    active.texture_filtering = TextureFilter::Aniso4; active.draw_distance_scale = 1.5f;
    CHECK(profile_kind(active, ProfileDomain::Controls) == ProfileKind::RE9);
    CHECK(profile_kind(active, ProfileDomain::Graphics) == ProfileKind::Custom);
    active.right_aim_gain_x = 0.999f;
    CHECK(profile_kind(active, ProfileDomain::Controls) == ProfileKind::RE9);
    active.right_aim_gain_x = 0.994f;
    CHECK(profile_kind(active, ProfileDomain::Controls) == ProfileKind::Custom);
    active = re9_profile(); active.third_person_distance = 1.45f;
    CHECK(profile_kind(active, ProfileDomain::Controls) == ProfileKind::Custom);
    active = re9_profile(); active.button_map[7] = kUnboundButton;
    CHECK(profile_kind(active, ProfileDomain::Controls) == ProfileKind::Custom);
    const auto file = path("builtin-sidecar.bin");
    CHECK(save_settings_file((file + ".controls-custom").c_str(), re9_profile()));
    const auto before = custom(); active = before;
    CHECK(!load_custom_profile(file.c_str(), ProfileDomain::Controls, active));
    CHECK(encoded_equal(active, before) && active.calibration_request == before.calibration_request);
    // A built-in-only save creates no false Custom snapshot.
    CHECK(save_profiled_settings(path("builtin-only.bin").c_str(), re9_profile()));
    CHECK(!fs::exists(path("builtin-only.bin") + ".controls-custom"));
    CHECK(!fs::exists(path("builtin-only.bin") + ".graphics-custom"));
}
}
int main() {
    fs::remove_all(root); fs::create_directories(root);
    domain_copy_and_identity(); migration_and_restore(); snapshots_survive_default_preferences();
    invalid_custom_is_unchanged(); recovery(); failed_snapshot_preserves_active();
    re9_recipe_switch_and_restart(); builtin_saves_preserve_existing_custom(); re9_rejection_and_failed_save(); re9_canonical_identity();
    fs::remove_all(root);
    if (failures) { std::fprintf(stderr, "%d profile checks failed\n", failures); return 1; }
    std::puts("profiles: independent domains, canonical identity, migration, recovery and failed writes PASS");
}

