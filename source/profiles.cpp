#include "re4/profiles.hpp"
#include "re4/settings_file.hpp"
#include <array>
#include <string>

namespace re4 {
namespace {
bool valid_domain(ProfileDomain domain) {
    return domain == ProfileDomain::Controls || domain == ProfileDomain::Graphics;
}
void copy_graphics(Settings& destination, const Settings& source) {
    destination.texture_filtering = source.texture_filtering;
    destination.shadow_mode = source.shadow_mode;
    destination.anti_aliasing = source.anti_aliasing;
    destination.draw_distance_scale = source.draw_distance_scale;
}
std::string snapshot_path(const char* path, ProfileDomain domain) {
    return std::string(path) + (domain == ProfileDomain::Controls ? ".controls-custom" : ".graphics-custom");
}
}
Settings default_controls_profile() {
    Settings settings;
    settings.analog_enabled = false;
    settings.gyro_enabled = false;
    settings.remap_enabled = false;
    settings.button_map = identity_button_map();
    settings.free_camera = false;
    settings.move_while_aiming = false;
    settings.move_while_knife = false;
    settings.move_while_reloading = false;
    settings.unified_controls = false;
    settings.re6_response = false;
    settings.first_person = false;
    settings.reticle = ReticleStyle::Off;
    settings.turn_gain = settings.camera_gain = settings.camera_return_speed = 1;
    settings.aim_gain_x = settings.aim_gain_y = 1;
    settings.right_aim_gain_x = settings.right_aim_gain_y = 1;
    settings.movement_gain = settings.third_person_distance = 1;
    return settings;
}
ProfileKind next_control_profile(ProfileKind current, int direction) {
    constexpr ProfileKind kinds[]{ProfileKind::Default, ProfileKind::RE6, ProfileKind::Custom};
    unsigned index = 0;
    for (unsigned i = 0; i < 3; ++i) if (kinds[i] == current) index = i;
    return kinds[(index + (direction < 0 ? 2 : 1)) % 3];
}
Settings re6_profile() {
    Settings settings;
    settings.analog_enabled = true;
    settings.remap_enabled = true;
    settings.gyro_enabled = true;
    settings.free_camera = true;
    settings.camera_return_speed = 0;
    settings.third_person_distance = 1.15f;
    settings.move_while_aiming = true;
    settings.move_while_knife = true;
    settings.move_while_reloading = true;
    settings.unified_controls = true;
    settings.reticle = ReticleStyle::Crosshair;
    settings.re6_response = true;
    settings.aim_gain_x = 1;
    settings.aim_gain_y = 1;
    // Physical Switch L3 -> B: hold run; other native actions retain identity.
    settings.button_map[4] = 1;
    return settings;
}
void copy_profile(Settings& destination, const Settings& source, ProfileDomain domain) {
    if (domain == ProfileDomain::Graphics) {
        copy_graphics(destination, source);
    } else if (domain == ProfileDomain::Controls) {
        const Settings preserved = destination;
        // Copy the whole structure so future gameplay fields belong to Controls.
        destination = source;
        copy_graphics(destination, preserved);
        destination.language = preserved.language;
        destination.calibration_request = preserved.calibration_request;
        destination.gyro_bias_x = preserved.gyro_bias_x;
        destination.gyro_bias_y = preserved.gyro_bias_y;
    }
}
bool same_profile(const Settings& first, const Settings& second, ProfileDomain domain) {
    if (!valid_domain(domain)) return false;
    Settings normalized_first, normalized_second;
    copy_profile(normalized_first, first, domain);
    copy_profile(normalized_second, second, domain);
    std::array<std::uint8_t, kSettingsBytes> first_bytes{}, second_bytes{};
    return encode_settings(normalized_first, first_bytes.data(), first_bytes.size())
        && encode_settings(normalized_second, second_bytes.data(), second_bytes.size())
        && first_bytes == second_bytes;
}
ProfileKind profile_kind(const Settings& settings, ProfileDomain domain) {
    if (same_profile(settings, domain == ProfileDomain::Controls ? default_controls_profile() : Settings{}, domain)) return ProfileKind::Default;
    if (domain == ProfileDomain::Controls && same_profile(settings, re6_profile(), domain)) return ProfileKind::RE6;
    return ProfileKind::Custom;
}
bool save_profiled_settings(const char* path, const Settings& settings) {
    if (!path || !*path || !valid_settings(settings)) return false;
    Settings previous;
    const bool have_previous = load_settings_file(path, previous);
    for (const auto domain : {ProfileDomain::Controls, ProfileDomain::Graphics}) {
        const Settings* snapshot = nullptr;
        if (profile_kind(settings, domain) == ProfileKind::Custom) snapshot = &settings;
        else if (have_previous && profile_kind(previous, domain) == ProfileKind::Custom) {
            Settings existing;
            // Migrate an installation without a Custom sidecar once. Selecting a
            // built-in must never replace an already recoverable Custom profile.
            if (!load_custom_profile(path, domain, existing)) snapshot = &previous;
        }
        if (snapshot && !save_settings_file(snapshot_path(path, domain).c_str(), *snapshot)) return false;
    }
    // Sidecars are individually recoverable, not a multi-file transaction.
    return save_settings_file(path, settings);
}
bool load_custom_profile(const char* path, ProfileDomain domain, Settings& out) {
    if (!path || !*path || !valid_domain(domain)) return false;
    const auto sidecar = snapshot_path(path, domain);
    Settings snapshot;
    if (!load_settings_file(sidecar.c_str(), snapshot)
        || profile_kind(snapshot, domain) != ProfileKind::Custom) return false;
    copy_profile(out, snapshot, domain);
    return true;
}
ProfileResult switch_profile(const char* path, Settings& active, ProfileDomain domain, ProfileKind kind) {
    if (!valid_domain(domain)
        || (kind != ProfileKind::Default && kind != ProfileKind::Custom && kind != ProfileKind::RE6)
        || ((kind == ProfileKind::RE6) && domain != ProfileDomain::Controls))
        return ProfileResult::SaveFailed;
    Settings next = active;
    if (kind == ProfileKind::Default) copy_profile(next, domain == ProfileDomain::Controls ? default_controls_profile() : Settings{}, domain);
    else if (kind == ProfileKind::RE6) copy_profile(next, re6_profile(), domain);
    else if (!load_custom_profile(path, domain, next)) return ProfileResult::MissingCustom;
    if (same_profile(active, next, domain)) return ProfileResult::Unchanged;
    if (!save_profiled_settings(path, next)) return ProfileResult::SaveFailed;
    active = next;
    return ProfileResult::Applied;
}
}
