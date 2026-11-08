#include "live_debug.hpp"
#include "game_link.hpp"
#include "re4/live_debug_server.hpp"
#include "re4/live_diagnostics.hpp"
#include <switch.h>
#include <atomic>
#include <cstdio>
#include <cstring>

namespace {
std::atomic<overlay_debug::State> frontend{overlay_debug::State::Off};
std::atomic<re4::LaunchMode> launch_mode{re4::LaunchMode::Normal};
bool sockets_owned=false;
const GameLink* link_owner=nullptr;
char trace_path[768]{};
struct Worker {GameLink link;bool sm_ready=false,snapshot_logged=false;} worker;
void wipe(char* value,std::size_t count){volatile char* p=value;while(count--)*p++=0;}
void set_trace_path(const char* key_path){
    trace_path[0]=0;
    const char* last=nullptr;
    for(const char* p=key_path;*p;++p)if(*p=='/'||*p=='\\')last=p;
    const auto prefix=last?std::size_t(last-key_path)+1:0;
    constexpr char filename[]="live-debug.log";
    if(prefix+sizeof(filename)>sizeof(trace_path))return;
    std::memcpy(trace_path,key_path,prefix);
    std::memcpy(trace_path+prefix,filename,sizeof(filename));
}
void record_transport_error(void* context,const re4::LiveDebugTransportError& error){
    if(!re4::automatic_diagnostics(launch_mode.load(std::memory_order_acquire))||!trace_path[0])return;
    char line[192]{};
    const int length=std::snprintf(line,sizeof(line),
        "v=%s stage=%u fd=%d result=%d errno=%d bsd=%08X ms=%u\n",
        re4::kVersion,error.stage,error.fd,error.result,error.error,error.bsd_result,error.elapsed_ms);
    if(length<=0||std::size_t(length)>=sizeof(line))return;
    FILE* file=std::fopen(trace_path,"ab");if(!file)return;
    auto& value=*static_cast<Worker*>(context);
    bool logged=false;
    if(!std::fseek(file,0,SEEK_END)){
        const auto size=std::ftell(file);
        if(size>=0&&size<=32768-length&&std::fwrite(line,1,std::size_t(length),file)==std::size_t(length)
            &&!value.snapshot_logged){
            // Keep one local game observation even when TCP cannot deliver it.
            // This uses the same isolated, read-only worker as AUTH+SNAPSHOT.
            char json[re4::kLiveDebugJsonLimit];std::size_t bytes=0;
            re4::LiveDiagnosticSnapshot snapshot;
            if(value.sm_ready)value.link.read_diagnostic_snapshot(snapshot);
            else snapshot.status=re4::LiveDiagnosticStatus::ServiceUnavailable;
            if(re4::format_live_diagnostics(snapshot,json,sizeof(json),bytes)
                &&bytes+10<=std::size_t(32768-size-length)){
                logged=std::fwrite("snapshot=",1,9,file)==9
                    &&std::fwrite(json,1,bytes,file)==bytes&&std::fputc('\n',file)!=EOF;
            }
        }
    }
    if(std::fclose(file))logged=false;
    if(logged)value.snapshot_logged=true;
}
bool read_token(const char* path,char (&token)[65]){
    if(!path)return false;
    FILE* file=std::fopen(path,"rb");if(!file)return false;
    char bytes[68]{};auto count=std::fread(bytes,1,67,file);
    bool valid=!std::ferror(file)&&std::fgetc(file)==EOF&&count<=66;
    if(std::fclose(file))valid=false;
    while(count&&(bytes[count-1]=='\r'||bytes[count-1]=='\n'))--count;
    valid=valid&&count==64;
    for(std::size_t i=0;i<count&&valid;++i)
        valid=(bytes[i]>='0'&&bytes[i]<='9')||(bytes[i]>='a'&&bytes[i]<='f')||(bytes[i]>='A'&&bytes[i]<='F');
    if(valid){std::memcpy(token,bytes,64);token[64]=0;}
    wipe(bytes,sizeof(bytes));return valid;
}
void* initialize_worker(void* context){
    auto& value=*static_cast<Worker*>(context);
    value.snapshot_logged=false;
    if(!re4::automatic_diagnostics(launch_mode.load(std::memory_order_acquire)))return context;
    value.sm_ready=R_SUCCEEDED(smInitialize());
    if(value.sm_ready){if(link_owner)value.link.initialize_shared(*link_owner);else value.link.initialize();}
    return context;
}
std::size_t snapshot_worker(void* context,char* json,std::size_t capacity){
    if(!re4::automatic_diagnostics(launch_mode.load(std::memory_order_acquire)))return 0;
    auto& value=*static_cast<Worker*>(context);re4::LiveDiagnosticSnapshot snapshot;
    if(value.sm_ready)value.link.read_diagnostic_snapshot(snapshot);
    else snapshot.status=re4::LiveDiagnosticStatus::ServiceUnavailable;
    std::size_t size=0;
    return re4::format_live_diagnostics(snapshot,json,capacity,size)?size:0;
}
void destroy_worker(void* context){
    auto& value=*static_cast<Worker*>(context);
    if(value.sm_ready){value.link.close();smExit();value.sm_ready=false;}
}
}
namespace overlay_debug {
void share_link(const GameLink* owner){link_owner=owner;}
void start(const char* path){
    if(!re4::automatic_diagnostics(launch_mode.load(std::memory_order_acquire))){frontend=State::Off;return;}
    if(sockets_owned)return;
    char token[65]{};
    if(!read_token(path,token)){frontend=State::MissingKey;return;}
    set_trace_path(path);
    if(R_FAILED(smInitialize())){wipe(token,sizeof(token));frontend=State::ServicesUnavailable;return;}
    SocketInitConfig config{};
    config.tcp_tx_buf_size=config.tcp_rx_buf_size=0x4000;
    config.udp_tx_buf_size=config.udp_rx_buf_size=0x1000;
    // Listener plus each accepted client need a full TCP buffer set; 1 resets every client before accept.
    config.sb_efficiency=4;config.num_bsd_sessions=1;config.bsd_service_type=BsdServiceType_User;
    const Result result=socketInitialize(&config);smExit();
    if(R_FAILED(result)){wipe(token,sizeof(token));frontend=State::SocketsUnavailable;return;}
    sockets_owned=true;frontend=State::Starting;
    const bool started=re4::start_live_debug_server(token,{&worker,initialize_worker,snapshot_worker,destroy_worker,record_transport_error});
    wipe(token,sizeof(token));
    if(!started){re4::stop_live_debug_server();socketExit();sockets_owned=false;frontend=State::Failed;}
}
void stop(){
    // Publish disabled before joining: stale/error callbacks cannot open logs
    // or issue another diagnostic snapshot while the worker drains.
    launch_mode.store(re4::LaunchMode::Normal,std::memory_order_release);
    if(sockets_owned){re4::stop_live_debug_server();socketExit();sockets_owned=false;}
    trace_path[0]=0;
    frontend=State::Off;
}
void set_mode(re4::LaunchMode next,const char* path){
    if(!re4::automatic_diagnostics(next)){stop();return;}
    launch_mode.store(re4::LaunchMode::Diagnostics,std::memory_order_release);start(path);
}
re4::LaunchMode mode(){return launch_mode.load(std::memory_order_acquire);}
State state(){
    if(!sockets_owned)return frontend;
    switch(re4::live_debug_server_status()){
        case re4::LiveDebugStatus::Starting:return State::Starting;
        case re4::LiveDebugStatus::Listening:return State::Listening;
        default:return State::Failed;
    }
}
std::uint16_t port(){return state()==State::Listening?re4::live_debug_server_port():0;}
}
