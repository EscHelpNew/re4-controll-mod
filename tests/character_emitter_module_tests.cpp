#define RE4_NATURAL_FIXTURE_ONLY
#include "natural_character_module_tests.cpp"
#undef RE4_NATURAL_FIXTURE_ONLY
namespace {
std::uintptr_t emitter_script=0;
struct EmitterFixture:HeldFixture {
    alignas(32) std::uint8_t bones[5][0x1F0]{},effects[3][0x108]{};
    EmitterFixture(unsigned handle=0,unsigned parent=10,bool linked=false):HeldFixture(handle,parent){
        re4::CharacterBindBone source[26]{},donor[26]{};
        for(unsigned i=0;i<26;++i){source[i].id=donor[i].id=std::uint8_t(i);source[i].parent=donor[i].parent=re4::character_core_parents[i];}
        source[0].position={0,1000,0};donor[0].position={0,700,0};
        donor[16].position={-30,0,0};donor[17].position={0,40,0};
        assert(re4::character_retarget_map(source,26,donor,26,character_banks[0].map));
        const auto physical=[=](unsigned i){return linked?(i*7)%26:i;};
        for(unsigned i=0;i<26;++i){
            auto* bone=player_bones[physical(i)];
            character_store(bone,0x78,i?static_cast<void*>(player_bones[physical(re4::character_core_parents[i])]):static_cast<void*>(f.actor));
            character_store(bone,0x108,i<25?static_cast<void*>(player_bones[physical(i+1)]):nullptr);
            auto inverse=identity_matrix;inverse.m[1][3]=-1000;
            character_store(bone,0x110,inverse);
        }
        for(unsigned i=0;i<5;++i){
            character_store(bones[i],0x18,identity_matrix);
            if(i<4)character_store(bones[i],0x108,static_cast<void*>(bones[i+1]));
        }
        character_store(bones[0],0x78,static_cast<void*>(player_bones[physical(parent)]));
        character_store(bones[1],0x18,base);weapon[0x11A]=5;
        character_store(weapon,0x108,static_cast<void*>(bones));
        game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(bones),sizeof(bones),1});
        game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(effects),sizeof(effects),1});
        character_store(f.image.data(),0x113CC00,3u);character_store(f.image.data(),0x113CC10,static_cast<void*>(effects));
        effects[0][0x18]=1;effects[0][0x19]=0x10;effects[0][3]=0x3B;effects[0][0x10]=0;
        character_store(effects[0],0x28,static_cast<void*>(weapon));character_store(effects[0],0x38,std::uint16_t(0xFE));
        character_store(effects[0],0x40,identity_matrix);
        emitter_script=reinterpret_cast<std::uintptr_t>(weapon_archive+0x480);character_store(effects[0],0x20,emitter_script);
        character_native.effect_script=+[](unsigned group,unsigned variant,unsigned silent)->void*{
            assert((group==0x38||group==0x3B||group==0x3D||group==0x3E||group==0x3F||group==0x48||group==0x49||group==0x4C)&&silent==1&&variant<2);
            return reinterpret_cast<void*>(emitter_script);
        };
        character_banks[0].token.store(character_serial.fetch_add(1)+1);
        submit();draw_weapon();assert(drawn_head!=weapon_part);
        character_emitter_point_calls=character_emitter_product_calls=character_emitter_copy_calls=0;
    }
    const re4::ViewMatrix3x4* world(unsigned part=3){return reinterpret_cast<const re4::ViewMatrix3x4*>(bones[part]+0x18);}
    re4::ViewVec3 point(std::uintptr_t caller,const re4::ViewMatrix3x4* m=nullptr){
        const re4::ViewVec3 input{1,2,3};re4::ViewVec3 out{};character_emitter_caller=main_base+caller;
        CharacterEmitterPoint::Callback(m?m:world(),&input,&out);return out;
    }
};
bool emitter_near(re4::ViewVec3 a,re4::ViewVec3 b){return std::fabs(a.x-b.x)<.002&&std::fabs(a.y-b.y)<.002&&std::fabs(a.z-b.z)<.002;}
}
int main(){
    {
        EmitterFixture y;game_test::memory_queries=0;
        assert(emitter_near(y.point(0x4EC134),{1,2,3}));
        assert(game_test::memory_queries==0);
        y.point(0x1F4A48);const unsigned cold=game_test::memory_queries;
        game_test::memory_queries=0;
        for(unsigned i=0;i<10;++i)y.point(0x1F4A48);
        std::printf("emitter ownership queries: cold=%u repeated=%u per10calls\n",cold,game_test::memory_queries);
        assert(cold<=12&&game_test::memory_queries<=120);
    }
    for(unsigned handle=0;handle<2;++handle){
        EmitterFixture y(handle);
        const auto before=y.f.input_state;std::uint8_t source[sizeof(y.player_bones)];std::memcpy(source,y.player_bones,sizeof(source));
        for(auto caller:{0x2E8FB0u,0x1D6A4Cu,0x1F4A48u,0x1F68ACu,0x1E0194u}){
            auto p=y.point(caller,caller==0x2E8FB0?y.world(0):nullptr);assert(emitter_near(p,{1,-298,3}));
        }
        assert(character_emitter_point_calls==5);
        auto* hand=reinterpret_cast<const re4::ViewMatrix3x4*>(y.player_bones[10]+0x18);
        for(auto caller:{0x31DF10u,0x340190u,0x340D00u,0x3416F8u,0x34212Cu,0x3483B0u,0x34A4F8u,
                         0x34C110u,0x34CBE0u,0x34EEE0u,0x350858u,0x351234u,0x352BB0u})
            assert(emitter_near(y.point(caller,hand),{1,-298,3}));
        assert(character_emitter_point_calls==18);
        re4::ViewMatrix3x4 view{{{0,-2,0,7},{1,0,0,8},{0,0,1,9}}},out{};
        for(auto caller:{0x1F4364u,0x1F4424u,0x1F4CF0u,0x1F5284u,0x1F59E0u,0x1D7F48u}){
            character_emitter_caller=main_base+caller;CharacterEmitterProduct::Callback(&view,y.world(),&out);
            assert(out.m[0][3]==607&&out.m[1][3]==8&&out.m[2][3]==9);
        }
        character_emitter_caller=main_base+0x1F4C8C;CharacterEmitterProduct::Callback(y.world(),&view,&out);
        assert(out.m[0][3]==7&&out.m[1][3]==-292&&out.m[2][3]==9);
        assert(character_emitter_product_calls==7);
        character_emitter_caller=main_base+0x1D7DA4;CharacterEmitterCopy::Callback(y.world(),&out);assert(out.m[1][3]==-300);
        // A detached particle's owned matrix is no longer a native held-bone
        // pointer, so its future matrix consumers preserve the single shift.
        character_emitter_caller=main_base+0x1F4364;CharacterEmitterProduct::Callback(&identity_matrix,&out,&out);assert(out.m[1][3]==-300);
        character_emitter_caller=main_base+0x1E81F0;
        CharacterEmitterCopy::Callback(reinterpret_cast<const re4::ViewMatrix3x4*>(y.effects[0]+0x40),&out);
        assert(out.m[1][3]==-300);
        assert(!std::memcmp(source,y.player_bones,sizeof(source))&&y.f.input_state==before);
        assert(y.world()->m[1][3]==0&&character_value<re4::ViewMatrix3x4>(y.effects[0],0x40).m[1][3]==0);
        // A new pose changes current source worlds without a Skin callback.
        auto spine=identity_matrix;spine.m[1][3]=40;character_store(y.player_bones[2],0x18,spine);
        assert(!emitter_near(y.point(0x1F4A48),{1,-298,3}));
        assert(emitter_near(y.point(0x4EC134),{1,2,3})); // Native Skin caller excluded.
    }
    {
        EmitterFixture y(0,10,true); // Linked logical order differs from allocation order.
        assert(emitter_near(y.point(0x1F4A48),{1,-298,3}));
    }
    for(unsigned parent:{10u,16u,17u}){
        EmitterFixture y(1,parent);
        const re4::ViewVec3 expected=parent==10?re4::ViewVec3{1,-298,3}:parent==16?re4::ViewVec3{-29,-298,3}:re4::ViewVec3{1,-258,3};
        assert(emitter_near(y.point(0x1F4A48),expected));
        // Native producers explicitly using BODY10 follow its donor pivot,
        // independently of which other native parent owns this held handle.
        assert(emitter_near(y.point(0x340190,reinterpret_cast<const re4::ViewMatrix3x4*>(y.player_bones[10]+0x18)),{1,-298,3}));
    }
    {
        EmitterFixture y;
        const auto descriptor=character_value<re4::ViewMatrix3x4>(y.weapon_part,0x74);
        const auto palette=y.f.input_state;
        const re4::ViewMatrix3x4 moved{{{0,-1,0,50},{1,0,0,70},{0,0,1,-20}}};
        for(auto& bone:y.player_bones)character_store(bone,0x18,moved);
        for(unsigned i=0;i<5;++i){
            auto value=moved;if(i==1)assert(re4::character_emitter_detail::product(moved,y.base,value));
            character_store(y.bones[i],0x18,value);
        }
        // Body and held matrices advance together, as native2B4D70 does; the
        // source mesh-local74 and old Skin palette remain unchanged.
        assert(emitter_near(y.point(0x1F4A48),{348,71,-17}));
        assert(!std::memcmp(&descriptor,y.weapon_part+0x74,sizeof(descriptor))&&y.f.input_state==palette);
    }
    {
        EmitterFixture y;
        const auto start=reinterpret_cast<std::uintptr_t>(y.player_bones);
        std::erase_if(game_test::memory_maps,[&](const MemoryInfo& region){return region.addr==start;});
        for(auto& bone:y.player_bones)game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(bone),sizeof(bone),1});
        // Every bone is separately readable, but the current-world query must
        // not assume the first proved region covers the next linked node.
        assert(emitter_near(y.point(0x1F4A48),{1,2,3}));
        assert(character_banks[0].fault.load()==0&&character_banks[0].phase.load()==CharacterPhase::Active);
    }
    for(unsigned invalid=0;invalid<17;++invalid){
        EmitterFixture y;auto& bank=character_banks[0];
        if(invalid==0)bank.phase.store(CharacterPhase::Retiring);
        if(invalid==1)bank.fault.store(1);
        if(invalid==2)bank.appearance=re4::CharacterAppearance::Leon;
        if(invalid==3)character_store(y.weapon,0x3F8,static_cast<void*>(y.body));
        if(invalid==4)character_store(y.weapon,0x180,static_cast<void*>(y.extra));
        if(invalid==5)character_store(y.player_bones[4],0x78,static_cast<void*>(y.player_bones[0]));
        if(invalid==6)character_store(y.player_bones[4],0x108,static_cast<void*>(y.player_bones[4]));
        if(invalid==7){auto bad=identity_matrix;bad.m[0][0]=std::numeric_limits<float>::quiet_NaN();character_store(y.player_bones[4],0x18,bad);}
        if(invalid==8)character_renderer_ready.store(false);
        if(invalid==9)bank.prepared=false;
        if(invalid==10)bank.drawn.store(0);
        if(invalid==11)character_teardown.store(true);
        if(invalid==12)bank.map.count=0;
        if(invalid==13)character_store(y.weapon_part,0x20,static_cast<void*>(y.archive));
        if(invalid==14)character_render_lock.test_and_set();
        if(invalid==15)character_store(y.bones[0],0x78,static_cast<void*>(y.player_bones[9]));
        if(invalid==16){
            auto changed=y.part;changed.m[0][3]+=1;character_store(y.weapon_part,0x74,changed);
            y.draw_weapon();assert(drawn_head==y.weapon_part); // Same owner, changed descriptor is native.
        }
        assert(emitter_near(y.point(0x1F4A48),{1,2,3}));
        if(invalid==14)character_render_lock.clear();
    }
    for(unsigned invalid=0;invalid<10;++invalid){
        EmitterFixture y;re4::ViewMatrix3x4 out{};
        if(invalid==0)y.effects[0][0x18]=0;
        if(invalid==1)y.effects[0][0x19]=0x11;
        if(invalid==2)character_store(y.effects[0],0x28,static_cast<void*>(y.body));
        if(invalid==3)character_store(y.effects[0],0x30,1u);
        if(invalid==4)y.effects[0][3]=0x37;
        if(invalid==5)y.effects[0][0x10]=2;
        if(invalid==6)character_store(y.effects[0],0x20,emitter_script+4);
        if(invalid==7)character_store(y.effects[0],0x38,std::uint16_t(0));
        if(invalid==8)character_store(y.f.image.data(),0x113CC00,0u);
        if(invalid==9)character_native.effect_script=+[](unsigned,unsigned,unsigned)->void*{
            character_banks[0].phase.store(CharacterPhase::Retiring);
            return reinterpret_cast<void*>(emitter_script);
        };
        character_emitter_caller=main_base+0x1E81F0;
        CharacterEmitterCopy::Callback(reinterpret_cast<const re4::ViewMatrix3x4*>(y.effects[0]+0x40),&out);
        assert(out.m[1][3]==0&&character_emitter_copy_calls==1);
    }
    std::puts("actual current-pose laser/shot/muzzle/shell callbacks and native fallbacks PASS");
}
