#include "re4/app_reports.hpp"
#include <cstdio>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#define CHECK(e) do { if(!(e)){std::fprintf(stderr,"FAIL line%d: %s\n",__LINE__,#e);std::exit(1);} } while(0)
namespace fs=std::filesystem;
int main(){
    const auto root=fs::temp_directory_path()/("re4-app-report-callback30-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    CHECK(!fs::exists(root));fs::create_directories(root/"switch/re4-controls");
    fs::create_directories(root/"atmosphere/crash_reports");
    const std::string sample="synthetic-system-fault-for-callback-test\n";
    {std::ofstream f(root/"atmosphere/crash_reports/test.log",std::ios::binary);f<<sample;}
    const auto own=(root/"switch/re4-controls").string(),sd=root.string();
    re4::AppJournal journal(own.c_str(),"callback-test",0,0,30,false);
    journal.note("renderer_ready");
    re4::AppReports reports{journal,sd.c_str()};
    std::vector<char> buffer(1024*1024+16,char(0x6d));
    const auto size=re4::AppReports::report(buffer.data(),1024*1024,&reports);
    CHECK(size<1024*1024&&buffer[size]==0);
    const std::string text(buffer.data(),size);
    CHECK(text.find("stage=renderer_ready")!=std::string::npos);
    CHECK(text.find(sample)!=std::string::npos);
    for(std::size_t i=1024*1024;i<buffer.size();++i)CHECK(buffer[i]==char(0x6d));
    for(const auto capacity:{1u,2u,15u,127u,4096u}){
        std::vector<char> small(capacity+16,char(0x3c));
        const auto n=re4::AppReports::report(small.data(),capacity,&reports);
        CHECK(n<capacity&&small[n]==0);
        for(std::size_t i=capacity;i<small.size();++i)CHECK(small[i]==char(0x3c));
    }
    CHECK(re4::AppReports::report(nullptr,0,&reports)==0);
    char small[8]{};CHECK(re4::AppReports::report(small,sizeof(small),nullptr)==0);
    fs::remove(root/"atmosphere/crash_reports/test.log");
    const auto n=re4::AppReports::report(buffer.data(),1024*1024,&reports);
    CHECK(std::string(buffer.data(),n).find("stage=renderer_ready")!=std::string::npos);
    CHECK(fs::exists(root/"switch/re4-controls/app-report-0.log"));
    // Delete only this fixture's exact known files and empty directories.
    fs::remove(root/"switch/re4-controls/app-report-0.log");
    fs::remove(root/"switch/re4-controls");fs::remove(root/"switch");
    fs::remove(root/"atmosphere/crash_reports");fs::remove(root/"atmosphere");fs::remove(root);
    std::puts("App download: own journal plus fixed system snapshot, bounded and manual callback PASS");
}
