#include "re4/system_reports.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winioctl.h>
#endif

namespace fs=std::filesystem;
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"system reports line %d: %s\n",__LINE__,#x);std::exit(1);}} while(false)
namespace {
fs::path from_utf8(const std::string& text){
#if defined(__cpp_char8_t)
 return fs::path(std::u8string(text.begin(),text.end()));
#else
 return fs::u8path(text);
#endif
}
std::string utf8(const fs::path& path){const auto value=path.u8string();return {value.begin(),value.end()};}
std::string read(const fs::path& path){std::ifstream in(path,std::ios::binary);CHECK(bool(in));return {std::istreambuf_iterator<char>(in),{}};}
void put(const fs::path& path,const std::string& text,int age=0){
 fs::create_directories(path.parent_path());
 std::ofstream out(path,std::ios::binary);CHECK(bool(out));out.write(text.data(),std::streamsize(text.size()));out.close();CHECK(bool(out));
 fs::last_write_time(path,fs::file_time_type::clock::now()-std::chrono::hours(24)+std::chrono::seconds(age));
}
// Try Windows' unprivileged symbolic-link flag before falling back to a
// directory junction, which exercises the same reparse-point rejection.
bool create_link(const fs::path& target,const fs::path& link,bool directory){
 std::error_code error;
 if(directory)fs::create_directory_symlink(target,link,error);
 else fs::create_symlink(target,link,error);
 if(!error)return true;
#ifdef _WIN32
 if(CreateSymbolicLinkW(link.c_str(),target.c_str(),(directory?SYMBOLIC_LINK_FLAG_DIRECTORY:0)|0x2))return true;
 if(!directory)return false;
 if(!fs::create_directory(link,error)||error)return false;
 HANDLE handle=CreateFileW(link.c_str(),GENERIC_WRITE,0,nullptr,OPEN_EXISTING,
  FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_BACKUP_SEMANTICS,nullptr);
 if(handle==INVALID_HANDLE_VALUE){fs::remove(link,error);return false;}
 const auto print=target.wstring(),substitute=std::wstring(L"\\??\\")+print;
 struct Junction {
  DWORD tag;WORD length,reserved;
  WORD substitute_offset,substitute_length,print_offset,print_length;
  wchar_t path[2048];
 } data{};
 data.tag=IO_REPARSE_TAG_MOUNT_POINT;
 data.substitute_length=WORD(substitute.size()*sizeof(wchar_t));
 data.print_offset=WORD((substitute.size()+1)*sizeof(wchar_t));
 data.print_length=WORD(print.size()*sizeof(wchar_t));
 data.length=WORD(8+data.print_offset+data.print_length+sizeof(wchar_t));
 CHECK(substitute.size()+print.size()+2<std::size(data.path));
 std::copy(substitute.begin(),substitute.end(),data.path);
 std::copy(print.begin(),print.end(),data.path+substitute.size()+1);
 DWORD returned=0;
 const bool ok=DeviceIoControl(handle,FSCTL_SET_REPARSE_POINT,&data,DWORD(data.length+8),nullptr,0,&returned,nullptr)!=0;
 CloseHandle(handle);if(!ok)fs::remove(link,error);return ok;
#else
 return false;
#endif
}

struct Fixture {
 fs::path root;
 Fixture(){
  static unsigned serial=0;
  auto temp=fs::absolute(fs::temp_directory_path());if(temp.filename().empty())temp=temp.parent_path();
  root=temp/from_utf8("re4-system-reports-\xD0\x96\xD1\x83\xD1\x80\xD0\xBD\xD0\xB0\xD0\xBB-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(++serial));
  CHECK(root.parent_path()==temp);CHECK(fs::create_directory(root));
 }
 ~Fixture(){CHECK(root.is_absolute()&&root.filename().string().find("re4-system-reports-")==0);std::error_code ec;fs::remove_all(root,ec);}
 fs::path at(const char* path)const{return root/path;}
 std::string report(std::size_t capacity=400*1024)const{
  std::vector<char> out(capacity+32,char(0x65));
  const auto n=re4::append_system_reports(out.data(),capacity,utf8(root).c_str());
  CHECK(n<=350*1024);
  if(capacity){CHECK(n<capacity&&out[n]=='\0');CHECK(std::strlen(out.data())==n);}
  else CHECK(n==0);
  for(std::size_t i=capacity;i<out.size();++i)CHECK(out[i]==char(0x65));
  return {out.data(),n};
 }
};
std::size_t count(const std::string& text,const char* needle){std::size_t n=0,pos=0;while((pos=text.find(needle,pos))!=std::string::npos){++n;pos+=std::strlen(needle);}return n;}
std::vector<std::string> report_paths(const std::string& text){
 std::vector<std::string> result;std::size_t at=0;
 while((at=text.find("=== /atmosphere/",at))!=std::string::npos){
  const auto first=at+4,last=text.find(" ===",first);CHECK(last!=std::string::npos);
  result.push_back(text.substr(first,last-first));at=last+4;
 }
 return result;
}
using Snapshot=std::map<std::string,std::pair<std::string,fs::file_time_type>>;
Snapshot snapshot(const fs::path& root){
 Snapshot result;
 for(const auto& entry:fs::recursive_directory_iterator(root)){
  const auto state=entry.symlink_status();
  if(fs::is_regular_file(state))result[utf8(entry.path().lexically_relative(root))]={read(entry.path()),entry.last_write_time()};
 }
 return result;
}
}

int main(){
 // A per-directory quota, ascending timestamps or nondeterministic ties breaks this fixture.
 {
  Fixture f;
  put(f.at("atmosphere/crash_reports/old.log"),"old-report-body",1);
  put(f.at("atmosphere/crash_reports/newest.log"),"newest-report-body",90);
  put(f.at("atmosphere/fatal_reports/recent.txt"),"recent-report-body",80);
  put(f.at("atmosphere/fatal_errors/crash.bin"),"BINARY_MUST_NOT_BE_READ",70);
  put(f.at("atmosphere/crash_reports/tie-b.log"),"tie-b-body",60);
  put(f.at("atmosphere/crash_reports/tie-a.log"),"tie-a-body",60);
  put(f.at("atmosphere/fatal_reports/older.log"),"older-report-body",2);
  const auto tie_time=fs::last_write_time(f.at("atmosphere/crash_reports/tie-a.log"));
  fs::last_write_time(f.at("atmosphere/crash_reports/tie-b.log"),tie_time);
  const auto text=f.report();
  const std::vector<std::string> expected={"/atmosphere/crash_reports/newest.log",
   "/atmosphere/fatal_reports/recent.txt","/atmosphere/fatal_errors/crash.bin",
   "/atmosphere/crash_reports/tie-a.log","/atmosphere/crash_reports/tie-b.log"};
  CHECK(report_paths(text)==expected);
  CHECK(text.find("old-report-body")==std::string::npos&&text.find("older-report-body")==std::string::npos);
  CHECK(text.find("newest-report-body")!=std::string::npos&&text.find("recent-report-body")!=std::string::npos);
  CHECK(text.find("BINARY_MUST_NOT_BE_READ")==std::string::npos);
  CHECK(text.find("bytes=23")!=std::string::npos&&text.find("mtime_unix=")!=std::string::npos);
  CHECK(text.find("binary metadata only")!=std::string::npos);
 }
 // Reading a byte beyond 64KiB, tail sampling, missing markers or failing the total cap breaks this fixture.
 {
  Fixture f;
  for(unsigned i=0;i<5;++i)put(f.root/("atmosphere/crash_reports/big-"+std::to_string(i)+".log"),std::string(65536,char('A'+i))+"TAIL_SECRET_NEVER_READ",int(i));
  const auto before=snapshot(f.root);const auto text=f.report();
  CHECK(text.size()>=5*65536&&text.size()<=350*1024);
  CHECK(text.find("TAIL_SECRET_NEVER_READ")==std::string::npos);
  CHECK(count(text,"[truncated: text limit 65536 bytes]")==5);
  for(char c='A';c<='E';++c)CHECK(count(text,std::string(65536,c).c_str())==1);
  CHECK(snapshot(f.root)==before);
 }
 // Suffix-prefix matching, nested recursion, exporting credentials or following file links breaks this fixture.
 {
  Fixture f;
  put(f.at("atmosphere/crash_reports/plain.LOG"),"SAFE_TEXT",1);
  put(f.at("atmosphere/crash_reports/fake.log.bak"),"PRIVATE_FALSE_EXTENSION",100);
  put(f.at("atmosphere/crash_reports/wrong.txt.tmp"),"PRIVATE_FALSE_EXTENSION",100);
  put(f.at("atmosphere/crash_reports/token.txt"),"PRIVATE_TOKEN",100);
  put(f.at("atmosphere/crash_reports/PRIVATE-report.log"),"PRIVATE_NAME",100);
  put(f.at("atmosphere/crash_reports/prod.keys.log"),"PRIVATE_KEYS",100);
  put(f.at("atmosphere/crash_reports/credentials.txt"),"PRIVATE_CREDENTIAL",100);
  put(f.at("atmosphere/crash_reports/secret.log"),"PRIVATE_SECRET",100);
  put(f.at("atmosphere/crash_reports/subdir.log/nested.txt"),"PRIVATE_NESTED",100);
  put(f.at("outside.txt"),"PRIVATE_LINK_TARGET",100);
  put(f.at("switch/re4-controls/other.log"),"PRIVATE_OUTSIDE_ALLOWLIST",100);
  const bool link_created=create_link(f.at("outside.txt"),f.at("atmosphere/crash_reports/link.log"),false);if(!link_created)std::puts("SKIP file symlink creation: host permission");
  const auto before=snapshot(f.root);const auto text=f.report();
  CHECK(report_paths(text)==std::vector<std::string>{"/atmosphere/crash_reports/plain.LOG"});
  CHECK(text.find("SAFE_TEXT")!=std::string::npos&&text.find("PRIVATE_")==std::string::npos);
  CHECK(text.find("link.log")==std::string::npos&&text.find("subdir.log")==std::string::npos);
  CHECK(snapshot(f.root)==before);
 }
 // Following either a fixed report-directory or its atmosphere ancestor link breaks this fixture.
 {
  Fixture f;
  put(f.at("outside/report.log"),"PRIVATE_DIRECTORY_LINK",10);
  fs::create_directories(f.at("atmosphere"));
  std::error_code ec;
  if(create_link(f.at("outside"),f.at("atmosphere/crash_reports"),true)){const auto text=f.report();CHECK(text.find("PRIVATE_DIRECTORY_LINK")==std::string::npos&&report_paths(text).empty());}
  else std::puts("SKIP directory symlink creation: host permission");
  fs::remove(f.at("atmosphere/crash_reports"),ec);fs::remove(f.at("atmosphere"),ec);
  if(create_link(f.at("outside"),f.at("atmosphere"),true)){CHECK(f.report().find("PRIVATE_DIRECTORY_LINK")==std::string::npos);}
  else std::puts("SKIP ancestor symlink creation: host permission");
 }
 // Roots with traversal components and linked roots must never export a report.
 {
  Fixture f;put(f.at("atmosphere/crash_reports/safe.log"),"SAFE_ROOT_CONTENT",1);
  std::vector<char> out(400*1024);
  auto root=utf8(f.root/"atmosphere"/"..");
  auto n=re4::append_system_reports(out.data(),out.size(),root.c_str());
  CHECK(std::string(out.data(),n).find("SAFE_ROOT_CONTENT")==std::string::npos);
  fs::create_directory(f.at("mount-link-target"));
  put(f.at("mount-link-target/atmosphere/crash_reports/linked.log"),"PRIVATE_LINKED_ROOT",1);
  if(create_link(f.at("mount-link-target"),f.at("mount-link"),true)){
   root=utf8(f.at("mount-link"));n=re4::append_system_reports(out.data(),out.size(),root.c_str());
   CHECK(std::string(out.data(),n).find("PRIVATE_LINKED_ROOT")==std::string::npos);
  }else std::puts("SKIP linked root creation: host permission");
 }
 // A mislabeled text file may contain NULs; its export must remain valid text.
 {
  Fixture f;put(f.at("atmosphere/crash_reports/nul.txt"),std::string("before\0after",12),1);
  const auto text=f.report();CHECK(text.find("before?after")!=std::string::npos);
  CHECK(read(f.at("atmosphere/crash_reports/nul.txt"))==std::string("before\0after",12));
 }

 // Missing directories and invalid roots must still produce a bounded, terminated diagnostic.
 {
  Fixture f;
  for(std::size_t capacity:{0u,1u,2u,7u,31u,4096u})f.report(capacity);
  const auto text=f.report();CHECK(text.find("unavailable")!=std::string::npos);
  CHECK(re4::append_system_reports(nullptr,0,nullptr)==0);
  CHECK(re4::append_system_reports(nullptr,100,utf8(f.root).c_str())==0);
  std::array<char,16> out;out.fill(char(0x65));
  CHECK(re4::append_system_reports(out.data(),0,utf8(f.root).c_str())==0&&out[0]==char(0x65));
  const auto n=re4::append_system_reports(out.data(),out.size(),nullptr);CHECK(n<out.size()&&out[n]=='\0');
  put(f.at("root-file"),"DO_NOT_CHANGE",1);
  re4::append_system_reports(out.data(),out.size(),utf8(f.at("root-file")).c_str());
  CHECK(read(f.at("root-file"))=="DO_NOT_CHANGE");
  re4::append_system_reports(out.data(),out.size(),utf8(f.at("missing-root")).c_str());
 }
 // A tiny output must never damage its surrounding bytes even with a large real report.
 {
  Fixture f;put(f.at("atmosphere/crash_reports/large.log"),std::string(100000,'Q')+"TAIL_SECRET_NEVER_READ",1);
  for(std::size_t capacity:{0u,1u,2u,17u,64u,257u,65536u})f.report(capacity);
  CHECK(f.report(512).find("[truncated: output capacity]")!=std::string::npos);
 }
 // Enumeration must stop after the fixed scan budget; candidate storage must stay at five.
 {
  Fixture f;
  for(unsigned i=0;i<2100;++i)put(f.root/("atmosphere/crash_reports/report-"+std::to_string(i)+".log"),"bounded-entry",int(i));
  const auto text=f.report();CHECK(report_paths(text).size()==5);
  CHECK(text.find("scan limit 2048")!=std::string::npos);
 }
 std::puts("system reports: latest-five ordering, metadata, bounded text, path safety, unicode and read-only fixtures PASS");
}
