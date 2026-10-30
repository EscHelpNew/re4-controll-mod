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
    return same_profile(settings, Settings{}, domain) ? ProfileKind::Default : ProfileKind::Custom;
}
bool save_profiled_settings(const char* path, const Settings& settings) {
    if (!path || !*path || !valid_settings(settings)) return false;
    Settings previous;
    const bool have_previous = load_settings_file(path, previous);
    for (const auto domain : {ProfileDomain::Controls, ProfileDomain::Graphics}) {
        const Settings* snapshot = nullptr;
        if (profile_kind(settings, domain) == ProfileKind::Custom) snapshot = &settings;
        else if (have_previous && profile_kind(previous, domain) == ProfileKind::Custom) snapshot = &previous;
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
    if (!valid_domain(domain) || (kind != ProfileKind::Default && kind != ProfileKind::Custom))
        return ProfileResult::SaveFailed;
    Settings next = active;
    if (kind == ProfileKind::Default) copy_profile(next, Settings{}, domain);
    else if (!load_custom_profile(path, domain, next)) return ProfileResult::MissingCustom;
    if (same_profile(active, next, domain)) return ProfileResult::Unchanged;
    if (!save_profiled_settings(path, next)) return ProfileResult::SaveFailed;
    active = next;
    return ProfileResult::Applied;
}
}
