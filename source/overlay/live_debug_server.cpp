#include "re4/live_debug_server.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#ifdef __SWITCH__
#include <switch.h>
#include <switch/services/bsd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#elif defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <thread>
#endif

namespace {
bool valid_token(const char* token){
 if(!token)return false;
 for(unsigned i=0;i<64;++i){
  const auto c=token[i];
  if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))return false;
 }
 return token[64]==0;
}
void erase_token(char* token){for(unsigned i=0;i<65;++i)static_cast<volatile char*>(token)[i]=0;}
struct Session {
 const re4::detail::LiveDebugIo& io;
 bool cancelled()const{return io.cancelled&&io.cancelled(io.context);}
 bool send(const void* bytes,std::size_t count)const{
  auto* p=static_cast<const char*>(bytes);
  while(count&&!cancelled()){
   const auto n=io.write(io.context,p,count);
   if(n<=0||std::size_t(n)>count)return false;
   count-=std::size_t(n);p+=n;
  }
  return !count&&!cancelled();
 }
 bool text(const char* text)const{return send(text,std::strlen(text));}
 bool line(char (&out)[81],std::size_t& size)const{
  std::memset(out,0,sizeof(out));size=0;
  while(!cancelled()){
   char c=0;if(io.read(io.context,&c,1)!=1)return false;
   if(c=='\n')return true;
   if(c<' '||c>'~'||size==80)return false;
   out[size++]=c;
  }
  return false;
 }
};

#ifdef _WIN32
using Socket=SOCKET;
constexpr Socket invalid_socket=INVALID_SOCKET;
void close_socket(Socket socket){closesocket(socket);}
bool nonblocking(Socket socket){u_long mode=1;return !ioctlsocket(socket,FIONBIO,&mode);}
#else
using Socket=int;
constexpr Socket invalid_socket=-1;
void close_socket(Socket socket){::close(socket);}
bool nonblocking(Socket socket){const int flags=fcntl(socket,F_GETFL,0);return flags>=0&&!fcntl(socket,F_SETFL,flags|O_NONBLOCK);}
#endif
int wait_socket(Socket socket,bool writing){
 fd_set set;FD_ZERO(&set);FD_SET(socket,&set);timeval timeout{0,100000};
 return select(int(socket+1),writing?nullptr:&set,writing?&set:nullptr,nullptr,&timeout);
}
using Clock=std::chrono::steady_clock;
struct SocketFailure {int error;std::uint32_t bsd_result;int bsd_error=0;};
SocketFailure socket_failure(){
#ifdef _WIN32
 const int error=WSAGetLastError();return {error,0};
#elif defined(__SWITCH__)
 const int error=errno;const auto result=socketGetLastResult();const int native_error=g_bsdErrno;return {error,result,native_error};
#else
 const int error=errno;return {error,0};
#endif
}
bool transient_error(int error){
#ifdef _WIN32
 return error==WSAEWOULDBLOCK||error==WSAEINTR;
#else
 return error==EAGAIN||error==EWOULDBLOCK||error==EINTR;
#endif
}
std::uint64_t observation_milliseconds(){
#ifdef __SWITCH__
 return armGetSystemTick()/19200;
#else
 return std::uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count());
