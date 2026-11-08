#include "re4/live_debug_server.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>
#include <cassert>
#include <iostream>
#include <string>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using TestSocket=SOCKET;
constexpr auto bad_socket=INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstdarg>
using TestSocket=int;
constexpr auto bad_socket=-1;
#endif
namespace fault {
std::atomic<TestSocket> accepted{bad_socket};
std::atomic<unsigned> stage{0},hits{0},reply_sends{0};
std::atomic<int> configure_result{-1};
bool trigger(unsigned value){if(stage==value&&hits.fetch_add(1)==0)return true;return false;}
void set_error(){
#ifdef _WIN32
 WSASetLastError(WSAEINVAL);
#else
 errno=EINVAL;
#endif
}
TestSocket accept_call(TestSocket s,sockaddr* address,
#ifdef _WIN32
 int* length
#else
 socklen_t* length
#endif
){const auto fd=::accept(s,address,length);accepted=fd;return fd;}
int select_call(int count,fd_set* reads,fd_set* writes,fd_set* errors,timeval* timeout){
 const auto fd=accepted.load();
 if(fd!=bad_socket&&((reads&&FD_ISSET(fd,reads)&&trigger(3))||(writes&&FD_ISSET(fd,writes)&&trigger(5)))){set_error();return -1;}
 return ::select(count,reads,writes,errors,timeout);
}
int recv_call(TestSocket fd,char* data,int size,int flags){
 if(fd==accepted&&trigger(4)){set_error();return -1;}
 return int(::recv(fd,data,size,flags));
}
int send_call(TestSocket fd,const char* data,int size,int flags){
 if(fd==accepted&&trigger(6)){set_error();return -1;}
 if(fd==accepted)++reply_sends;
 return int(::send(fd,data,size,flags));
}
#ifdef _WIN32
int ioctl_call(TestSocket fd,long command,u_long* value){
 if(fd==accepted&&trigger(2)){set_error();return configure_result;}
 return ::ioctlsocket(fd,command,value);
}
#else
int fcntl_call(int fd,int command,...){
 va_list args;va_start(args,command);const auto value=va_arg(args,int);va_end(args);
 if(fd==accepted&&trigger(command==F_GETFL?1:2)){set_error();return -1;}
 return ::fcntl(fd,command,value);
}
#endif
}
int fixture_send(TestSocket fd,const char* bytes,int count,int flags){return fault::send_call(fd,bytes,count,flags);}
// Interpose platform calls only in this translation unit; production contains
// no fault-injection API. All uninjected socket operations use real loopback.
#define accept fault::accept_call
#define select fault::select_call
#define recv fault::recv_call
#define send fixture_send
#ifdef _WIN32
#define ioctlsocket fault::ioctl_call
#else
#define fcntl fault::fcntl_call
#endif
#include "../source/overlay/live_debug_server.cpp"
#undef accept
#undef select
#undef recv
#undef send
#undef ioctlsocket
#undef fcntl
namespace {
constexpr char token[]="0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
struct Capture {
 std::atomic<unsigned> errors{0},snapshots{0};
 re4::LiveDebugTransportError last{};
 std::thread::id thread;
 static void* init(void* p){static_cast<Capture*>(p)->thread=std::this_thread::get_id();return p;}
 static std::size_t snapshot(void* p,char* out,std::size_t){++static_cast<Capture*>(p)->snapshots;std::memcpy(out,"{}",2);return 2;}
 static void destroy(void*){}
 static void error(void* p,const re4::LiveDebugTransportError& value){
  auto& c=*static_cast<Capture*>(p);assert(c.thread==std::this_thread::get_id());c.last=value;++c.errors;
 }
 re4::LiveDebugCallbacks callbacks(){return {this,init,snapshot,destroy,error};}
};
void close_client(TestSocket fd){
#ifdef _WIN32
 closesocket(fd);
#else
 close(fd);
#endif
}
void wait_listening(){
 const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(2);
 while(re4::live_debug_server_status()==re4::LiveDebugStatus::Starting&&std::chrono::steady_clock::now()<until)std::this_thread::yield();
 assert(re4::live_debug_server_status()==re4::LiveDebugStatus::Listening);
}
void check_stage(unsigned expected){
 Capture capture;fault::accepted=bad_socket;fault::stage=expected;fault::hits=0;fault::reply_sends=0;
 assert(re4::start_live_debug_server(token,capture.callbacks(),0));wait_listening();
 const auto fd=::socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);assert(fd!=bad_socket);
 sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);address.sin_port=htons(re4::live_debug_server_port());
 assert(!connect(fd,reinterpret_cast<sockaddr*>(&address),sizeof(address)));
 if(expected==4||expected==5||expected==6){const auto auth=std::string("AUTH ")+token+"\n";assert(::send(fd,auth.data(),int(auth.size()),0)==int(auth.size()));}
 if(expected==8){
#ifdef _WIN32
  assert(!shutdown(fd,SD_SEND));
#else
  assert(!shutdown(fd,SHUT_WR));
#endif
 }
 std::string reply;const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(10);
 while(std::chrono::steady_clock::now()<until){
  fd_set reads;FD_ZERO(&reads);FD_SET(fd,&reads);timeval timeout{0,100000};
  if(::select(int(fd+1),&reads,nullptr,nullptr,&timeout)<=0)continue;
  char bytes[128];const int n=int(::recv(fd,bytes,sizeof(bytes),0));if(n<=0)break;reply.append(bytes,std::size_t(n));
 }
 close_client(fd);re4::stop_live_debug_server();
 assert(capture.errors==1);assert(!capture.snapshots);assert(capture.last.stage==expected);
 assert(capture.last.fd>=0&&capture.last.bsd_result==0&&capture.last.elapsed_ms<10000);
 if(expected==7)assert(capture.last.elapsed_ms>=7000);
 if(expected!=7&&expected!=8)assert(capture.last.result==(expected==2?fault::configure_result.load():-1)&&capture.last.error!=0);
#ifdef _WIN32
 if(expected==2){assert(reply.empty());return;} // Configure failed; cannot assume nonblocking.
#endif
 char prefix[80];std::snprintf(prefix,sizeof(prefix),"ERR TRANSPORT %u %d %d %d 00000000 ",expected,capture.last.fd,capture.last.result,capture.last.error);
 assert(reply.rfind(prefix,0)==0&&reply.back()=='\n'&&reply.size()<=98);
}
void first_failure_test(){
 Server owner;Capture capture;capture.thread=std::this_thread::get_id();owner.callbacks=capture.callbacks();
 Server::Observation observation{owner,invalid_socket};
 observation.record(1,0x4000,{EINVAL,0x1234ABCD,EINVAL});
 observation.record(6,-1,{EIO,0x5678});observation.report();
 assert(capture.errors==1&&capture.last.stage==1&&capture.last.result==0x4000);
 assert(capture.last.error==EINVAL&&capture.last.bsd_result==0x1234ABCD);
}
}
int main(){
#ifdef _WIN32
 WSADATA data{};assert(!WSAStartup(MAKEWORD(2,2),&data));
#else
 check_stage(1);
#endif
 first_failure_test();
 for(unsigned stage:{2u,3u,4u,5u,6u,7u,8u})check_stage(stage);
#ifdef _WIN32
 fault::configure_result=0x4000;check_stage(2);fault::configure_result=-1;
#endif
#ifdef _WIN32
 WSACleanup();
#endif
 std::cout<<"live debug native transport fault tests passed\n";
}
