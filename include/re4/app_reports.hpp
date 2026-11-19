#pragma once
#include "re4/app_journal.hpp"
#include "re4/system_reports.hpp"
#include "re4/local_update_server.hpp"
#include <algorithm>
namespace re4 {
// This context is used only by the explicitly started local receiver. Merely
// constructing it performs no IO, service initialization or background work.
struct AppReports {
    AppJournal& journal;
    const char* sd_root;
    static constexpr std::size_t capacity_limit=kLocalUpdateReportCapacity;
    static_assert(kAppJournalReportLimit+kSystemReportLimit<capacity_limit,"combined report must fit transport");
    static std::size_t report(char* out,std::size_t capacity,void* context){
        if(!out||!capacity)return 0;
        out[0]='\0';
        if(!context)return 0;
        capacity=std::min(capacity,capacity_limit);
        const auto& reports=*static_cast<AppReports*>(context);
        const auto own=AppJournal::report(out,capacity,&reports.journal);
        if(own>=capacity-1)return own;
        return own+append_system_reports(out+own,capacity-own,reports.sd_root);
    }
};
}
