#include "re4/modern_move.hpp"
#include "re4/profile.hpp"
#include "re4/response.hpp"
#include "re4/shared.hpp"
#include "game_stubs/lib.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>
using namespace re4;
namespace {
template<class T> void put(std::vector<std::uint8_t>& b,std::size_t o,T v){assert(o+sizeof(v)<=b.size());std::memcpy(b.data()+o,&v,sizeof(v));}
struct Asset {
    std::vector<std::uint8_t> b=std::vector<std::uint8_t>(1024);
    std::uintptr_t base() const{return reinterpret_cast<std::uintptr_t>(b.data());}
    Asset(){
        put<std::uint16_t>(b,0,40);b[2]=4;
        const std::uint16_t flags[]={1,0x40,2,2};const std::uint8_t bones[]={0,0,18,10};
        for(unsigned i=0;i<4;++i){put(b,3+2*i,flags[i]);b[11+i]=bones[i];put<std::uint64_t>(b,20+8*i,64+80*i);
            for(unsigned axis=0;axis<3;++axis){const auto at=64+80*i+axis*16;put<std::uint16_t>(b,at,1);put<std::uint16_t>(b,at+2,0);put<float>(b,at+4,float(i+axis));put<float>(b,at+8,0);put<float>(b,at+12,0);}
        }
    }
    ModernMoveMemory memory(){return ModernMoveMemory::span(b.data(),b.size());}
};
void parser(){
    Asset a;ModernMoveMotion m;
    assert(m.parse(a.memory(),a.base(),0));
    assert(m.count()==4 && m.translation_track()==0 && m.rotation_track()==1);
    assert(m.map()[0]==255 && m.map()[1]==255 && m.map()[2]==18 && m.map()[3]==255);
    assert(m.track(0)==a.base()+64 && m.frames()==40);
    assert(m.get<std::uint64_t>(8)==reinterpret_cast<std::uintptr_t>(m.tracks()));
    assert(m.get<std::uint32_t>(0x58)&(1u<<30));assert(!m.get<std::uint64_t>(0xE8));
    ModernMoveMotion copy=m;assert(copy.get<std::uint64_t>(8)==reinterpret_cast<std::uintptr_t>(copy.tracks()));
    const auto original=a.b;assert(m.parse(a.memory(),a.base(),0));assert(a.b==original);
    put<std::uint64_t>(a.b,20,a.base()+64);assert(m.parse(a.memory(),a.base(),0));
    put<std::uint64_t>(a.b,20,std::numeric_limits<std::uint64_t>::max());assert(!m.parse(a.memory(),a.base(),0));
    a=Asset{};put<float>(a.b,68,std::numeric_limits<float>::infinity());assert(!m.parse(a.memory(),a.base(),0));
    a=Asset{};put<std::uint16_t>(a.b,3,0x3001);assert(!m.parse(a.memory(),a.base(),0));
    a=Asset{};put<std::uint16_t>(a.b,3,2);assert(!m.parse(a.memory(),a.base(),0));
    a=Asset{};put<std::uint16_t>(a.b,0,1);assert(!m.parse(a.memory(),a.base(),0));
    a=Asset{};put<std::uint16_t>(a.b,64,42);assert(!m.parse(a.memory(),a.base(),0));
    a=Asset{};put<std::uint16_t>(a.b,64,2);put<std::uint16_t>(a.b,66,1);put<std::uint16_t>(a.b,68,1);assert(!m.parse(a.memory(),a.base(),0));
    a=Asset{};assert(!m.parse(ModernMoveMemory::span(a.b.data(),30),a.base(),0));
    a=Asset{};put<std::uint16_t>(a.b,512,3);a.b[514]=1;put<std::uint32_t>(a.b,516,0);put<std::uint32_t>(a.b,520,64);put<std::uint32_t>(a.b,524,128);
    assert(m.parse(a.memory(),a.base(),a.base()+512));assert(m.frames()==3 && m.get<std::uint16_t>(0x54)==0x1005);
    put<std::uint32_t>(a.b,520,0);assert(!m.parse(a.memory(),a.base(),a.base()+512));
}
}
namespace { void cycle_and_gates(); void reload_gates(); void poses(); }
namespace runtime_fixture {void run();}
int main(){parser();cycle_and_gates();reload_gates();poses();runtime_fixture::run();std::puts("modern movement tests passed");}

