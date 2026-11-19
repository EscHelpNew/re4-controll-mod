#pragma once
#include <cstddef>

namespace re4 {
// Only the three fixed Atmosphere crash/fatal folders are read. No files are
// created or changed. Returns bytes written excluding the terminating NUL.
inline constexpr std::size_t kSystemReportFileLimit=64*1024;
inline constexpr std::size_t kSystemReportLimit=350*1024;
std::size_t append_system_reports(char* out,std::size_t capacity,const char* sd_root);
}