#endif
}
void pause_ms(unsigned milliseconds){
#ifdef __SWITCH__
 svcSleepThread(std::int64_t(milliseconds)*1000000);
#else
 std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
#endif
}
Socket open_listener(std::uint16_t port,std::uint16_t& bound){
 bound=0;
 Socket listener=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
 bool ok=listener!=invalid_socket;
 if(ok){
#ifndef _WIN32
  // The server actively closes snapshot connections; allow an overlay reopen
  // to reclaim its fixed port while those connections remain in TIME_WAIT.
  // Windows already permits this restart; its SO_REUSEADDR additionally
  // permits competing live binds, so retain the default Windows behavior.
  const int reuse=1;
  ok=!setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
#endif
  sockaddr_in address{};address.sin_family=AF_INET;address.sin_port=htons(port);address.sin_addr.s_addr=htonl(INADDR_ANY);
  ok=ok&&nonblocking(listener)&&!bind(listener,reinterpret_cast<sockaddr*>(&address),sizeof(address))&&!listen(listener,2);
  if(ok){
#ifdef _WIN32
   int length=sizeof(address);
#else
   socklen_t length=sizeof(address);
#endif
   ok=!getsockname(listener,reinterpret_cast<sockaddr*>(&address),&length);
   if(ok)bound=ntohs(address.sin_port);
  }
 }
 if(!ok&&listener!=invalid_socket){close_socket(listener);listener=invalid_socket;}
 return listener;
}
struct Server {
 std::atomic<bool> stopping{false};
 std::atomic<re4::LiveDebugStatus> status{re4::LiveDebugStatus::Stopped};
 std::atomic<std::uint16_t> port{0};
 std::uint16_t requested_port=0;
 Socket listener=invalid_socket;
 bool created=false;
 char token[65]{};
 re4::LiveDebugCallbacks callbacks{};
#ifdef __SWITCH__
 Thread thread{};
#else
 std::thread thread;
#endif
 struct Observation {
  Server& owner;Socket socket;
  std::uint64_t started=observation_milliseconds();
  re4::LiveDebugTransportError error{};
  bool configured=false;
  void record(unsigned stage,int result,SocketFailure failure){
   if(error.stage)return;
   const auto elapsed=observation_milliseconds()-started;
   error={stage,int(socket),result,failure.error,failure.bsd_result,
    std::uint32_t(std::min<std::uint64_t>(elapsed,UINT32_MAX))};
  }
  bool configure(){
#ifdef _WIN32
   u_long mode=1;const int result=ioctlsocket(socket,FIONBIO,&mode);
   if(result){const auto failure=socket_failure();record(2,result,failure);return false;}
#else
   const int flags=fcntl(socket,F_GETFL,0);
   const auto get_failure=socket_failure();
   // libnx converts a BSD -1 into the positive O_NONBLOCK bit pattern.
   // Inspect the captured native error, not possibly stale C errno alone.
   if(flags<0||get_failure.bsd_result||get_failure.bsd_error!=0){record(1,flags,get_failure);return false;}
   const int result=fcntl(socket,F_SETFL,flags|O_NONBLOCK);
   const auto set_failure=socket_failure();
   if(result||set_failure.bsd_result||set_failure.bsd_error!=0){record(2,result,set_failure);return false;}
#endif
   configured=true;return true;
  }
  void report(){
   if(!error.stage)return;
   if(owner.callbacks.transport_error)owner.callbacks.transport_error(owner.callbacks.context,error);
   if(socket==invalid_socket)return;
#ifdef _WIN32
   // A failed FIONBIO leaves blocking state unknown; logging still works.
   if(!configured)return;
   constexpr int flags=0;
#else
   // Per-call nonblocking also works when accepted-socket fcntl failed.
   constexpr int flags=MSG_DONTWAIT
#if defined(MSG_NOSIGNAL) && !defined(__SWITCH__)
    |MSG_NOSIGNAL
#endif
    ;
#endif
   char line[98];const int length=std::snprintf(line,sizeof(line),
    "ERR TRANSPORT %u %d %d %d %08X %u\n",unsigned(error.stage),error.fd,error.result,error.error,
    unsigned(error.bsd_result),unsigned(error.elapsed_ms));
   if(length<=0||std::size_t(length)>=sizeof(line))return;
   std::size_t offset=0;
   // No select, waits, retries on EAGAIN, or unbounded draining on teardown.
   for(unsigned attempt=0;attempt<4&&offset<std::size_t(length);++attempt){
    const int n=::send(socket,line+offset,length-int(offset),flags);
    if(n<=0||n>length-int(offset))break;
    offset+=std::size_t(n);
   }
   char discard[128];
   for(unsigned attempt=0;attempt<4;++attempt){
    if(recv(socket,discard,sizeof(discard),
#ifdef _WIN32
      0
#else
      MSG_DONTWAIT
#endif
      )<=0)break;
   }
  }
 };
 struct Connection {
  Server& owner;
  Socket socket;
  Observation& observation;
  Clock::time_point deadline=Clock::now()+std::chrono::seconds(8);
  static bool cancelled(void* p){
   const auto& c=*static_cast<Connection*>(p);
   if(c.observation.error.stage||c.owner.stopping.load(std::memory_order_relaxed))return true;
   if(Clock::now()<c.deadline)return false;
   c.observation.record(7,-1,{0,0});return true;
  }
  int transfer(void* bytes,std::size_t count,bool writing){
   while(!cancelled(this)){
    const auto ready=wait_socket(socket,writing);
    if(ready<0){
     const auto failure=socket_failure();if(transient_error(failure.error))continue;
     observation.record(writing?5:3,ready,failure);return -1;
    }
    if(!ready)continue;
    const auto size=int(std::min<std::size_t>(count,re4::kLiveDebugJsonLimit));
#if defined(MSG_NOSIGNAL)
    constexpr int send_flags=MSG_NOSIGNAL;
#else
    constexpr int send_flags=0;
#endif
    const int n=writing?::send(socket,static_cast<const char*>(bytes),size,send_flags)
                       :recv(socket,static_cast<char*>(bytes),size,0);
    if(n<0){
     const auto failure=socket_failure();if(transient_error(failure.error))continue;
     observation.record(writing?6:4,n,failure);
    }else if(!n)observation.record(writing?6:8,n,{0,0});
    return n;
   }
   return -1;
  }
  static int read(void* p,void* bytes,std::size_t count){return static_cast<Connection*>(p)->transfer(bytes,count,false);}
  static int write(void* p,const void* bytes,std::size_t count){return static_cast<Connection*>(p)->transfer(const_cast<void*>(bytes),count,true);}
 };
 void run(){
  void* worker=callbacks.initialize(callbacks.context);
  if(!worker){status.store(re4::LiveDebugStatus::InitializeFailed);return;}
  status.store(re4::LiveDebugStatus::Listening);
  while(!stopping.load(std::memory_order_relaxed)){
   if(listener==invalid_socket){
    // Network loss (sleep, dock, Wi-Fi drop) invalidates the listener. Keep
    // the worker alive and rebind instead of leaving a dead backlog that
    // resets every client until the overlay is reloaded.
    std::uint16_t bound=0;
    listener=open_listener(requested_port,bound);
    if(listener==invalid_socket){status.store(re4::LiveDebugStatus::Starting);pause_ms(1000);continue;}
    port.store(bound);status.store(re4::LiveDebugStatus::Listening);
   }
   const int ready=wait_socket(listener,false);
   if(ready<0){
    const auto failure=socket_failure();if(transient_error(failure.error))continue;
    listener_error(9,ready,failure);close_socket(listener);listener=invalid_socket;port.store(0);
    // A persistent select failure can survive a successful rebind. Bound the
    // socket/log churn while checking cancellation during the recovery delay.
    for(unsigned waited=0;waited<100&&!stopping.load(std::memory_order_relaxed);waited+=10)pause_ms(10);
    continue;
   }
   if(!ready)continue;
   const auto client=accept(listener,nullptr,nullptr);
   if(client==invalid_socket){
    // ECONNABORTED and friends belong to one client, never to the server.
    const auto failure=socket_failure();if(transient_error(failure.error))continue;
    listener_error(10,-1,failure);pause_ms(50);continue;
   }
   Observation observation{*this,client};
   if(observation.configure()){
    Connection connection{*this,client,observation};
    re4::detail::serve_live_debug_session(token,worker,callbacks.snapshot,
      {&connection,Connection::read,Connection::write,Connection::cancelled});
   }
   observation.report();
   close_socket(client);
  }
  callbacks.destroy(worker);
 }
 void listener_error(unsigned stage,int result,SocketFailure failure){
  if(!callbacks.transport_error)return;
  re4::LiveDebugTransportError error{stage,int(listener),result,failure.error,failure.bsd_result,0};
  callbacks.transport_error(callbacks.context,error);
 }
 static void entry(void* p){static_cast<Server*>(p)->run();}
} server;
}