// Appended test group is included by main below.
namespace {
unsigned advances=0,roots=0,translations=0;
std::uint32_t advance_motion(void* p){++advances;auto* b=static_cast<std::uint8_t*>(p);float f=0,speed=0;std::memcpy(&f,b+0xD4,4);std::memcpy(&speed,b+0xDC,4);f+=speed;std::memcpy(b+0xD4,&f,4);return 0;}
void root_motion(void*,void* p,unsigned,ModernMoveVec3* d,ModernMoveVec3* r){++roots;auto* b=static_cast<std::uint8_t*>(p);float speed=0;std::memcpy(&speed,b+0xDC,4);*d={0,0,10*speed};*r={};}
void translate_actor(void*,const ModernMoveVec3*){++translations;}
void sample_motion(const ModernMoveSample* p,ModernMoveVec3* out,std::uint16_t*,unsigned,unsigned){*out={0,0,p->frame*10};}
void cycle_and_gates(){
    ModernMoveFrame frame;frame.active_player=true;frame.character=0;frame.mode=0;frame.routine=6;frame.phase=1;frame.subphase=1;frame.generic_firearm=true;frame.native_aim=true;frame.input_live=true;frame.held=0x11;frame.frame_factor=1;frame.x=0;frame.y=1;
    assert(!modern_move_allowed(frame));frame.aim_enabled=true;assert(modern_move_allowed(frame));
    frame.frozen=true;assert(!modern_move_allowed(frame));frame.frozen=false;frame.phase=4;assert(!modern_move_allowed(frame));frame.phase=1;
    frame.routine=11;assert(!modern_move_allowed(frame));frame.knife_enabled=true;frame.phase=2;frame.held=0x800ull;assert(modern_move_allowed(frame));
    frame.held=1ull<<48;assert(!modern_move_allowed(frame)); // Unrelated edge input is not knife hold.
    frame.held=0x10;assert(!modern_move_allowed(frame)); // Gun aim cannot activate knife movement.
    frame.held=0x800;
    frame.character=2;assert(!modern_move_allowed(frame));frame.character=0;frame.scripted=true;assert(!modern_move_allowed(frame));frame.scripted=false;
    auto disk=modern_move_disk(1,1);assert(std::fabs(std::hypot(disk.x,disk.z)-1)<1e-5f);
    assert(!modern_move_disk(std::numeric_limits<float>::quiet_NaN(),1).z);
    ModernMoveBasis basis{{{2,0,0},{0,2,0},{0,0,2}}};ModernMoveVec3 delta;
    assert(modern_move_world_step({0,0,10},basis,0,1,0,delta));assert(delta.z==20);
    assert(modern_move_world_step({0,0,10},basis,1,0,0,delta));assert(delta.x==-20 && std::fabs(delta.z)<1e-5f);
    Asset asset;ModernMoveMotion motion;assert(motion.parse(asset.memory(),asset.base(),0));assert(motion.prime(sample_motion));
    ModernMoveEngine engine{sample_motion,advance_motion,root_motion,translate_actor};ModernMoveCycle cycle;
    assert(cycle.bind(motion));advances=roots=translations=0;
    assert(cycle.step(engine,nullptr,1,0,1,0,basis,1,delta));assert(advances==2&&roots==2&&translations==1&&delta.z==20);
    assert(!cycle.step(engine,nullptr,1,0,1,0,basis,1,delta));assert(translations==1);
    assert(cycle.step(engine,nullptr,2,1,1,0,basis,9,delta));assert(std::hypot(delta.x,delta.z)<=20.0001f);
    assert(cycle.step(engine,nullptr,3,0,0,0,basis,1,delta)==false);assert(translations==2);
    assert(!cycle.step(engine,nullptr,4,0,1,0,basis,std::numeric_limits<float>::infinity(),delta));
    basis.m[0][0]=std::numeric_limits<float>::quiet_NaN();assert(!cycle.step(engine,nullptr,5,0,1,0,basis,1,delta));assert(!cycle.valid());
}
void reload_gates(){
    ModernMoveFrame reload;reload.reload_enabled=true;reload.active_player=true;
    reload.generic_firearm=true;reload.input_live=true;reload.routine=6;reload.phase=4;reload.frame_factor=1;
    // Native reload initialization/playback/return blend do not require held
    // aim, native aim, or either of the other movement options.
    for(unsigned sub:{0,1,2,3}){
        reload.subphase=sub;assert(modern_reload_allowed(reload)&&modern_move_allowed(reload));
    }
    reload.subphase=0;
    const auto rejected=[](const ModernMoveFrame& frame){assert(!modern_reload_allowed(frame)&&!modern_move_allowed(frame));};
    auto candidate=reload;candidate.reload_enabled=false;rejected(candidate);
    candidate=reload;candidate.active_player=false;rejected(candidate);
    candidate=reload;candidate.character=1;rejected(candidate);
    candidate=reload;candidate.mode=1;rejected(candidate);
    candidate=reload;candidate.generic_firearm=false;rejected(candidate);
    candidate=reload;candidate.input_live=false;rejected(candidate);
    candidate=reload;candidate.scripted=true;rejected(candidate);
    candidate=reload;candidate.frozen=true;rejected(candidate);
    for(unsigned routine:{0,1,4,5,7,11,15,17}){candidate=reload;candidate.routine=routine;rejected(candidate);}
    for(unsigned phase:{0,1,2,3,5,6,7,8,255}){candidate=reload;candidate.phase=phase;rejected(candidate);}
    for(unsigned sub:{4,5,255}){candidate=reload;candidate.subphase=sub;rejected(candidate);}
    for(float factor:{0.f,-1.f,2.01f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
        candidate=reload;candidate.frame_factor=factor;rejected(candidate);
    }
    candidate=reload;candidate.x=std::numeric_limits<float>::quiet_NaN();rejected(candidate);
    candidate=reload;candidate.y=std::numeric_limits<float>::infinity();rejected(candidate);
    reload.frame_factor=2;assert(modern_reload_allowed(reload));
}
}

// Compile and execute the actual native adapter. Only engine entry points are
// replaced, with a bounded simulated address space and the real hook bodies.
namespace runtime_fixture {
std::uintptr_t main_base=0;
re4::Settings settings;
re4::SharedControl control{};
unsigned sensor_attributes=1;
bool native_aim_value=true;
int native_aim(void*){return native_aim_value;}
int (*aiming_predicate)(void*)=native_aim;
void test_ModernPlayerUpdate(void*);
std::uint32_t test_ModernRoot(void*,void*);
void test_ModernPose(void*,int);
void* unified_root_actor=nullptr;float unified_root_yaw=0;bool test_unified_walk=false;
void unified_player_update(void* actor,void(*original)(void*)){original(actor);}
bool unified_walk_root(void*,float& yaw){yaw=-1.57079632679f;return test_unified_walk;}
#include "../source/game/modern_move.inc"

std::vector<unsigned> order;
unsigned evaluation_count=0,healthy_value=1;
bool simulate_nan=false;
int phase_before_root=-1;
float expected_local=0,hit_z=0,collision_z=0;
std::array<std::uint8_t,0x1E0>* active_bones=nullptr;
std::uint8_t* active_actor=nullptr;
float& scalar(void* p,std::size_t offset){return *reinterpret_cast<float*>(static_cast<std::uint8_t*>(p)+offset);}
void test_ModernPlayerUpdate(void* actor){
    assert(scalar(active_bones[17].data(),0xB4)==expected_local);
    order.push_back(1);
    if(phase_before_root>=0)static_cast<std::uint8_t*>(actor)[0x116]=std::uint8_t(phase_before_root);
    assert(ModernRoot::Callback(actor,nullptr)==0x17);
    // The native weapon creates its ray/knife sweep only after native root and
    // world/IK. Native physics then processes the translated actor once.
    order.push_back(5);hit_z=scalar(actor,0xAC);
    order.push_back(6);collision_z=scalar(actor,0xAC);
    scalar(actor,0x12C)=scalar(actor,0xAC); // Native end-of-frame previous point.
}
std::uint32_t test_ModernRoot(void* actor,void*){
    order.push_back(3);ModernPose::Callback(actor,0);ModernPose::Callback(actor,0);return 0x17;
}
void test_ModernPose(void*,int){
    for(unsigned i=0;i<26;++i)for(unsigned j=0;j<18;++j)assert(*reinterpret_cast<std::uint16_t*>(active_bones[i].data()+0x1B4+2*j)==9999);
    order.push_back(4);*reinterpret_cast<std::uint32_t*>(active_bones[18].data()+0x1D8)|=0x10000414;
}
std::uint32_t advance(void* motion){return advance_motion(motion);}
void root(void* actor,void* motion,unsigned f,ModernMoveVec3* delta,ModernMoveVec3* rotation){root_motion(actor,motion,f,delta,rotation);}
void translate(void* actor,const ModernMoveVec3* delta){
    order.push_back(2);scalar(actor,0xA4)+=delta->x;scalar(actor,0xAC)+=delta->z;
    // Match native81D0 current-only translation. Previous world points remain.
    for(unsigned i=0;i<26;++i){scalar(active_bones[i].data(),0x80)+=delta->x;scalar(active_bones[i].data(),0x88)+=delta->z;}
}
void evaluate(void*,void* motion){
    ++evaluation_count;order.push_back(7);
    assert(!(*reinterpret_cast<std::uint32_t*>(static_cast<std::uint8_t*>(motion)+0x58)&(1u<<27)));
    for(unsigned i=17;i<26;++i)for(unsigned j=0;j<18;++j){auto* cursor=reinterpret_cast<std::uint16_t*>(active_bones[i].data()+0x1B4+2*j);assert(*cursor==0);*cursor=7;}
    for(unsigned i=0;i<17;++i)for(unsigned j=0;j<18;++j)assert(*reinterpret_cast<std::uint16_t*>(active_bones[i].data()+0x1B4+2*j)==9999);
    const auto* map=*reinterpret_cast<const std::uint8_t**>(static_cast<std::uint8_t*>(motion)+0x40);
    const auto n=static_cast<std::uint8_t*>(motion)[0x38];
    for(unsigned i=0;i<n;++i)if(map[i]<26){assert(map[i]>=17);scalar(active_bones[map[i]].data(),0xB0)=0.3f;}
    if(simulate_nan)scalar(active_bones[17].data(),0xB4)=std::numeric_limits<float>::quiet_NaN();
}
void rotation(void* matrix,const void* angles){auto* out=static_cast<float*>(matrix);std::fill(out,out+12,0.f);out[0]=out[5]=out[10]=1;out[1]=static_cast<const float*>(angles)[1];}
void position(void* matrix,const void* point){auto* out=static_cast<float*>(matrix);for(unsigned i=0;i<3;++i)out[4*i+3]=static_cast<const float*>(point)[i];}
void scale(void*,const void*){}
void copy_matrix(const void* source,void* target){std::memcpy(target,source,48);}
int healthy(){return int(healthy_value);}
struct Fixture {
    std::vector<std::uint8_t> image=std::vector<std::uint8_t>(0x1270000),state=std::vector<std::uint8_t>(0x8700),actor=std::vector<std::uint8_t>(0x950),table=std::vector<std::uint8_t>(0x330);
    std::array<std::array<std::uint8_t,0x1E0>,26> bones{};
    Asset forward,back,injured_forward,injured_back;
    template<class T>void store(std::vector<std::uint8_t>& d,std::size_t o,T value){put(d,o,value);}
    void register_memory(void* p,std::size_t n){game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(p),n,3});}
    Fixture(){
        modern_pose.discard();modern_cycle.reset();modern_bound_owner={};modern_bound_asset=modern_bound_clip=0;
        modern_update_actor=nullptr;modern_epoch=0;modern_root_done=modern_stepped=modern_pose_done=false;
        control={};settings={};sensor_attributes=1;native_aim_value=true;healthy_value=1;simulate_nan=false;phase_before_root=-1;evaluation_count=0;order.clear();expected_local=0;
        main_base=reinterpret_cast<std::uintptr_t>(image.data());active_actor=actor.data();active_bones=bones.data();
        game_test::memory_maps.clear();register_memory(image.data(),image.size());register_memory(state.data(),state.size());register_memory(actor.data(),actor.size());register_memory(table.data(),table.size());register_memory(bones.data(),sizeof(bones));
        for(auto* asset:{&forward,&back,&injured_forward,&injured_back})register_memory(asset->b.data(),asset->b.size());
        store(image,re4::profile::player_slot,reinterpret_cast<std::uintptr_t>(actor.data()));store(image,0xB1AE08,reinterpret_cast<std::uintptr_t>(state.data()));
        store(image,re4::profile::input_flags,std::uint64_t(0x10));store(image,0x1265A28,main_base+0x31CC60);store(image,0xAFF39C,2);image[0x1131718+0x54]=10;
        store(state,0xA4,1.f);store(state,0x160,1.f);
        actor[0x115]=6;actor[0x116]=actor[0x117]=1;actor[0x11A]=119;store(actor,0x352,std::uint16_t(0x300));
        store(actor,0x108,reinterpret_cast<std::uintptr_t>(bones.data()));store(actor,0x428,reinterpret_cast<std::uintptr_t>(bones.data()));store(actor,0x548,reinterpret_cast<std::uintptr_t>(table.data()));
        for(unsigned i=0;i<3;++i){store(actor,0x18+20*i,1.f);store(actor,0xBC+4*i,1.f);}
        const int parents[]={-1,0,1,2,3,2,5,5,7,8,9,2,11,11,13,14,15,0,17,18,19,20,17,22,23,24};
        for(unsigned i=0;i<26;++i){
            const auto parent=i?reinterpret_cast<std::uintptr_t>(bones[parents[i]].data()):reinterpret_cast<std::uintptr_t>(actor.data());
            std::memcpy(bones[i].data()+0x78,&parent,8);
            const auto next=i+1<26?reinterpret_cast<std::uintptr_t>(bones[i+1].data()):0;std::memcpy(bones[i].data()+0x108,&next,8);
            for(unsigned j=0;j<3;++j){scalar(bones[i].data(),0xBC+4*j)=1;scalar(bones[i].data(),0x48+20*j)=1;}
            scalar(bones[i].data(),0x8C)=12;scalar(bones[i].data(),0x94)=13;
            for(unsigned j=0;j<18;++j){const std::uint16_t cursor=9999;std::memcpy(bones[i].data()+0x1B4+2*j,&cursor,2);}
        }
        store(table,0x10,forward.base());store(table,0x40,back.base());store(table,0x308,injured_forward.base());store(table,0x318,injured_back.base());
        modern_native={{sample_motion,advance,root,translate},evaluate,rotation,position,scale,copy_matrix,healthy};
        settings.move_while_aiming=true;settings.left_deadzone=0.05f;control.raw_ly=1;
    }
    void update(){ModernPlayerUpdate::Callback(actor.data());}
};
void run(){
    // Reload has its own option and survives aim release from initialization
    // through return blend, without replacing the native upper-body clip.
    for(unsigned sub : {0,1,2,3}){
        Fixture f;settings.move_while_aiming=false;settings.move_while_reloading=true;
        f.actor[0x116]=4;f.actor[0x117]=std::uint8_t(sub);
        f.store(f.image,re4::profile::input_flags,std::uint64_t(0));native_aim_value=false;
        const std::uint32_t primary_cursor=0x12345678,secondary_cursor=0x87654321;
        f.store(f.actor,0x2FC,primary_cursor);f.store(f.actor,0x300,secondary_cursor);
        f.update();
        assert(control.modern_move_steps==1&&control.modern_pose_steps==1&&evaluation_count==1);
        assert(hit_z==10&&collision_z==10&&scalar(f.actor.data(),0x12C)==10);
        assert((order==std::vector<unsigned>{1,2,3,7,4,4,5,6}));
        assert(*reinterpret_cast<const std::uint32_t*>(f.actor.data()+0x2FC)==primary_cursor);
        assert(*reinterpret_cast<const std::uint32_t*>(f.actor.data()+0x300)==secondary_cursor);
        assert(scalar(f.bones[10].data(),0xB0)==0);
        assert(!modern_move_aim_active(f.actor.data()));
        assert(modern_move_reload_active(f.actor.data()));
        // A neutral stick restores the prior leg pose without advancing gait.
        control.raw_ly=0;f.update();
        assert(control.modern_move_steps==1&&evaluation_count==1&&!modern_pose.pending());
        settings.move_while_reloading=false;f.update();
        assert(control.modern_move_steps==1&&control.modern_move_status==0&&!modern_pose.pending());
    }
    {
        Fixture f;settings.move_while_aiming=false;settings.move_while_reloading=true;
        f.actor[0x116]=4;f.actor[0x117]=0;
        f.store(f.image,re4::profile::input_flags,std::uint64_t(0));native_aim_value=false;
        // Recheck the actual native root state, rather than caching eligibility
        // at PlayerUpdate entry before native code has changed phase.
        assert(modern_move_reload_active(f.actor.data()));phase_before_root=1;f.update();
        assert(!control.modern_move_steps&&!modern_pose.pending());
        phase_before_root=-1;f.actor[0x116]=4;
        f.store(f.image,0x1265A28,main_base+0x3270F0);f.update();assert(!control.modern_move_steps);
        f.store(f.image,0x1265A28,main_base+0x31CC60);
        f.store(f.actor,0x352,std::uint16_t(0x100));f.update();assert(!control.modern_move_steps);
        f.store(f.actor,0x352,std::uint16_t(0x300));
        f.actor[0x117]=4;f.update();assert(!control.modern_move_steps);
        f.actor[0x117]=0;f.image[0x11C7D55]=1;f.update();assert(!control.modern_move_steps);
        f.image[0x11C7D55]=0;f.state[0x86FF]=1;f.update();assert(!control.modern_move_steps);
        f.state[0x86FF]=0;f.state[0x5092]=4;f.update();assert(!control.modern_move_steps);
        f.state[0x5092]=0;f.store(f.state,0xA4,0.f);f.update();assert(!control.modern_move_steps);
        f.store(f.state,0xA4,1.f);f.update();assert(control.modern_move_steps==1);
    }
    // These installation helpers are compile-checked here; actual hook offsets
    // are checked against the verified image by module profile tests.
    assert(!modern_move_matches(nullptr,0));auto installer=&modern_move_install;(void)installer;
    for(float height:{-1000.f,1000.f})for(float planar:{0.f,-.000043711f}){
        Fixture f;settings.unified_controls=true;
        f.store(f.state,0x14C,ModernMoveVec3{});
        f.store(f.state,0x158,ModernMoveVec3{0,height,planar});
        f.update();
        assert(control.modern_move_steps==1);
        assert(std::fabs(scalar(f.actor.data(),0xAC)-10)<.0001f);
        assert(std::fabs(scalar(f.actor.data(),0xA4))<.0001f);
        assert(hit_z==10&&collision_z==10); // Straight-up/down gyro cannot stop or reverse forward walking.
    }
    {
        Fixture f;settings.unified_controls=true;f.actor[0x115]=1;control.raw_lx=1;control.raw_ly=0;
        test_unified_walk=true;f.update();
        assert(evaluation_count==0&&control.modern_move_steps==0&&control.modern_pose_steps==1);
        assert(std::fabs(scalar(f.bones[17].data(),0xB4)+1.57079632679f)<1e-5f);
        test_unified_walk=false;expected_local=0;f.update();assert(!modern_pose.pending());
    }
    {
        Fixture f;settings.move_while_aiming=false;settings.move_while_knife=true;
        f.actor[0x115]=11;f.store(f.image,re4::profile::input_flags,std::uint64_t(0x800));
        for(unsigned phase : {1,2}){
            f.actor[0x116]=std::uint8_t(phase);f.actor[0x117]=1;order.clear();
            const float before=scalar(f.actor.data(),0xAC);f.update();
            assert(scalar(f.actor.data(),0xAC)==before+10);
            assert(hit_z==before+10 && collision_z==before+10);
            assert((order==std::vector<unsigned>{1,2,3,7,4,4,5,6}));
        }
        assert(control.modern_move_steps==2 && control.modern_pose_steps==2);
        f.store(f.image,re4::profile::input_flags,std::uint64_t(1ull<<48));f.update();
        assert(control.modern_move_steps==2 && !modern_pose.pending());
        f.store(f.image,re4::profile::input_flags,std::uint64_t(0x800));settings.move_while_knife=false;f.update();
        assert(control.modern_move_steps==2 && control.modern_move_status==0);
    }
    {
        Fixture f;assert(modern_move_aim_active(f.actor.data()));f.update();
        assert(control.modern_move_steps==1&&control.modern_pose_steps==1&&evaluation_count==1);
        assert(hit_z==10&&collision_z==10&&scalar(f.actor.data(),0x12C)==10);
        assert((order==std::vector<unsigned>{1,2,3,7,4,4,5,6}));
        assert(scalar(f.bones[18].data(),0x94)==13); // Previous point untouched.
        assert(scalar(f.bones[10].data(),0xB0)==0); // Upper body untouched.
        control.raw_lx=control.raw_ly=0;order.clear();f.update();assert(control.modern_move_steps==1&&evaluation_count==1&&!modern_pose.pending());
        assert((*reinterpret_cast<std::uint32_t*>(f.bones[18].data()+0x1D8)&0x10000414)==0x10000414);
        control.raw_ly=0.1f;f.update();assert(control.modern_move_steps==2&&control.modern_move_distance>0&&control.modern_move_distance<1);
        settings.move_while_aiming=false;f.update();assert(control.modern_move_status==0&&!modern_pose.pending());
    }
    {
        Fixture f;control.raw_ly=-1;f.update();assert(modern_bound_asset==f.back.base());
        healthy_value=0;f.update();assert(modern_bound_asset==f.injured_back.base());
        control.raw_ly=1;f.update();assert(modern_bound_asset==f.injured_forward.base());
        healthy_value=1;f.update();assert(modern_bound_asset==f.forward.base());
        settings.move_while_aiming=false;settings.move_while_knife=true;f.actor[0x115]=11;f.actor[0x116]=2;
        f.store(f.image,re4::profile::input_flags,std::uint64_t(0x800ull));f.update();assert(control.modern_move_steps==5);
        f.actor[0x115]=6;f.actor[0x116]=4;f.update();assert(control.modern_move_steps==5&&!modern_pose.pending());
    }
    {
        Fixture f;f.image[0x11C7D55]=1;assert(!modern_move_aim_active(f.actor.data()));f.update();assert(!control.modern_move_steps);
        f.image[0x11C7D55]=0;f.state[0x86FF]=1;f.update();assert(!control.modern_move_steps);
        f.state[0x86FF]=0;f.state[0x5092]=4;f.update();assert(!control.modern_move_steps);
        f.state[0x5092]=0;f.table[0x10]=0;f.store(f.table,0x10,std::uintptr_t(1));f.update();assert(!control.modern_move_steps&&control.modern_move_status==2);
        assert(modern_move_aim_active(f.actor.data()));
        f.store(f.table,0x10,f.forward.base());simulate_nan=true;f.update();assert(control.modern_move_steps==1&&!control.modern_pose_steps&&!modern_pose.pending());
        assert(scalar(f.bones[17].data(),0xB4)==0);
    }
    {
        Fixture f;control.raw_lx=1;control.raw_ly=0;f.update();assert(scalar(f.actor.data(),0xA4)==-10&&std::fabs(scalar(f.bones[17].data(),0xB4)+1.5707963f)<1e-5f);
        settings.move_while_aiming=false;f.update();assert(scalar(f.bones[17].data(),0xB4)==0);
        settings.move_while_aiming=true;f.actor[0x116]=2;f.actor[0x117]=0;f.update();assert(control.modern_move_steps==2);
        f.actor[0x117]=2;f.update();assert(control.modern_move_steps==2&&!modern_pose.pending());
    }
}
}

