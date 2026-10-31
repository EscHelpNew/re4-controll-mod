#include "re4/support_report.hpp"
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <string>
#define CHECK(c) do {if(!(c)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#c);std::exit(1);}}while(0)
namespace fs=std::filesystem;
void put(const fs::path& p,const std::string& value){fs::create_directories(p.parent_path());std::ofstream(p,std::ios::binary)<<value;}
std::string get(const fs::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(){
 const auto root=fs::path("build/support-report-fixture");fs::remove_all(root);fs::create_directories(root);
 put(root/"atmosphere/crash_reports/001_01002a000cd48000.log","RE6 failure, \"quoted\"\nline2\n");
 put(root/"atmosphere/crash_reports/999_420000000007e51a.log","Old unrelated crash\n");
 put(root/"switch/re4-controls/token.txt","PRIVATE_TEST_SENTINEL");
 put(root/"switch/re4-controls/telemetry/test.csv.part","#capture\nPartial frame\n");
 put(root/"switch/re4-controls/reload-bridge.log","installed_game=RE6\nprogram_main_build=TEST\n");
 const auto output=root/"switch/re4-controls/telemetry/support-test.csv";
 CHECK(re4::write_support_report(root.string().c_str(),output.string().c_str(),"0.11.1"));
 const auto text=get(output);
 CHECK(text.find("#re4_controls_support")!=std::string::npos&&text.find("0.11.1")!=std::string::npos);
 CHECK(text.find("001_01002a000cd48000.log")!=std::string::npos&&text.find("RE6 failure, \"\"quoted\"\"")!=std::string::npos);
 CHECK(text.find("test.csv.part")!=std::string::npos&&text.find("Partial frame")!=std::string::npos);
 CHECK(text.find("program_main_build=TEST")!=std::string::npos&&text.find("PRIVATE_TEST_SENTINEL")==std::string::npos);
 CHECK(text.size()<re4::kSupportReportLimit);
 CHECK(!re4::write_support_report(root.string().c_str(),output.string().c_str(),"0.11.2")&&get(output)==text);
 CHECK(!re4::write_support_report(root.string().c_str(),(root/"missing/out.csv").string().c_str(),"0.11.1"));
 fs::remove_all(root);std::puts("support report tests passed");
}
