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
alignas(32) std::array<float,native_vertices*3> native_positions{},native_normals{};
bool skin_success=true,native_nonfinite=false;float native_base=0;
int blade_native_skin(void*,void*){
    ++skin_calls;for(unsigned i=0;i<native_positions.size();++i){native_positions[i]=float(i+1)+native_base;native_normals[i]=float(i%3==1);}
    if(native_nonfinite)native_normals[17]=std::numeric_limits<float>::quiet_NaN();
    return skin_success?1:0;
}
void blade_native_draw(void* actor,void* chain,const float*,unsigned){
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
int main(){
    {
        // The ordinary snapshot remains native-skinned, so only its owned
        // descriptor follows the shorter donor's visual grip, even with M
        // rotated. Native geometry/model matrices remain byte-identical.
        AttachedFixture y;auto& b=character_banks[0];
        re4::CharacterBindBone source[26]{},donor[26]{};
        for(unsigned i=0;i<26;++i){source[i].id=donor[i].id=std::uint8_t(i);source[i].parent=donor[i].parent=re4::character_core_parents[i];}
        source[0].position={0,1000,0};donor[0].position={0,700,0};
        assert(re4::character_retarget_map(source,26,donor,26,b.map));
        const re4::ViewMatrix3x4 m{{{0,-1,0,13},{1,0,0,7},{0,0,1,-9}}};character_store(y.blade,0x74,m);
        const auto native_before=std::vector<std::uint8_t>(y.blade,y.blade+sizeof(y.blade));
        y.submit();assert(drawn_parts.size()==owned_count+1);
        const auto corrected=character_value<re4::ViewMatrix3x4>(drawn_parts.back(),0x74);
        assert(std::fabs(corrected.m[1][3]+293)<.0001f);
        assert(std::memcmp(native_before.data(),y.blade,native_before.size())==0);
    }
    {
        // A hidden optional blade with unavailable native arrays must not keep
        // the already skinned donor body at WaitingSkin/Leon.
        AttachedFixture y;auto& b=character_banks[0];b.token.store(42);
        settings.character_appearance=character_desired=b.appearance;
        character_store(y.blade,8,1u);character_store(y.blade,0xB0,static_cast<void*>(nullptr));
        character_store(y.blade,0xC8,static_cast<void*>(nullptr));
        y.submit();assert(drawn_head==b.parts[0]&&drawn_parts.size()==owned_count);
        character_appearance_update(y.f.actor);
        assert(control.appearance_status==unsigned(re4::CharacterAppearanceStatus::Active)&&control.appearance_error==0);
    }
    {
        AttachedFixture y;auto& b=character_banks[0];b.token.store(42);
        settings.character_appearance=character_desired=b.appearance;
        character_store(y.blade,8,1u);native_nonfinite=true;
        std::memset(b.deformed[0]+b.ready.deformed_slot_bytes,0xCC,3968);
        y.submit();assert(drawn_head==b.parts[0]&&drawn_parts.size()==owned_count);
        for(unsigned i=0;i<3968;++i)assert(b.deformed[0][b.ready.deformed_slot_bytes+i]==0xCC);
        character_appearance_update(y.f.actor);assert(control.appearance_status==unsigned(re4::CharacterAppearanceStatus::Active));
    }
    {
        AttachedFixture y;auto& b=character_banks[0];b.appearance=re4::CharacterAppearance::Krauser;b.token.store(42);
        settings.character_appearance=character_desired=b.appearance;
        character_store(y.blade,8,1u);character_store(y.face,0x58,reinterpret_cast<void*>(0xDEAD0000));
        y.submit();assert(drawn_head==b.parts[0]&&drawn_parts.size()==owned_count);
        character_appearance_update(y.f.actor);assert(control.appearance_status==unsigned(re4::CharacterAppearanceStatus::Active));
    }
    for(unsigned bad_body=0;bad_body<3;++bad_body){
        AttachedFixture y;auto& b=character_banks[0];b.token.store(42);
        if(bad_body==0)y.f.actor[0x118]=1;
        if(bad_body==1)y.f.actor[0x11A]=118;
        if(bad_body==2)character_store(y.f.actor,0x188,static_cast<void*>(y.extra));
        y.submit();assert(drawn_head==y.body&&b.drawn.load()!=b.token.load());
    }
    AttachedFixture x;auto& bank=character_banks[0];
    const auto actor_before=std::vector<std::uint8_t>(x.f.actor,x.f.actor+sizeof(x.f.actor));
    const auto blade_before=std::vector<std::uint8_t>(x.blade,x.blade+sizeof(x.blade));
    x.submit();assert(skin_calls==1&&draw_calls==1&&effect_calls==1&&drawn_head==bank.parts[0]);
    assert(drawn_parts.size()==owned_count+1); // Native visible blade was omitted here before the fix.
    auto* owned_blade=drawn_parts.back();assert(owned_blade!=x.blade&&owned_blade!=x.hands);
    assert(character_value<void*>(owned_blade,0x18)==x.archive+0x200&&character_value<void*>(owned_blade,0x28)==nullptr);
    assert((character_value<unsigned>(owned_blade,8)&8)&&character_value<std::uint8_t>(owned_blade,0xA7)==255);
    auto* copied=character_value<const float*>(owned_blade,0xB0);assert(copied!=native_positions.data()&&copied[0]==1&&copied[488]==489);
    native_positions.fill(-1000);assert(copied[0]==1&&copied[488]==489); // Delayed worker uses slot-owned bytes.
    assert(std::memcmp(actor_before.data(),x.f.actor,actor_before.size())==0&&std::memcmp(blade_before.data(),x.blade,blade_before.size())==0);
    assert(character_value<void*>(bank.parts[owned_count-1],0x28)==nullptr);
    {
        // Pending worker commands keep both the descriptor and slot geometry immutable.
        const auto descriptor=std::vector<std::uint8_t>(static_cast<std::uint8_t*>(owned_blade),static_cast<std::uint8_t*>(owned_blade)+0x1A8);
        native_base=10000;CharacterSkin::Callback(x.f.actor,x.body);
        assert(copied[0]==1&&std::memcmp(descriptor.data(),owned_blade,descriptor.size())==0);
        CharacterDraw::Callback(x.f.actor,x.body,nullptr,0);assert(drawn_head==x.body);
        alignas(16) static std::uint8_t command[0x20]{};character_frame_probe=nullptr;
        character_native.command=+[](std::uint64_t bytes)->void*{assert(bytes==sizeof(command));std::memset(command,0,sizeof(command));character_store(command,8,std::uint16_t(bytes));return command;};
        CharacterFrameEnd::Callback(nullptr,0);character_value<void(*)(void*)>(command,0)(command);
        assert(bank.slot_ack[0].load()==bank.slot_token[0].load());
        view_frame.fetch_add(1);x.submit();assert(drawn_head==bank.parts[0]&&character_value<const float*>(drawn_parts.back(),0xB0)[0]==10001);
        const auto slot0=std::vector<std::uint8_t>(bank.deformed[0],bank.deformed[0]+owned_count*64+3968);
        character_store(x.f.input_state.data(),0x2C,1u);native_base=20000;x.submit();
        assert(drawn_head==bank.parts[0]&&character_value<const float*>(drawn_parts.back(),0xB8)[0]==20001);
        assert(std::memcmp(slot0.data(),bank.deformed[0],slot0.size())==0);
    }
    for(unsigned bad=0;bad<20;++bad){
        AttachedFixture y;auto& b=character_banks[0];b.token.store(42);
        settings.character_appearance=character_desired=b.appearance;
        switch(bad){
        case 0:character_store(y.f.actor,0,main_base+0xB09A58);break;
        case 1:character_store(y.blade,0,main_base+0xB02080);break;
        case 2:character_store(y.blade,0x18,static_cast<void*>(y.archive+0x100));break;
        case 3:character_store(y.blade,0x20,static_cast<void*>(y.archive+0x410));break;
        case 4:character_store(y.extra,0x28,static_cast<void*>(y.hands));break;
        case 5:character_store(y.hands,0x28,static_cast<void*>(y.body));break;
        case 6:y.archive[0x229]=118;break;
        case 7:character_store(y.archive,0x258,std::uint16_t(162));break;
        case 8:character_store(y.blade,8,11u);break;
        case 9:character_store(y.blade,0x148,std::uint16_t(1));break;
        case 10:character_store(y.blade,0xB0,static_cast<void*>(nullptr));break;
        case 11:b.capacity=b.ready.deformed_slot_bytes+3967;break;
        case 12:++b.ready.deformed_slot_bytes;break;
        case 13:native_nonfinite=true;break;
        case 14:character_store(y.archive,0x34,0xFFFFFFF0u);break;
        case 15:{auto invalid=identity_matrix;invalid.m[1][2]=std::numeric_limits<float>::infinity();character_store(y.blade,0x74,invalid);break;}
        case 16:character_store(y.archive,0x238,0xA0000300u);break; // Still raw offsets.
        case 17:character_store(y.archive,0x238,0xA0100300u);break; // Partial initialization.
        case 18:character_store(y.archive,0x238,0xA0200300u);break;
        case 19:character_store(y.archive,0x238,0xA0300301u);break; // Unrelated format bit.
        }
        y.submit();assert(drawn_head==b.parts[0]&&drawn_parts.size()==owned_count&&draw_calls==1&&skin_calls==1&&b.phase.load()==CharacterPhase::Active);
        assert(character_value<void*>(b.parts[owned_count-1],0x28)==nullptr);
        character_appearance_update(y.f.actor);
        assert(control.appearance_status==unsigned(re4::CharacterAppearanceStatus::Active)&&control.appearance_error==0);
    }
    {
        AttachedFixture y;character_store(y.blade,8,1u);character_store(y.blade,0xA7,std::uint8_t(37));y.submit();
        assert(drawn_parts.size()==owned_count&&character_value<unsigned>(y.blade,8)==1&&character_value<std::uint8_t>(y.blade,0xA7)==37);
    }
    {
        AttachedFixture y;auto& b=character_banks[0];character_store(y.face,0x58,static_cast<void*>(nullptr));y.submit();
        assert(drawn_head==b.parts[0]&&drawn_parts.size()==owned_count); // No blade means no extra suffix gate.
    }
    {
        AttachedFixture y;CharacterSkin::Callback(y.f.actor,y.body);character_store(y.blade,8,1u);
        CharacterDraw::Callback(y.f.actor,y.body,nullptr,0);assert(drawn_head==character_banks[0].parts[0]&&drawn_parts.size()==owned_count); // Changed optional source is omitted; body stays ready.
    }
    {
        AttachedFixture y;skin_success=false;y.submit();assert(drawn_head==y.body&&skin_calls==1&&draw_calls==1);
    }
    {
        AttachedFixture y;auto& b=character_banks[0];b.capacity=b.ready.deformed_slot_bytes+3968;y.submit();
        assert(drawn_head==b.parts[0]); // Exact boundary admitted; one byte less rejected above.
    }
    {
        AttachedFixture y;auto& b=character_banks[0];b.appearance=re4::CharacterAppearance::Krauser;
        alignas(32) std::uint8_t own_bin[0x98]{},own_tpl[0x10]{};re4::ViewVec3 own_vertex{-19,5,14};
        character_store(own_bin,0x48,static_cast<void*>(&own_vertex));character_store(own_bin,0x50,static_cast<void*>(&y.normal));
        constexpr unsigned knife=3;auto& piece=b.ready.pieces[knife];piece.role=re4::CharacterAssetRole::Knife;piece.bin={own_bin,sizeof(own_bin)};piece.tpl={own_tpl,sizeof(own_tpl)};
        character_store(b.parts[knife],0x18,static_cast<void*>(own_bin));character_store(b.parts[knife],0x20,static_cast<void*>(own_tpl));
        character_store(b.parts[knife-1],0x28,static_cast<void*>(b.parts[knife+1]));character_store(b.parts[knife],0x28,static_cast<void*>(nullptr));
        const re4::ViewMatrix3x4 attached_matrix{{{0,-1,0,13},{1,0,0,7},{0,0,1,-9}}};character_store(y.blade,0x74,attached_matrix);
        re4::CharacterBindBone source[26]{},donor[26]{};
        for(unsigned i=0;i<26;++i){source[i].id=donor[i].id=std::uint8_t(i);source[i].parent=donor[i].parent=re4::character_core_parents[i];}
        source[0].position={0,1000,0};donor[0].position={0,700,0};
        assert(re4::character_retarget_map(source,26,donor,26,b.map));
        b.knife_orientation={{0,700,0},{1,0,0}};b.knife_orientation_valid=true;
        b.capacity=b.ready.deformed_slot_bytes; // Krauser needs no ordinary blade copy tail.
        y.submit();assert(drawn_head==b.parts[0]&&drawn_parts.size()==owned_count);
        auto* selected=drawn_parts.back();assert(selected!=b.parts[knife]&&character_value<void*>(selected,0x18)==own_bin&&character_value<void*>(selected,0x20)==own_tpl);
        const auto expected=character_value<re4::ViewMatrix3x4>(selected,0x74);
        re4::ViewVec3 actual,flipped,reference;
        assert(re4::view_transform_point(b.visual[0].knife_half_turn,own_vertex,flipped));
        flipped.y+=300;assert(re4::view_transform_point(attached_matrix,flipped,reference));reference.y-=300;
        assert(re4::view_transform_point(expected,own_vertex,actual));
        assert(std::fabs(actual.x-reference.x)<.0001f&&std::fabs(actual.y-reference.y)<.0001f&&std::fabs(actual.z-reference.z)<.0001f);
        assert(character_value<const float*>(selected,0xB0)[0]==-19);
        character_store(b.parts[knife],0x74,identity_matrix); // Independent actor path cannot change attached snapshot.
        assert(std::memcmp(static_cast<std::uint8_t*>(selected)+0x74,&expected,sizeof(expected))==0);
        assert(character_value<void*>(b.parts[owned_count-1],0x28)==nullptr);
        // An invalid half-turn suppresses this optional blade only. The full
        // natural body still draws and can publish Active/error0.
        b.slot_ack[0].store(b.slot_token[0].load());b.knife_orientation_valid=false;
        settings.character_appearance=character_desired=b.appearance;b.token.store(42);
        y.submit();assert(drawn_head==b.parts[0]&&drawn_parts.size()==owned_count-1);
        character_appearance_update(y.f.actor);assert(control.appearance_status==unsigned(re4::CharacterAppearanceStatus::Active)&&!control.appearance_error);
    }

}


