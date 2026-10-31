#pragma once
#include <cstddef>
namespace re4 {
inline constexpr std::size_t kSupportReportLimit=4*1024*1024;
// Fixed diagnostic directories only. No arbitrary remote path access. The
// destination must be new; completion is published by renaming its temp file.
bool write_support_report(const char* sd_root,const char* destination,const char* version);
}
