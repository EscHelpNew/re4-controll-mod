#include "re4/app_journal.hpp"
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#ifdef RE4_APP_JOURNAL_TEST_SYNC
#include <cerrno>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#endif
namespace fs=std::filesystem;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"journal line%d: %s\n",__LINE__,#x);std::exit(1);}}while(false)
namespace {
fs::path from_utf8(const std::string& s){
#if defined(__cpp_char8_t)
 return fs::path(std::u8string(s.begin(),s.end()));
#else
 return fs::u8path(s);
#endif
}
std::string utf8(const fs::path& p){const auto s=p.u8string();return std::string(s.begin(),s.end());}
std::string read(const fs::path& p){std::ifstream f(p,std::ios::binary);return std::string(std::istreambuf_iterator<char>(f),{});}
void write(const fs::path& p,const std::string& text){std::ofstream f(p,std::ios::binary);CHECK(bool(f));f.write(text.data(),std::streamsize(text.size()));CHECK(bool(f));}
struct Fixture {
 fs::path root;
 Fixture(){
  static unsigned serial=0;
  root=fs::absolute(fs::path(".superpowers/sdd/2026-11-15-app-journal")/from_utf8("host-\xD0\x96\xD1\x83\xD1\x80\xD0\xBD\xD0\xB0\xD0\xBB-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(++serial)));
  CHECK(fs::create_directories(root));
 }
 ~Fixture(){
  std::error_code ec;
  for(unsigned i=0;i<5;++i)fs::remove(file(i),ec);
  for(const char* name:{"token.txt","argv.txt","credentials.keys","outside.txt","app-report-9.log","unrelated.log","root-is-file.txt"})fs::remove(root/name,ec);
  fs::remove(root,ec);
 }
 fs::path file(unsigned i)const{return root/("app-report-"+std::to_string(i)+".log");}
 std::string report(re4::AppJournal& j,std::size_t cap=re4::kAppJournalReportLimit+1){
  std::vector<char> out(cap+16,char(0x6D));const auto n=re4::AppJournal::report(out.data(),cap,&j);
  CHECK(n<=re4::kAppJournalReportLimit&&n<cap&&out[n]=='\0');
  for(std::size_t i=cap;i<out.size();++i)CHECK(out[i]==char(0x6D));
  return std::string(out.data(),n);
 }
};
}
#ifdef RE4_APP_JOURNAL_TEST_SYNC
namespace {
unsigned sync_calls=0,flush_checks=0;
bool fail_next_sync=false,inspect_sync=false;
fs::path sync_path;
std::string expected_flushed_bytes;
}
extern "C" int re4_app_journal_test_sync(FILE* file){
 ++sync_calls;
 if(inspect_sync){
  CHECK(read(sync_path).find(expected_flushed_bytes)!=std::string::npos);
  ++flush_checks;
 }
 if(fail_next_sync){fail_next_sync=false;errno=ENOSPC;return -1;}
#ifdef _WIN32
 return _commit(_fileno(file));
#else
 return fsync(fileno(file));
#endif
}
#endif
int main(){
#ifdef RE4_APP_JOURNAL_TEST_SYNC
 {
  Fixture f;const auto root=utf8(f.root);re4::AppJournal j(root.c_str(),"sync-order",0,0,0,false);
  sync_path=f.file(0);expected_flushed_bytes="stage=sync_probe rc=0x00000023\n";inspect_sync=true;
  const auto before=sync_calls;j.note("sync_probe",35);
  CHECK(sync_calls==before+1&&flush_checks>0);inspect_sync=false;
  expected_flushed_bytes="stage=sync_failure rc=0x00000007\n";inspect_sync=true;fail_next_sync=true;
  j.note("sync_failure",7);inspect_sync=false;
  const auto stored=read(f.file(0));const auto text=f.report(j);
  CHECK(text.find("journal_error="+std::to_string(ENOSPC))!=std::string::npos);
  const auto calls_after_failure=sync_calls;j.note("after_sync_failure",9);
  CHECK(sync_calls==calls_after_failure&&read(f.file(0))==stored);
  CHECK(f.report(j).find("last_stage=after_sync_failure")!=std::string::npos);
 }
 {
  Fixture f;const auto root=utf8(f.root);fail_next_sync=true;
  re4::AppJournal j(root.c_str(),"startup-sync-failure",0,0,0,false);
  j.note("startup_continues",11);
  const auto text=f.report(j);
  CHECK(text.find("journal_error="+std::to_string(ENOSPC))!=std::string::npos);
  CHECK(text.find("last_stage=startup_continues")!=std::string::npos);
 }
#endif

 {
  Fixture f;const auto root=utf8(f.root);
  re4::AppJournal j(root.c_str(),"0.28.14",123,2,456,true);
  CHECK(fs::is_regular_file(f.file(0)));
  j.note("sdl_ready",0x1234);j.note("clean_exit");
  const auto text=read(f.file(0));CHECK(text.find("version=0.28.14")!=std::string::npos);
  CHECK(text.find("hos=123")!=std::string::npos&&text.find("applet=2")!=std::string::npos);
  CHECK(text.find("pid=456")!=std::string::npos&&text.find("recovery=1")!=std::string::npos);
  CHECK(text.find("stage=sdl_ready rc=0x00001234\n")!=std::string::npos);
  CHECK(text.find("stage=clean_exit rc=0x00000000\n")!=std::string::npos);
  CHECK(f.report(j).find("clean_exit")!=std::string::npos);
 }
 {
  Fixture f;const auto root=utf8(f.root);write(f.root/"unrelated.log","unrelated-retained");
  for(unsigned start=0;start<6;++start){
   re4::AppJournal j(root.c_str(),("run-"+std::to_string(start)).c_str(),0,0,start,false);
   j.note("startup_done");
  }
  for(unsigned i=0;i<5;++i){const auto text=read(f.file(i));CHECK(text.find("version=run-"+std::to_string(5-i))!=std::string::npos);CHECK(text.find("run-0")==std::string::npos);CHECK(fs::file_size(f.file(i))<=65536);}
  CHECK(read(f.root/"unrelated.log")=="unrelated-retained");CHECK(!fs::exists(f.file(5)));
 }
 {
  Fixture f;const auto root=utf8(f.root);re4::AppJournal j(root.c_str(),"cap",0,0,0,false);
  const std::string line="stage=tail rc=0x00000000\n";
  write(f.file(0),std::string(65536-line.size(),'X'));j.note("tail");
  CHECK(fs::file_size(f.file(0))==65536);CHECK(read(f.file(0)).substr(65536-line.size())==line);
  for(unsigned i=0;i<100;++i)j.note("overflow",1);
  CHECK(fs::file_size(f.file(0))==65536);CHECK(read(f.file(0)).find("overflow")==std::string::npos);
 }
 {
  Fixture f;const auto root=utf8(f.root);re4::AppJournal j(root.c_str(),"bounded",0,0,0,false);
  for(unsigned i=0;i<5;++i)write(f.file(i),std::string(65536+128,char('A'+i))+"DO_NOT_READ_TAIL");
  auto all=f.report(j);CHECK(all.size()<=5*65536+2048);CHECK(all.find("DO_NOT_READ_TAIL")==std::string::npos);
  for(unsigned i=0;i<5;++i)CHECK(all.find("app-report-"+std::to_string(i)+".log")!=std::string::npos);
  for(std::size_t cap:{1u,2u,17u,64u,257u})CHECK(f.report(j,cap).size()<cap);
  fs::remove(f.file(1));fs::remove(f.file(3));CHECK(f.report(j).find("missing")!=std::string::npos);
  CHECK(re4::AppJournal::report(nullptr,0,&j)==0);CHECK(re4::AppJournal::report(nullptr,30,&j)==0);
  char out[8]{};CHECK(re4::AppJournal::report(out,sizeof(out),nullptr)==0);
 }
 {
  Fixture f;const auto root=utf8(f.root);re4::AppJournal j(root.c_str(),"safe",0,0,0,false);
  for(const char* name:{"token.txt","argv.txt","credentials.keys","app-report-9.log"})write(f.root/name,"PRIVATE_SECRET_NEVER_EXPORT");
  j.note("ok\nPRIVATE_STAGE_INJECTION");j.note("bad\rPRIVATE_STAGE_INJECTION");
  j.note(std::string(300,'Z').c_str());j.note(nullptr);j.note("after_rejection");
  const auto all=f.report(j);CHECK(all.find("PRIVATE_")==std::string::npos);
  CHECK(all.find("invalid_stage")!=std::string::npos&&all.find("after_rejection")!=std::string::npos);
  CHECK(all.find("token.txt")==std::string::npos&&all.find("credentials.keys")==std::string::npos);
 }
 {
  Fixture f;write(f.root/"root-is-file.txt","do-not-touch");const auto root=utf8(f.root/"root-is-file.txt");
  re4::AppJournal j(root.c_str(),"failure",0,0,0,true);j.note("ui_failed",3);
  CHECK(read(f.root/"root-is-file.txt")=="do-not-touch");const auto text=f.report(j);
  CHECK(text.find("ui_failed")!=std::string::npos&&text.find("journal_error=")!=std::string::npos);
 }
 {
  Fixture f;CHECK(fs::create_directory(f.file(4)));const auto root=utf8(f.root);
  re4::AppJournal j(root.c_str(),"directory-obstacle",0,0,0,false);j.note("io_failed");
  CHECK(fs::is_directory(f.file(4)));CHECK(f.report(j).find("io_failed")!=std::string::npos);
 }
 {
  Fixture f;const auto root=utf8(f.root);re4::AppJournal j(root.c_str(),"changed-path",0,0,0,false);
  fs::remove(f.file(0));CHECK(fs::create_directory(f.file(0)));j.note("append_failed");
  CHECK(fs::is_directory(f.file(0)));CHECK(f.report(j).find("append_failed")!=std::string::npos);
 }
 {
  Fixture f;write(f.root/"outside.txt","PRIVATE_LINK_TARGET");std::error_code ec;
  fs::create_symlink(f.root/"outside.txt",f.file(4),ec);
  if(!ec){
   const auto root=utf8(f.root);re4::AppJournal j(root.c_str(),"link",0,0,0,false);j.note("safe_failure");
   CHECK(read(f.root/"outside.txt")=="PRIVATE_LINK_TARGET");CHECK(f.report(j).find("PRIVATE_LINK_TARGET")==std::string::npos);
  }
 }
 {
  Fixture f;const auto root=utf8(f.root);re4::AppJournal j(root.c_str(),"threads",0,0,0,false);
  std::thread producer([&]{for(unsigned i=0;i<100;++i)j.note("bounded_event",i);});
  for(unsigned i=0;i<100;++i){CHECK(!f.report(j,4096).empty());}
  producer.join();
  CHECK(fs::file_size(f.file(0))<=65536);
 }
 std::puts("own app journal: five-file rotation, exact cap, bounded export, stage safety and nonfatal IO PASS");
}
