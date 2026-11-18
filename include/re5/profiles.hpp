#pragma once
#include "re5/settings.hpp"
#include "re4/profiles.hpp"
namespace re5 {
re4::Settings graphics_view(const Settings&);
re4::ProfileKind profile_kind(const Settings&,re4::ProfileDomain,unsigned player);
bool save_edited_settings(const char*,const Settings&,re4::ProfileDomain,unsigned player);
re4::ProfileResult switch_profile(const char*,Settings&,re4::ProfileDomain,re4::ProfileKind,unsigned player);
}