namespace re4::detail {
void serve_live_debug_session(const char* token,void* worker,
 std::size_t (*snapshot)(void*,char*,std::size_t),const LiveDebugIo& io){
 if(!valid_token(token)||!snapshot||!io.read||!io.write)return;
 Session session{io};char line[81]{};std::size_t size=0;
 const bool complete=session.line(line,size);
 // Always compare all 64 secret characters. Length/prefix affect the result
 // but never cause an early exit from the key comparison.
 volatile unsigned difference=unsigned(size^69u)|unsigned(!complete);
 constexpr char prefix[]="AUTH ";
 for(unsigned i=0;i<5;++i)difference=difference|unsigned(static_cast<unsigned char>(line[i])^prefix[i]);
 for(unsigned i=0;i<64;++i)difference=difference|unsigned(static_cast<unsigned char>(line[i+5])^static_cast<unsigned char>(token[i]));
 std::memset(line,0,sizeof(line));
 if(difference){session.text("ERR AUTH\n");return;}
 if(!session.text("OK RE4-DIAG 1\n"))return;
 if(!session.line(line,size)||size!=8||std::memcmp(line,"SNAPSHOT",8)){
  session.text("ERR REQUEST\n");return;
 }
 if(session.cancelled())return;
 char json[kLiveDebugJsonLimit];const auto count=snapshot(worker,json,sizeof(json));
 if(session.cancelled())return;
 if(!count||count>sizeof(json)){session.text("ERR SNAPSHOT\n");return;}
 char header[40]{};
 const int length=std::snprintf(header,sizeof(header),"OK JSON %u\n",unsigned(count));
 if(length>0&&std::size_t(length)<sizeof(header)&&session.send(header,std::size_t(length)))session.send(json,count);
}
}

