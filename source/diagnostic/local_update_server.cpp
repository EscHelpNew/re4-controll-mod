#include "re4/local_update_server.hpp"
#include "re4/character_digest.hpp"
#include "reload_core.h"
#include "reload_project.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cerrno>
#include <sys/stat.h>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>
#ifdef __SWITCH__
#include <switch.h>
#include <switch/services/bsd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#elif defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <io.h>
#include <thread>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#endif

namespace re4 {
namespace {
#ifdef _WIN32
using Socket=SOCKET;
constexpr Socket invalid_socket=INVALID_SOCKET;
void close_socket(Socket fd){closesocket(fd);}
bool nonblocking(Socket fd){u_long enabled=1;return ioctlsocket(fd,FIONBIO,&enabled)==0;}
#else
using Socket=int;
constexpr Socket invalid_socket=-1;
void close_socket(Socket fd){close(fd);}
bool nonblocking(Socket fd){
    const auto flags=fcntl(fd,F_GETFL,0);
#ifdef __SWITCH__
    if(flags<0||socketGetLastResult()||g_bsdErrno)return false;
#else
    if(flags<0)return false;
#endif
    const int result=fcntl(fd,F_SETFL,flags|O_NONBLOCK);
#ifdef __SWITCH__
    return !result&&!socketGetLastResult()&&!g_bsdErrno;
#else
    return !result;
#endif
}
#endif
bool transient_socket_error(){
#ifdef _WIN32
    const auto error=WSAGetLastError();return error==WSAEINTR||error==WSAEWOULDBLOCK;
#else
    return errno==EINTR||errno==EAGAIN||errno==EWOULDBLOCK;
#endif
}
enum class FileKind {Missing,Regular,Other};
FileKind file_kind(const char* path){
    struct stat info{};
    if(stat(path,&info))return errno==ENOENT?FileKind::Missing:FileKind::Other;
    return (info.st_mode&S_IFMT)==S_IFREG?FileKind::Regular:FileKind::Other;
}
using Clock=std::chrono::steady_clock;
constexpr std::uint16_t port=28285;
constexpr std::size_t report_capacity=384u*1024u;
constexpr auto io_slice=std::chrono::milliseconds(50);
void erase_token(char* key){volatile char* p=key;for(unsigned i=0;i<65;++i)p[i]=0;}
std::string escaped(const std::string& value){
    std::string out;
    for(const unsigned char c:value){
        if(c=='"'||c=='\\')out+='\\';
        if(c>=32&&c<127)out+=char(c);else out+='?';
    }
    return out;
}
bool token_from_file(const char* path,char (&out)[65]){
    if(!path)return false;
    FILE* f=std::fopen(path,"rb");if(!f)return false;
    char input[68]{};auto count=std::fread(input,1,67,f);
    bool ok=!std::ferror(f)&&std::fgetc(f)==EOF;
    if(std::fclose(f))ok=false;
    while(count&&(input[count-1]=='\n'||input[count-1]=='\r'))--count;
    ok=ok&&count==64;input[count]=0;
    if(ok)ok=reload_token_valid(input);
    if(ok)std::memcpy(out,input,65);
    volatile char* wipe=input;for(auto& c:input){(void)c;*wipe++=0;}
    return ok;
}
std::uint32_t le32(const unsigned char* p){return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|(std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);}
std::uint64_t le64(const unsigned char* p){return le32(p)|(std::uint64_t(le32(p+4))<<32);}
// Only this application's compatible NRO is an update; no arbitrary executable.
bool own_image(const char* path){
    if(!reload_bridge_valid(path))return false;
    FILE* f=std::fopen(path,"rb");if(!f)return false;
    unsigned char h[128]{},aset[40]{},nacp[0x4000]{};
    bool ok=std::fread(h,1,sizeof(h),f)==sizeof(h);
    const auto image=le32(h+0x18);
    ok=ok&&!std::fseek(f,0,SEEK_END);const auto length=std::ftell(f);
    ok=ok&&length>=0&&image<=std::uint64_t(length)&&std::uint64_t(length)-image>=sizeof(aset)
        &&!std::fseek(f,long(image),SEEK_SET)&&std::fread(aset,1,sizeof(aset),f)==sizeof(aset)&&!std::memcmp(aset,"ASET",4);
    const auto offset=le64(aset+24),bytes=le64(aset+32);
    ok=ok&&bytes==sizeof(nacp)&&offset<=std::uint64_t(length)-image&&bytes<=std::uint64_t(length)-image-offset
        &&!std::fseek(f,long(image+offset),SEEK_SET)&&std::fread(nacp,1,sizeof(nacp),f)==sizeof(nacp);
    if(std::fclose(f))ok=false;
    return ok&&le64(nacp+0x3038)==NXDK_TITLE_ID
        &&!std::memcmp(nacp,NXDK_APP_NAME,sizeof(NXDK_APP_NAME))
        &&!std::memcmp(nacp+0x200,NXDK_APP_AUTHOR,sizeof(NXDK_APP_AUTHOR));
}
bool title_verified_on_owner(){
#ifdef __SWITCH__
    u64 current=0;
    // This describes the current host title, not a claim about installed NACP.
    return R_SUCCEEDED(svcGetInfo(&current,InfoType_ProgramId,CUR_PROCESS_HANDLE,0))&&current==NXDK_TITLE_ID;
#else
    return false;
#endif
}
}

struct LocalUpdateServer::Impl {
    mutable std::mutex mutex;
    LocalUpdateSnapshot view;
    std::string root,app,version;
    char token[65]{};
    ReportFn report=nullptr;void* context=nullptr;
    bool created=false,sockets=false,title_verified=false;
    std::atomic<bool> stopping{false},done{false},bridge_compatible{false};
#ifdef __SWITCH__
    Thread thread{};
#else
    std::thread thread;
#endif
    void set(LocalUpdateState state,const std::string& detail){
        std::lock_guard<std::mutex> lock(mutex);
        view.detail=detail;
        if(!stopping.load())view.state=state;
    }
    void failed(const char* detail){set(LocalUpdateState::Failed,detail);}
    int ready(Socket fd,bool writing,Clock::time_point deadline){
        while(!stopping.load()){
            const auto now=Clock::now();if(now>=deadline)return 0;
            const auto left=std::min(io_slice,std::chrono::duration_cast<std::chrono::milliseconds>(deadline-now));
            timeval timeout{0,long(std::max<std::int64_t>(1,left.count())*1000)};
            fd_set set;FD_ZERO(&set);FD_SET(fd,&set);
            const int result=select(int(fd)+1,writing?nullptr:&set,writing?&set:nullptr,nullptr,&timeout);
            if(result>0)return 1;
            if(result<0){if(transient_socket_error())continue;return -1;}
        }
        return 0;
    }
    bool transfer(Socket fd,void* bytes,std::size_t count,bool writing,Clock::time_point deadline){
        auto* at=static_cast<char*>(bytes);
        while(count&&!stopping.load()){
            if(ready(fd,writing,deadline)<=0)return false;
            const auto chunk=int(std::min<std::size_t>(count,32768));
#ifdef MSG_NOSIGNAL
            constexpr int flags=MSG_NOSIGNAL;
#else
            constexpr int flags=0;
#endif
            const int n=writing?::send(fd,at,chunk,flags):recv(fd,at,chunk,0);
            if(n<0&&transient_socket_error())continue;
            if(n<=0)return false;
            at+=n;count-=unsigned(n);
        }
        return !count&&!stopping.load();
    }
    bool reply(Socket fd,const std::string& text){return transfer(fd,const_cast<char*>(text.data()),text.size(),true,Clock::now()+std::chrono::seconds(5));}
    bool line(Socket fd,char (&out)[256]){
        const auto deadline=Clock::now()+std::chrono::seconds(5);
        for(unsigned i=0;i<255;++i){char c=0;if(!transfer(fd,&c,1,false,deadline))return false;
            if(c=='\n'){out[i]=0;return true;}if(c<' '||c>'~')return false;out[i]=c;}
        return false;
    }
    void progress(std::uint32_t received,std::uint32_t total){std::lock_guard<std::mutex> lock(mutex);view.received=received;view.total=total;}
    void store(Socket fd,const ReloadCommand& command){
        if(command.target!=RELOAD_BRIDGE&&command.target!=RELOAD_GAME){reply(fd,"ERR target\n");return;}
        char incoming[512]{};
        if(!reload_path(incoming,sizeof(incoming),root.c_str(),command.target,1)){reply(fd,"ERR path\n");return;}
        FILE* f=std::fopen(incoming,"wb");
        if(!f){reply(fd,"ERR storage\n");return;}
        set(LocalUpdateState::Receiving,"Receiving update; reopen the app manually after storage.");progress(0,std::uint32_t(command.bytes));
        bool ok=reply(fd,"OK SEND\n");CharacterDigest digest;unsigned char buffer[8192];std::size_t received=0;
        const auto deadline=Clock::now()+std::chrono::seconds(120);
        while(ok&&received<command.bytes&&!stopping.load()){
            const auto count=std::min(sizeof(buffer),command.bytes-received);
            ok=transfer(fd,buffer,count,false,deadline)&&std::fwrite(buffer,1,count,f)==count&&digest.update(buffer,count);
            if(ok){received+=count;progress(std::uint32_t(received),std::uint32_t(command.bytes));}
        }
        ok=ok&&!stopping.load()&&received==command.bytes;
        unsigned char actual[32]{};ok=ok&&digest.finish(actual)&&!std::memcmp(actual,command.sha256,sizeof(actual));
        if(ok&&std::fflush(f))ok=false;
#ifdef _WIN32
        if(ok&&_commit(_fileno(f)))ok=false;
#else
        if(ok&&fsync(fileno(f)))ok=false;
#endif
        if(std::fclose(f))ok=false;
        // Cancellation is honored until publication. The short rename transaction
        // then runs to completion, keeping the old image recoverable as .previous.
        ok=ok&&!stopping.load()&&own_image(incoming);
        bool rollback_incomplete=false;
        if(ok&&!stopping.load()){
            char final[512]{},backup[544]{};
            ok=reload_path(final,sizeof(final),root.c_str(),command.target,0);
            if(ok){
                std::snprintf(backup,sizeof(backup),"%s.previous",final);
                const auto old=file_kind(final);const bool had_old=old==FileKind::Regular;
                ok=old!=FileKind::Other;
                if(ok&&had_old){
                    const auto previous=file_kind(backup);
                    ok=previous!=FileKind::Other&&(previous==FileKind::Missing||std::remove(backup)==0)
                        &&std::rename(final,backup)==0;
                }
                if(ok&&std::rename(incoming,final)){
                    if(had_old&&std::rename(backup,final))rollback_incomplete=true;
                    ok=false;
                }
            }
        }else ok=false;
        if(!ok){std::remove(incoming);reply(fd,"ERR transfer_or_verification\n");set(LocalUpdateState::Listening,rollback_incomplete?"Publication failed; previous image remains in .previous.":"Transfer rejected; current app preserved.");return;}
        if(command.target==RELOAD_BRIDGE)bridge_compatible.store(true);
        char hex[65]{};for(unsigned i=0;i<32;++i)std::snprintf(hex+i*2,3,"%02x",actual[i]);
        set(LocalUpdateState::Stored,std::string(command.target==RELOAD_BRIDGE?"BRIDGE":"GAME")+" stored; SHA256 "+hex+". Close and reopen the app manually.");
        reply(fd,"OK STORED\n");
    }
    void connection(Socket fd){
        char request[256]{};
        if(!line(fd,request)||std::strncmp(request,"AUTH ",5)||!reload_token_equal(token,request+5)){reply(fd,"ERR auth\n");return;}
        std::memset(request,0,sizeof(request));
        if(!reply(fd,"OK AUTH\n")||!line(fd,request))return;
        ReloadCommand command{};if(!reload_parse(request,&command)){reply(fd,"ERR command\n");return;}
        if(command.kind==RELOAD_RUN){reply(fd,"ERR manual_restart_required\n");return;}
        if(command.kind==RELOAD_PUT){store(fd,command);return;}
        if(command.kind==RELOAD_CRASH){
            if(!report){reply(fd,"ERR report\n");return;}
            std::vector<char> bytes(report_capacity);
            const auto size=report(bytes.data(),bytes.size(),context);
            if(stopping.load())return;
            if(!size||size>bytes.size()){reply(fd,"ERR report\n");return;}
            std::string text(bytes.data(),size);
            for(auto at=text.find(token);at!=std::string::npos;at=text.find(token,at+10))text.replace(at,64,"[REDACTED]");
            if(reply(fd,"OK REPORT "+std::to_string(text.size())+"\n"))transfer(fd,text.data(),text.size(),true,Clock::now()+std::chrono::seconds(15));
            return;
        }
        if(command.kind==RELOAD_STATUS){
            const auto status=std::string("{\"protocol\":2,\"app\":\"")+escaped(app)+"\",\"version\":\""+escaped(version)
                +"\",\"image_sha256\":\"\",\"bridge_protocol\":\""+RELOAD_BRIDGE_MARKER+"\",\"bridge_compatible\":"+(bridge_compatible.load()?"true":"false")
                +",\"title_verified\":"+(title_verified?"true":"false")+",\"manual_restart_required\":true}\n";
            reply(fd,status);
        }
    }
    void run(){
        const auto listener=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
        if(listener==invalid_socket){failed("Could not create local update socket.");done.store(true);return;}
#ifndef _WIN32
        const int reuse=1;setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
#endif
        sockaddr_in address{};address.sin_family=AF_INET;address.sin_port=htons(port);address.sin_addr.s_addr=htonl(INADDR_ANY);
        if(!nonblocking(listener)||bind(listener,reinterpret_cast<sockaddr*>(&address),sizeof(address))||listen(listener,2)){
            close_socket(listener);failed("Local update port is unavailable.");done.store(true);return;
        }
        set(LocalUpdateState::Listening,"Local update ready. Stop before leaving; reopen manually after upload.");
        while(!stopping.load()){
            const auto result=ready(listener,false,Clock::now()+io_slice);
            if(result<0){failed("Local update connection failed.");break;}
            if(!result)continue;
            const auto client=accept(listener,nullptr,nullptr);if(client==invalid_socket)continue;
            if(nonblocking(client))connection(client);
            close_socket(client);
        }
        close_socket(listener);done.store(true);
    }
#ifdef __SWITCH__
    static void entry(void* context){static_cast<Impl*>(context)->run();}
#endif
    void release_sockets(){
        if(!sockets)return;
#ifdef __SWITCH__
        socketExit();
#elif defined(_WIN32)
        WSACleanup();
#endif
        sockets=false;
    }
};

LocalUpdateServer::LocalUpdateServer():impl_(new Impl){}
LocalUpdateServer::~LocalUpdateServer(){
    request_stop();
    if(impl_->created){
#ifdef __SWITCH__
        threadWaitForExit(&impl_->thread);threadClose(&impl_->thread);
#else
        impl_->thread.join();
#endif
        impl_->created=false;
    }
    impl_->release_sockets();erase_token(impl_->token);
}
bool LocalUpdateServer::start(const char* root,const char* app,const char* version,const char* token_file,ReportFn report,void* context){
    poll();if(impl_->created)return false;
    impl_->stopping.store(false);
    if(!root||!*root||std::strlen(root)>400||!app||!*app||std::strlen(app)>80||!version||!*version||std::strlen(version)>32
        ||!token_from_file(token_file,impl_->token)){
        impl_->failed("Local update credential or configuration unavailable.");return false;
    }
    impl_->root=root;impl_->app=app;impl_->version=version;impl_->report=report;impl_->context=context;
    impl_->stopping.store(false);impl_->done.store(false);
    impl_->set(LocalUpdateState::Starting,"Starting local update.");impl_->progress(0,0);
#ifdef __SWITCH__
    const bool sockets_ready=R_SUCCEEDED(socketInitializeDefault());
#elif defined(_WIN32)
    WSADATA data{};const bool sockets_ready=WSAStartup(MAKEWORD(2,2),&data)==0;
#else
    const bool sockets_ready=true;
#endif
    if(!sockets_ready){impl_->failed("Could not initialize local networking.");erase_token(impl_->token);return false;}
    impl_->sockets=true;impl_->title_verified=title_verified_on_owner();
    char bridge[512]{};impl_->bridge_compatible.store(reload_path(bridge,sizeof(bridge),root,RELOAD_BRIDGE,0)&&reload_bridge_valid(bridge));
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
#ifdef __SWITCH__
        in_addr address{};address.s_addr=gethostid();char text[INET_ADDRSTRLEN]{};
        impl_->view.address=inet_ntop(AF_INET,&address,text,sizeof(text))?std::string(text)+":28285":"TCP 28285";
#else
        impl_->view.address="127.0.0.1:28285";
#endif
    }
#ifdef __SWITCH__
    auto rc=threadCreate(&impl_->thread,Impl::entry,impl_.get(),nullptr,128*1024,0x30,-2);
    const bool created=R_SUCCEEDED(rc);
    if(created)rc=threadStart(&impl_->thread);
    if(R_FAILED(rc)){if(created)threadClose(&impl_->thread);impl_->release_sockets();impl_->failed("Could not start local update worker.");erase_token(impl_->token);return false;}
#else
    try{impl_->thread=std::thread([this]{impl_->run();});}
    catch(...){impl_->release_sockets();impl_->failed("Could not start local update worker.");erase_token(impl_->token);return false;}
#endif
    impl_->created=true;return true;
}
void LocalUpdateServer::request_stop(){
    if(!impl_->created){
        std::lock_guard<std::mutex> lock(impl_->mutex);impl_->view.state=LocalUpdateState::Off;impl_->view.address.clear();return;
    }
    impl_->stopping.store(true);
    std::lock_guard<std::mutex> lock(impl_->mutex);impl_->view.state=LocalUpdateState::Stopping;
}
void LocalUpdateServer::poll(){
    if(!impl_->created||!impl_->done.load())return;
#ifdef __SWITCH__
    if(R_FAILED(waitSingle(waiterForThread(&impl_->thread),0)))return;
    threadClose(&impl_->thread);
#else
    impl_->thread.join();
#endif
    impl_->created=false;impl_->release_sockets();erase_token(impl_->token);impl_->report=nullptr;impl_->context=nullptr;
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if(impl_->view.state!=LocalUpdateState::Failed)impl_->view.state=LocalUpdateState::Off;
    impl_->view.address.clear();
}
LocalUpdateSnapshot LocalUpdateServer::snapshot()const{std::lock_guard<std::mutex> lock(impl_->mutex);return impl_->view;}
bool LocalUpdateServer::stopped()const{return !impl_->created;}
}
