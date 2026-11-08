#define RE4_EMITTER_FIXTURE_ONLY
#include "character_emitter_module_tests.cpp"
#undef RE4_EMITTER_FIXTURE_ONLY

namespace {
// Native producer boundaries are authenticated in profile.hpp; these fixtures
// follow their data flow through BODY10, point+linear transforms, endpoint
// addition and the synchronous hit/small-model consumers. Spread inputs are
// supplied at the RNG boundary; native RNG itself is not emulated. No game
// binary or native instruction dump is embedded.
const re4::ViewMatrix3x4* native_bone_world(void* actor,unsigned id){
    auto p=character_value<std::uintptr_t>(actor,0x108);
    if(character_value<unsigned>(actor,8)&0x2000)p+=id*0x1F0;
    else for(unsigned i=0;i<id;++i)p=character_value<std::uintptr_t>(reinterpret_cast<void*>(p),0x108);
    return reinterpret_cast<const re4::ViewMatrix3x4*>(p+0x18);
}
re4::ViewVec3 add(re4::ViewVec3 a,re4::ViewVec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
re4::ViewVec3 subtract(re4::ViewVec3 a,re4::ViewVec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
re4::ViewVec3 rotate(const re4::ViewMatrix3x4& world,re4::ViewVec3 p){
    auto linear=world;for(auto& row:linear.m)row[3]=0;
    re4::ViewVec3 out{};assert(re4::view_transform_point(linear,p,out));return out;
}
struct NativeHit {re4::ViewVec3 origin{},end{};};
struct NativeProducer {const char* name;std::uintptr_t point_return,hit_call;float local_x;unsigned rays;};
constexpr NativeProducer firearms[]{
    {"handguns",0x31DF10,0x31DFE4,234.5f,1},
    {"TMP first",0x3283BC,0x3284A8,0,1},
    {"TMP repeated",0x328288,0x328364,0,1},
    {"shotguns",0x336898,0x3368E4,-15.5f,13},
    {"alternate handgun",0x346F04,0x346FD8,234.5f,1},
};
std::vector<NativeHit> native_fire(EmitterFixture& f,const NativeProducer& producer,bool enabled=true){
    std::vector<NativeHit> hits;
    for(unsigned pellet=0;pellet<producer.rays;++pellet){
        // Native31DEDC/328254/328388/336778/346ED0 resolve the
        // CURRENT player's BODY10, independently of the rendered weapon part.
        const auto* hand=native_bone_world(f.f.actor,10);
        re4::ViewVec3 origin{producer.local_x,-24,38.33f};
        character_emitter_caller=main_base+(enabled?producer.point_return:0);
        CharacterEmitterPoint::Callback(hand,&origin,&origin);
        // Native279AB0 consumes the same source matrix's LINEAR part only;
        // translating the origin must preserve spread, orientation and range.
        const re4::ViewVec3 spread{-50000,float(pellet)*1.25f+2.5f,-3.75f};
        const auto direction=rotate(*hand,spread);
        hits.push_back({origin,add(origin,direction)});
    }
    return hits;
}
struct NativeShell {re4::ViewVec3 current{},previous{},rotation{},velocity{};};
struct NativeShellProducer {
    std::uintptr_t point_return;int held_part=-1;
    re4::ViewVec3 local{-109,-22,90};
};
constexpr NativeShellProducer shells[]{
    {0x2D6310},{0x2DA670},{0x2DAF40},{0x2E2A90},{0x2E5594,-1,{-216,-24,105.9f}},{0x2E7FD0},
    {0x2DB2AC,0,{-270,0,100}},{0x2E3238,1,{-240,-20,70}},
    {0x340190},{0x340D00,-1,{-163,-163,100}},{0x3416F8,-1,{-201,-6.9f,1.8f}},
    {0x34212C,-1,{-163.31f,-6.87f,83.13f}},{0x3483B0},
    {0x34A4F8,-1,{-201,-6.9f,1.8f}},{0x34C110},{0x34CBE0},{0x34EEE0,-1,{-163,-163,100}},
    {0x350858,-1,{-201,-6.9f,1.8f}},{0x351234,-1,{-216,-24,105.9f}},{0x352BB0}};
NativeShell native_shell(EmitterFixture& f,NativeShellProducer producer,bool enabled=true){
    // Eighteen helpers load the player through GOT B25D88; two supported
    // held-source families select their exact root/part1 instead.
    const auto slot=character_value<std::uintptr_t>(f.f.image.data(),0xB25D88);
    void* actor=character_value<void*>(reinterpret_cast<void*>(slot),0);
    const auto* hand=producer.held_part<0?native_bone_world(actor,10)
        :native_bone_world(f.weapon,unsigned(producer.held_part));
    re4::ViewVec3 point=producer.local;
    character_emitter_caller=main_base+(enabled?producer.point_return:0);
    CharacterEmitterPoint::Callback(hand,&point,&point);
    const auto velocity=rotate(*hand,{6,12,11});
    // Native2C11B0 copies x2 to both model+A4 and model+128 before return.
    // It also copies rotation and velocity; it does not retain x2's pointer.
    return {point,point,{},velocity};
}
void prepare_native_globals(EmitterFixture& f){
    character_store(f.f.image.data(),0xB25D88,main_base+re4::profile::player_slot);
    // The original TMP class resides outside the later33F000..353000 family.
    character_store(f.weapon,0,main_base+0xB053B8);
    character_store(f.f.image.data(),0xB053B8+0x28,main_base+0x2E8590);
    character_store(f.f.image.data(),0xB053B8+0x58,main_base+0x2DA4A0);
    character_store(f.f.image.data(),0xB053B8+0x98,main_base+0x2E8D80);
    character_store(f.weapon,0x400,std::uint16_t(0x30));
    f.weapon[0x118]=0x2D; // Actual base TMP pool type from3429D4.
}
struct NativePrlFixture:EmitterFixture {
    alignas(32) std::uint8_t prl_bones[9][0x1F0]{};
    NativePrlFixture(){
        for(unsigned i=0;i<9;++i){
            character_store(prl_bones[i],0x18,i==1?base:identity_matrix);
            if(i<8)character_store(prl_bones[i],0x108,static_cast<void*>(prl_bones[i+1]));
        }
        // Authentic2D8AA4..AC4 attaches the P.R.L. root to BODY9.
        // This exact special parent intentionally has no owned held drawing.
        character_store(prl_bones[0],0x78,static_cast<void*>(player_bones[9]));
        character_store(weapon,0x108,static_cast<void*>(prl_bones));weapon[0x11A]=9;
        character_store(weapon,0,main_base+0xB052E8);
        character_store(f.image.data(),0xB052E8+0x28,main_base+0x2E8590);
        character_store(f.image.data(),0xB052E8+0x58,main_base+0x2D8FD0);
        character_store(f.image.data(),0xB052E8+0x98,main_base+0x2E8D80);
        character_store(weapon,0x400,std::uint16_t(65));
        game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(prl_bones),sizeof(prl_bones),1});
    }
};
NativeHit native_prl(NativePrlFixture& f,bool charged,bool enabled){
    const auto* muzzle=native_bone_world(f.weapon,8);
    const auto point=[&](const re4::ViewMatrix3x4* matrix,re4::ViewVec3 local,std::uintptr_t lr){
        character_emitter_caller=main_base+(enabled?lr:0);
        CharacterEmitterPoint::Callback(matrix,&local,&local);return local;
    };
    const auto origin=point(muzzle,{800,0,0},charged?0x2D92E4:0x2D91E4);
    const auto end=charged?add(origin,rotate(*muzzle,{-50000,0,0}))
        :point(native_bone_world(f.weapon,0),{-3000,0,0},0x2D9208);
    if(!charged){
        // Native2D96C0 repeats the same part8 origin before cone selection.
        const auto cone_origin=point(muzzle,{800,0,0},0x2D9714);
        assert(emitter_near(origin,cone_origin));
        assert(emitter_near(subtract(end,cone_origin),subtract(end,origin)));
    }
    return {origin,end};
}
}
int main(){
    {
        EmitterFixture f;CharacterEmitterReader reader;
        re4::CharacterEmitterPose pose{};std::uintptr_t pointers[26]{};
        assert(character_emitter_current(character_banks[0],pose,pointers,reader));
        assert(pose.source10.m[1][3]==-1000);
        assert(pose.donor10.m[1][3]==-1000);
        assert(emitter_near(pose.delta[3],{0,-300,0}));
    }
    for(const auto& producer:firearms){
        EmitterFixture f;prepare_native_globals(f);
        const auto bones=std::vector<std::uint8_t>(reinterpret_cast<std::uint8_t*>(f.player_bones),
            reinterpret_cast<std::uint8_t*>(f.player_bones)+sizeof(f.player_bones));
        const auto native=native_fire(f,producer,false),actual=native_fire(f,producer);
        for(unsigned i=0;i<actual.size();++i){
            const re4::ViewVec3 expected{0,-300,0};
            if(!emitter_near(subtract(actual[i].origin,native[i].origin),expected))
                std::fprintf(stderr,"unshifted actual hit producer %s at%zx\n",producer.name,std::size_t(producer.hit_call));
            assert(emitter_near(subtract(actual[i].origin,native[i].origin),expected));
            assert(emitter_near(subtract(actual[i].end,native[i].end),expected));
            assert(emitter_near(subtract(actual[i].end,actual[i].origin),subtract(native[i].end,native[i].origin)));
        }
        assert(!std::memcmp(bones.data(),f.player_bones,bones.size()));
    }
    for(auto site:shells){
        EmitterFixture f;prepare_native_globals(f);
        const auto native=native_shell(f,site,false),actual=native_shell(f,site);
        if(!emitter_near(subtract(actual.current,native.current),{0,-300,0}))
            std::fprintf(stderr,"unshifted shell producer at%zx\n",std::size_t(site.point_return-4));
        assert(emitter_near(subtract(actual.current,native.current),{0,-300,0}));
        assert(emitter_near(actual.current,actual.previous));
        assert(emitter_near(actual.rotation,native.rotation)&&emitter_near(actual.velocity,native.velocity));
    }
    {
        EmitterFixture f;prepare_native_globals(f);
        // Authentic Mine class/type/parent from342EA4,2DB650 and2DB704.
        // BODY9 is intentionally outside this release's held-owner admission.
        character_store(f.weapon,0,main_base+0xB05558);f.weapon[0x118]=0x36;
        character_store(f.f.image.data(),0xB05558+0x28,main_base+0x2E8590);
        character_store(f.f.image.data(),0xB05558+0x58,main_base+0x2DB920);
        character_store(f.f.image.data(),0xB05558+0x98,main_base+0x2E8D80);
        character_store(f.bones[0],0x78,static_cast<void*>(f.player_bones[9]));
        const NativeShellProducer mine{0x2DBD94,0,{-348,-63,38}};
        const auto before=native_shell(f,mine,false),actual=native_shell(f,mine);
        assert(emitter_near(actual.current,before.current)&&emitter_near(actual.previous,before.previous));
        assert(emitter_near(actual.velocity,before.velocity));
        f.draw_weapon();assert(drawn_head==f.weapon_part);
    }
    {
        EmitterFixture f;prepare_native_globals(f);
        // Native351B60 selects BODY10, rotates the launch direction, transforms
        // the local origin, clips it with native8330, then1B43C0 copies the
        // resulting origin to current/previous. The private native audit
        // authenticates both constants and this data flow.
        const auto* hand=native_bone_world(f.f.actor,10);
        re4::ViewVec3 native{},actual{};
        const re4::ViewVec3 local{0,0,140};
        assert(re4::view_transform_point(*hand,local,native));
        character_emitter_caller=main_base+0x351BB8;
        CharacterEmitterPoint::Callback(hand,&local,&actual);
        assert(emitter_near(subtract(actual,native),{0,-300,0}));
        NativeShell projectile{actual,actual,{},rotate(*hand,{-1500,0,0})};
        assert(emitter_near(projectile.current,projectile.previous));
        assert(emitter_near(projectile.velocity,rotate(*hand,{-1500,0,0})));
    }
    for(bool charged:{false,true}){
        NativePrlFixture f;
        const auto before=native_prl(f,charged,false),actual=native_prl(f,charged,true);
        assert(emitter_near(actual.origin,before.origin)&&emitter_near(actual.end,before.end));
        assert(emitter_near(subtract(actual.end,actual.origin),subtract(before.end,before.origin)));
        f.draw_weapon();assert(drawn_head==f.weapon_part);
        // Cone target bones belong to enemies; no native target transforms,
        // scoped camera origin or camera-derived rocket origin is admitted.
        for(auto lr:{0x2D9880u,0x2D9890u,0x333978u,0x2E1D00u}){
            game_test::memory_queries=0;
            assert(emitter_near(f.point(lr,native_bone_world(f.f.actor,7)),{1,2,3}));
            assert(game_test::memory_queries==0);
        }
    }
    std::puts("native firearm/bow and21shell census:20supported shifted; authentic Mine native fallback PASS");
}
