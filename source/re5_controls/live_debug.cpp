#include "live_debug.hpp"
#include "game_link.hpp"
#include "re4/live_debug_server.hpp"
#include <switch.h>
#include <atomic>
#include <cstdio>
#include <cstring>

namespace {
std::atomic<bool> monitoring{false};
std::atomic<re5_overlay_debug::State> frontend{re5_overlay_debug::State::Off};
bool sockets_owned=false;
const Re5GameLink* link_owner=nullptr;
struct Worker {Re5GameLink link;bool sm_ready=false;} worker;
void wipe(char* bytes,std::size_t count){volatile char* p=bytes;while(count--)*p++=0;}
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
 if(!monitoring.load(std::memory_order_acquire))return nullptr;
 if(!link_owner){frontend=re5_overlay_debug::State::ServicesUnavailable;return nullptr;}
 auto& value=*static_cast<Worker*>(context);
 value.sm_ready=R_SUCCEEDED(smInitialize());
 if(!value.sm_ready){frontend=re5_overlay_debug::State::ServicesUnavailable;return nullptr;}
 // Keep diagnostic state private, but borrow the owner's existing debug
 // session: opening another dmnt:cht session can block at service capacity.
 value.link.initialize_shared(*link_owner);return context;
}
std::size_t snapshot_worker(void* context,char* json,std::size_t capacity){
 if(!monitoring.load(std::memory_order_acquire))return 0;
 return static_cast<Worker*>(context)->link.diagnostic_snapshot(json,capacity);
}
void destroy_worker(void* context){
 auto& value=*static_cast<Worker*>(context);
 if(value.sm_ready){value.link.close();smExit();value.sm_ready=false;}
}
}
namespace re5_overlay_debug {
void share_link(const Re5GameLink* owner){link_owner=owner;}
void set_enabled(bool enable,const char* path){
 if(!enable){stop();return;}
 monitoring.store(true,std::memory_order_release);
 if(sockets_owned)return;
 char token[65]{};
 if(!read_token(path,token)){frontend=State::MissingKey;return;}
 if(R_FAILED(smInitialize())){wipe(token,sizeof(token));frontend=State::ServicesUnavailable;return;}
 SocketInitConfig config{};
 config.tcp_tx_buf_size=config.tcp_rx_buf_size=0x4000;
 config.udp_tx_buf_size=config.udp_rx_buf_size=0x1000;
 config.sb_efficiency=4;config.num_bsd_sessions=1;config.bsd_service_type=BsdServiceType_User;
 const Result result=socketInitialize(&config);smExit();
 if(R_FAILED(result)){wipe(token,sizeof(token));frontend=State::SocketsUnavailable;return;}
 sockets_owned=true;frontend=State::Starting;
 const bool started=re4::start_live_debug_server(token,{&worker,initialize_worker,snapshot_worker,destroy_worker,nullptr},28774);
 wipe(token,sizeof(token));
 if(!started){re4::stop_live_debug_server();socketExit();sockets_owned=false;frontend=State::Failed;}
}
bool enabled(){return monitoring.load(std::memory_order_acquire);}
void stop(){
 // Disable snapshots before joining, then release BSD after the worker services.
 monitoring.store(false,std::memory_order_release);
 if(sockets_owned){re4::stop_live_debug_server();socketExit();sockets_owned=false;}
 frontend=State::Off;
}
State state(){
 const auto current=frontend.load(std::memory_order_acquire);
 if(!sockets_owned||current==State::ServicesUnavailable)return current;
 switch(re4::live_debug_server_status()){
  case re4::LiveDebugStatus::Starting:return State::Starting;
  case re4::LiveDebugStatus::Listening:return State::Listening;
  default:return State::Failed;
 }
}
std::uint16_t port(){return state()==State::Listening?re4::live_debug_server_port():0;}
}
