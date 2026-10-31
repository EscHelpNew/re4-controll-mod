#include "re4/support_report.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#endif
namespace {
bool plain_stat(const std::string& path,struct stat& st){
#ifdef _WIN32
 const auto attr=GetFileAttributesA(path.c_str());
 return attr!=INVALID_FILE_ATTRIBUTES&&!(attr&FILE_ATTRIBUTE_REPARSE_POINT)&&!stat(path.c_str(),&st);
#else
 return !lstat(path.c_str(),&st)&&!S_ISLNK(st.st_mode);
#endif
}
struct Writer {
 FILE* file;std::size_t bytes=0;bool ok=true;
 void data(const char* p,std::size_t n){if(!ok)return;if(n>re4::kSupportReportLimit-bytes){ok=false;return;}ok=std::fwrite(p,1,n,file)==n;bytes+=n;}
 void text(const std::string& s){data(s.data(),s.size());}
 void quoted(const std::string& s){text("\"");for(char c:s){if(c=='\"')text("\"");data(&c,1);}text("\"");}
 void row(const char* kind,const std::string& path,const std::string& value){quoted(kind);text(",");quoted(path);text(",");quoted(value);text("\n");}
};
bool ends(const std::string& s,const char* suffix){const auto n=std::strlen(suffix);return s.size()>=n&&s.compare(s.size()-n,n,suffix)==0;}
bool private_name(const std::string& name){
 std::string lower=name;for(char& c:lower)if(c>='A'&&c<='Z')c=char(c-'A'+'a');
 for(const auto* word:{"token","credential","secret",".keys"})if(lower.find(word)!=std::string::npos)return true;
 return false;
}
struct Entry {std::string name;struct stat st{};};
void contents(Writer& w,const std::string& full,const std::string& relative,std::size_t maximum){
 FILE* f=std::fopen(full.c_str(),"rb");if(!f){w.row("read_error",relative,std::to_string(errno));return;}
 char buffer[4096];std::size_t read=0;
 while(read<maximum){const auto n=std::fread(buffer,1,std::min(sizeof(buffer),maximum-read),f);if(!n)break;w.row("content",relative,std::string(buffer,n));read+=n;}
 if(std::ferror(f))w.row("read_error",relative,std::to_string(errno));
 if(!std::feof(f)&&!std::fseek(f,0,SEEK_END)){
  const auto length=std::ftell(f);
  if(length>long(read)){w.row("omitted_bytes",relative,std::to_string(length-long(read)));if(!std::fseek(f,std::max(long(read),length-4096),SEEK_SET)){const auto n=std::fread(buffer,1,sizeof(buffer),f);if(n)w.row("tail",relative,std::string(buffer,n));}}
 }
 std::fclose(f);
}
void directory(Writer& w,const std::string& root,const char* relative,unsigned& files_read){
 const std::string full=root+relative;struct stat directory_stat{};
 if(!plain_stat(full,directory_stat)||!S_ISDIR(directory_stat.st_mode)){w.row("directory_unavailable",relative,std::to_string(errno));return;}
 DIR* dir=opendir(full.c_str());
 if(!dir){w.row("directory_unavailable",relative,std::to_string(errno));return;}
 std::vector<Entry> entries;unsigned scanned=0;
 while(const auto* e=readdir(dir)){
  if(++scanned>2048){w.row("directory_scan_limit",relative,"2048");break;}
  const std::string name=e->d_name;if(name=="."||name==".."||private_name(name))continue;
  struct stat st{};if(!plain_stat(full+"/"+name,st))continue;
  if(entries.size()==512){w.row("directory_entry_limit",relative,"512");break;}
  entries.push_back({name,st});
 }
 closedir(dir);
 // RE6 errors are included even when old console timestamps sort other games
 // ahead of them. The inventory also retains all bounded candidate names.
 std::sort(entries.begin(),entries.end(),[](const Entry& a,const Entry& b){
  const bool ar=a.name.find("01002a000cd48000")!=std::string::npos,br=b.name.find("01002a000cd48000")!=std::string::npos;
  if(ar!=br)return ar;if(a.st.st_mtime!=b.st.st_mtime)return a.st.st_mtime>b.st.st_mtime;return a.name<b.name;
 });
 unsigned log_count=0;
 for(const auto& e:entries){
  const std::string path=std::string(relative)+"/"+e.name;
  w.row(S_ISDIR(e.st.st_mode)?"directory":"file",path,"bytes="+std::to_string(e.st.st_size)+" mtime="+std::to_string(e.st.st_mtime));
  if(!S_ISREG(e.st.st_mode)||files_read>=32)continue;
  bool include=false;std::size_t maximum=65536;
  if(std::strstr(relative,"atmosphere/")&&(ends(e.name,".log")||ends(e.name,".txt"))&&log_count<12){include=true;++log_count;}
  if(std::strstr(relative,"/telemetry")&&e.name.rfind("support-",0)!=0&&(ends(e.name,".csv")||ends(e.name,".csv.part"))){include=true;maximum=8192;}
  if(std::strcmp(relative,"/switch/re4-controls")==0&&(e.name=="reload-bridge.log"||e.name=="game-inspection.txt"))include=true;
  if(std::strstr(relative,"/config/")&&ends(e.name,".log")&&log_count<4){include=true;++log_count;}
  if(include){++files_read;contents(w,full+"/"+e.name,path,maximum);}
 }
}
}
namespace re4 {
bool write_support_report(const char* sd_root,const char* destination,const char* version){
 if(!sd_root||!destination||!version)return false;
 struct stat st{};if(!stat(destination,&st))return false;
 const std::string temporary=std::string(destination)+".tmp";
 int flags=O_WRONLY|O_CREAT|O_EXCL;
#ifdef O_BINARY
 flags|=O_BINARY;
#endif
 const int fd=open(temporary.c_str(),flags,0666);if(fd<0)return false;
 FILE* file=fdopen(fd,"wb");if(!file){close(fd);std::remove(temporary.c_str());return false;}
 Writer writer{file};writer.text("#re4_controls_support,abi=1\nkind,path,data\n");writer.row("application","version",version);
 unsigned count=0;std::string root=sd_root;while(!root.empty()&&root.back()=='/')root.pop_back();
 for(const auto* folder:{"/switch/re4-controls","/switch/re4-controls/telemetry",
      "/atmosphere/crash_reports","/atmosphere/fatal_reports","/atmosphere/fatal_errors",
      "/switch/.overlays","/atmosphere/contents/01002A000CD48000/exefs",
      "/config/uberhand","/config/ultrahand","/config/tesla"})directory(writer,root,folder,count);
 writer.row("complete","files_read",std::to_string(count));
 bool ok=writer.ok;if(std::fflush(file))ok=false;
#ifdef _WIN32
 if(_commit(fd))ok=false;
#else
 if(fsync(fd))ok=false;
#endif
 if(std::fclose(file))ok=false;
 if(ok)ok=std::rename(temporary.c_str(),destination)==0;
 if(!ok)std::remove(temporary.c_str());return ok;
}
}
