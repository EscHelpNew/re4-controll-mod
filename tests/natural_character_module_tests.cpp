#define main game_module_baseline_main
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#include "game_module_tests.cpp"
#pragma GCC diagnostic pop
#undef main
namespace {
constexpr re4::ViewMatrix3x4 identity_matrix{{{1,0,0,0},{0,1,0,0},{0,0,1,0}}};
constexpr unsigned native_vertices=163,owned_count=7;
re4::CharacterNativeMatrix palette_blade[255]{};
void* drawn_head=nullptr;std::vector<void*> drawn_parts;unsigned skin_calls=0,draw_calls=0,effect_calls=0;
re4::ViewMatrix3x4 last_draw_view{};bool draw_view_valid=false;
alignas(32) std::array<float,native_vertices*3> native_positions{},native_normals{};
bool skin_success=true,native_nonfinite=false;float native_base=0;
int blade_native_skin(void*,void*){
    ++skin_calls;for(unsigned i=0;i<native_positions.size();++i){native_positions[i]=float(i+1)+native_base;native_normals[i]=float(i%3==1);}
    if(native_nonfinite)native_normals[17]=std::numeric_limits<float>::quiet_NaN();
    return skin_success?1:0;
}
void blade_native_draw(void* actor,void* chain,const float* view,unsigned){
    draw_view_valid=view!=nullptr;if(view)std::memcpy(&last_draw_view,view,sizeof(last_draw_view));
    if(character_value<unsigned>(actor,8)&0x10){assert(character_value<std::uintptr_t>(actor,0x3A0)==main_base+0xAF4798);++effect_calls;}
    ++draw_calls;drawn_head=chain;drawn_parts.clear();
    for(void* p=chain;p;p=character_value<void*>(p,0x28)){if(std::find(drawn_parts.begin(),drawn_parts.end(),p)!=drawn_parts.end())break;assert(drawn_parts.size()<32);drawn_parts.push_back(p);}
}
void blade_deform(void* out,const void* in,const void*,unsigned count){assert(count==1);std::memcpy(out,in,12);}
struct AttachedFixture {
    Fixture f;
    alignas(32) std::uint8_t archive[0x600]{},body[0x1A8]{},extra[0x1A8]{},blade[0x1A8]{},hands[0x1A8]{},face[0x70]{};
    alignas(32) std::uint8_t donor_bin[0x98]{};
    re4::CharacterWeight8 weight{{0,0,0},1,{100,0,0},0};std::uint16_t wi=0;
    re4::ViewVec3 position{1,2,3},normal{0,1,0};
    AttachedFixture(){
        character_appearance_reset();f.view_mode();character_renderer_ready.store(true);skin_calls=draw_calls=effect_calls=0;drawn_head=nullptr;drawn_parts.clear();skin_success=true;native_nonfinite=false;native_base=0;
        character_store(f.actor,8,0x10u);character_store(f.actor,0x3A0,main_base+0xAF4798);
        character_store(f.image.data(),0xAF4798,6u);character_store(f.image.data(),0xAF47A0,main_base+0xAF4768);
        character_store(f.actor,0,main_base+0xB09A60);f.actor[0x11A]=119;
        character_store(f.actor,0x180,static_cast<void*>(body));character_store(f.actor,0x108,static_cast<void*>(archive+0x100));
        character_store(f.actor,0x950,static_cast<void*>(face));character_store(face,0x58,static_cast<void*>(blade));
        character_store(face,0x38,static_cast<void*>(hands));character_store(face,0x40,static_cast<void*>(hands)); // Alias is irrelevant.
        character_store(f.input_state.data(),0x70,static_cast<void*>(archive));
        character_store(archive,0,16u);character_store(archive,0x10,0x100u);character_store(archive,0x14,0x400u);character_store(archive,0x34,0x200u);
        archive[0x129]=archive[0x229]=119;archive[0x228]=1;character_store(archive,0x242,std::uint16_t(1));
        character_store(archive,0x258,std::uint16_t(native_vertices));character_store(archive,0x25A,std::uint16_t(native_vertices));
        // Live BIN9 after native2B4540 relocation, as observed on the console.
        character_store(archive,0x238,0xA0300300u);character_store(archive,0x404,2u);
        character_store(body,0x18,static_cast<void*>(archive+0x100));character_store(body,0x28,static_cast<void*>(extra));
        character_store(extra,0x28,static_cast<void*>(blade));character_store(blade,0x28,static_cast<void*>(hands));
        character_store(blade,0x18,static_cast<void*>(archive+0x200));character_store(blade,0x20,static_cast<void*>(archive+0x400));
        character_store(blade,0,main_base+0xB02088);character_store(blade,8,9u);character_store(blade,0xA7,std::uint8_t(255));character_store(blade,0x74,identity_matrix);
        for(unsigned slot=0;slot<3;++slot){character_store(blade,0xB0+slot*8,native_positions.data());character_store(blade,0xC8+slot*8,native_normals.data());}
        for(auto range:std::vector<MemoryInfo>{{main_base,f.image.size(),1},{reinterpret_cast<std::uintptr_t>(f.actor),sizeof(f.actor),1},
            {reinterpret_cast<std::uintptr_t>(f.input_state.data()),f.input_state.size(),1},{reinterpret_cast<std::uintptr_t>(archive),sizeof(archive),1},
            {reinterpret_cast<std::uintptr_t>(body),sizeof(body),1},{reinterpret_cast<std::uintptr_t>(extra),sizeof(extra),1},
            {reinterpret_cast<std::uintptr_t>(blade),sizeof(blade),1},{reinterpret_cast<std::uintptr_t>(hands),sizeof(hands),1},
            {reinterpret_cast<std::uintptr_t>(face),sizeof(face),1},{reinterpret_cast<std::uintptr_t>(native_positions.data()),sizeof(native_positions),1},
            {reinterpret_cast<std::uintptr_t>(native_normals.data()),sizeof(native_normals),1}})game_test::memory_maps.push_back(range);
        auto& bank=character_banks[0];assert(character_identity(f.actor,bank.identity));bank.appearance=re4::CharacterAppearance::Ashley;bank.phase.store(CharacterPhase::Active);
        bank.prepared=true;bank.capacity=character_deformed_slot;bank.ready={};static const re4::CharacterRetargetMap empty_map{};bank.map=empty_map;bank.ready.piece_count=owned_count;bank.ready.deformed_slot_bytes=owned_count*64;
        character_store(donor_bin,0x48,static_cast<void*>(&position));character_store(donor_bin,0x50,static_cast<void*>(&normal));
        for(unsigned i=0;i<owned_count;++i){std::memset(bank.parts[i],0,0x1A8);auto& piece=bank.ready.pieces[i];piece.bin={donor_bin,sizeof(donor_bin)};piece.weights={reinterpret_cast<std::uint8_t*>(&weight),sizeof(weight)};
            piece.weight_count=piece.vertex_count=piece.normal_count=1;piece.position_weights=piece.normal_weights={reinterpret_cast<std::uint8_t*>(&wi),sizeof(wi)};
            character_store(bank.parts[i],8,9u);character_store(bank.parts[i],0x18,static_cast<void*>(donor_bin));
            character_store(bank.parts[i],0x28,i+1<owned_count?static_cast<void*>(bank.parts[i+1]):nullptr);
            for(unsigned slot=0;slot<3;++slot){character_store(bank.parts[i],0xB0+slot*8,static_cast<void*>(bank.deformed[slot]+i*64));character_store(bank.parts[i],0xC8+slot*8,static_cast<void*>(bank.deformed[slot]+i*64+32));}
        }
        bank.map.count=26;for(unsigned i=0;i<26;++i)bank.map.bones[i].source=std::uint8_t(i);
        for(unsigned i=0;i<119;++i)character_store(f.input_state.data(),0x1CC+i*sizeof(identity_matrix),identity_matrix);
        character_native.output=+[]()->void*{return palette_blade;};character_native.positions=character_native.normals=blade_deform;
        character_skin_probe=blade_native_skin;character_draw_probe=blade_native_draw;
    }
    void submit(){assert(CharacterSkin::Callback(f.actor,body)==int(skin_success));CharacterDraw::Callback(f.actor,body,nullptr,0);}
};
}

