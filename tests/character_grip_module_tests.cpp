// Actual Skin/held Draw/emitter adapters; external native deformation records
// the palette it consumes instead of requiring the Switch graphics device.
#define RE4_EMITTER_FIXTURE_ONLY
#include "character_emitter_module_tests.cpp"
#undef RE4_EMITTER_FIXTURE_ONLY
#define main character_grip_pure_fixture_main
#include "character_grip_tests.cpp"
#undef main

namespace {
constexpr unsigned grip_bones[]{7,8,9,10,13,14,15,16};
std::vector<re4::ViewMatrix3x4> body_palettes;
unsigned body_piece=0;
struct GripMenuReader {
    std::uintptr_t actor,held,state;
    template<class T>bool value(std::uintptr_t p,T& out){
        assert(!(p>=actor+0x114&&p<=actor+0x117)&&p!=actor+0x938&&p!=held+0x3F8&&p!=main_base+0x1265A28);
        assert(p!=state+0x5092&&p!=state+0x509E&&p!=state+0x1AB&&p!=state+0x86FF);
        return character_read(p,out);
    }
    bool region(std::uintptr_t p,std::size_t bytes){return character_region(p,bytes);}
};
void grip_native_positions(void* out,const void* in,const void*,unsigned count){
    assert(count==1);
    for(unsigned i=0;i<(body_piece==0?2u:1u);++i){
        re4::ViewMatrix3x4 matrix{};
        for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)matrix.m[r][c]=palette_blade[i].value[c*3+r];
        body_palettes.push_back(matrix);
    }
    ++body_piece;std::memcpy(out,in,12);
}
struct GripModuleFixture:EmitterFixture {
    Rig rig{{4,-2,0}};
    re4::CharacterWeight8 grip_weights[8]{};
    const re4::ViewMatrix3x4 root{{{0,-1,0,20},{1,0,0,-30},{0,0,1,10}}};
    GripModuleFixture(unsigned handle=0,unsigned parent=10):EmitterFixture(handle,parent){
        auto& bank=character_banks[0];bank.map=rig.map;
        for(unsigned i=0;i<8;++i)grip_weights[i]={{std::uint8_t(grip_bones[i]),0,0},1,{100,0,0},0};
        for(unsigned i=0;i<owned_count;++i){
            auto& piece=bank.ready.pieces[i];const unsigned first=i?i+1:0,count=i?1:2;
            piece.weights={reinterpret_cast<std::uint8_t*>(grip_weights+first),count*sizeof(grip_weights[0])};piece.weight_count=count;
        }
        for(unsigned i=0;i<26;++i){
            auto bind=identity_matrix,inverse=identity_matrix;
            bind.m[0][3]=rig.source[i].x;bind.m[1][3]=rig.source[i].y;bind.m[2][3]=rig.source[i].z;
            inverse.m[0][3]=-rig.source[i].x;inverse.m[1][3]=-rig.source[i].y;inverse.m[2][3]=-rig.source[i].z;
            re4::ViewMatrix3x4 world;assert(re4::character_emitter_detail::product(root,bind,world));
            character_store(player_bones[i],0x18,world);character_store(player_bones[i],0x110,inverse);
        }
        for(unsigned i=0;i<5;++i)character_store(bones[i],0x18,root);
        re4::ViewMatrix3x4 world_base;assert(re4::character_emitter_detail::product(root,base,world_base));base=world_base;
        character_store(bones[1],0x18,base);
        character_store(f.image.data(),0x1265A28,main_base+0x3351A0);
        f.actor[0x114]=0;f.actor[0x115]=6;f.actor[0x116]=1;f.actor[0x117]=1;
        f.input_state[0x5092]=f.input_state[0x86FF]=0;
        character_native.positions=grip_native_positions;
        for(auto& frame:bank.visual)frame=CharacterVisualFrame{};
        for(unsigned i=0;i<3;++i){bank.slot_ack[i].store(bank.slot_token[i].load());bank.skinned[i].store(0);}
        bank.emitter.valid=false;
    }
    void body(){
        auto& bank=character_banks[0];for(unsigned i=0;i<3;++i)bank.slot_ack[i].store(bank.slot_token[i].load());
        const auto before_skin=skin_calls,before_draw=draw_calls;
        ++view_frame;body_piece=0;body_palettes.clear();submit();
        assert(skin_calls==before_skin+1&&draw_calls==before_draw+1);
        assert(bank.fault.load()==0&&bank.phase.load()==CharacterPhase::Active&&body_palettes.size()==8);
    }
    void expect_body(bool fitted){
        const auto pose=rig.fit(fitted);
        for(unsigned i=0;i<8;++i)for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)
            assert(std::fabs(body_palettes[i].m[r][c]-pose.core[grip_bones[i]].m[r][c])<2e-5);
        const auto& frame=character_banks[0].visual[0];
        assert(emitter_near(frame.shift[0],pose.joint_shift[10])&&near(frame.shift[1],pose.joint_shift[16]));
        if(fitted){
            assert(!std::memcmp(&frame.shift[0],&frame.shift[1],sizeof(re4::ViewVec3)));
            re4::CharacterRetargetPose actual=rig.natural();for(unsigned i=0;i<8;++i)actual.core[grip_bones[i]]=body_palettes[i];
            actual.joint_shift[10]=frame.shift[0];actual.joint_shift[16]=frame.shift[1];
            contact_and_lengths(rig,rig.natural(),actual);
        }
    }
    void expect_held_and_emitter(bool fitted){
        const auto pose=rig.fit(fitted);const auto before_draw=draw_calls;draw_weapon();assert(drawn_head!=weapon_part&&draw_calls==before_draw+1);
        const auto corrected=character_value<re4::ViewMatrix3x4>(drawn_head,0x74);
        const re4::ViewVec3 vertex{7,-3,9};
        const auto native_point=::point(base,::point(part,vertex)),owned_point=::point(base,::point(corrected,vertex));
        auto linear=root;for(auto& row:linear.m)row[3]=0;
        const auto parent=character_value<std::uintptr_t>(bones[0],0x78);
        const unsigned parent_id=parent==reinterpret_cast<std::uintptr_t>(player_bones[17])?17:parent==reinterpret_cast<std::uintptr_t>(player_bones[16])?16:10;
        const auto delta=::point(linear,pose.joint_shift[parent_id]);assert(near(sub(owned_point,native_point),delta,2e-4));
        // Ordinary weapon orientation and dimensions stay unchanged.
        for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)assert(std::fabs(corrected.m[r][c]-part.m[r][c])<1e-5);
        const re4::ViewVec3 input{1,2,3};const auto original=::point(*world(),input);
        auto original_calls=character_emitter_point_calls;
        assert(near(sub(EmitterFixture::point(0x1F4A48),original),delta,2e-4)&&character_emitter_point_calls==original_calls+1);
        const auto* hand=reinterpret_cast<const re4::ViewMatrix3x4*>(player_bones[10]+0x18);
        original_calls=character_emitter_point_calls;
        assert(near(sub(EmitterFixture::point(0x31DF10,hand),::point(*hand,input)),::point(linear,pose.joint_shift[10]),2e-4)&&character_emitter_point_calls==original_calls+1);
    }
};
}
int main(){
    {
        // A supported auxiliary/knife mesh cannot authorize a two-handed
        // fit when the actual primary gun has an unsupported alternate path.
        GripModuleFixture y(1);alignas(32) std::uint8_t primary[sizeof(y.weapon)]{};
        std::memcpy(primary,y.weapon,sizeof(primary));character_store(primary,0x188,static_cast<void*>(y.extra));
        y.weapon[0x118]=0x24;
        character_store(y.owner,0x38,static_cast<void*>(primary));
        game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(primary),sizeof(primary),1});
        y.body();assert(!character_grip_enabled(character_banks[0],character_banks[0]));
        y.expect_body(false);y.expect_held_and_emitter(false);
    }
    {
        // Once the actual primary is supported, the auxiliary draw still
        // consumes the same common fit; primary-only admission is not a ban
        // on correcting the second handle's immutable geometry.
        GripModuleFixture y(1);alignas(32) std::uint8_t primary[sizeof(y.weapon)]{};
        std::memcpy(primary,y.weapon,sizeof(primary));character_store(y.owner,0x38,static_cast<void*>(primary));
        game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(primary),sizeof(primary),1});
        y.body();assert(character_grip_enabled(character_banks[0],character_banks[0]));y.expect_body(true);y.expect_held_and_emitter(true);
    }
    for(unsigned handle:{0u,1u})for(unsigned parent:{10u,16u})for(auto handler:{0x3270F0u,0x332CC0u,0x3351A0u}){
        GripModuleFixture y(handle,parent);character_store(y.f.image.data(),0x1265A28,main_base+handler);
        const auto native_state=y.f.input_state;
        std::array<std::uint8_t,sizeof(y.f.actor)> native_actor{};std::memcpy(native_actor.data(),y.f.actor,native_actor.size());
        std::array<std::uint8_t,sizeof(y.player_bones)> native_bones{};std::memcpy(native_bones.data(),y.player_bones,native_bones.size());
        const auto descriptor=std::array<std::uint8_t,sizeof(y.weapon_part)>{};auto original_descriptor=descriptor;std::memcpy(original_descriptor.data(),y.weapon_part,original_descriptor.size());
        const bool enabled=handle==0;
        y.body();assert(character_grip_enabled(character_banks[0],character_banks[0])==enabled);y.expect_body(enabled);y.expect_held_and_emitter(enabled);
        assert(y.f.input_state==native_state&&!std::memcmp(y.player_bones,native_bones.data(),native_bones.size()));
        assert(!std::memcmp(y.f.actor,native_actor.data(),native_actor.size()));
        assert(!std::memcmp(y.weapon_part,original_descriptor.data(),original_descriptor.size()));
    }
    for(unsigned phase:{1u,2u})for(unsigned subphase:{0u,1u}){
        GripModuleFixture y;y.f.actor[0x116]=std::uint8_t(phase);y.f.actor[0x117]=std::uint8_t(subphase);
        const bool enabled=phase==2||subphase==1;y.body();y.expect_body(enabled);y.expect_held_and_emitter(enabled);
    }
    for(unsigned invalid=0;invalid<12;++invalid){
        GripModuleFixture y;
        if(invalid==0)character_store(y.f.image.data(),0x1265A28,main_base+0x33FFC0); // One-handed dispatcher.
        if(invalid==1)character_store(y.f.image.data(),0x1265A28,std::uintptr_t(0));
        if(invalid==2)y.f.actor[0x114]=1;
        if(invalid==3)y.f.actor[0x115]=7; // Reload routine.
        if(invalid==4)y.f.actor[0x115]=15; // Knife routine.
        if(invalid==5)y.f.actor[0x116]=3;
        if(invalid==6)y.f.actor[0x117]=2;
        if(invalid==7)y.f.input_state[0x5092]=4; // Scripted input.
        if(invalid==8)y.f.input_state[0x86FF]=1; // Frozen/cutscene state.
        if(invalid==9)character_store(y.bones[0],0x78,static_cast<void*>(y.player_bones[17]));
        if(invalid==10)y.f.input_state[0x509E]=0x80;
        if(invalid==11)y.f.input_state[0x1AB]=0x40;
        y.body();assert(!character_grip_enabled(character_banks[0],character_banks[0]));y.expect_body(false);y.expect_held_and_emitter(false);
    }
    {
        // Same exact world bytes, aim-context switch: the fresh emitter cache
        // must not reuse the previous fit merely because no Skin ran again.
        GripModuleFixture y;y.body();y.expect_body(true);y.expect_held_and_emitter(true);
        const auto world=character_banks[0].emitter.world[10];y.f.actor[0x115]=7;
        const auto natural=y.rig.natural();auto linear=y.root;for(auto& row:linear.m)row[3]=0;
        const re4::ViewVec3 input{1,2,3};
        assert(near(sub(y.EmitterFixture::point(0x1F4A48),::point(*y.world(),input)),::point(linear,natural.joint_shift[10]),2e-4));
        assert(!character_banks[0].emitter.two_handed&&!std::memcmp(&world,&character_banks[0].emitter.world[10],sizeof(world)));
    }
    {
        GripModuleFixture y;auto& bank=character_banks[0];
        alignas(32) std::uint8_t models[4][0x3C8]{},menu_bones[26][0x1F0]{},binds[119*16]{},body_part[0x1A8]{},preview[0x500]{};
        std::memcpy(preview,y.weapon_archive,sizeof(preview));
        for(const auto field:{0u,0x20u,0x48u,0x50u,0x70u,0x78u}){
            const auto prior=character_value<std::uintptr_t>(preview+0x100,field);
            character_store(preview+0x100,field,reinterpret_cast<std::uintptr_t>(preview)+(prior-reinterpret_cast<std::uintptr_t>(y.weapon_archive)));
        }
        character_store(y.weapon_part,0x18,static_cast<void*>(preview+0x100));character_store(y.weapon_part,0x20,static_cast<void*>(preview+0x300));
        character_store(y.f.image.data(),0x1270F70+0x258,static_cast<void*>(preview));
        for(unsigned i=0;i<119;++i){
            const unsigned parent=i<26?re4::character_core_parents[i]:0;
            binds[i*16]=std::uint8_t(i);binds[i*16+1]=std::uint8_t(parent);
            const auto source=i<26?(i?sub(y.rig.source[i],y.rig.source[parent]):y.rig.source[i]):re4::ViewVec3{};
            std::memcpy(binds+i*16+4,&source,12);
            if(i<26){
                const auto donor=i?sub(y.rig.donor[i],y.rig.donor[parent]):y.rig.donor[i];
                bank.ready.body_bind[i]={std::uint8_t(i),std::uint8_t(parent),donor};
                character_store(menu_bones[i],0x18,identity_matrix);
                if(i<25)character_store(menu_bones[i],0x108,static_cast<void*>(menu_bones[i+1]));
            }
        }
        bank.ready.body_bone_count=26;character_store(y.archive,0x100,static_cast<void*>(binds));
        character_store(body_part,0x18,static_cast<void*>(y.archive+0x100));
        for(unsigned i:{0u,1u}){character_store(models[i],0,main_base+0xAFF538);character_store(models[i],8,1u);}
        models[0][0x11A]=119;models[1][0x11A]=5;
        character_store(models[0],0x108,static_cast<void*>(menu_bones));character_store(models[0],0x180,static_cast<void*>(body_part));
        character_store(models[1],0x108,static_cast<void*>(y.bones));character_store(models[1],0x180,static_cast<void*>(y.weapon_part));
        character_store(y.bones[0],0x78,static_cast<void*>(menu_bones[10]));
        character_store(y.f.image.data(),0xAFF4A8+8,static_cast<void*>(models));character_store(y.f.image.data(),0xAFF4A8+0x10,4u);
        character_store(y.f.image.data(),0xAFF4A8+0x14,0x3C8u);character_store(y.f.image.data(),0x1272140,static_cast<void*>(models[0]));
        character_store(y.f.image.data(),0x1272148,static_cast<void*>(models[1]));
        for(auto range:std::vector<MemoryInfo>{{reinterpret_cast<std::uintptr_t>(models),sizeof(models),1},{reinterpret_cast<std::uintptr_t>(menu_bones),sizeof(menu_bones),1},
            {reinterpret_cast<std::uintptr_t>(binds),sizeof(binds),1},{reinterpret_cast<std::uintptr_t>(body_part),sizeof(body_part),1},{reinterpret_cast<std::uintptr_t>(preview),sizeof(preview),1}})
            game_test::memory_maps.push_back(range);
        bank.menu.deformed=reinterpret_cast<std::uint8_t (*)[character_deformed_slot]>(bank.raw+2*1024*1024);bank.menu.capacity=character_deformed_slot;
        character_native.initialize=+[](void*,void*,unsigned){return true;};
        character_native.attach=+[](void* p,void* bin,void* tpl){character_store(p,0x18,bin);character_store(p,0x20,tpl);character_store(p,8,8u);};
        // The case uses its equipped-item table, not main cEm state.
        character_store(y.f.image.data(),0x1265A28,std::uintptr_t(0));
        models[0][0x114]=7;models[0][0x115]=99;models[0][0x116]=88;models[0][0x117]=77;
        y.f.input_state[0x5092]=4;y.f.input_state[0x509E]=0x80;y.f.input_state[0x1AB]=0x40;y.f.input_state[0x86FF]=1;
        character_store(y.f.image.data(),0x124D918+0x18,std::uint16_t(0x23));character_store(y.f.image.data(),0xA07CC6,std::uint16_t(0x23));
        GripMenuReader reader{reinterpret_cast<std::uintptr_t>(models[0]),reinterpret_cast<std::uintptr_t>(models[1]),reinterpret_cast<std::uintptr_t>(y.f.input_state.data())};
        const auto draw_menu=[&](bool expected,bool primary_supported=true){
            for(unsigned i=0;i<3;++i)bank.menu.slot_ack[i].store(bank.menu.slot_token[i].load());
            ++view_frame;body_piece=0;body_palettes.clear();CharacterSkin::Callback(models[0],body_part);
            assert(bank.menu.prepared&&body_palettes.size()==8&&bank.fault.load()==0);
            assert(character_grip_enabled_read(bank,bank.menu,reader)==expected);
            const auto pose=y.rig.fit(expected);
            for(unsigned i=0;i<8;++i)for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)
                assert(std::fabs(body_palettes[i].m[r][c]-pose.core[grip_bones[i]].m[r][c])<2e-5);
            assert(near(bank.menu.visual[0].shift[0],pose.joint_shift[10])&&near(bank.menu.visual[0].shift[1],pose.joint_shift[16]));
            CharacterDraw::Callback(models[0],body_part,nullptr,0);assert(drawn_head==bank.menu.parts[0]);
            CharacterDraw::Callback(models[1],y.weapon_part,nullptr,0);assert((drawn_head!=y.weapon_part)==primary_supported);
        };
        for(unsigned mode:{7u,8u,9u,10u,11u,33u}){y.f.image[0xA07CC6+3]=std::uint8_t(mode);draw_menu(true);}
        y.f.image[0xA07CC6+3]=1;draw_menu(false);
        y.f.image[0xA07CC6+3]=7;character_store(y.f.image.data(),0xA07CC6+0x16,std::uint16_t(0x23));draw_menu(false); // Ambiguous table row.
        character_store(y.f.image.data(),0xA07CC6+0x16,std::uint16_t(0));character_store(y.f.image.data(),0x124D918+0x18,std::uint16_t(0xFF));draw_menu(false);
        // The preview has the same primary-only admission rule, while model3
        // remains independently drawable as a supported auxiliary model.
        character_store(y.f.image.data(),0x124D918+0x18,std::uint16_t(0x23));std::memcpy(models[3],models[1],sizeof(models[3]));
        character_store(y.f.image.data(),0x1272158,static_cast<void*>(models[3]));character_store(models[1],0x188,static_cast<void*>(y.extra));
        draw_menu(false,false);CharacterDraw::Callback(models[3],y.weapon_part,nullptr,0);assert(drawn_head!=y.weapon_part);
    }
    std::puts("actual body Skin, held Draw and fresh emitter share two-handed grip; native state fallbacks PASS");return 0;
}
