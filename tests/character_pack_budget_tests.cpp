#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>
#undef assert
#define assert(c) do {if(!(c)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#c);std::exit(1);}} while(0)

using u64=std::uint64_t;
using Result=unsigned;
constexpr unsigned CUR_THREAD_HANDLE=0xffff8000;
std::uint64_t test_thread=7;
Result svcGetThreadId(u64* out,unsigned handle){assert(handle==CUR_THREAD_HANDLE);*out=test_thread;return 0;}
#define R_FAILED(v) ((v)!=0)
std::uintptr_t main_base=0;
std::size_t allocatable=256u*1024*1024;
unsigned headroom_calls=0;
std::vector<std::uint8_t> image(0x1190000);
template<class T> T character_value(const void* p,std::size_t off){T v;std::memcpy(&v,static_cast<const char*>(p)+off,sizeof(v));return v;}
template<class T> void character_store(void* p,std::size_t off,T v){std::memcpy(static_cast<char*>(p)+off,&v,sizeof(v));}
template<class T> bool character_read(std::uintptr_t p,T& out){
    if(p<main_base||p-main_base>image.size()||sizeof(T)>image.size()-(p-main_base))return false;
    std::memcpy(&out,reinterpret_cast<void*>(p),sizeof(T));return true;
}
int test_CharacterPackOpen(const char*,void*);
void* test_CharacterPackAllocate(unsigned,unsigned);
template<auto F> struct Hook {
    template<class... A> static decltype(auto) Orig(A... a){return F(a...);}
    static void InstallAtOffset(std::size_t){}
};
#define HOOK_DEFINE_TRAMPOLINE(Name) struct Name : Hook<&test_##Name>
#include "../source/game/character_pack_budget.inc"
extern "C" std::size_t character_pack_budget_headroom(const void* allocator){
    assert(allocator==reinterpret_cast<void*>(0x5678)&&test_thread==7);++headroom_calls;return allocatable;
}

