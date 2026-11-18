#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
namespace re4 {
inline constexpr std::size_t kAppJournalFileLimit=64*1024;
inline constexpr unsigned kAppJournalCount=5;
inline constexpr std::size_t kAppJournalReportLimit=kAppJournalCount*kAppJournalFileLimit+2048;
class AppJournal {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    AppJournal(const char* root,const char* version,unsigned hos,unsigned applet,
        std::uint64_t pid,bool recovery);
    ~AppJournal();
    AppJournal(const AppJournal&)=delete;
    AppJournal& operator=(const AppJournal&)=delete;
    void note(const char* fixed_stage,std::uint32_t rc=0);
    // Exact returned byte count excludes the terminator. No files except the
    // five fixed app-report-N.log paths are read; errors remain nonfatal.
    static std::size_t report(char* out,std::size_t cap,void* context);
};
}
