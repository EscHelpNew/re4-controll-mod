#include "live_debug.hpp"
#include "game_link.hpp"
#include "re4/live_debug_server.hpp"
#include "re4/live_diagnostics.hpp"
#include <cassert>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>
namespace {
unsigned sm_calls=0,sm_exits=0,sockets=0,socket_exits=0,starts=0,stops=0,link_inits=0,link_closes=0,reads=0,sm_fail=0;
bool socket_fail=false,server_fail=false,started=false,callback_during_stop=false;
re4::LiveDebugCallbacks callbacks{};void* worker_context=nullptr;
std::vector<unsigned> order;
void reset(){
    overlay_debug::stop();sm_calls=sm_exits=sockets=socket_exits=starts=stops=link_inits=link_closes=reads=sm_fail=0;
    socket_fail=server_fail=started=callback_during_stop=false;order.clear();
}
}
Result smInitialize(){return ++sm_calls==sm_fail?1:0;}
void smExit(){++sm_exits;}
Result socketInitialize(const SocketInitConfig* c){
    ++sockets;assert(c->tcp_tx_buf_size==0x4000&&c->tcp_rx_buf_size==0x4000&&c->num_bsd_sessions==1&&c->sb_efficiency==4);
    return socket_fail?1:0;
}
void socketExit(){++socket_exits;order.push_back(2);}
void GameLink::initialize(){++link_inits;}
void GameLink::initialize_shared(const GameLink&){++link_inits;}
void GameLink::close(){++link_closes;order.push_back(1);}
bool GameLink::read_diagnostic_snapshot(re4::LiveDiagnosticSnapshot& s){++reads;s.status=re4::LiveDiagnosticStatus::NoGame;return false;}
namespace re4 {
bool start_live_debug_server(const char* token,LiveDebugCallbacks c,std::uint16_t port){
    ++starts;assert(std::strlen(token)==64&&port==28773);if(server_fail)return false;
    callbacks=c;worker_context=c.initialize(c.context);started=true;return true;
}
void stop_live_debug_server(){++stops;if(started){
    if(callback_during_stop){
        char bytes[16]{};assert(callbacks.snapshot(worker_context,bytes,sizeof(bytes))==0);
        callbacks.transport_error(callbacks.context,{9,99,-1,22,0,0});
    }
    callbacks.destroy(worker_context);started=false;
}}
LiveDebugStatus live_debug_server_status(){return started?LiveDebugStatus::Listening:LiveDebugStatus::Stopped;}
std::uint16_t live_debug_server_port(){return started?28773:0;}
bool format_live_diagnostics(const LiveDiagnosticSnapshot&,char* out,std::size_t cap,std::size_t& size){
    assert(cap>2);std::memcpy(out,"{}",3);size=2;return true;
}
}
int main(){
    const auto dir=std::filesystem::temp_directory_path()/
        ("re4-live-debug-runtime-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    assert(std::filesystem::create_directory(dir));const auto key=dir/"connection.key";
    const auto write=[&](const std::string& text){std::ofstream file(key,std::ios::binary);file<<text;};
    write(std::string(64,'1'));
    reset();overlay_debug::start(key.string().c_str());
    assert(overlay_debug::state()==overlay_debug::State::Off&&!sm_calls&&!sockets&&!starts);
    assert(!std::filesystem::exists(dir/"live-debug.log"));
    std::filesystem::remove(key);
    reset();overlay_debug::set_mode(re4::LaunchMode::Diagnostics,key.string().c_str());assert(overlay_debug::state()==overlay_debug::State::MissingKey&&!sm_calls&&!sockets);
    write(std::string(64,'1')+"X");overlay_debug::start(key.string().c_str());assert(!sockets);
    write(std::string(64,'1')+"\r\n");
    reset();overlay_debug::set_mode(re4::LaunchMode::Diagnostics,key.string().c_str());assert(overlay_debug::state()==overlay_debug::State::Listening&&overlay_debug::port()==28773);
    assert(sockets==1&&starts==1&&link_inits==1&&sm_calls==2&&sm_exits==1);
    assert(callbacks.transport_error);
    callbacks.transport_error(callbacks.context,{3,63,-1,22,0,0});
    const auto log=dir/"live-debug.log";
    const auto contents=[&](){std::ifstream file(log,std::ios::binary);return std::string(std::istreambuf_iterator<char>(file),{});};
    assert(contents().find("stage=3 fd=63 result=-1 errno=22 bsd=00000000 ms=0")!=std::string::npos);
    assert(contents().find("snapshot={}")!=std::string::npos&&reads==1);
    assert(contents().find(std::string(64,'1'))==std::string::npos);
    {std::ofstream file(log,std::ios::binary|std::ios::trunc);file<<std::string(32768,'x');}
    callbacks.transport_error(callbacks.context,{2,4,-1,22,0xD401,7});
    assert(contents()==std::string(32768,'x')); // Bounded disk use without overwriting history.
    overlay_debug::start(key.string().c_str());assert(sockets==1&&starts==1);
    char json[64]{};assert(callbacks.snapshot(worker_context,json,sizeof(json))==2&&reads==2);
    callback_during_stop=true;const auto before_stop_log=contents();
    overlay_debug::set_mode(re4::LaunchMode::Normal,key.string().c_str());assert(link_closes==1&&sm_exits==2&&socket_exits==1&&(order==std::vector<unsigned>{1,2}));
    assert(overlay_debug::mode()==re4::LaunchMode::Normal&&contents()==before_stop_log&&reads==2);
    std::filesystem::remove(log);
    callbacks.transport_error(callbacks.context,{9,99,-1,22,0,0});
    assert(!std::filesystem::exists(log));
    overlay_debug::stop();assert(socket_exits==1&&overlay_debug::state()==overlay_debug::State::Off);
    overlay_debug::start(key.string().c_str());assert(socket_exits==1&&starts==1); // Legacy start cannot override Normal.
    overlay_debug::set_mode(re4::LaunchMode::Diagnostics,key.string().c_str());assert(starts==2&&sockets==2&&link_inits==2);
    callbacks.transport_error(callbacks.context,{1,7,-1,22,0,0});assert(std::filesystem::exists(log)&&reads==3);
    reset();sm_fail=1;overlay_debug::set_mode(re4::LaunchMode::Diagnostics,key.string().c_str());assert(overlay_debug::state()==overlay_debug::State::ServicesUnavailable&&!sockets);
    reset();socket_fail=true;overlay_debug::set_mode(re4::LaunchMode::Diagnostics,key.string().c_str());assert(overlay_debug::state()==overlay_debug::State::SocketsUnavailable&&sm_exits==1&&!starts);
    reset();server_fail=true;overlay_debug::set_mode(re4::LaunchMode::Diagnostics,key.string().c_str());assert(overlay_debug::state()==overlay_debug::State::Failed&&socket_exits==1&&!link_inits);
    reset();sm_fail=2;overlay_debug::set_mode(re4::LaunchMode::Diagnostics,key.string().c_str());assert(overlay_debug::state()==overlay_debug::State::Listening&&!link_inits);
    assert(callbacks.snapshot(worker_context,json,sizeof(json))==2);overlay_debug::stop();assert(!link_closes&&sm_exits==1&&socket_exits==1);
    std::filesystem::remove(log);std::filesystem::remove(key);std::filesystem::remove(dir);
    std::puts("overlay launch mode: default no IO, enable/disable/re-enable, stale callbacks, failures and shutdown ordering PASS");
}