namespace {
struct HeldFixture:AttachedFixture {
    alignas(32) std::uint8_t player_bones[26][0x1F0]{},weapon[0x450]{},weapon_bones[2][0x1F0]{},owner[0x48]{},weapon_part[0x1A8]{},weapon_archive[0x500]{};
    re4::ViewMatrix3x4 base{{{0,-1,0,17},{1,0,0,31},{0,0,1,-9}}};
    re4::ViewMatrix3x4 part{{{0,0,1,13},{0,1,0,7},{-1,0,0,-9}}};
    HeldFixture(unsigned handle=0,unsigned parent=10){
        auto& bank=character_banks[0];character_store(face,0x58,static_cast<void*>(nullptr));
        re4::CharacterBindBone source[26]{},donor[26]{};
        for(unsigned i=0;i<26;++i){
            source[i].id=donor[i].id=std::uint8_t(i);source[i].parent=donor[i].parent=re4::character_core_parents[i];
            character_store(player_bones[i],0x18,identity_matrix);
            if(i<25)character_store(player_bones[i],0x108,static_cast<void*>(player_bones[i+1]));
        }
        source[0].position={0,1000,0};donor[0].position={0,700,0};
        assert(re4::character_retarget_map(source,26,donor,26,bank.map));
        character_store(f.actor,0x108,static_cast<void*>(player_bones));character_store(f.actor,0x938,static_cast<void*>(owner));
        character_store(owner,0x38+handle*8,static_cast<void*>(weapon));
        character_store(weapon,0,main_base+0xB0A668);character_store(f.image.data(),0xB0A668+0x28,main_base+0x2E8590);
        character_store(f.image.data(),0xB0A668+0x58,main_base+0x33FFC0); // Real handgun fire virtual.
        character_store(f.image.data(),0xB0A668+0x98,main_base+0x2E8D80);
        weapon[0x118]=0x33;weapon[0x11A]=2;
        character_store(weapon,0x108,static_cast<void*>(weapon_bones));character_store(weapon,0x180,static_cast<void*>(weapon_part));
        character_store(weapon,0x3F8,static_cast<void*>(f.actor));
        character_store(weapon_bones[0],0x18,identity_matrix);character_store(weapon_bones[0],0x78,static_cast<void*>(player_bones[parent]));
        character_store(weapon_bones[0],0x108,static_cast<void*>(weapon_bones[1]));character_store(weapon_bones[1],0x18,base);
        character_store(f.input_state.data(),0x58,static_cast<void*>(weapon_archive));
        character_store(weapon_archive,0,3u);character_store(weapon_archive,0x10,0x100u);character_store(weapon_archive,0x14,0x300u);character_store(weapon_archive,0x18,0x400u);
        character_store(weapon_archive,0x100,static_cast<void*>(weapon_archive+0x400));weapon_archive[0x129]=1;
        character_store(weapon_archive,0x138,0xA0300300u);character_store(weapon_archive,0x158,std::uint16_t(1));character_store(weapon_archive,0x15A,std::uint16_t(1));
        character_store(weapon_archive,0x148,static_cast<void*>(weapon_archive+0x420));character_store(weapon_archive,0x150,static_cast<void*>(weapon_archive+0x440));
        weapon_archive[0x400]=1;
        character_store(weapon_part,0,main_base+0xB02088);character_store(weapon_part,8,9u);character_store(weapon_part,0x18,static_cast<void*>(weapon_archive+0x100));
        character_store(weapon_part,0x20,static_cast<void*>(weapon_archive+0x300));character_store(weapon_part,0x74,part);
        for(auto range:std::vector<MemoryInfo>{{reinterpret_cast<std::uintptr_t>(player_bones),sizeof(player_bones),1},
            {reinterpret_cast<std::uintptr_t>(weapon),sizeof(weapon),1},{reinterpret_cast<std::uintptr_t>(weapon_bones),sizeof(weapon_bones),1},
            {reinterpret_cast<std::uintptr_t>(owner),sizeof(owner),1},{reinterpret_cast<std::uintptr_t>(weapon_part),sizeof(weapon_part),1},
            {reinterpret_cast<std::uintptr_t>(weapon_archive),sizeof(weapon_archive),1}})game_test::memory_maps.push_back(range);
        assert(character_identity(f.actor,bank.identity));
    }
    void draw_weapon(){CharacterDraw::Callback(weapon,weapon_part,nullptr,0);}
};
struct WeightedFixture:HeldFixture {
    alignas(32) re4::ViewVec3 positions[2]{{-3,7,11},{5,-13,17}},normals[2]{{0,1,0},{0,1,0}};
    WeightedFixture(unsigned handle=0,unsigned parent=10):HeldFixture(handle,parent){
        game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(positions),sizeof(positions),1});game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(normals),sizeof(normals),1});
        weapon_archive[0x128]=2;weapon_archive[0x129]=2;character_store(weapon_archive,0x142,std::uint16_t(2));
        character_store(weapon_archive,0x158,std::uint16_t(2));character_store(weapon_archive,0x15A,std::uint16_t(2));
        character_store(weapon_archive,0x120,static_cast<void*>(weapon_archive+0x380));
        character_store(weapon_archive,0x170,static_cast<void*>(weapon_archive+0x460));character_store(weapon_archive,0x178,static_cast<void*>(weapon_archive+0x470));
        const re4::CharacterWeight8 weights[2]{{{0,0,0},1,{100,0,0},0},{{1,0,0},1,{100,0,0},0}};
        std::memcpy(weapon_archive+0x380,weights,sizeof(weights));weapon_archive[0x400]=0;weapon_archive[0x401]=255;weapon_archive[0x410]=1;
        character_store(weapon_archive,0x462,std::uint16_t(1));character_store(weapon_archive,0x472,std::uint16_t(1));
        for(unsigned slot=0;slot<3;++slot){character_store(weapon_part,0xB0+slot*8,static_cast<void*>(positions));character_store(weapon_part,0xC8+slot*8,static_cast<void*>(normals));}
    }
};

}
#ifndef RE4_NATURAL_FIXTURE_ONLY
int main(){
    for(unsigned handle=0;handle<2;++handle)for(unsigned parent:{10u,16u,17u}){
        HeldFixture y(handle,parent);auto& b=character_banks[0];
        const auto actor=std::vector<std::uint8_t>(y.weapon,y.weapon+sizeof(y.weapon));
        const auto native=std::vector<std::uint8_t>(y.weapon_part,y.weapon_part+sizeof(y.weapon_part));
        const auto bones=std::vector<std::uint8_t>(reinterpret_cast<std::uint8_t*>(y.weapon_bones),reinterpret_cast<std::uint8_t*>(y.weapon_bones)+sizeof(y.weapon_bones));
        y.submit();const auto body_token=b.slot_token[0].load();
        // Another native skin operation reuses the palette before this draw.
        auto poisoned=identity_matrix;poisoned.m[1][3]=99999;for(unsigned i=0;i<119;++i)character_store(y.f.input_state.data(),0x1CC+i*sizeof(poisoned),poisoned);
        y.draw_weapon();assert(drawn_head!=y.weapon_part&&drawn_parts.size()==1);
        const auto corrected=character_value<re4::ViewMatrix3x4>(drawn_head,0x74);
        re4::ViewVec3 original,actual,local;const re4::ViewVec3 sample{7,-3,9};
        assert(re4::view_transform_point(y.part,sample,local)&&re4::view_transform_point(y.base,local,original));
        assert(re4::view_transform_point(corrected,sample,local)&&re4::view_transform_point(y.base,local,actual));
        assert(std::fabs(actual.x-original.x)<.0001f&&std::fabs(actual.y-original.y+300)<.0001f&&std::fabs(actual.z-original.z)<.0001f);
        assert(b.slot_token[0].load()>body_token&&b.slot_ack[0].load()<b.slot_token[0].load());
        assert(!std::memcmp(actor.data(),y.weapon,actor.size())&&!std::memcmp(native.data(),y.weapon_part,native.size())&&!std::memcmp(bones.data(),y.weapon_bones,bones.size()));
        const auto owned=std::vector<std::uint8_t>(static_cast<std::uint8_t*>(drawn_head),static_cast<std::uint8_t*>(drawn_head)+0x1A8);
        auto* prior=drawn_head;CharacterSkin::Callback(y.f.actor,y.body);y.draw_weapon();assert(drawn_head==y.weapon_part);
        assert(!std::memcmp(owned.data(),prior,owned.size())); // Pending consumer remains immutable.
    }
    for(bool weapon_first:{false,true}){
        // Menu models are exactly3C8 bytes and have independent pose/descriptor
        // slots. Their root-parent link proves ownership without actor3F8.
        WeightedFixture y;auto& b=character_banks[0];y.submit();CharacterSkin::Callback(y.weapon,y.weapon_part);y.draw_weapon();auto* gameplay=drawn_head;
        assert(gameplay!=y.weapon_part);const auto* gameplay_geometry=character_value<const re4::ViewVec3*>(gameplay,0xB0);
        const auto gameplay_before=std::vector<std::uint8_t>(static_cast<std::uint8_t*>(gameplay),static_cast<std::uint8_t*>(gameplay)+0x1A8);
        alignas(32) std::uint8_t models[4][0x3C8]{},bones[26][0x1F0]{},binds[119*16]{},body_part[0x1A8]{},preview_archive[0x500]{};
        std::memcpy(preview_archive,y.weapon_archive,sizeof(preview_archive));
        for(const auto field:{0u,0x20u,0x48u,0x50u,0x70u,0x78u}){
            const auto prior=character_value<std::uintptr_t>(preview_archive+0x100,field);
            character_store(preview_archive+0x100,field,reinterpret_cast<std::uintptr_t>(preview_archive)+(prior-reinterpret_cast<std::uintptr_t>(y.weapon_archive)));
        }
        character_store(y.weapon_part,0x18,static_cast<void*>(preview_archive+0x100));character_store(y.weapon_part,0x20,static_cast<void*>(preview_archive+0x300));
        character_store(y.f.image.data(),0x1270F70+0x258,static_cast<void*>(preview_archive));
        game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(preview_archive),sizeof(preview_archive),1});
        re4::CharacterBindBone donor[26]{};
        for(unsigned i=0;i<119;++i){
            binds[i*16]=std::uint8_t(i);binds[i*16+1]=i<26?re4::character_core_parents[i]:0;
            const re4::ViewVec3 local=i?re4::ViewVec3{}:re4::ViewVec3{0,1000,0};std::memcpy(binds+i*16+4,&local,12);
            if(i<26){donor[i]={std::uint8_t(i),re4::character_core_parents[i],i?re4::ViewVec3{}:re4::ViewVec3{0,700,0}};
                character_store(bones[i],0x18,identity_matrix);if(i<25)character_store(bones[i],0x108,static_cast<void*>(bones[i+1]));}
        }
        b.ready.body_bone_count=26;std::copy_n(donor,26,b.ready.body_bind.data());
        character_store(y.archive,0x100,static_cast<void*>(binds));
        character_store(body_part,0x18,static_cast<void*>(y.archive+0x100));
        character_store(models[0],0,main_base+0xAFF538);character_store(models[0],8,1u);models[0][0x11A]=119;
        character_store(models[0],0x108,static_cast<void*>(bones));character_store(models[0],0x180,static_cast<void*>(body_part));
        character_store(models[1],0,main_base+0xAFF538);character_store(models[1],8,1u);models[1][0x11A]=2;
        character_store(models[1],0x108,static_cast<void*>(y.weapon_bones));character_store(models[1],0x180,static_cast<void*>(y.weapon_part));
        character_store(y.weapon_bones[0],0x78,static_cast<void*>(bones[10]));
        character_store(y.f.image.data(),0xAFF4A8+8,static_cast<void*>(models));character_store(y.f.image.data(),0xAFF4A8+0x10,4u);character_store(y.f.image.data(),0xAFF4A8+0x14,0x3C8u);
        character_store(y.f.image.data(),0x1272140,static_cast<void*>(models[0]));character_store(y.f.image.data(),0x1272148,static_cast<void*>(models[1]));
        for(auto range:std::vector<MemoryInfo>{{reinterpret_cast<std::uintptr_t>(models),sizeof(models),1},{reinterpret_cast<std::uintptr_t>(bones),sizeof(bones),1},
            {reinterpret_cast<std::uintptr_t>(binds),sizeof(binds),1},{reinterpret_cast<std::uintptr_t>(body_part),sizeof(body_part),1}})game_test::memory_maps.push_back(range);
        b.menu.deformed=reinterpret_cast<std::uint8_t (*)[character_deformed_slot]>(b.raw+2*1024*1024);b.menu.capacity=character_deformed_slot;
        character_native.initialize=+[](void*,void*,unsigned){return true;};
        character_native.attach=+[](void* p,void* bin,void* tpl){character_store(p,0x18,bin);character_store(p,0x20,tpl);character_store(p,8,8u);};
        for(unsigned i=0;i<119;++i)character_store(y.f.input_state.data(),0x1CC+i*sizeof(identity_matrix),identity_matrix);
        if(weapon_first)CharacterSkin::Callback(models[1],y.weapon_part);
        CharacterSkin::Callback(models[0],body_part);assert(b.menu.prepared);
        if(!weapon_first)CharacterSkin::Callback(models[1],y.weapon_part);
        const re4::ViewMatrix3x4 menu_view{{{1,0,0,13},{0,0,-1,7},{0,1,0,-9}}};
        const auto view_before=menu_view;
        CharacterDraw::Callback(models[0],body_part,&menu_view.m[0][0],0);assert(drawn_head==b.menu.parts[0]);
        assert(draw_view_valid&&std::fabs(last_draw_view.m[2][3]-291)<.0001f);
        CharacterDraw::Callback(models[1],y.weapon_part,&menu_view.m[0][0],0);assert(drawn_head!=y.weapon_part&&drawn_head!=gameplay);
        assert(draw_view_valid&&std::fabs(last_draw_view.m[2][3]-291)<.0001f&&!std::memcmp(&menu_view,&view_before,sizeof(menu_view)));
        assert(b.menu.slot_token[0].load()>b.menu.slot_ack[0].load());
        const auto* menu_geometry=character_value<const re4::ViewVec3*>(drawn_head,0xB0);
        assert(menu_geometry!=y.positions&&menu_geometry!=gameplay_geometry&&menu_geometry[0].x==-3);
        y.positions[0].x=9999;assert(menu_geometry[0].x==-3&&gameplay_geometry[0].x==-3);
        character_store(y.weapon_part,8,11u); // Native held fallback still shares the case lift.
        CharacterDraw::Callback(models[1],y.weapon_part,&menu_view.m[0][0],0);assert(drawn_head==y.weapon_part);
        assert(draw_view_valid&&std::fabs(last_draw_view.m[2][3]-291)<.0001f);
        character_store(y.weapon_part,8,9u);character_store(y.f.image.data(),0x1270F70+0x258,static_cast<void*>(nullptr));
        CharacterDraw::Callback(models[1],y.weapon_part,&menu_view.m[0][0],0);assert(drawn_head==y.weapon_part);
        assert(draw_view_valid&&std::fabs(last_draw_view.m[2][3]-291)<.0001f);
        assert(!std::memcmp(gameplay_before.data(),gameplay,gameplay_before.size()));
    }
    for(unsigned handle=0;handle<2;++handle)for(unsigned parent:{10u,16u,17u}){
        WeightedFixture y(handle,parent);auto& b=character_banks[0];
        alignas(32) re4::ViewVec3 previous[2]{{77,78,79},{80,81,82}};
        game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(previous),sizeof(previous),1});
        for(unsigned slot=0;slot<3;++slot)character_store(y.weapon_part,0xB0+slot*8,static_cast<void*>(previous));
        y.submit();assert(drawn_head==b.parts[0]);y.draw_weapon();assert(drawn_head==y.weapon_part); // No successful native weapon Skin yet.
        // Native4EC354/398 replaces current-slot output pointers on every
        // successful weapon skin, after an earlier body reservation.
        character_store(y.weapon_part,0xB0,static_cast<void*>(y.positions));
        assert(CharacterSkin::Callback(y.weapon,y.weapon_part)==1);y.draw_weapon();assert(drawn_head!=y.weapon_part);
        const auto* copied=character_value<const re4::ViewVec3*>(drawn_head,0xB0);assert(copied!=y.positions&&copied[0].x==-3&&copied[1].z==17);
        y.positions[0].x=99999;assert(copied[0].x==-3); // Worker consumes only owned immutable geometry.
        assert(b.slot_token[0].load()>b.slot_ack[0].load());
        assert(CharacterSkin::Callback(y.weapon,y.weapon_part)==1);y.draw_weapon();assert(character_value<const re4::ViewVec3*>(drawn_head,0xB0)[0].x==-3);
        b.slot_ack[0].store(b.slot_token[0].load());view_frame.fetch_add(1);
        y.submit();assert(CharacterSkin::Callback(y.weapon,y.weapon_part)==1);y.draw_weapon();
        assert(character_value<const re4::ViewVec3*>(drawn_head,0xB0)[0].x==99999);
    }
    for(unsigned family=0;family<2;++family)for(unsigned texture=0;texture<3;++texture){
        HeldFixture y;auto& b=character_banks[0];
        character_store(y.weapon,0,main_base+0xB0AC48);y.weapon[0x118]=0x3C;
        character_store(y.f.image.data(),0xB0AC48+0x28,main_base+0x2E8590);
        character_store(y.f.image.data(),0xB0AC48+0x58,main_base+0x2E94F0);
        character_store(y.f.image.data(),0xB0AC48+0x98,main_base+0x2E8D80);
        // Native3474D4..347574 obtains BIN102/TPL103 from state70.
        alignas(32) std::uint8_t body_resource[0x1000]{};
        character_store(body_resource,0,125u);character_store(body_resource,0x10,0x300u);character_store(body_resource,0x14,0x700u);
        character_store(body_resource,family?0x1F4:0x1A8,0x800u);
        character_store(body_resource,family?0x1F8+texture*4:0x1AC+texture*8,0xA00u);
        std::memcpy(body_resource+0x300,y.archive+0x100,0x98);std::memcpy(body_resource+0x800,y.weapon_archive+0x100,0x98);
        character_store(body_resource+0x800,0,static_cast<void*>(body_resource+0xB00));body_resource[0xB01]=255;
        character_store(body_resource+0x800,0x48,static_cast<void*>(body_resource+0xB20));character_store(body_resource+0x800,0x50,static_cast<void*>(body_resource+0xB40));
        character_store(y.f.input_state.data(),0x70,static_cast<void*>(body_resource));character_store(y.body,0x18,static_cast<void*>(body_resource+0x300));
        character_store(y.weapon_part,0x18,static_cast<void*>(body_resource+0x800));character_store(y.weapon_part,0x20,static_cast<void*>(body_resource+0xA00));
        y.weapon[0x11A]=1;
        game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(body_resource),sizeof(body_resource),1});
        assert(character_identity(y.f.actor,b.identity));
        y.submit();assert(drawn_head==b.parts[0]);y.draw_weapon();assert(drawn_head!=y.weapon_part);
        std::uintptr_t menu_origin=0;
        assert(character_held_archive(b,reinterpret_cast<std::uintptr_t>(y.weapon),true,menu_origin)
            &&menu_origin==reinterpret_cast<std::uintptr_t>(body_resource)); // Native case grenade/egg resource path.
        y.weapon[0x118]=0x33;y.draw_weapon();assert(drawn_head==y.weapon_part); // This archive is class-qualified.
    }
    for(unsigned bad=0;bad<10;++bad){
        WeightedFixture y;auto& b=character_banks[0];
        std::memset(b.held_geometry[0][0],0x5C,character_held_geometry_capacity);
        if(bad==0)y.positions[0].x=std::numeric_limits<float>::quiet_NaN();
        if(bad==1)y.normals[1].z=std::numeric_limits<float>::infinity();
        if(bad==5)character_store(y.weapon_part,0xB0,static_cast<void*>(nullptr));
        if(bad==6)character_store(y.weapon_archive,0x462,std::uint16_t(2));
        if(bad==7)y.weapon_archive[0x380]=2;
        if(bad==8){y.weapon_archive[0x383]=3;y.weapon_archive[0x384]=80;y.weapon_archive[0x385]=30;}
        y.submit();assert(drawn_head==b.parts[0]);
        if(bad==2)view_frame.fetch_add(1);
        if(bad==3)skin_success=false;
        if(bad==4){auto changed=y.part;changed.m[2][3]+=1;character_store(y.weapon_part,0x74,changed);}
        if(bad==9)character_store(y.f.input_state.data(),0x2C,1u);
        CharacterSkin::Callback(y.weapon,y.weapon_part);
        if(bad==9)character_store(y.f.input_state.data(),0x2C,0u);
        y.draw_weapon();assert(drawn_head==y.weapon_part);
        for(unsigned i=0;i<character_held_geometry_capacity;++i)assert(b.held_geometry[0][0][i]==0x5C);
        assert(b.phase.load()==CharacterPhase::Active); // Optional copy failure cannot retire the body.
    }
    {
        WeightedFixture y;auto& b=character_banks[0];
        constexpr unsigned count=5460; // Aligned position+normal arrays exceed the measured cap.
        std::vector<re4::ViewVec3> geometry(count,{1,2,3});std::vector<std::uint16_t> indices(count,0);
        game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(geometry.data()),geometry.size()*sizeof(geometry[0]),1});
        game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(indices.data()),indices.size()*2,1});
        character_store(y.weapon_archive,0x158,std::uint16_t(count));character_store(y.weapon_archive,0x15A,std::uint16_t(count));
        character_store(y.weapon_archive,0x148,geometry.data());character_store(y.weapon_archive,0x150,geometry.data());
        character_store(y.weapon_archive,0x170,indices.data());character_store(y.weapon_archive,0x178,indices.data());
        y.submit();assert(drawn_head==b.parts[0]);CharacterSkin::Callback(y.weapon,y.weapon_part);y.draw_weapon();assert(drawn_head==y.weapon_part);
    }
    {
        // A late invalid piece must not publish a partial geometry batch.
        WeightedFixture y;auto& b=character_banks[0];
        alignas(32) std::uint8_t second[0x1A8]{};
        alignas(32) re4::ViewVec3 bad_normals[2]{{0,1,0},{0,1,std::numeric_limits<float>::quiet_NaN()}};
        game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(second),sizeof(second),1});
        game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(bad_normals),sizeof(bad_normals),1});
        std::memcpy(second,y.weapon_part,sizeof(second));character_store(y.weapon_part,0x28,static_cast<void*>(second));
        std::memcpy(y.weapon_archive+0x200,y.weapon_archive+0x100,0x98);
        character_store(y.weapon_archive,0,4u);character_store(y.weapon_archive,0x1C,0x200u);
        character_store(second,0x18,static_cast<void*>(y.weapon_archive+0x200));character_store(second,0xC8,static_cast<void*>(bad_normals));
        std::memset(b.held_geometry[0][0],0x5C,character_held_geometry_capacity);
        y.submit();assert(drawn_head==b.parts[0]);CharacterSkin::Callback(y.weapon,y.weapon_part);y.draw_weapon();assert(drawn_head==y.weapon_part);
        for(unsigned i=0;i<character_held_geometry_capacity;++i)assert(b.held_geometry[0][0][i]==0x5C);
    }
    for(unsigned bad=0;bad<12;++bad){
        HeldFixture y;auto& b=character_banks[0];
        if(bad==0)y.weapon_archive[0x129]=2; // Weighted/multi-bone geometry is unproved.
        if(bad==1)character_store(y.weapon,0x188,static_cast<void*>(y.weapon_part));
        if(bad==2)character_store(y.weapon,8,0x4000u); // Alternate native matrix base.
        if(bad==3)character_store(y.weapon,0x3F8,static_cast<void*>(y.body));
        if(bad==4)character_store(y.weapon_bones[0],0x78,static_cast<void*>(y.player_bones[9]));
        if(bad==5)character_store(y.weapon_part,0x28,static_cast<void*>(y.weapon_part));
        if(bad==6)character_store(y.weapon_part,8,11u); // Morph/deformed buffers are not borrowed.
        if(bad==7)character_store(y.weapon_archive,0x138,0xA0000300u); // Not relocated.
        if(bad==8)y.weapon_archive[0x400]=2; // Invalid selected rigid bone.
        if(bad==9)character_store(y.weapon_part,0x148,std::uint16_t(1));
        if(bad==10)character_store(y.f.image.data(),0xB0A668+0x98,main_base+0x2E8D84); // Wrong common base method.
        if(bad==11){ // Firearm geometry cannot be admitted from a player DAT.
            std::memcpy(y.archive+0x480,y.weapon_archive+0x100,0x98);character_store(y.archive,0x38,0x480u);
            character_store(y.weapon_part,0x18,static_cast<void*>(y.archive+0x480));character_store(y.weapon_part,0x20,static_cast<void*>(y.archive+0x400));
        }
        y.submit();assert(drawn_head==b.parts[0]);y.draw_weapon();assert(drawn_head==y.weapon_part);
    }
    std::puts("natural character actual callbacks: natural rigs, rigid/weighted held copies, native immutability, fences PASS");
}


#endif
