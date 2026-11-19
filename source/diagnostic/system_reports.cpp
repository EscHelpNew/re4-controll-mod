#include "re4/system_reports.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {
constexpr unsigned kScanLimit=2048;
constexpr unsigned kReportCount=5;
struct Metadata {
 std::uint64_t bytes=0;
 std::int64_t seconds=0;
 std::uint32_t nanoseconds=0;
};
struct Entry {
 std::string full,relative;
 Metadata metadata;
 bool binary=false;
};
struct Writer {
 char* out;
 std::size_t limit,used=0;
 void bytes(const char* data,std::size_t length){
  const auto n=std::min(length,limit-used);
  if(n)std::memcpy(out+used,data,n);
  used+=n;
 }
 void text(const char* text){bytes(text,std::strlen(text));}
 void text(const std::string& text){bytes(text.data(),text.size());}
 std::size_t remaining()const{return limit-used;}
};
#ifdef _WIN32
std::wstring wide_path(const std::string& path){
 const int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path.c_str(),-1,nullptr,0);
 if(n<=0)return {};
 std::wstring result(std::size_t(n),L'\0');
 if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path.c_str(),-1,&result[0],n))return {};
 result.resize(std::size_t(n-1));return result;
}
std::string utf8_name(const wchar_t* name){
 const int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,name,-1,nullptr,0,nullptr,nullptr);
 if(n<=0)return {};
 std::string result(std::size_t(n),'\0');
 if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,name,-1,&result[0],n,nullptr,nullptr))return {};
 result.resize(std::size_t(n-1));return result;
}
bool plain_directory(const std::string& path){
 const auto wide=wide_path(path);if(wide.empty())return false;
 const auto attributes=GetFileAttributesW(wide.c_str());
 return attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_DIRECTORY)
  &&!(attributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DEVICE));
}
using File=HANDLE;
const File kInvalidFile=INVALID_HANDLE_VALUE;
void close_file(File file){CloseHandle(file);}
bool metadata(File file,Metadata& result){
 BY_HANDLE_FILE_INFORMATION info{};
 if(GetFileType(file)!=FILE_TYPE_DISK||!GetFileInformationByHandle(file,&info)
    ||(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DEVICE)))return false;
 result.bytes=(std::uint64_t(info.nFileSizeHigh)<<32)|info.nFileSizeLow;
 const std::uint64_t ticks=(std::uint64_t(info.ftLastWriteTime.dwHighDateTime)<<32)|info.ftLastWriteTime.dwLowDateTime;
 constexpr std::uint64_t epoch=116444736000000000ull;
 // Split before subtracting to retain valid pre-1970 timestamps without unsigned underflow.
 result.seconds=std::int64_t(ticks/10000000)-std::int64_t(epoch/10000000);
 result.nanoseconds=std::uint32_t(ticks%10000000)*100;
 return true;
}
File open_plain(const std::string& path,bool read,Metadata& result){
 const auto wide=wide_path(path);if(wide.empty())return kInvalidFile;
 const File file=CreateFileW(wide.c_str(),read?GENERIC_READ:0,
  FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
 if(file==kInvalidFile)return kInvalidFile;
 if(!metadata(file,result)){close_file(file);return kInvalidFile;}
 return file;
}
bool plain_file(const std::string& path,Metadata& result){
 const auto file=open_plain(path,false,result);
 if(file==kInvalidFile)return false;
 close_file(file);return true;
}
std::size_t read_file(File file,char* buffer,std::size_t size,bool& error){
 DWORD got=0;
 if(!ReadFile(file,buffer,DWORD(size),&got,nullptr)){error=true;return 0;}
 return std::size_t(got);
}
#else
bool plain_directory(const std::string& path){
 struct stat info{};return !lstat(path.c_str(),&info)&&S_ISDIR(info.st_mode)&&!S_ISLNK(info.st_mode);
}
void metadata(const struct stat& info,Metadata& result){
 result.bytes=info.st_size<0?0:std::uint64_t(info.st_size);
 result.seconds=std::int64_t(info.st_mtime);
#if defined(__linux__)
 result.nanoseconds=std::uint32_t(info.st_mtim.tv_nsec);
#elif defined(__APPLE__)
 result.nanoseconds=std::uint32_t(info.st_mtimespec.tv_nsec);
#endif
}
bool plain_file(const std::string& path,Metadata& result){
 struct stat info{};
 if(lstat(path.c_str(),&info)||!S_ISREG(info.st_mode)||S_ISLNK(info.st_mode)||info.st_size<0)return false;
 metadata(info,result);return true;
}
using File=int;
constexpr File kInvalidFile=-1;
void close_file(File file){close(file);}
File open_plain(const std::string& path,bool,Metadata& result){
 // O_NONBLOCK prevents a replaced FIFO from blocking before fstat rejects it.
 int flags=O_RDONLY|O_NONBLOCK;
#ifdef O_NOFOLLOW
 flags|=O_NOFOLLOW;
#endif
 struct stat before{};
 if(lstat(path.c_str(),&before)||!S_ISREG(before.st_mode)||S_ISLNK(before.st_mode))return kInvalidFile;
 const File file=open(path.c_str(),flags);
 if(file==kInvalidFile)return file;
 struct stat after{};
 if(fstat(file,&after)||!S_ISREG(after.st_mode)||after.st_size<0
    ||before.st_dev!=after.st_dev||before.st_ino!=after.st_ino){
  close_file(file);return kInvalidFile;
 }
 metadata(after,result);return file;
}
std::size_t read_file(File file,char* buffer,std::size_t size,bool& error){
 const auto got=read(file,buffer,size);
 if(got<0){error=true;return 0;}
 return std::size_t(got);
}
#endif

std::string join(const std::string& root,const std::string& relative){
 return root+(root.back()=='/'?"":"/")+(relative.front()=='/'?relative.substr(1):relative);
}
bool valid_root(const char* input,std::string& root){
 if(!input)return false;
 std::size_t n=0;while(n<=4096&&input[n])++n;
 if(!n||n>4096)return false;
 root.assign(input,n);
 // Root is a local mount supplied by the application, never a remote path.
 // Reject traversal components even for host callers.
 std::size_t start=0;
 for(std::size_t i=0;i<=root.size();++i){
  if(i==root.size()||root[i]=='/'||root[i]=='\\'){
   if(root.compare(start,i-start,"..")==0)return false;
   start=i+1;
  }
 }
 while(root.size()>1&&(root.back()=='/'||root.back()=='\\'))root.pop_back();
 if(root.back()==':')root+='/';
 return true;
}
bool allowed_name(const std::string& name,bool& binary){
 if(name.empty()||name=="."||name==".."||name.size()>1024)return false;
 std::string lower=name;
 for(char& value:lower){
  const auto c=static_cast<unsigned char>(value);
  if(c<32||c==127||c=='/'||c=='\\'||c==':')return false;
  if(c>='A'&&c<='Z')value=char(c-'A'+'a');
 }
 for(const auto* private_word:{"token","credential","secret","key","private","password","passwd"})
  if(lower.find(private_word)!=std::string::npos)return false;
 const auto dot=lower.rfind('.');
 if(dot==std::string::npos)return false;
 const auto extension=lower.substr(dot);
 binary=extension==".bin";
 return binary||extension==".log"||extension==".txt";
}
bool newer(const Entry& a,const Entry& b){
 if(a.metadata.seconds!=b.metadata.seconds)return a.metadata.seconds>b.metadata.seconds;
 if(a.metadata.nanoseconds!=b.metadata.nanoseconds)return a.metadata.nanoseconds>b.metadata.nanoseconds;
 return a.relative<b.relative;
}
struct Latest {
 std::array<Entry,kReportCount> entries;
 unsigned count=0;
 void consider(const std::string& folder,const char* relative,const std::string& name){
  bool binary=false;if(!allowed_name(name,binary))return;
  Entry entry{join(folder,name),std::string(relative)+"/"+name,{},binary};
  if(!plain_file(entry.full,entry.metadata))return;
  unsigned at=0;
  if(count<entries.size())at=count++;
  else if(newer(entry,entries[count-1]))at=count-1;
  else return;
  while(at&&newer(entry,entries[at-1])){entries[at]=std::move(entries[at-1]);--at;}
  entries[at]=std::move(entry);
 }
};
void directory(Writer& out,Latest& latest,const std::string& root,const char* relative){
 const auto full=join(root,relative);
 if(!plain_directory(full)){out.text(relative);out.text(": unavailable\n");return;}
 unsigned scanned=0;bool capped=false;
#ifdef _WIN32
 const auto pattern=wide_path(join(full,"*"));WIN32_FIND_DATAW info{};
 const HANDLE search=pattern.empty()?INVALID_HANDLE_VALUE:FindFirstFileW(pattern.c_str(),&info);
 if(search==INVALID_HANDLE_VALUE){out.text(relative);out.text(": unavailable\n");return;}
 do {
  ++scanned;
  if(!(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DEVICE)))
   latest.consider(full,relative,utf8_name(info.cFileName));
  if(scanned==kScanLimit){capped=true;break;}
 }while(FindNextFileW(search,&info));
 const auto error=GetLastError();FindClose(search);
 if(!capped&&error!=ERROR_NO_MORE_FILES){out.text(relative);out.text(": enumeration error\n");}
#else
 DIR* search=opendir(full.c_str());
 if(!search){out.text(relative);out.text(": unavailable\n");return;}
 int error=0;
 while(scanned<kScanLimit){
  errno=0;const auto* entry=readdir(search);
  if(!entry){error=errno;break;}
  ++scanned;latest.consider(full,relative,entry->d_name);
 }
 capped=scanned==kScanLimit;closedir(search);
 if(error){out.text(relative);out.text(": enumeration error\n");}
#endif
 if(capped){out.text(relative);out.text(": scan limit 2048 reached\n");}
}
void report(Writer& out,const Entry& entry){
 out.text("\n=== ");out.text(entry.relative);out.text(" ===\n");
 char info[192]{};
 std::snprintf(info,sizeof(info),"bytes=%llu mtime_unix=%lld.%09u\n",
  static_cast<unsigned long long>(entry.metadata.bytes),
  static_cast<long long>(entry.metadata.seconds),unsigned(entry.metadata.nanoseconds));
 out.text(info);
 if(entry.binary){out.text("[binary metadata only]\n");return;}
 if(!out.remaining())return;
 Metadata current{};
 const auto file=open_plain(entry.full,true,current);
 if(file==kInvalidFile){out.text("[read unavailable]\n");return;}
 const auto maximum=std::size_t(std::min<std::uint64_t>(re4::kSystemReportFileLimit,current.bytes));
 const char* marker=current.bytes>re4::kSystemReportFileLimit?"\n[truncated: text limit 65536 bytes]\n":nullptr;
 std::size_t content_limit=maximum;
 if(maximum+(marker?std::strlen(marker):0)>out.remaining()){
  marker="\n[truncated: output capacity]\n";
  const auto marker_size=std::strlen(marker);
  content_limit=std::min(maximum,out.remaining()>marker_size?out.remaining()-marker_size:0);
 }
 char buffer[4096];std::size_t consumed=0;bool error=false;
 while(consumed<content_limit&&out.remaining()){
  const auto size=std::min({sizeof(buffer),content_limit-consumed,out.remaining()});
  const auto got=read_file(file,buffer,size,error);if(!got)break;
  // Keep the export a NUL-terminated text buffer even for a mislabeled file.
  for(std::size_t i=0;i<got;++i){
   const auto c=static_cast<unsigned char>(buffer[i]);
   if((c<32&&c!='\n'&&c!='\r'&&c!='\t')||c==127)buffer[i]='?';
  }
  out.bytes(buffer,got);consumed+=got;
 }
 close_file(file);
 if(error)out.text("\n[read error]\n");
 else if(marker&&consumed==content_limit&&current.bytes>consumed)out.text(marker);
}
}
namespace re4 {
std::size_t append_system_reports(char* out,std::size_t capacity,const char* sd_root){
 if(!out||!capacity)return 0;
 out[0]='\0';Writer writer{out,std::min(capacity-1,kSystemReportLimit)};
 if(!writer.remaining())return 0;
 writer.text("\nRE Controls system crash/fatal reports (read-only snapshot)\n");
 std::string root;
 if(!valid_root(sd_root,root)||!plain_directory(root)){
  writer.text("system report root unavailable\n");out[writer.used]='\0';return writer.used;
 }
 if(!plain_directory(join(root,"atmosphere"))){
  writer.text("/atmosphere: unavailable\n");out[writer.used]='\0';return writer.used;
 }
 Latest latest;
 for(const auto* relative:{"/atmosphere/crash_reports","/atmosphere/fatal_reports","/atmosphere/fatal_errors"}){
  if(!writer.remaining())break;
  directory(writer,latest,root,relative);
 }
 if(!latest.count)writer.text("no matching regular system reports available\n");
 for(unsigned i=0;i<latest.count&&writer.remaining();++i)report(writer,latest.entries[i]);
 out[writer.used]='\0';return writer.used;
}
}
