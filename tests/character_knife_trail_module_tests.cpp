#define RE4_EMITTER_FIXTURE_ONLY
#include "character_emitter_module_tests.cpp"
#undef RE4_EMITTER_FIXTURE_ONLY
namespace {
struct TrailFixture:EmitterFixture {
    alignas(32) std::uint8_t pool[2][0x1F8]{},data[0x10C40]{};
    alignas(32) re4::ViewVec3 vertices[native_vertices]{};
    re4::CharacterWeight8 weight10{{10,0,0},1,{100,0,0},0};
    std::uint16_t indices[native_vertices]{};
    TrailFixture(){
        auto& bank=character_banks[0];
        character_store(face,0x58,static_cast<void*>(blade));
        for(unsigned i=0;i<native_vertices;++i)vertices[i]={float(i),1000,float(i)*2};
        character_store(archive,0x220,static_cast<void*>(&weight10));
        character_store(archive,0x248,static_cast<void*>(vertices));
        character_store(archive,0x270,static_cast<void*>(indices));
        character_store(archive,0x278,static_cast<void*>(indices));
        for(auto range:std::vector<MemoryInfo>{{reinterpret_cast<std::uintptr_t>(pool),sizeof(pool),1},
            {reinterpret_cast<std::uintptr_t>(data),sizeof(data),1},{reinterpret_cast<std::uintptr_t>(vertices),sizeof(vertices),1},
            {reinterpret_cast<std::uintptr_t>(&weight10),sizeof(weight10),1},{reinterpret_cast<std::uintptr_t>(indices),sizeof(indices),1}})game_test::memory_maps.push_back(range);
        character_store(f.image.data(),0x113B140,static_cast<void*>(data));
        character_store(data,0x10C20,static_cast<void*>(pool));character_store(data,0x10C30,2u);
        character_store(pool[0],0,main_base+0xAF19F0);pool[0][0x20]=1;pool[0][0x21]=0x12;pool[0][0x18]=0x2B;
        character_store(pool[0],0x30,static_cast<void*>(f.actor));
        character_store(pool[0],0x38,character_value<unsigned>(f.actor,0x110));
        character_store(pool[0],0x40,static_cast<void*>(player_bones[10]));
        character_store(pool[0],0x4C,re4::ViewVec3{-330,-20,350});
        // Admit the same exact attached blade used by the owned body draw.
        for(unsigned slot=0;slot<3;++slot){bank.slot_ack[slot].store(bank.slot_token[slot].load());}
        submit();assert(bank.attached_valid[0]);
    }
    re4::ViewVec3 sample(std::uintptr_t caller=0x1E0720){
        character_emitter_caller=main_base+caller;re4::ViewVec3 out{};
        CharacterEmitterPoint::Callback(reinterpret_cast<const re4::ViewMatrix3x4*>(player_bones[10]+0x18),
            reinterpret_cast<const re4::ViewVec3*>(pool[0]+0x4C),&out);return out;
    }
};
struct IndependentTrailFixture:TrailFixture {
    alignas(32) re4::ViewVec3 blade_vertices[654]{},blade_normals[654]{};
    std::uint16_t blade_indices[654]{};re4::CharacterWeight8 blade_weight{{1,0,0},1,{100,0,0},0};
    alignas(32) std::uint8_t bind[32]{};
    IndependentTrailFixture(){
        character_store(face,0x58,static_cast<void*>(nullptr));
        for(unsigned i=0;i<654;++i){blade_vertices[i]={float(i)*.1f,1,float(i)*.3f};blade_normals[i]={0,1,0};}
        bind[0]=0;bind[1]=255;bind[16]=1;bind[17]=0;character_store(bind,20,re4::ViewVec3{-77,-19,6});
        character_store(weapon,0,main_base+0xB0AAF8);weapon[0x118]=0x24;weapon[0x11A]=2;
        character_store(f.image.data(),0xB0AAF8+0x28,main_base+0x2E8590);character_store(f.image.data(),0xB0AAF8+0x98,main_base+0x2E8D80);
        character_store(weapon_archive,0x18,0x100u); // Exact initializer BIN2.
        character_store(weapon_archive,0x100,static_cast<void*>(bind));weapon_archive[0x129]=2;weapon_archive[0x128]=1;
        character_store(weapon_archive,0x142,std::uint16_t(1));
        character_store(weapon_archive,0x158,std::uint16_t(654));character_store(weapon_archive,0x15A,std::uint16_t(654));
        character_store(weapon_archive,0x120,static_cast<void*>(&blade_weight));
        character_store(weapon_archive,0x148,static_cast<void*>(blade_vertices));character_store(weapon_archive,0x150,static_cast<void*>(blade_normals));
        character_store(weapon_archive,0x170,static_cast<void*>(blade_indices));character_store(weapon_archive,0x178,static_cast<void*>(blade_indices));
        character_store(weapon_part,0x74,identity_matrix);character_store(weapon_part,0xA7,std::uint8_t(255));
        for(unsigned i=0;i<3;++i){character_store(weapon_part,0xB0+i*8,static_cast<void*>(blade_vertices));character_store(weapon_part,0xC8+i*8,static_cast<void*>(blade_normals));}
        character_store(bones[1],0x18,character_translation({-77,-19,6}));character_store(bones[1],0x110,character_translation({77,19,-6}));
        for(auto r:std::vector<MemoryInfo>{{reinterpret_cast<std::uintptr_t>(blade_vertices),sizeof(blade_vertices),1},
            {reinterpret_cast<std::uintptr_t>(blade_normals),sizeof(blade_normals),1},{reinterpret_cast<std::uintptr_t>(blade_indices),sizeof(blade_indices),1},
            {reinterpret_cast<std::uintptr_t>(&blade_weight),sizeof(blade_weight),1},{reinterpret_cast<std::uintptr_t>(bind),sizeof(bind),1}})game_test::memory_maps.push_back(r);
        auto& bank=character_banks[0];bank.slot_ack[0].store(bank.slot_token[0].load());submit();
        assert(CharacterSkin::Callback(weapon,weapon_part)==1);draw_weapon();assert(drawn_head!=weapon_part);
    }
};
}
int main(){
    for(auto caller:{0x1E0720u,0x1E0C20u}){
        TrailFixture y;const auto original=character_value<re4::ViewVec3>(y.pool[0],0x4C);
        // Current source10 = inverse bind translation(-1000Y), ordinary body
        // blade follows donor shift(-300Y). Actual tip is vertex162.
        assert(emitter_near(y.sample(caller),{162,-300,324}));
        assert(character_emitter_point_calls==1);
        assert(emitter_near(character_value<re4::ViewVec3>(y.pool[0],0x4C),original));
    }
    {
        TrailFixture y;const re4::ViewMatrix3x4 moved{{{0,-1,0,50},{1,0,0,70},{0,0,1,-20}}};
        for(auto& bone:y.player_bones)character_store(bone,0x18,moved);
        // Native source worlds move coherently without reading an old Skin
        // palette or requiring another owned draw.
        assert(emitter_near(y.sample(),{350,232,304}));
        assert(character_value<re4::ViewMatrix3x4>(y.player_bones[10],0x18).m[0][3]==50);
    }
    {
        TrailFixture y;auto& bank=character_banks[0];bank.appearance=re4::CharacterAppearance::Krauser;
        bank.knife_tip_valid=bank.knife_orientation_valid=true;
        bank.knife_tip={{12,700,-90},1};bank.knife_orientation={{0,700,0},{0,1,0}};
        re4::CharacterEmitterPose pose{};std::uintptr_t pointers[26]{};CharacterEmitterReader reader;
        assert(character_emitter_current(bank,pose,pointers,reader));
        re4::ViewMatrix3x4 half{},part{};assert(re4::character_knife_half_turn(bank.knife_orientation,pose.donor10,half));
        assert(character_matrix_product(character_translation({0,-300,0}),identity_matrix,part));
        assert(character_matrix_product(part,character_translation({0,300,0}),part));
        assert(character_matrix_product(part,half,part));
        re4::ViewVec3 expected{};assert(re4::character_blade_tip_world(bank.knife_tip,pose.donor10,part,identity_matrix,expected));
        assert(emitter_near(y.sample(),expected));
        assert(expected.z>0); // The approved Krauser half-turn is included.
        bank.knife_tip_valid=false;assert(emitter_near(y.sample(),{-330,-20,350}));
    }
    {
        IndependentTrailFixture y;
        re4::CharacterEmitterPose pose{};std::uintptr_t pointers[26]{};CharacterEmitterReader reader;re4::ViewVec3 tip{};
        assert(character_emitter_current(character_banks[0],pose,pointers,reader));
        assert(character_knife_independent_tip(character_banks[0],pose,reader,tip));const auto sample=y.sample();
        assert(emitter_near(sample,{65.3f,-299,195.9f}));
        y.blade_weight.bone[0]=0;assert(emitter_near(y.sample(),{-330,-20,350}));
    }
    {
        TrailFixture y;character_store(y.blade,8,1u);character_store(y.blade,0xA7,std::uint8_t(0));
        // Visibility/alpha can end before the live ribbon history. The same
        // immutable blade must keep supplying its tip to remaining samples.
        assert(emitter_near(y.sample(),{162,-300,324}));
    }
    for(bool krauser:{false,true})for(bool independent_visible:{false,true}){
        IndependentTrailFixture y;auto& bank=character_banks[0];
        if(krauser){
            bank.appearance=re4::CharacterAppearance::Krauser;bank.knife_tip_valid=bank.knife_orientation_valid=true;
            bank.knife_tip={{12,700,-90},1};bank.knife_orientation={{0,700,0},{0,1,0}};
            bank.ready.pieces[owned_count-1].role=re4::CharacterAssetRole::Knife;
        }
        character_store(y.face,0x58,static_cast<void*>(y.blade));
        if(krauser)character_store(y.blade,0x74,character_translation({0,0,7}));
        if(!independent_visible){character_store(y.weapon_part,8,1u);character_store(y.weapon_part,0xA7,std::uint8_t(0));}
        bank.slot_ack[0].store(bank.slot_token[0].load());y.submit();
        assert(CharacterSkin::Callback(y.weapon,y.weapon_part)==1);y.draw_weapon();
        character_store(y.blade,0xA7,std::uint8_t(0)); // Alpha-only hidden transition.
        re4::ViewVec3 expected=independent_visible?re4::ViewVec3{65.3f,-299,195.9f}:re4::ViewVec3{162,-300,324};
        if(krauser){
            re4::CharacterEmitterPose pose{};std::uintptr_t pointers[26]{};CharacterEmitterReader reader;
            assert(character_emitter_current(bank,pose,pointers,reader));re4::ViewMatrix3x4 half{};
            assert(re4::character_knife_half_turn(bank.knife_orientation,pose.donor10,half));
            assert(re4::character_blade_tip_world(bank.knife_tip,pose.donor10,half,identity_matrix,expected));
            if(!independent_visible)expected.z+=7;
        }
        assert(emitter_near(y.sample(),expected));
    }
    for(unsigned invalid=0;invalid<13;++invalid){
        TrailFixture y;
        if(invalid==0)y.pool[0][0x18]=0x2A;
        if(invalid==1)y.pool[0][0xB]=1;
        if(invalid==2)y.pool[0][0x21]=0x11;
        if(invalid==3)y.pool[0][0x20]=0;
        if(invalid==4)character_store(y.pool[0],0x38,12345u);
        if(invalid==5)character_store(y.pool[0],0x40,static_cast<void*>(y.player_bones[9]));
        if(invalid==6)character_store(y.blade,0x74,character_translation({1,0,0}));
        if(invalid==7)y.indices[0]=1;
        if(invalid==8)character_store(y.pool[0],0,main_base+0xAF19F8);
        if(invalid==9)character_banks[0].phase.store(CharacterPhase::Retiring);
        if(invalid==10)character_banks[0].drawn.store(0);
        if(invalid==11)y.weight10.bone[0]=9;
        if(invalid==12)y.vertices[3].z=std::numeric_limits<float>::quiet_NaN();
        assert(emitter_near(y.sample(),{-330,-20,350}));
        assert(character_banks[0].fault.load()==0&&(invalid==9||character_banks[0].phase.load()==CharacterPhase::Active));
    }
    return 0;
}
