#include "re4/app_journal.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif
#ifdef RE4_APP_JOURNAL_TEST_SYNC
extern "C" int re4_app_journal_test_sync(FILE*);
#endif
namespace {
enum class Kind {Missing,File,Directory,Unsafe,Error};
#ifdef _WIN32
std::wstring wide_path(const std::string& path){
    const int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path.c_str(),-1,nullptr,0);
    if(n<=0){errno=EINVAL;return {};}
    std::wstring result(std::size_t(n),L'\0');
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path.c_str(),-1,&result[0],n)){errno=EINVAL;return {};}
    result.resize(std::size_t(n-1));return result;
}
Kind kind(const std::string& path){
    const auto wide=wide_path(path);if(wide.empty())return Kind::Error;
    const auto attr=GetFileAttributesW(wide.c_str());
    if(attr==INVALID_FILE_ATTRIBUTES){
        const auto e=GetLastError();
        if(e==ERROR_FILE_NOT_FOUND||e==ERROR_PATH_NOT_FOUND)return Kind::Missing;
        errno=EIO;return Kind::Error;
    }
    if(attr&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DEVICE))return Kind::Unsafe;
    return attr&FILE_ATTRIBUTE_DIRECTORY?Kind::Directory:Kind::File;
}
FILE* open_file(const std::string& path,const char* mode){
    const auto wide=wide_path(path);if(wide.empty())return nullptr;
    wchar_t wide_mode[4]{};for(unsigned i=0;mode[i]&&i<3;++i)wide_mode[i]=wchar_t(mode[i]);
    return _wfopen(wide.c_str(),wide_mode);
}
int remove_file(const std::string& path){const auto wide=wide_path(path);return wide.empty()?-1:_wremove(wide.c_str());}
int rename_file(const std::string& from,const std::string& to){
    const auto a=wide_path(from),b=wide_path(to);return a.empty()||b.empty()?-1:_wrename(a.c_str(),b.c_str());
}
#else
Kind kind(const std::string& path){
    struct stat st{};
    if(lstat(path.c_str(),&st)){return errno==ENOENT?Kind::Missing:Kind::Error;}
    if(S_ISLNK(st.st_mode))return Kind::Unsafe;
    if(S_ISDIR(st.st_mode))return Kind::Directory;
    return S_ISREG(st.st_mode)?Kind::File:Kind::Unsafe;
}
FILE* open_file(const std::string& path,const char* mode){return std::fopen(path.c_str(),mode);}
int remove_file(const std::string& path){return std::remove(path.c_str());}
int rename_file(const std::string& from,const std::string& to){return std::rename(from.c_str(),to.c_str());}
#endif
int io_error(){return errno?errno:EIO;}
bool copy_label(const char* in,char* out,std::size_t size){
    if(!in||!size)return false;
    std::size_t n=0;
    for(;n<size;++n){
        const unsigned char c=static_cast<unsigned char>(in[n]);
        if(!c)break;
        if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')
            ||c=='_'||c=='-'||c=='.'||c==':'||c=='+'))return false;
    }
    if(!n||n==size)return false;
    std::memcpy(out,in,n);out[n]='\0';return true;
}
struct Writer {
    char* out;std::size_t limit,used=0;
    void bytes(const char* p,std::size_t size){
        const auto n=std::min(size,limit-used);if(n)std::memcpy(out+used,p,n);used+=n;
    }
    void text(const char* p){bytes(p,std::strlen(p));}
    std::size_t remaining()const{return limit-used;}
};
}
namespace re4 {
struct AppJournal::Impl {
    std::mutex mutex;
    std::array<std::string,kAppJournalCount> files{};
    char version[64]{},last_stage[81]{};
    unsigned hos=0,applet=0;
    std::uint64_t pid=0;
    bool recovery=false,ready=false,capped=false;
    int error=0;
    std::uint32_t last_rc=0;
    bool finish(FILE* f,const char* text,std::size_t n){
        int failure=0;
        if(std::fwrite(text,1,n,f)!=n)failure=io_error();
        if(std::fflush(f)&&!failure)failure=io_error();
        if(!failure){
#ifdef RE4_APP_JOURNAL_TEST_SYNC
            if(re4_app_journal_test_sync(f))failure=io_error();
#elif defined(_WIN32)
            if(_commit(_fileno(f)))failure=io_error();
#else
            if(fsync(fileno(f)))failure=io_error();
#endif
        }
        if(std::fclose(f)&&!failure)failure=io_error();
        if(failure){error=failure;ready=false;return false;}return true;
    }
    void append(const char* stage,std::uint32_t rc){
        if(!copy_label(stage,last_stage,sizeof(last_stage)))std::strcpy(last_stage,"invalid_stage");
        last_rc=rc;
        if(!ready)return;
        if(kind(files[0])!=Kind::File){error=EINVAL;ready=false;return;}
        FILE* f=open_file(files[0],"ab");if(!f){error=io_error();ready=false;return;}
        if(std::fseek(f,0,SEEK_END)){error=io_error();ready=false;std::fclose(f);return;}
        const auto length=std::ftell(f);
        if(length<0){error=io_error();ready=false;std::fclose(f);return;}
        char line[128]{};const auto n=std::snprintf(line,sizeof(line),"stage=%s rc=0x%08x\n",last_stage,unsigned(rc));
        if(n<=0||std::size_t(n)>=sizeof(line)){error=EINVAL;ready=false;std::fclose(f);return;}
        if(std::uint64_t(length)>kAppJournalFileLimit||std::size_t(n)>kAppJournalFileLimit-std::size_t(length)){
            capped=true;std::fclose(f);return;
        }
        finish(f,line,std::size_t(n));
    }
    void begin(const char* root){
        if(!root){error=EINVAL;return;}
        std::size_t n=0;while(n<=4096&&root[n])++n;
        if(!n||n>4096){error=EINVAL;return;}
        std::string folder(root,n);
        while(folder.size()>1&&(folder.back()=='/'||folder.back()=='\\'))folder.pop_back();
        if(kind(folder)!=Kind::Directory){error=EINVAL;return;}
        for(unsigned i=0;i<kAppJournalCount;++i){
            files[i]=folder+"/app-report-"+char('0'+i)+".log";
            const auto type=kind(files[i]);
            if(type!=Kind::File&&type!=Kind::Missing){error=EINVAL;return;}
        }
        // Preflight all five destinations before mutating any own report.
        if(kind(files.back())==Kind::File&&remove_file(files.back())){error=io_error();return;}
        for(unsigned i=kAppJournalCount-1;i>0;--i){
            if(kind(files[i-1])!=Kind::File)continue;
            if(kind(files[i])==Kind::File&&remove_file(files[i])){error=io_error();return;}
            if(rename_file(files[i-1],files[i])){error=io_error();return;}
        }
        FILE* f=open_file(files[0],"wb");if(!f){error=io_error();return;}
        char header[256]{};
        const auto written=std::snprintf(header,sizeof(header),
            "RE_CONTROLS_APP_REPORT_V1\nversion=%s hos=%u applet=%u pid=%llu recovery=%u\n",
            version,hos,applet,static_cast<unsigned long long>(pid),unsigned(recovery));
        if(written<=0||std::size_t(written)>=sizeof(header)){error=EINVAL;std::fclose(f);return;}
        ready=true;if(finish(f,header,std::size_t(written)))append("startup",0);
    }
};
AppJournal::AppJournal(const char* root,const char* version,unsigned hos,unsigned applet,
        std::uint64_t pid,bool recovery):impl_(new Impl){
    auto& j=*impl_;j.hos=hos;j.applet=applet;j.pid=pid;j.recovery=recovery;
    if(!copy_label(version,j.version,sizeof(j.version)))std::strcpy(j.version,"unknown");
    std::strcpy(j.last_stage,"startup");j.begin(root);
}
AppJournal::~AppJournal()=default;
void AppJournal::note(const char* stage,std::uint32_t rc){
    std::lock_guard<std::mutex> lock(impl_->mutex);impl_->append(stage,rc);
}
std::size_t AppJournal::report(char* out,std::size_t cap,void* context){
    if(!out||!cap)return 0;
    out[0]='\0';
    if(!context)return 0;
    auto& j=*static_cast<AppJournal*>(context)->impl_;
    std::lock_guard<std::mutex> lock(j.mutex);
    Writer w{out,std::min(cap-1,kAppJournalReportLimit)};
    char summary[512]{};
    std::snprintf(summary,sizeof(summary),
        "RE Controls own app journal\nversion=%s hos=%u applet=%u pid=%llu recovery=%u\n"
        "journal_error=%d capped=%u last_stage=%s rc=0x%08x\n",
        j.version,j.hos,j.applet,static_cast<unsigned long long>(j.pid),unsigned(j.recovery),
        j.error,unsigned(j.capped),j.last_stage,unsigned(j.last_rc));
    w.text(summary);
    for(unsigned i=0;i<kAppJournalCount;++i){
        char heading[64]{};std::snprintf(heading,sizeof(heading),"\n=== app-report-%u.log ===\n",i);w.text(heading);
        const auto type=j.files[i].empty()?Kind::Missing:kind(j.files[i]);
        if(type!=Kind::File){w.text(type==Kind::Missing?"missing\n":"unavailable\n");continue;}
        FILE* f=open_file(j.files[i],"rb");if(!f){w.text("read_error\n");continue;}
        char buffer[4096];std::size_t consumed=0;
        while(consumed<kAppJournalFileLimit&&w.remaining()){
            const auto size=std::min({sizeof(buffer),kAppJournalFileLimit-consumed,w.remaining()});
            const auto got=std::fread(buffer,1,size,f);if(!got)break;
            w.bytes(buffer,got);consumed+=got;
        }
        std::fclose(f);
    }
    out[w.used]='\0';return w.used;
}
}
