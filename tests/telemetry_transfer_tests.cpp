#include "re6/log_server.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <cstdlib>
#include <chrono>
#ifdef _WIN32
#include <winsock2.h>
#endif
#undef assert
#define assert(check) do { if(!(check)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#check);std::exit(1);} } while(false)

namespace fs=std::filesystem;
static const std::string token(64,'a'); // Public deterministic test credential.
struct Wire {
 std::string in,out;std::size_t pos=0,chunk=3;int reads=0,writes=0;
 int fail_read=-1,fail_write=-1;bool cancel=false;
 std::function<void(Wire&)> on_write;
 static int read(void* p,void* b,std::size_t n){
  auto& w=*static_cast<Wire*>(p);
  if(w.reads++==w.fail_read)return -1;
  n=std::min({n,w.chunk,w.in.size()-w.pos});
  std::memcpy(b,w.in.data()+w.pos,n);w.pos+=n;return int(n);
 }
 static int write(void* p,const void* b,std::size_t n){
  auto& w=*static_cast<Wire*>(p);
  if(w.writes++==w.fail_write)return -1;
  n=std::min(n,w.chunk);w.out.append(static_cast<const char*>(b),n);
  if(w.on_write)w.on_write(w);
  return int(n);
 }
 static bool cancelled(void* p){return static_cast<Wire*>(p)->cancel;}
 void run(const fs::path& root){re6::detail::serve_log_session(root.string().c_str(),token.c_str(),{this,read,write,cancelled});}
};
static void put(const fs::path& p,const std::string& data){std::ofstream(p,std::ios::binary)<<data;}
int main(){
 const fs::path root="build/telemetry_transfer_fixture";
 fs::remove_all(root);fs::create_directories(root);
 put(root/"capture.csv","abc");put(root/"unfinished.csv.part","private");
 put(root/"other.txt","private");fs::create_directory(root/"directory.csv");
 const std::string auth="AUTH "+token+"\n";
 {Wire w;w.in="AUTH "+std::string(64,'b')+"\nLIST\n";w.run(root);assert(w.out=="ERR AUTH\n");}
 {Wire w;w.in=auth+"LIST\n";w.run(root);assert(w.out=="OK AUTH\nOK LIST 1\nFILE capture.csv 3\nEND\n");}
 const std::string good="OK AUTH\nOK FILE 3 ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\nabc\nOK DONE\n";
 {Wire w;w.in=auth+"GET capture.csv\n";w.run(root);assert(w.out==good);assert(w.reads>20&&w.writes>20);}
 put(root/"bulk.csv",std::string(70000,'x'));
 {Wire w;w.in=auth+"GET bulk.csv\n";w.run(root);
  assert(w.out=="OK AUTH\nOK FILE 70000 bca09f4a757d5571c7d9f3341d4301f3c391c090826acc1a3013c6bcb7c01722\n"+std::string(70000,'x')+"\nOK DONE\n");}
 fs::remove(root/"bulk.csv");put(root/"empty.csv","");
 {Wire w;w.in=auth+"GET empty.csv\n";w.run(root);
  assert(w.out=="OK AUTH\nOK FILE 0 e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855\n\nOK DONE\n");}
 fs::remove(root/"empty.csv");
 for(const char* name:{"../capture.csv","/capture.csv","capture.csv.part","x\\capture.csv","a..csv","directory.csv","unfinished.csv.part","other.txt"}){
  Wire w;w.in=auth+"GET "+name+"\n";w.run(root);assert(w.out=="OK AUTH\nERR FILE\n");
 }
 {Wire w;w.in=auth+std::string(300,'x')+"\n";w.run(root);assert(w.out=="OK AUTH\nERR REQUEST\n");}
 {Wire w;w.in=auth+"LIST\n";w.fail_read=1;w.run(root);assert(w.out.empty());}
 {Wire w;w.in=auth+"GET capture.csv\n";w.fail_write=4;w.run(root);assert(w.out.size()<=12);}
 {Wire w;w.in=auth+"LIST\n";w.cancel=true;w.run(root);assert(w.out.empty());}
 {Wire w;w.in=auth+"GET capture.csv\n";bool changed=false;
  w.on_write=[&](Wire& s){if(!changed&&s.out.find("\nabc")!=std::string::npos){changed=true;put(root/"capture.csv","abcdef");}};
  w.run(root);assert(changed);assert(w.out.find("OK DONE")==std::string::npos);
 }
 {
  char credential[65]{};
  for(const auto& suffix:{std::string{},std::string{"\n"},std::string{"\r\n"}}){
   put(root/"token.txt",token+suffix);
   assert(re6::load_log_token((root/"token.txt").string().c_str(),credential));
   assert(std::string(credential)==token);
  }
  put(root/"token.txt",token+"\r\nEXTRA");
  assert(!re6::load_log_token((root/"token.txt").string().c_str(),credential)&&!credential[0]);
  fs::remove(root/"token.txt");
 }
 put(root/"capture.csv","abc");
 {Wire w;w.in=auth+"GET capture.csv\n";bool changed=false;
  w.on_write=[&](Wire& s){if(!changed&&s.out.find("\nabc")!=std::string::npos){changed=true;put(root/"capture.csv","xyz");}};
  w.run(root);assert(changed);assert(w.out.find("OK DONE")==std::string::npos);
 }
 put(root/"capture.csv","abc");
 {std::error_code ec;fs::create_symlink(fs::absolute(root/"capture.csv"),root/"link.csv",ec);
  if(!ec){Wire w;w.in=auth+"GET link.csv\n";w.run(root);assert(w.out=="OK AUTH\nERR FILE\n");fs::remove(root/"link.csv");}}
 {std::ofstream f(root/"huge.csv",std::ios::binary);f.seekp(re6::kMaxTelemetryLogBytes);f.put('x');}
 {Wire w;w.in=auth+"GET huge.csv\n";w.run(root);assert(w.out=="OK AUTH\nERR FILE\n");}
 fs::remove(root/"huge.csv");
 for(std::size_t i=0;i<re6::kMaxTelemetryLogEntries;++i)put(root/("entry"+std::to_string(i)+".csv"),"");
 {Wire w;w.in=auth+"LIST\n";w.run(root);assert(w.out=="OK AUTH\nERR LIMIT\n");}
 fs::remove_all(root);
#ifdef _WIN32
 // Stop must interrupt an idle accepted client and the idle accept loop.
 assert(!re6::start_log_server("bad"));
 assert(re6::start_log_server(token.c_str()));
 assert(!re6::start_log_server(token.c_str()));
 const SOCKET sock=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
 assert(sock!=INVALID_SOCKET);
 sockaddr_in address{};address.sin_family=AF_INET;address.sin_port=htons(re6::kTelemetryLogPort);
 address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
 assert(!connect(sock,reinterpret_cast<sockaddr*>(&address),sizeof(address)));
 const DWORD timeout=1000;setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<const char*>(&timeout),sizeof(timeout));
 assert(send(sock,auth.data(),int(auth.size()),0)==int(auth.size()));
 std::string reply;char byte;
 while(reply.size()<8&&recv(sock,&byte,1,0)==1)reply+=byte;
 assert(reply=="OK AUTH\n");
 const auto start=std::chrono::steady_clock::now();
 re6::stop_log_server();closesocket(sock);
 assert(std::chrono::steady_clock::now()-start<std::chrono::seconds(2));
 assert(re6::start_log_server(token.c_str()));re6::stop_log_server();
 re6::stop_log_server();
#endif
 std::puts("telemetry transfer core tests passed");
}
