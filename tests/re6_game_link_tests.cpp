#include "../source/re6_controls/game_link.cpp"
#include <cassert>
#include <cstdio>
#include <vector>
#undef assert
#define assert(c) do{if(!(c)){std::fprintf(stderr,"RE6 IPC line %d: %s\n",__LINE__,#c);std::exit(1);}}while(false)
namespace {
constexpr u64 base=0x100000;
std::array<u8,0x50000> memory{};
u64 pid=42,title=re6::profile::title_id,mapping_count=1;
unsigned attach_result=0,writes=0,reads=0,fail_write=0;
bool correct_build=true;
std::vector<std::size_t> sizes;
re6::SharedControl snapshot(){re6::SharedControl c{};std::memcpy(&c,memory.data()+8,sizeof(c));return c;}
void reset(){
    memory.fill(0);pid=42;title=re6::profile::title_id;mapping_count=1;
    attach_result=writes=reads=fail_write=0;correct_build=true;sizes.clear();
    auto c=re6::initial_shared_control();c.status=1;c.capabilities=re6::kCapabilityFiltering;c.sequence=4;
    c.sampler_overrides=7;
    std::memcpy(memory.data()+8,&c,sizeof(c));
}
}
Result pminfoInitialize(){return 0;}void pminfoExit(){}
Result smGetService(Service* s,const char*){s->active=true;return 0;}
bool serviceIsActive(Service* s){return s->active;}void serviceClose(Service* s){s->active=false;}
Result pmdmntGetApplicationProcessId(u64* out){*out=pid;return 0;}
Result pminfoGetProgramId(u64* out,u64){*out=title;return 0;}
Result test_dispatch(Service*,unsigned cmd,const void* in,std::size_t,void* out,std::size_t,TestDispatchParams args){
    if(cmd==65003)return attach_result;
    if(cmd==65002){auto* m=static_cast<Re6Metadata*>(out);*m={};m->pid=pid;m->title=title;
        std::memcpy(m->build_id,re6::profile::build_id.data(),32);if(!correct_build)m->build_id[0]^=1;return 0;}
    if(cmd==65101){*static_cast<u64*>(out)=*static_cast<const u64*>(in)?0:mapping_count;
        auto* maps=static_cast<MemoryInfo*>(const_cast<void*>(args.buffers[0].data));maps[0]={base,memory.size(),MemType_CodeMutable,Perm_Rw};return 0;}
    assert(cmd==65102||cmd==65103);
    struct Range{u64 address,size;};const auto r=*static_cast<const Range*>(in);
    assert(r.address>=base&&r.address-base+r.size<=memory.size());auto* p=memory.data()+r.address-base;
    if(cmd==65102){++reads;std::memcpy(const_cast<void*>(args.buffers[0].data),p,r.size);}
    else{++writes;sizes.push_back(r.size);if(writes==fail_write)return 5;std::memcpy(p,args.buffers[0].data,r.size);}
    return 0;
}
int main(){
    reset();Re6GameLink link;link.initialize();assert(link.connect());
    assert(link.diagnostics.sampler_overrides==7);
    re6::Settings s;s.texture_filtering=re6::TextureFilter::Af8x;
    assert(link.apply(s));assert(snapshot().sequence==6);
    assert((sizes==std::vector<std::size_t>{4,re6::kSettingsBytes,4}));
    re6::Settings decoded;const auto c=snapshot();assert(re6::decode_settings(c.settings,sizeof(c.settings),decoded));
    assert(decoded.texture_filtering==re6::TextureFilter::Af8x);
    ++pid;assert(!link.apply(s)&&writes==3);link.close();
    for(unsigned failure=1;failure<=3;++failure){reset();link.initialize();assert(link.connect());fail_write=failure;
        assert(!link.apply(s)&&writes==failure);assert(failure==1?snapshot().sequence==4:(snapshot().sequence&1));link.close();}
    for(unsigned gate=0;gate<7;++gate){reset();
        if(gate==0)title=1;
        if(gate==1)correct_build=false;
        if(gate==2)attach_result=1;
        if(gate==3)mapping_count=33;
        auto header=snapshot();
        if(gate==4)header.abi=0;
        if(gate==5)header.status=0;
        if(gate==6)header.marker[0]='X';
        std::memcpy(memory.data()+8,&header,sizeof(header));link.initialize();assert(!link.connect()&&writes==0);link.close();}
    reset();link.initialize();assert(link.connect());auto header=snapshot();header.capabilities=0;
    std::memcpy(memory.data()+8,&header,sizeof(header));assert(!link.apply(s)&&writes==0);link.close();
    // The module also owns a 1024-slot journal preceding this control block.
    // Discovery must not assume the block is in the first128KiB of RW data.
    reset();header=snapshot();memory.fill(0);std::memcpy(memory.data()+0x43000,&header,sizeof(header));
    link.initialize();assert(link.connect());link.close();
    std::puts("RE6 controls IPC: build identity, bounds, diagnostics, transaction and restart guards PASS");
}
