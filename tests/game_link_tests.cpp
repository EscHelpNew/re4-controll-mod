#include "../source/overlay/game_link.cpp"
#include <cassert>
#include <cmath>
#include <vector>
namespace {
constexpr u64 address=0x100000;
std::array<u8,0x1000> memory{};
u64 pid=42,title=re4::profile::title_id,mapping_count=1;
unsigned attach_rc=0,write_count=0,fail_write=0,reads=0;
bool build_supported=true;
std::vector<std::size_t> write_sizes;
re4::SharedControl snapshot(){
    re4::SharedControl block{};std::memcpy(&block,memory.data()+8,sizeof(block));return block;
}
void reset(){
    memory.fill(0);pid=42;title=re4::profile::title_id;mapping_count=1;
    attach_rc=write_count=fail_write=reads=0;build_supported=true;write_sizes.clear();
    re4::SharedControl block{};std::memcpy(block.marker,re4::kSharedMarker,16);
    block.abi=re4::kSharedAbi;block.status=1;block.capabilities=re4::kCapabilityTurning;block.sequence=4;
    block.fps_overrides=7;block.reticle_draws=9;block.modern_move_steps=11;block.modern_pose_steps=13;
    block.modern_move_status=3;block.modern_move_distance=12.5f;
    std::memcpy(memory.data()+8,&block,sizeof(block));
}
}
Result pminfoInitialize(){return 0;}void pminfoExit(){}
Result smGetService(Service* s,const char*){s->active=true;return 0;}
bool serviceIsActive(Service* s){return s->active;}void serviceClose(Service* s){s->active=false;}
Result pmdmntGetApplicationProcessId(u64* p){*p=pid;return 0;}
Result pminfoGetProgramId(u64* p,u64){*p=title;return 0;}
Result test_dispatch(Service*,unsigned command,const void* in,std::size_t,void* out,std::size_t,TestDispatchParams p){
    if(command==65003)return attach_rc;
    if(command==65002){
        auto* m=static_cast<Metadata*>(out);*m={};m->pid=pid;m->title=title;
        std::memcpy(m->build_id,re4::profile::build_id,32);if(!build_supported)m->build_id[0]^=1;
        return 0;
    }
    if(command==65101){
        const u64 offset=*static_cast<const u64*>(in);
        *static_cast<u64*>(out)=offset?0:mapping_count;
        auto* mappings=static_cast<MemoryInfo*>(const_cast<void*>(p.buffers[0].data));
        mappings[0]={address,memory.size(),MemType_CodeMutable,Perm_Rw};return 0;
    }
    assert(command==65102||command==65103);
    struct Range{u64 addr,size;};const auto r=*static_cast<const Range*>(in);
    assert(r.addr>=address && r.addr-address+r.size<=memory.size());
    auto* target=memory.data()+r.addr-address;
    if(command==65102){++reads;std::memcpy(const_cast<void*>(p.buffers[0].data),target,r.size);}
    else{
        ++write_count;write_sizes.push_back(r.size);
        if(fail_write==write_count)return 5;
        std::memcpy(target,p.buffers[0].data,r.size);
    }
    return 0;
}
int main(){
    reset();
    {
        GameLink link;link.initialize();assert(link.connect());
        assert(link.details.find("fps=7 reticle=9 move=11 pose=13 status=3 distance=12.500")!=std::string::npos);
        re4::Settings s;s.turn_gain=0.3f;assert(link.apply(s));
        const auto block=snapshot();assert(block.sequence==6);
        assert((write_sizes==std::vector<std::size_t>{4,re4::kSettingsBytes,4}));
        re4::Settings decoded;assert(re4::decode_settings(block.settings,sizeof(block.settings),decoded));
        assert(std::fabs(decoded.turn_gain-0.3f)<0.0001f);
        ++pid;assert(!link.apply(s) && write_count==3);link.close();
    }
    for(unsigned failure=1;failure<=3;++failure){
        reset();GameLink link;link.initialize();assert(link.connect());fail_write=failure;
        assert(!link.apply({}));assert(write_count==failure);
        assert(failure==1 ? snapshot().sequence==4 : (snapshot().sequence&1));
        link.close();
    }
    {
        reset();title=1;GameLink link;link.initialize();assert(!link.connect() && reads==0);link.close();
        reset();build_supported=false;link.initialize();assert(!link.connect() && reads==0);link.close();
        reset();attach_rc=7;link.initialize();assert(!link.connect() && reads==0);link.close();
        reset();mapping_count=33;link.initialize();assert(!link.connect() && reads==0);link.close();
        reset();memory[8]=0;link.initialize();assert(!link.connect() && write_count==0);link.close();
        reset();auto old=snapshot();old.abi=re4::kSharedAbi-1;
        std::memcpy(memory.data()+8,&old,sizeof(old));
        link.initialize();assert(!link.connect() && write_count==0);link.close();
    }
    std::puts("actual game IPC: identity, mapping bounds, odd/payload/even and restart protection PASS");
}