unsigned opens=0,closes=0,reserves=0,releases=0,loads=0,ordinary_allocations=0;
bool fail_reserve=false,missing=false,yz2=false,consume=true,handoff=true,other_thread=false;
bool aligned_reserve=false,fill_io_on_reserve=false,reap_finishes=true;
bool nested=false;
alignas(128) std::array<std::uint8_t,8192> fixed_reservation{};
std::vector<void*> allocations;
void* transferred=nullptr;
const char* prefix(){return "bio4";}
void close_file(void* info){assert(character_value<std::uintptr_t>(info,0x20)==0xabc);++closes;}
void* reserve(unsigned n,const char*,unsigned){
    ++reserves;if(fail_reserve)return nullptr;
    assert(n==4224);
    if(fill_io_on_reserve){character_store(image.data(),0x1184300+0x50,1u);character_store(image.data(),0x11852A0,std::uintptr_t(1));}
    if(aligned_reserve)return fixed_reservation.data();
    void* p=std::malloc(n);assert(p);allocations.push_back(p);return p;
}
void release(void* p){++releases;assert(p);if(p!=fixed_reservation.data())std::free(p);}
int reap(){
    if(reap_finishes)for(unsigned i=0;i<20;++i)character_store(image.data(),0x11852A0+i*8,std::uintptr_t(0));
    return 0;
}
int test_CharacterPackOpen(const char* path,void* info){
    ++opens;const std::string s=path;
    assert(s=="bio4\\ImagePackHD\\01000001.pack"||s=="bio4\\ImagePackHD\\01000001.pack.yz2"
        ||s=="bio4\\ImagePack\\01000001.pack"||s=="bio4\\ImagePack\\01000001.pack.yz2");
    if(s.ends_with("yz2")?!yz2:missing)return 0;
    character_store(info,0x20,std::uintptr_t(0xabc));character_store(info,0x1c,4096u);character_store(info,0x50,4096u);return 1;
}
void* test_CharacterPackAllocate(unsigned,unsigned){++ordinary_allocations;return reinterpret_cast<void*>(0xdef);}
void native_load(void* context,unsigned file,unsigned options){
    assert(context==reinterpret_cast<void*>(0x123)&&file==0x22&&options==3);++loads;
    if(nested){bool retry=false;assert(!character_pack_budget_load(context,file,0x01000001,native_load,options,&retry)&&retry);}
    alignas(8) std::array<std::uint8_t,0x60> info;info.fill(0x5a);
    const bool hd=character_value<unsigned>(image.data(),0xAF65B0)!=0;
    assert(!CharacterPackOpen::Callback(hd?"bio4\\ImagePackHD\\01000001.pack.yz2":"bio4\\ImagePack\\01000001.pack.yz2",info.data()));
    if(handoff){
        assert(CharacterPackOpen::Callback(hd?"bio4\\ImagePackHD\\01000001.pack":"bio4\\ImagePack\\01000001.pack",info.data()));
        for(unsigned i=0;i<info.size();++i)if(!((i>=0x1c&&i<0x28)||(i>=0x50&&i<0x54)))assert(info[i]==0x5a);
        assert(character_value<std::uintptr_t>(info.data(),0x20)==0xabc);
    }
    if(other_thread){test_thread=8;assert(CharacterPackAllocate::Callback(4096,128)==reinterpret_cast<void*>(0xdef));test_thread=7;}
    if(consume){transferred=CharacterPackAllocate::Callback(4096,128);assert(transferred);assert((reinterpret_cast<std::uintptr_t>(transferred)&127)==0);}
}
void reset(){
    std::fill(image.begin(),image.end(),0);main_base=reinterpret_cast<std::uintptr_t>(image.data());
    character_store(image.data(),0xAF65B0,1u);character_store(image.data(),0x11807D4,1u);
    character_store(image.data(),0x11807D8+0x22*4,0x01000001u);
    character_store(image.data(),0x1180D40+0x158,std::uintptr_t(1));
    character_store(image.data(),0xB27538,std::uintptr_t(0x5678));
    for(unsigned i=0;i<64;++i)character_store(image.data(),0x11829B0+i*16+8,0xffffffffu);
    opens=closes=reserves=releases=loads=ordinary_allocations=0;
    fail_reserve=missing=yz2=other_thread=false;consume=handoff=true;transferred=nullptr;test_thread=7;
    aligned_reserve=fill_io_on_reserve=false;reap_finishes=true;
    nested=false;
    allocatable=256u*1024*1024;headroom_calls=0;
    character_pack_budget_native={close_file,prefix,reserve,release,reap,character_pack_budget_headroom};
}
bool load(){return character_pack_budget_load(reinterpret_cast<void*>(0x123),0x22,0x01000001,native_load,3);}
void native_free(){assert(transferred);const auto offset=*(static_cast<std::uint8_t*>(transferred)-1);release(static_cast<std::uint8_t*>(transferred)-offset);transferred=nullptr;}
int main(){
    reset();assert(load());assert(loads==1&&opens==2&&closes==0&&reserves==1&&releases==0);native_free();assert(releases==1);
    reset();fail_reserve=true;assert(!load());assert(loads==0&&closes==1&&reserves==1&&releases==0);
    reset();allocatable=128u*1024*1024-1;assert(!load());assert(!loads&&closes==1&&releases==1);
    assert(headroom_calls==1);
    reset();allocatable=128u*1024*1024;assert(load());assert(headroom_calls==1);native_free();
    reset();character_pack_budget_native.headroom=nullptr;assert(!load());assert(!loads&&closes==1&&releases==1&&!headroom_calls);
    reset();character_store(image.data(),0xB27538,std::uintptr_t(0));assert(!load());assert(!loads&&closes==1&&releases==1&&!headroom_calls);
    reset();missing=true;assert(!load());assert(loads==0&&closes==0&&reserves==0);
    reset();yz2=true;assert(!load());assert(loads==0&&opens==1&&closes==1&&reserves==0);
    reset();other_thread=true;assert(load());assert(ordinary_allocations==1&&reserves==1&&releases==0&&headroom_calls==1);native_free();
    reset();character_store(image.data(),0x1184300+0x50,1u);character_store(image.data(),0x11852A0,std::uintptr_t(1));
    assert(!load());assert(!loads&&!opens&&!reserves);
    reset();consume=false;handoff=false;assert(!load());assert(loads==1&&closes==1&&releases==1);
    reset();consume=false;assert(!load());assert(loads==1&&closes==0&&releases==1);
    reset();for(unsigned i=0;i<20;++i)character_store(image.data(),0x11852A0+i*8,std::uintptr_t(1));
    assert(load());native_free(); // Finished native handles are reaped first.
    reset();bool retry=false;character_store(image.data(),0x1184300+0x50,1u);
    assert(!character_pack_budget_load(reinterpret_cast<void*>(0x123),0x22,0x01000001,native_load,3,&retry)&&retry);
    reset();reap_finishes=false;character_store(image.data(),0x11852A0,std::uintptr_t(1));retry=false;
    assert(!character_pack_budget_load(reinterpret_cast<void*>(0x123),0x22,0x01000001,native_load,3,&retry)&&retry);
    reset();fill_io_on_reserve=true;retry=false;
    assert(!character_pack_budget_load(reinterpret_cast<void*>(0x123),0x22,0x01000001,native_load,3,&retry)&&retry);
    assert(!loads&&closes==1&&releases==1);
    reset();aligned_reserve=true;assert(load());assert(*(static_cast<std::uint8_t*>(transferred)-1)==128);native_free();assert(releases==1);
    reset();character_store(image.data(),0xAF65B0,0u);assert(load());native_free();
    reset();character_store(image.data(),0xAF65B0,2u);assert(load());native_free();
    reset();nested=true;assert(load());assert(loads==1&&reserves==1);native_free();
    reset();character_store(image.data(),0x1180D40+0x158,std::uintptr_t(0));assert(!load());assert(!loads&&!opens);
    reset();character_store(image.data(),0x11807D8+0x22*4,0x01000002u);assert(!load());assert(!loads&&!opens);
    reset();character_store(image.data(),0x11829B0+8,0x01000001u);character_store(image.data(),0x11829B0,std::uintptr_t(1));
    character_pack_budget_native.headroom=nullptr;allocatable=0;
    assert(load());assert(!loads&&!opens&&!reserves&&!headroom_calls);
    std::puts("character pack budget: admission, held-file handoff, thread isolation and ownership PASS");
}
