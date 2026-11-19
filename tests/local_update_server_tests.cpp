#include "re4/local_update_server.hpp"
#include "re4/character_digest.hpp"
#include <cassert>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using Socket=SOCKET;
constexpr Socket invalid=INVALID_SOCKET;
void close_socket(Socket s){closesocket(s);}
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
using Socket=int;
constexpr Socket invalid=-1;
void close_socket(Socket s){close(s);}
#endif
using namespace re4;
using Clock=std::chrono::steady_clock;
namespace fs=std::filesystem;
namespace {
constexpr const char* token="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
constexpr const char* digest="3a0d2e4388d7b9ec4c23f7973be052444270f78c9d484c19e78b4b4a8ef0d08a";
void check(bool ok,const char* reason){if(!ok){std::cerr<<"FAIL: "<<reason<<'\n';std::exit(1);}}
std::string read_file(const fs::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
void write_file(const fs::path& p,const std::string& data){std::ofstream f(p,std::ios::binary);f.write(data.data(),std::streamsize(data.size()));}
void u32(std::string& b,unsigned off,unsigned n){for(unsigned i=0;i<4;++i)b[off+i]=char(n>>(8*i));}
std::string image(){
 std::string b(0x7038,0);std::memcpy(&b[0x10],"NRO0",4);u32(b,0x18,0x3000);
 for(unsigned i=0;i<3;++i){u32(b,0x20+i*8,i*0x1000);u32(b,0x24+i*8,0x1000);}
 b[0x100]=42;std::memcpy(&b[0x1000],"GAME_REMOTE_RELOAD_BRIDGE_V2",sizeof("GAME_REMOTE_RELOAD_BRIDGE_V2"));
 std::memcpy(&b[0x3000],"ASET",4);u32(b,0x3018,56);u32(b,0x3020,0x4000);
 constexpr unsigned n=0x3038;std::memcpy(&b[n],"RE4 Controls",13);std::memcpy(&b[n+0x200],"re4-controls",13);
 constexpr std::uint64_t own=0x0100E4C04E540000ull;for(unsigned i=0;i<8;++i)b[n+0x3038+i]=char(own>>(8*i));
 std::memcpy(&b[n+0x3060],"0.99.0",7);return b;
}
struct Client {
 Socket fd=invalid;
 Client(){fd=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);check(fd!=invalid,"client socket");
  sockaddr_in address{};address.sin_family=AF_INET;address.sin_port=htons(28285);address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
#ifdef _WIN32
  DWORD timeout=2000;setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<const char*>(&timeout),sizeof(timeout));
#else
  timeval timeout{2,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
#endif
  check(connect(fd,reinterpret_cast<sockaddr*>(&address),sizeof(address))==0,"connect manually started listener");
 }
 ~Client(){if(fd!=invalid)close_socket(fd);}
 void send(const std::string& data){std::size_t at=0;while(at<data.size()){
#ifdef MSG_NOSIGNAL
  constexpr int flags=MSG_NOSIGNAL;
#else
  constexpr int flags=0;
#endif
  const int count=::send(fd,data.data()+at,int(data.size()-at),flags);check(count>0,"send fixture");at+=unsigned(count);}}
 std::string line(){std::string out;char c=0;while(out.size()<1024){const int n=recv(fd,&c,1,0);if(n<=0)break;if(c=='\n')return out;out+=c;}return out;}
 std::string bytes(std::size_t n){std::string out(n,0);std::size_t at=0;while(at<n){const int count=recv(fd,out.data()+at,int(n-at),0);check(count>0,"receive full report");at+=unsigned(count);}return out;}
 void auth(){send(std::string("AUTH ")+token+"\n");check(line()=="OK AUTH","protocol-compatible authentication");}
};
bool wait_state(LocalUpdateServer& server,LocalUpdateState want){
 const auto end=Clock::now()+std::chrono::seconds(3);
 do {server.poll();if(server.snapshot().state==want)return true;std::this_thread::sleep_for(std::chrono::milliseconds(5));}while(Clock::now()<end);
 return false;
}
void stop(LocalUpdateServer& server){const auto start=Clock::now();server.request_stop();
 while(!server.stopped()&&Clock::now()-start<std::chrono::seconds(2)){server.poll();std::this_thread::sleep_for(std::chrono::milliseconds(5));}
 check(server.stopped()&&server.snapshot().state==LocalUpdateState::Off,"cancelled worker must stop and permit safe socket teardown");
 check(Clock::now()-start<std::chrono::seconds(1),"network cancellation must not wait for transfer timeout");
}
std::size_t report(char* out,std::size_t cap,void* context){const auto& text=*static_cast<const std::string*>(context);if(text.size()>cap)return cap+1;std::memcpy(out,text.data(),text.size());return text.size();}
void put(Client& c,const std::string& bytes,const char* target,const char* sha=digest){
 c.auth();c.send(std::string("PUT ")+target+" "+std::to_string(bytes.size())+" "+sha+"\n");check(c.line()=="OK SEND","PUT accepted");c.send(bytes);
}
}
int main(){
#ifdef _WIN32
 WSADATA data{};check(!WSAStartup(MAKEWORD(2,2),&data),"host WSA initialization");
#endif
 const auto root=fs::temp_directory_path()/("re4-local-update-"+std::to_string(Clock::now().time_since_epoch().count()));fs::create_directories(root);
 const auto token_path=root/"token.txt";write_file(token_path,std::string(token)+"\n");const auto valid=image();
  CharacterDigest fixture_hash;unsigned char fixture_sha[32]{};fixture_hash.update(valid.data(),valid.size());fixture_hash.finish(fixture_sha);
 char fixture_hex[65]{};for(unsigned i=0;i<32;++i)std::snprintf(fixture_hex+i*2,3,"%02x",fixture_sha[i]);
 if(std::strcmp(fixture_hex,digest))std::cerr<<"Fixture actual digest "<<fixture_hex<<" expected "<<digest<<'\n';
 check(!std::strcmp(fixture_hex,digest),"independently generated fixture SHA must match serialized NRO");
 LocalUpdateServer server;check(server.stopped()&&server.snapshot().state==LocalUpdateState::Off,"constructor must remain offline");
 {const auto probe=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);check(probe!=invalid,"offline listener probe");
  sockaddr_in address{};address.sin_family=AF_INET;address.sin_port=htons(28285);address.sin_addr.s_addr=htonl(INADDR_ANY);
  check(!bind(probe,reinterpret_cast<sockaddr*>(&address),sizeof(address)),"constructor must not reserve a listener port");close_socket(probe);}
 std::string journal(200000,'j');journal.replace(0,10,"own report");
 const auto start=[&]{return server.start(root.string().c_str(),"RE4 Controls","0.99.0",token_path.string().c_str(),report,&journal);};
 check(start(),"manual Start must create a transfer service");check(wait_state(server,LocalUpdateState::Listening),"manual service must become listening");
 check(!start(),"second Start must not replace live worker state");
 {Client c;c.auth();c.send("STATUS\n");const auto status=c.line();check(status.find("\"protocol\":2")!=std::string::npos&&status.find("0.99.0")!=std::string::npos,"cached STATUS reports active app version");}
 {Client c;c.send("AUTH bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\n");check(c.line()=="ERR auth","wrong authentication rejected");}
 {Client c;c.auth();c.send("RUN GAME\n");check(c.line()=="ERR manual_restart_required","RUN must never launch or request resident lifetime");}
 {Client c;c.auth();c.send("CRASH\n");check(c.line()=="OK REPORT 200000","reports exceed old 64KiB cap without truncation");check(c.bytes(journal.size())==journal,"CRASH returns only supplied own-app report");}
 // The real transport must fit all own and system reports, not only a direct callback.
 journal=std::string(700*1024,'s');journal.replace(journal.size()-18,18,"SYSTEM_REPORT_TAIL");
 {Client c;c.auth();c.send("CRASH\n");check(c.line()=="OK REPORT "+std::to_string(journal.size()),"actual CRASH must accept combined own and system reports beyond384KiB");check(c.bytes(journal.size())==journal,"system report tail survives real transport");}
 journal=std::string("private ")+token+" end";
 {Client c;c.auth();c.send("CRASH\n");check(c.line()=="OK REPORT 22","credential redaction updates report byte length");check(c.bytes(22)=="private [REDACTED] end","CRASH must not disclose the service credential");}
 journal.assign(kLocalUpdateReportCapacity+1,'x');
 {Client c;c.auth();c.send("CRASH\n");check(c.line()=="ERR report","oversized callback result must not read outside its buffer");}
 journal="own app report";
 write_file(root/"game.nro","prior game image");
 {Client c;put(c,valid,"GAME");check(c.line()=="OK STORED","verified GAME stored");}
 check(read_file(root/"game.nro")==valid&&read_file(root/"game.nro.previous")=="prior game image","successful PUT preserves old image as previous");
 {Client c;put(c,valid,"BRIDGE");check(c.line()=="OK STORED","second connection can store BRIDGE without restarting server");}
 check(read_file(root/"bridge.nro")==valid,"only allowlisted bridge path written");
 check(server.snapshot().state==LocalUpdateState::Stored&&!server.stopped(),"Stored must keep listener active for the other update slot");
 fs::remove(root/"game.nro.previous");fs::create_directory(root/"game.nro.previous");
 {Client c;put(c,valid,"GAME");check(c.line()=="ERR transfer_or_verification","backup path conflict rejects publication");}
 check(read_file(root/"game.nro")==valid&&fs::is_directory(root/"game.nro.previous"),"publication conflict preserves current image and conflicting path");
 fs::remove(root/"game.nro.previous");
 auto foreign=valid;foreign[0x3038]='X';
 {Client c;put(c,foreign,"GAME","c71fa4c9024c5725ec3f009bd882f9d32742a18e9b68a18d6c6baa90c6c65adb");check(c.line()=="ERR transfer_or_verification","SHA-valid foreign NACP must be rejected");}
 {Client c;put(c,valid,"GAME","0000000000000000000000000000000000000000000000000000000000000000");check(c.line()=="ERR transfer_or_verification","wrong SHA rejected");}
 check(read_file(root/"game.nro")==valid&&!fs::exists(root/"game.nro.incoming"),"wrong hash must preserve current image and remove incomplete file");
 {Client c;c.auth();c.send(std::string("PUT GAME ")+std::to_string(valid.size())+" "+digest+"\n");check(c.line()=="OK SEND","truncated PUT begins");c.send(valid.substr(0,128));}
 {Client c;c.auth();c.send("STATUS\n");check(c.line().find("\"protocol\":2")!=std::string::npos,"server accepts next connection after truncated transfer");}
 check(read_file(root/"game.nro")==valid&&!fs::exists(root/"game.nro.incoming"),"truncated PUT removes only its temporary file");
 {Client c;c.auth();c.send(std::string("PUT LAB ")+std::to_string(valid.size())+" "+digest+"\n");check(c.line()=="ERR target","local updater rejects LAB paths");}
 {Client c;c.auth();c.send("PUT GAME 67108865 0000000000000000000000000000000000000000000000000000000000000000\n");check(c.line()=="ERR command","oversized PUT rejected before opening file");}
 {Client c;c.send("AUTH ");stop(server);}
 check(start()&&wait_state(server,LocalUpdateState::Listening),"stopped service can manually restart on same port");
 {Client c;c.auth();c.send(std::string("PUT GAME ")+std::to_string(valid.size())+" "+digest+"\n");check(c.line()=="OK SEND","cancellable PUT begins");c.send(valid.substr(0,128));stop(server);}
 check(read_file(root/"game.nro")==valid&&!fs::exists(root/"game.nro.incoming"),"stop during PUT cannot publish partial image");
 check(read_file(token_path)==std::string(token)+"\n","local update never alters credential file");
 stop(server);write_file(token_path,"short");check(!start()&&server.stopped()&&server.snapshot().state==LocalUpdateState::Failed,"invalid credential must fail before opening listener");
 stop(server);check(server.snapshot().state==LocalUpdateState::Off,"Back from stopped startup failure must return Off");
 fs::remove_all(root);
#ifdef _WIN32
 WSACleanup();
#endif
 std::cout<<"PASS: manual local update lifecycle, authenticated protocol, report, verified storage, cancellation, no RUN\n";
}
