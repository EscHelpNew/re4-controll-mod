#include "re4/live_debug_server.hpp"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
#include <atomic>
#include <chrono>
#include <thread>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#endif

namespace {
constexpr char token[]="0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
struct Session {
 std::string input,output;
 std::size_t cursor=0,chunk=2,writes=0,cancel_after=~std::size_t(0),snapshot_size=11;
 unsigned snapshots=0;
 bool cancel=false;
 static int read(void* p,void* bytes,std::size_t n){
  auto& s=*static_cast<Session*>(p);n=std::min({n,s.chunk,s.input.size()-s.cursor});
  std::memcpy(bytes,s.input.data()+s.cursor,n);s.cursor+=n;return int(n);
 }
 static int write(void* p,const void* bytes,std::size_t n){
  auto& s=*static_cast<Session*>(p);n=std::min(n,s.chunk);
  s.output.append(static_cast<const char*>(bytes),n);++s.writes;return int(n);
 }
 static bool cancelled(void* p){auto& s=*static_cast<Session*>(p);return s.cancel||s.writes>=s.cancel_after;}
 static std::size_t snapshot(void* p,char* out,std::size_t cap){
  auto& s=*static_cast<Session*>(p);++s.snapshots;
  if(s.snapshot_size<=cap)std::memset(out,'x',s.snapshot_size);
  if(s.snapshot_size==11)std::memcpy(out,"{\"ok\":true}",11);
  return s.snapshot_size;
 }
 void run(){re4::detail::serve_live_debug_session(token,this,snapshot,{this,read,write,cancelled});}
};
std::string request(const std::string& command="SNAPSHOT\n"){return std::string("AUTH ")+token+"\n"+command;}
void protocol_tests(){
 Session good;good.input=request();good.run();
 assert(good.output=="OK RE4-DIAG 1\nOK JSON 11\n{\"ok\":true}");assert(good.snapshots==1);
 Session bad;bad.input=request();bad.input[60]='X';bad.run();assert(bad.output=="ERR AUTH\n"&&!bad.snapshots);
 for(const auto& command:{"READ 0 32\n","WRITE 0 0\n","EXEC test\n","GET /secret\n","SNAPSHOT extra\n"}){
  Session denied;denied.input=request(command);denied.run();assert(denied.output=="OK RE4-DIAG 1\nERR REQUEST\n"&&!denied.snapshots);
 }
 Session oversized;oversized.input=request(std::string(81,'S')+"\n");oversized.run();
 assert(oversized.output=="OK RE4-DIAG 1\nERR REQUEST\n"&&!oversized.snapshots);
 Session auth_oversized;auth_oversized.input=std::string(81,'A')+"\n";auth_oversized.run();assert(auth_oversized.output=="ERR AUTH\n");
 Session invalid_control;invalid_control.input=request("SNAPSHOT\r\n");invalid_control.run();assert(!invalid_control.snapshots);
 Session truncated;truncated.input=request("SNAPSHOT");truncated.run();assert(!truncated.snapshots);
 Session cancelled;cancelled.input=request();cancelled.cancel=true;cancelled.run();assert(cancelled.output.empty()&&!cancelled.snapshots);
 Session partial;partial.input=request();partial.chunk=1;partial.cancel_after=3;partial.run();assert(partial.output=="OK "&&!partial.snapshots);
 for(auto count:{std::size_t(0),re4::kLiveDebugJsonLimit+1}){
  Session invalid;invalid.input=request();invalid.snapshot_size=count;invalid.run();assert(invalid.output=="OK RE4-DIAG 1\nERR SNAPSHOT\n");
 }
 Session max;max.input=request("SNAPSHOT\nREAD 1 2\n");max.snapshot_size=re4::kLiveDebugJsonLimit;max.run();
 assert(max.snapshots==1&&max.output.size()==std::strlen("OK RE4-DIAG 1\nOK JSON 16384\n")+re4::kLiveDebugJsonLimit);
 assert(max.cursor==request().size()); // One authenticated snapshot, then close.
}
using Clock=std::chrono::steady_clock;
#ifdef _WIN32
using Socket=SOCKET;
constexpr Socket invalid_socket=INVALID_SOCKET;
void close_socket(Socket socket){closesocket(socket);}
#else
using Socket=int;
constexpr Socket invalid_socket=-1;
void close_socket(Socket socket){close(socket);}
#endif
struct Worker {
 std::atomic<unsigned> initialized{0},captured{0},destroyed{0};
 std::thread::id id;
 bool fail=false;
 static void* initialize(void* p){auto& w=*static_cast<Worker*>(p);w.id=std::this_thread::get_id();++w.initialized;return w.fail?nullptr:p;}
 static std::size_t snapshot(void* p,char* json,std::size_t capacity){
  auto& w=*static_cast<Worker*>(p);assert(w.id==std::this_thread::get_id());assert(capacity==16384);++w.captured;
  constexpr char payload[]="{\"game\":\"absent\"}";std::memcpy(json,payload,sizeof(payload)-1);return sizeof(payload)-1;
 }
 static void destroy(void* p){auto& w=*static_cast<Worker*>(p);assert(w.id==std::this_thread::get_id());++w.destroyed;}
 re4::LiveDebugCallbacks callbacks(){return {this,initialize,snapshot,destroy};}
};
void await_status(re4::LiveDebugStatus expected){
 const auto deadline=Clock::now()+std::chrono::seconds(2);
 while(re4::live_debug_server_status()!=expected&&Clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(5));
 assert(re4::live_debug_server_status()==expected);
}
Socket connect_client(){
 const auto socket=::socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);assert(socket!=invalid_socket);
 sockaddr_in address{};address.sin_family=AF_INET;address.sin_port=htons(re4::live_debug_server_port());address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
 assert(!connect(socket,reinterpret_cast<sockaddr*>(&address),sizeof(address)));return socket;
}
void transmit(Socket socket,const std::string& text){
 // Deliberate one-byte sends exercise partial network framing too.
 for(const char c:text)assert(send(socket,&c,1,0)==1);
}
std::string receive_to_close(Socket socket,std::chrono::seconds timeout=std::chrono::seconds(3)){
 std::string result;const auto deadline=Clock::now()+timeout;
 while(Clock::now()<deadline){
  fd_set set;FD_ZERO(&set);FD_SET(socket,&set);timeval wait{0,100000};
  const int ready=select(int(socket+1),&set,nullptr,nullptr,&wait);assert(ready>=0);
  if(!ready)continue;
  char bytes[7];const int n=recv(socket,bytes,sizeof(bytes),0);assert(n>=0);
  if(!n)return result;
  result.append(bytes,std::size_t(n));
 }
 assert(false&&"server did not close connection before timeout");return result;
}
void loopback_tests(){
#ifdef _WIN32
 WSADATA data{};assert(!WSAStartup(MAKEWORD(2,2),&data));
#endif
 Worker worker;
 assert(!re4::start_live_debug_server("short",worker.callbacks(),0));
 assert(!re4::start_live_debug_server(token,{},0));
 assert(re4::start_live_debug_server(token,worker.callbacks(),0));await_status(re4::LiveDebugStatus::Listening);
 assert(re4::live_debug_server_port()!=0&&worker.initialized==1);
 assert(!re4::start_live_debug_server(token,worker.callbacks(),0));
 for(unsigned i=0;i<3;++i){
  const auto client=connect_client();transmit(client,request());
  assert(receive_to_close(client)=="OK RE4-DIAG 1\nOK JSON 17\n{\"game\":\"absent\"}");close_socket(client);
 }
 assert(worker.initialized==1&&worker.captured==3&&!worker.destroyed);
 const auto bad=connect_client();auto bad_request=std::string("AUTH ")+token+"\n";bad_request[5]='f';transmit(bad,bad_request);
 assert(receive_to_close(bad)=="ERR AUTH\n");close_socket(bad);assert(worker.captured==3);
 const auto idle=connect_client();transmit(idle,"AUTH ");std::this_thread::sleep_for(std::chrono::milliseconds(120));
 const auto before=Clock::now();re4::stop_live_debug_server();
 assert(Clock::now()-before<std::chrono::seconds(1));close_socket(idle);
 assert(worker.destroyed==1&&re4::live_debug_server_status()==re4::LiveDebugStatus::Stopped&&!re4::live_debug_server_port());
 re4::stop_live_debug_server();
 // Reopening the overlay must rebind its fixed port immediately, including
 // after the server was the active closer of a successful snapshot session.
 Worker restart;
 assert(re4::start_live_debug_server(token,restart.callbacks(),0));await_status(re4::LiveDebugStatus::Listening);
 const auto fixed_port=re4::live_debug_server_port();
 for(unsigned cycle=0;cycle<3;++cycle){
  const auto client=connect_client();transmit(client,request());
  assert(receive_to_close(client)=="OK RE4-DIAG 1\nOK JSON 17\n{\"game\":\"absent\"}");close_socket(client);
  re4::stop_live_debug_server();
  assert(re4::start_live_debug_server(token,restart.callbacks(),fixed_port));await_status(re4::LiveDebugStatus::Listening);
  assert(re4::live_debug_server_port()==fixed_port);
 }
 re4::stop_live_debug_server();assert(restart.initialized==4&&restart.destroyed==4&&restart.captured==3);
 Worker failure;failure.fail=true;assert(re4::start_live_debug_server(token,failure.callbacks(),0));
 await_status(re4::LiveDebugStatus::InitializeFailed);re4::stop_live_debug_server();assert(failure.initialized==1&&!failure.destroyed);
 // The connection deadline is absolute, including clients that never finish AUTH.
 Worker timeout;assert(re4::start_live_debug_server(token,timeout.callbacks(),0));await_status(re4::LiveDebugStatus::Listening);
 const auto stalled=connect_client();const auto start=Clock::now();
 const auto expired=receive_to_close(stalled,std::chrono::seconds(10));
 assert(expired.rfind("ERR TRANSPORT 7 ",0)==0&&expired.back()=='\n'&&expired.size()<=98);
 const auto duration=Clock::now()-start;assert(duration>=std::chrono::seconds(7)&&duration<std::chrono::seconds(10));
 close_socket(stalled);re4::stop_live_debug_server();assert(!timeout.captured&&timeout.destroyed==1);
#ifdef _WIN32
 WSACleanup();
#endif
}
}
int main(){protocol_tests();loopback_tests();std::cout<<"live debug server protocol and loopback tests passed\n";}
