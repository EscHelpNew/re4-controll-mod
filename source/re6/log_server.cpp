#include "re6/log_server.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#ifdef __SWITCH__
#include <switch.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#elif defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <wincrypt.h>
#include <thread>
#endif

namespace {
// Target uses libnx's incremental SHA256. Host builds use the operating system
// implementation, so the protocol engine can be exercised without Switch I/O.
struct Hash {
#ifdef __SWITCH__
 Sha256Context ctx;
 Hash(){sha256ContextCreate(&ctx);}
 bool update(const void* p,std::size_t n){sha256ContextUpdate(&ctx,p,n);return true;}
 bool finish(unsigned char* out){sha256ContextGetHash(&ctx,out);return true;}
#elif defined(_WIN32)
 HCRYPTPROV provider=0;HCRYPTHASH hash=0;
 Hash(){if(CryptAcquireContext(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT))CryptCreateHash(provider,CALG_SHA_256,0,0,&hash);}
 ~Hash(){if(hash)CryptDestroyHash(hash);if(provider)CryptReleaseContext(provider,0);}
 bool update(const void* p,std::size_t n){return hash&&CryptHashData(hash,static_cast<const BYTE*>(p),DWORD(n),0);}
 bool finish(unsigned char* out){DWORD n=32;return hash&&CryptGetHashParam(hash,HP_HASHVAL,out,&n,0)&&n==32;}
#else
#error Host SHA256 adapter currently supports Windows; target uses libnx.
#endif
};
bool token_valid(const char* token){
 if(!token||std::strlen(token)!=64)return false;
 for(unsigned i=0;i<64;++i)if(!((token[i]>='0'&&token[i]<='9')||(token[i]>='a'&&token[i]<='f')||(token[i]>='A'&&token[i]<='F')))return false;
 return true;
}
bool valid_name(const std::string& s){
 if(s.size()<5||s.size()>100||s.compare(s.size()-4,4,".csv")||s.find("..")!=std::string::npos)return false;
 for(char c:s)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'))return false;
 return s[0]!='.';
}
bool plain_stat(const char* path,struct stat& st){
#ifdef _WIN32
 const DWORD flags=GetFileAttributesA(path);
 if(flags==INVALID_FILE_ATTRIBUTES||(flags&FILE_ATTRIBUTE_REPARSE_POINT))return false;
 return !stat(path,&st);
#else
 return !lstat(path,&st)&&!S_ISLNK(st.st_mode);
#endif
}
bool regular(const struct stat& st){return S_ISREG(st.st_mode)&&st.st_size>=0&&std::uint64_t(st.st_size)<=re6::kMaxTelemetryLogBytes;}
bool unchanged(const struct stat& a,const struct stat& b){
 // MinGW's CRT fstat reports st_dev=0 while stat reports a drive number.
#ifndef _WIN32
 if(a.st_dev!=b.st_dev||a.st_ino!=b.st_ino)return false;
 if(a.st_mtim.tv_nsec!=b.st_mtim.tv_nsec||a.st_ctim.tv_nsec!=b.st_ctim.tv_nsec)return false;
#endif
 return a.st_size==b.st_size&&a.st_mtime==b.st_mtime&&a.st_ctime==b.st_ctime;
}
struct File {
 FILE* f=nullptr;struct stat initial{};
 ~File(){if(f)std::fclose(f);}
 bool open(const std::string& path){
  struct stat before{};
  if(!plain_stat(path.c_str(),before)||!regular(before))return false;
  int flags=O_RDONLY;
#ifdef O_BINARY
  flags|=O_BINARY;
#endif
#ifdef O_NOFOLLOW
  flags|=O_NOFOLLOW;
#endif
  const int fd=::open(path.c_str(),flags);
  if(fd<0)return false;
  if(fstat(fd,&initial)||!regular(initial)||!unchanged(before,initial)){::close(fd);return false;}
  f=fdopen(fd,"rb");if(!f)::close(fd);
  return f!=nullptr;
 }
 bool stable(const std::string& path){struct stat now{},entry{};return !fstat(fileno(f),&now)&&plain_stat(path.c_str(),entry)&&unchanged(initial,now)&&unchanged(initial,entry);}
};
struct Session {
 const re6::detail::LogIo& io;
 bool cancelled(){return io.cancelled&&io.cancelled(io.context);}
 bool send(const void* p,std::size_t n){
  auto b=static_cast<const char*>(p);
  while(n&&!cancelled()){
   const int count=io.write(io.context,b,n);
   if(count<=0||std::size_t(count)>n)return false;
   b+=count;n-=count;
  }
  return !n&&!cancelled();
 }
 bool send(const std::string& s){return send(s.data(),s.size());}
 bool line(std::string& s){
  s.clear();char c;
  while(!cancelled()&&s.size()<256){
   if(io.read(io.context,&c,1)!=1)return false;
   if(c=='\n')return true;
   if(c<' '||c>'~')return false;
   s+=c;
  }
  return false;
 }
 void list(const std::string& root){
  DIR* dir=opendir(root.c_str());
  if(!dir){send("ERR UNAVAILABLE\n");return;}
  struct Entry{std::string name;std::uint64_t size;};
  std::vector<Entry> entries;
  bool limit=false;std::size_t scanned=0;
  while(auto ent=readdir(dir)){
   if(cancelled()){closedir(dir);return;}
   // Bound work even when the directory contains only irrelevant entries.
   if(++scanned>8192){limit=true;break;}
   const std::string name=ent->d_name;struct stat st{};
   if(!valid_name(name)||!plain_stat((root+"/"+name).c_str(),st)||!regular(st))continue;
   if(entries.size()==re6::kMaxTelemetryLogEntries){limit=true;break;}
   entries.push_back({name,std::uint64_t(st.st_size)});
  }
  closedir(dir);
  if(limit){send("ERR LIMIT\n");return;}
  std::sort(entries.begin(),entries.end(),[](const Entry& a,const Entry& b){return a.name<b.name;});
  if(!send("OK LIST "+std::to_string(entries.size())+"\n"))return;
  for(const auto& e:entries)if(!send("FILE "+e.name+" "+std::to_string(e.size)+"\n"))return;
  send("END\n");
 }
 void get(const std::string& root,const std::string& name){
  File file;const auto path=root+"/"+name;
  if(!valid_name(name)||!file.open(path)){send("ERR FILE\n");return;}
  unsigned char buffer[32768],digest[32],sent_digest[32];Hash hash;
  std::uint64_t remaining=std::uint64_t(file.initial.st_size);
  while(remaining&&!cancelled()){
   const auto n=std::fread(buffer,1,std::size_t(std::min<std::uint64_t>(remaining,sizeof(buffer))),file.f);
   if(!n||!hash.update(buffer,n)){send("ERR FILE\n");return;}
   remaining-=n;
  }
  if(cancelled())return;
  if(!hash.finish(digest)||!file.stable(path)||std::fseek(file.f,0,SEEK_SET)){send("ERR CHANGED\n");return;}
  constexpr char hex[]="0123456789abcdef";char text[65]{};
  for(unsigned i=0;i<32;++i){text[2*i]=hex[digest[i]>>4];text[2*i+1]=hex[digest[i]&15];}
  if(!send("OK FILE "+std::to_string(file.initial.st_size)+" "+text+"\n"))return;
  Hash transmitted;remaining=std::uint64_t(file.initial.st_size);
  while(remaining&&!cancelled()){
   const auto n=std::fread(buffer,1,std::size_t(std::min<std::uint64_t>(remaining,sizeof(buffer))),file.f);
   if(!n||!transmitted.update(buffer,n)||!send(buffer,n))return;
   remaining-=n;
  }
  if(cancelled())return;
  // No success trailer unless the sent bytes and directory entry still match
  // the completed file hashed before the transfer.
  if(!transmitted.finish(sent_digest)||std::memcmp(digest,sent_digest,32)||!file.stable(path))return;
  // FAT timestamps can have coarse resolution. Re-read after transmission to
  // catch a same-size edit to an already-sent region within one timestamp tick.
  if(std::fseek(file.f,0,SEEK_SET))return;
  Hash current;remaining=std::uint64_t(file.initial.st_size);
  while(remaining&&!cancelled()){
   const auto n=std::fread(buffer,1,std::size_t(std::min<std::uint64_t>(remaining,sizeof(buffer))),file.f);
   if(!n||!current.update(buffer,n))return;
   remaining-=n;
  }
  if(cancelled()||!current.finish(sent_digest)||std::memcmp(digest,sent_digest,32)||!file.stable(path))return;
  send("\nOK DONE\n");
 }
};

#ifdef _WIN32
using Socket=SOCKET;
constexpr Socket invalid_socket=INVALID_SOCKET;
void close_socket(Socket fd){closesocket(fd);}
bool would_block(){const auto e=WSAGetLastError();return e==WSAEWOULDBLOCK||e==WSAEINTR;}
bool nonblocking(Socket fd){u_long mode=1;return !ioctlsocket(fd,FIONBIO,&mode);}
#else
using Socket=int;
constexpr Socket invalid_socket=-1;
void close_socket(Socket fd){::close(fd);}
bool would_block(){return errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR;}
bool nonblocking(Socket fd){const int flags=fcntl(fd,F_GETFL,0);return flags>=0&&!fcntl(fd,F_SETFL,flags|O_NONBLOCK);}
#endif
using Clock=std::chrono::steady_clock;
struct Server {
 std::atomic<bool> stopping{false};
 Socket listener=invalid_socket;
 bool created=false;char token[65]{};
#ifdef __SWITCH__
 Thread thread{};
#else
 std::thread thread;
#endif
 struct Connection {
  Server& server;Socket socket;
  Clock::time_point deadline=Clock::now()+std::chrono::seconds(120);
  static bool cancelled(void* p){const auto& c=*static_cast<Connection*>(p);return c.server.stopping||Clock::now()>=c.deadline;}
  int transfer(void* bytes,std::size_t count,bool writing){
   const auto idle=Clock::now()+std::chrono::seconds(8);
   while(!server.stopping&&Clock::now()<deadline&&Clock::now()<idle){
    fd_set set;FD_ZERO(&set);FD_SET(socket,&set);timeval wait{0,100000};
    const int ready=select(int(socket+1),writing?nullptr:&set,writing?&set:nullptr,nullptr,&wait);
    if(ready<0){if(would_block())continue;return -1;}
    if(!ready)continue;
    const int n=writing?::send(socket,static_cast<const char*>(bytes),int(count),0)
                       :recv(socket,static_cast<char*>(bytes),int(count),0);
    if(n<0&&would_block())continue;
    return n;
   }
   return -1;
  }
  static int read(void* p,void* bytes,std::size_t count){return static_cast<Connection*>(p)->transfer(bytes,count,false);}
  static int write(void* p,const void* bytes,std::size_t count){return static_cast<Connection*>(p)->transfer(const_cast<void*>(bytes),count,true);}
 };
 void run(){
  while(!stopping){
   fd_set set;FD_ZERO(&set);FD_SET(listener,&set);timeval wait{0,100000};
   const int ready=select(int(listener+1),&set,nullptr,nullptr,&wait);
   if(ready<0){if(would_block())continue;break;}
   if(!ready)continue;
   const Socket client=accept(listener,nullptr,nullptr);
   if(client==invalid_socket){if(would_block())continue;break;}
   if(nonblocking(client)){
    Connection conn{*this,client};
    re6::detail::serve_log_session(re6::kTelemetryLogRoot,token,{&conn,Connection::read,Connection::write,Connection::cancelled});
   }
   close_socket(client);
  }
 }
 static void entry(void* p){static_cast<Server*>(p)->run();}
} server;
}
namespace re6::detail {
void serve_log_session(const char* root,const char* token,const LogIo& io){
 if(!root||!token_valid(token)||!io.read||!io.write)return;
 Session s{io};std::string line;
 if(!s.line(line))return;
 unsigned diff=line.size()==69?0:1;
 if(line.size()==69){diff|=unsigned(line.compare(0,5,"AUTH "));for(unsigned i=0;i<64;++i)diff|=static_cast<unsigned char>(line[i+5])^static_cast<unsigned char>(token[i]);}
 if(diff){s.send("ERR AUTH\n");return;}
 if(!s.send("OK AUTH\n"))return;
 if(!s.line(line)){s.send("ERR REQUEST\n");return;}
 struct stat directory{};
 if(!plain_stat(root,directory)||!S_ISDIR(directory.st_mode)){s.send("ERR UNAVAILABLE\n");return;}
 if(line=="LIST")s.list(root);
 else if(line.compare(0,4,"GET ")==0)s.get(root,line.substr(4));
 else s.send("ERR REQUEST\n");
}
}