namespace re4 {
bool start_live_debug_server(const char* token,LiveDebugCallbacks callbacks,std::uint16_t port){
 if(server.created)return false;
 if(!valid_token(token)||!callbacks.initialize||!callbacks.snapshot||!callbacks.destroy)return false;
 server.status.store(LiveDebugStatus::Starting);
 std::uint16_t bound=0;
 server.listener=open_listener(port,bound);
 bool ok=server.listener!=invalid_socket;
 if(ok){server.port.store(bound);server.requested_port=bound;}
 if(ok){
  std::memcpy(server.token,token,65);server.callbacks=callbacks;server.stopping.store(false);
#ifdef __SWITCH__
  Result rc=threadCreate(&server.thread,Server::entry,&server,nullptr,64*1024,0x2D,-2);
  if(R_SUCCEEDED(rc)){rc=threadStart(&server.thread);if(R_FAILED(rc))threadClose(&server.thread);}
  ok=R_SUCCEEDED(rc);
#else
  try{server.thread=std::thread(Server::entry,&server);}catch(...){ok=false;}
#endif
  if(!ok)server.status.store(LiveDebugStatus::ThreadFailed);
 }else server.status.store(LiveDebugStatus::SocketFailed);
 if(!ok){
  if(server.listener!=invalid_socket)close_socket(server.listener);
  server.listener=invalid_socket;server.port.store(0);erase_token(server.token);server.callbacks={};return false;
 }
 server.created=true;return true;
}
void stop_live_debug_server(){
 if(server.created){
  server.stopping.store(true);
#ifdef __SWITCH__
  threadWaitForExit(&server.thread);threadClose(&server.thread);
#else
  server.thread.join();
#endif
  if(server.listener!=invalid_socket)close_socket(server.listener);
  server.listener=invalid_socket;server.created=false;
  erase_token(server.token);server.callbacks={};
 }
 server.port.store(0);server.status.store(LiveDebugStatus::Stopped);
}
LiveDebugStatus live_debug_server_status(){return server.status.load();}
std::uint16_t live_debug_server_port(){return server.port.load();}
}
