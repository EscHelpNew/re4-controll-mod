#pragma once
#include "../home_stubs/switch.h"
#include <thread>
struct Thread {std::thread thread;void(*entry)(void*)=nullptr;void* arg=nullptr;};
inline Result threadCreate(Thread* t,void(*entry)(void*),void* arg,void*,std::size_t,int,int){t->entry=entry;t->arg=arg;return 0;}
inline Result threadStart(Thread* t){t->thread=std::thread(t->entry,t->arg);return 0;}
inline Result threadWaitForExit(Thread* t){if(t->thread.joinable())t->thread.join();return 0;}
inline Result threadClose(Thread*){return 0;}
struct Sha256Context {std::vector<u8> bytes;};
inline void sha256ContextCreate(Sha256Context* h){h->bytes.clear();}
inline void sha256ContextUpdate(Sha256Context* h,const void* data,std::size_t count){const auto* bytes=static_cast<const u8*>(data);h->bytes.insert(h->bytes.end(),bytes,bytes+count);}
inline void sha256ContextGetHash(Sha256Context* h,void* out){sha256CalculateHash(out,h->bytes.data(),h->bytes.size());}
