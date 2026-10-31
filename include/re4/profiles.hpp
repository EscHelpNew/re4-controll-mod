#pragma once
#include "re4/settings.hpp"

namespace re4 {
enum class ProfileDomain { Controls, Graphics };
enum class ProfileKind { Default = 0, Custom = 1, RE6 = 3 };
enum class ProfileResult { Applied, Unchanged, MissingCustom, SaveFailed };

Settings default_controls_profile();
ProfileKind next_control_profile(ProfileKind current, int direction);
Settings re6_profile();
void copy_profile(Settings& destination, const Settings& source, ProfileDomain domain);
bool same_profile(const Settings& first, const Settings& second, ProfileDomain domain);
ProfileKind profile_kind(const Settings& settings, ProfileDomain domain);
bool save_profiled_settings(const char* path, const Settings& settings);
bool load_custom_profile(const char* path, ProfileDomain domain, Settings& out);
ProfileResult switch_profile(const char* path, Settings& active, ProfileDomain domain, ProfileKind kind);
}