namespace re6 {
bool load_log_token(const char* path,char (&token)[65]){
 std::memset(token,0,sizeof(token));if(!path)return false;
 FILE* file=std::fopen(path,"rb");if(!file)return false;
 char bytes[68]{};auto count=std::fread(bytes,1,67,file);
 bool ok=!std::ferror(file)&&std::fgetc(file)==EOF&&count<=66;
 if(std::fclose(file))ok=false;
 while(count&&(bytes[count-1]=='\n'||bytes[count-1]=='\r'))--count;
 bytes[count]=0;ok=ok&&count==64&&token_valid(bytes);
 if(ok)std::memcpy(token,bytes,65);
 std::memset(bytes,0,sizeof(bytes));return ok;
}
bool start_log_server(const char* token){
 if(server.created||!token_valid(token))return false;
#ifdef _WIN32
 WSADATA data{};if(WSAStartup(MAKEWORD(2,2),&data))return false;
#endif
 server.listener=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
 bool ok=server.listener!=invalid_socket;
 if(ok){
  int reuse=1;
  setsockopt(server.listener,SOL_SOCKET,SO_REUSEADDR,reinterpret_cast<const char*>(&reuse),sizeof(reuse));
  sockaddr_in address{};address.sin_family=AF_INET;address.sin_port=htons(kTelemetryLogPort);address.sin_addr.s_addr=htonl(INADDR_ANY);
  ok=nonblocking(server.listener)&&!bind(server.listener,reinterpret_cast<sockaddr*>(&address),sizeof(address))&&!listen(server.listener,2);
 }
 if(ok){
  std::memcpy(server.token,token,65);server.stopping=false;
#ifdef __SWITCH__
  Result rc=threadCreate(&server.thread,Server::entry,&server,nullptr,64*1024,0x2D,-2);
  if(R_SUCCEEDED(rc)){
   rc=threadStart(&server.thread);
   if(R_FAILED(rc))threadClose(&server.thread);
  }
  ok=R_SUCCEEDED(rc);
#else
  server.thread=std::thread(Server::entry,&server);
#endif
 }
 if(!ok){
  if(server.listener!=invalid_socket)close_socket(server.listener);
  server.listener=invalid_socket;std::memset(server.token,0,sizeof(server.token));
#ifdef _WIN32
  WSACleanup();
#endif
  return false;
 }
 server.created=true;return true;
}
void stop_log_server(){
 if(!server.created)return;
 server.stopping=true;
#ifdef __SWITCH__
 threadWaitForExit(&server.thread);threadClose(&server.thread);
#else
 server.thread.join();
#endif
 close_socket(server.listener);server.listener=invalid_socket;
 server.created=false;std::memset(server.token,0,sizeof(server.token));
#ifdef _WIN32
 WSACleanup();
#endif
}
}