namespace {
void poses(){
    struct Bone {float local[9]{0,0,0,0,0,0,1,1,1};float matrix[12]{};std::uint32_t flags=0;float world[3]{1,2,3},previous[3]{4,5,6};};
    std::array<Bone,9> native{};std::array<ModernMoveBone,9> bones{};
    for(unsigned i=0;i<9;++i)bones[i]={native[i].local,native[i].matrix,&native[i].flags};
    ModernMoveIdentity identity{1,2,3};ModernMovePose ledger;
    assert(ledger.begin(identity,bones));native[0].local[4]=1;native[1].local[3]=0.4f;native[1].flags|=0x10010000;native[1].matrix[0]=99;
    assert(ledger.finish());assert(ledger.pending());
    // IK owns bit28 as well. Native post-pose flags must survive restoration.
    native[1].flags|=0x414;
    assert(ledger.restore(identity,bones));assert(native[0].local[4]==0&&native[1].local[3]==0&&native[1].flags==0x10000414&&native[1].matrix[0]==0);
    assert(native[1].world[0]==1&&native[1].previous[0]==4); // Never erase world sweeps.
    assert(ledger.begin(identity,bones));native[0].local[4]=1;assert(ledger.finish());
    auto invalid=bones;invalid[0].local=reinterpret_cast<float*>(1);
    assert(!ledger.restore({9,9,9},invalid)); // No dereference on identity mismatch.
    assert(!ledger.pending());native[0].local[4]=0;
    for(unsigned frame=0;frame<100;++frame){assert(ledger.begin(identity,bones));native[0].local[4]+=0.5f;assert(ledger.finish());assert(ledger.restore(identity,bones));assert(native[0].local[4]==0);}
    assert(ledger.begin(identity,bones));native[0].local[4]=std::numeric_limits<float>::quiet_NaN();assert(!ledger.finish());assert(native[0].local[4]==0&&!ledger.pending());
    assert(ledger.begin(identity,bones));native[0].local[4]=0.5f;assert(ledger.finish());native[0].local[4]=0.7f;assert(ledger.restore(identity,bones));assert(native[0].local[4]==0.7f); // Later native change wins.
    assert(ledger.begin(identity,bones));native[0].local[4]=0.5f;native[0].matrix[0]=0.5f;assert(ledger.finish());native[0].matrix[0]=0.8f;assert(ledger.restore(identity,bones));assert(native[0].matrix[0]==0.8f);
    assert(ledger.begin(identity,bones));native[0].local[4]=0.5f;assert(ledger.finish());assert(!ledger.restore({1,2,3,99},bones));assert(native[0].local[4]==0.5f);
    std::array<std::int16_t,26> parent{};const int p[]={-1,0,1,2,3,2,5,5,7,8,9,2,11,11,13,14,15,0,17,18,19,20,17,22,23,24};
    for(unsigned i=0;i<26;++i){parent[i]=std::int16_t(p[i]);}assert(modern_move_leon_topology(parent,119));parent[22]=2;assert(!modern_move_leon_topology(parent,119));
    bool back=false;assert(std::fabs(modern_move_lower_yaw({-1,0,0},0,back)+1.5707963f)<1e-5f&&!back);
    assert(std::fabs(modern_move_lower_yaw({0,0,-1},0,back))<1e-5f&&back);
}
}


