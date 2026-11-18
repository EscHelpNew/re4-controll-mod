#include "live_debug.hpp"
#include "game_link.hpp"
#include "re4/live_debug_server.hpp"
#include <cassert>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>
namespace {
unsigned independent_inits=0;
Re5GameLink ui_owner;bool owner_alive=true,defer_initialize=false,pending_initialize=false;
unsigned sm_calls=0,sm_exits=0,sockets=0,socket_exits=0,starts=0,stops=0,link_inits=0,link_closes=0,reads=0,sm_fail=0;
bool socket_fail=false,server_fail=false,started=false,callback_during_stop=false;
re4::LiveDebugCallbacks callbacks{};void* worker_context=nullptr;Re5GameLink* worker_link=nullptr;
std::vector<unsigned> order;
void reset(){
 re5_overlay_debug::stop();sm_calls=sm_exits=sockets=socket_exits=starts=stops=link_inits=link_closes=reads=sm_fail=0;
 socket_fail=server_fail=started=callback_during_stop=defer_initialize=pending_initialize=false;worker_link=nullptr;order.clear();
 independent_inits=0;owner_alive=true;re5_overlay_debug::share_link(&ui_owner);
}
}
Result smInitialize(){return ++sm_calls==sm_fail?1:0;}
void smExit(){++sm_exits;}
Result socketInitialize(const SocketInitConfig* c){
 ++sockets;assert(c->tcp_tx_buf_size==0x4000&&c->tcp_rx_buf_size==0x4000&&c->num_bsd_sessions==1&&c->sb_efficiency==4&&c->bsd_service_type==BsdServiceType_User);
 return socket_fail?1:0;
}
void socketExit(){++socket_exits;order.push_back(2);}
// The owner already holds the only available debug-service session. A worker
// independently opening another session would wait indefinitely on hardware.
void Re5GameLink::initialize(){++independent_inits;assert(false&&"debug-service session capacity exhausted");}
void Re5GameLink::initialize_shared(const Re5GameLink& owner){
 assert(&owner==&ui_owner&&this!=&ui_owner&&owner_alive);++link_inits;worker_link=this;
}
void Re5GameLink::close(){assert(this==worker_link&&owner_alive);++link_closes;order.push_back(1);}
std::size_t Re5GameLink::diagnostic_snapshot(char* out,std::size_t cap){
 assert(this==worker_link);++reads;constexpr char json[]="{\"schema\":1,\"game\":\"RE5\",\"supported\":false}";
 if(cap<sizeof(json))return 0;
 std::memcpy(out,json,sizeof(json));return sizeof(json)-1;
}
namespace re4 {
bool start_live_debug_server(const char* token,LiveDebugCallbacks c,std::uint16_t port){
 ++starts;assert(std::strlen(token)==64&&port==28774);if(server_fail)return false;
 callbacks=c;if(defer_initialize){pending_initialize=true;return true;}
 worker_context=c.initialize(c.context);started=worker_context!=nullptr;return true;
}
void stop_live_debug_server(){++stops;
 if(pending_initialize){
  pending_initialize=false;assert(owner_alive);
  assert(callbacks.initialize(callbacks.context)==nullptr);
 }
 if(started){
 if(callback_during_stop){char bytes[64]{};assert(callbacks.snapshot(worker_context,bytes,sizeof(bytes))==0);}
 callbacks.destroy(worker_context);started=false;
}}
LiveDebugStatus live_debug_server_status(){return started?LiveDebugStatus::Listening:LiveDebugStatus::InitializeFailed;}
std::uint16_t live_debug_server_port(){return started?28774:0;}
}
int main(){
 const auto dir=std::filesystem::temp_directory_path()/("re5-monitor-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 assert(std::filesystem::create_directory(dir));const auto key=dir/"connection.key";
 const auto write=[&](const std::string& text){std::ofstream file(key,std::ios::binary);file<<text;};
 reset();assert(!re5_overlay_debug::enabled()&&re5_overlay_debug::state()==re5_overlay_debug::State::Off);
 re5_overlay_debug::set_enabled(false,key.string().c_str());assert(!sm_calls&&!sockets&&!starts&&re5_overlay_debug::state()==re5_overlay_debug::State::Off);
 re5_overlay_debug::set_enabled(true,key.string().c_str());assert(re5_overlay_debug::enabled()&&re5_overlay_debug::state()==re5_overlay_debug::State::MissingKey&&!sm_calls&&!sockets);
 for(const auto& text:{std::string(64,'1')+"X",std::string(64,'1')+"\n\n\n",std::string("bad")}){
  write(text);re5_overlay_debug::set_enabled(true,key.string().c_str());assert(re5_overlay_debug::state()==re5_overlay_debug::State::MissingKey&&!sockets);
 }
 write(std::string(64,'1')+"\r\n");
 reset();re5_overlay_debug::set_enabled(true,key.string().c_str());
 assert(re5_overlay_debug::enabled()&&re5_overlay_debug::state()==re5_overlay_debug::State::Listening&&re5_overlay_debug::port()==28774);
 assert(sockets==1&&starts==1&&link_inits==1&&sm_calls==2&&sm_exits==1&&!independent_inits);
 re5_overlay_debug::set_enabled(true,key.string().c_str());assert(sockets==1&&starts==1);
 char json[128]{};assert(callbacks.snapshot(worker_context,json,sizeof(json))==43&&reads==1&&std::strstr(json,"\"game\":\"RE5\""));
 callback_during_stop=true;re5_overlay_debug::set_enabled(false,key.string().c_str());
 assert(!re5_overlay_debug::enabled()&&re5_overlay_debug::state()==re5_overlay_debug::State::Off&&re5_overlay_debug::port()==0);
 assert(link_closes==1&&sm_exits==2&&socket_exits==1&&reads==1&&(order==std::vector<unsigned>{1,2}));
 re5_overlay_debug::stop();assert(socket_exits==1);
 re5_overlay_debug::set_enabled(true,key.string().c_str());assert(starts==2&&sockets==2&&link_inits==2);
 reset();sm_fail=1;re5_overlay_debug::set_enabled(true,key.string().c_str());assert(re5_overlay_debug::state()==re5_overlay_debug::State::ServicesUnavailable&&!sockets);
 reset();socket_fail=true;re5_overlay_debug::set_enabled(true,key.string().c_str());assert(re5_overlay_debug::state()==re5_overlay_debug::State::SocketsUnavailable&&sm_exits==1&&!starts);
 reset();server_fail=true;re5_overlay_debug::set_enabled(true,key.string().c_str());assert(re5_overlay_debug::state()==re5_overlay_debug::State::Failed&&socket_exits==1&&!link_inits);
 reset();sm_fail=2;re5_overlay_debug::set_enabled(true,key.string().c_str());assert(re5_overlay_debug::state()==re5_overlay_debug::State::ServicesUnavailable&&!link_inits&&!reads);
 re5_overlay_debug::stop();assert(!link_closes&&sm_exits==1&&socket_exits==1);
 reset();re5_overlay_debug::share_link(nullptr);re5_overlay_debug::set_enabled(true,key.string().c_str());
 assert(re5_overlay_debug::state()==re5_overlay_debug::State::ServicesUnavailable&&!independent_inits&&!link_inits);
 re5_overlay_debug::stop();
 reset();defer_initialize=true;re5_overlay_debug::set_enabled(true,key.string().c_str());
 const auto before_cancel_sm=sm_calls;
 re5_overlay_debug::stop();re5_overlay_debug::share_link(nullptr);owner_alive=false;
 assert(!pending_initialize&&!independent_inits&&!link_inits&&sm_calls==before_cancel_sm&&socket_exits==1);
 reset();re5_overlay_debug::set_enabled(true,key.string().c_str());re5_overlay_debug::stop();
 re5_overlay_debug::share_link(nullptr);owner_alive=false;
 assert(link_closes==1&&!independent_inits&&(order==std::vector<unsigned>{1,2}));
 std::filesystem::remove(key);std::filesystem::remove(dir);
 std::puts("RE5 monitor: opt-in, bounded key, borrowed session at exhausted capacity, cancellation, failures and owner lifetime PASS");
}
