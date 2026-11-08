#pragma once
#include <cstddef>
#include <cstdint>

namespace re4 {
inline constexpr std::uint16_t kLiveDebugPort=28773;
inline constexpr std::size_t kLiveDebugJsonLimit=16384;
enum class LiveDebugStatus { Stopped, Starting, Listening, InitializeFailed, SocketFailed, ThreadFailed };
struct LiveDebugTransportError {
 std::uint32_t stage=0;
 int fd=0,result=0,error=0;
 std::uint32_t bsd_result=0,elapsed_ms=0;
};
// All callbacks run on the dedicated worker. initialize returns its owned
// context; nullptr fails startup. snapshot returns exact JSON byte count.
// Callbacks must finish promptly; stop cannot interrupt a running callback.
struct LiveDebugCallbacks {
 void* context=nullptr;
 void* (*initialize)(void*)=nullptr;
 std::size_t (*snapshot)(void*,char*,std::size_t)=nullptr;
 void (*destroy)(void*)=nullptr;
 // First terminal error per accepted client, on the worker; original context.
 // Stages: 1=getflags,2=setflags,3=read_select,4=recv,5=write_select,
 // 6=send,7=deadline,8=peer_eof. Listener errors (server keeps running):
 // 9=listen_select (listener is rebuilt), 10=accept. No authentication/game bytes.
 void (*transport_error)(void*,const LiveDebugTransportError&)=nullptr;
};
// Caller owns BSD/WSA initialization. Start/stop are called from one owner
// thread; status/port may be read concurrently. Stop before socketExit().
// Start means the port is bound and worker started; initialization is async.
bool start_live_debug_server(const char* token,LiveDebugCallbacks callbacks,std::uint16_t port=kLiveDebugPort);
void stop_live_debug_server();
LiveDebugStatus live_debug_server_status();
std::uint16_t live_debug_server_port();
namespace detail {
struct LiveDebugIo {
 void* context;
 int (*read)(void*,void*,std::size_t);
 int (*write)(void*,const void*,std::size_t);
 bool (*cancelled)(void*);
};
// Protocol engine; adapters bound blocking I/O and timeout/cancellation.
void serve_live_debug_session(const char* token,void* worker,
 std::size_t (*snapshot)(void*,char*,std::size_t),const LiveDebugIo& io);
}
}
