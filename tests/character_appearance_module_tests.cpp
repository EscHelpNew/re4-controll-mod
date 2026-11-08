#define main game_module_baseline_main
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#include "game_module_tests.cpp"
#pragma GCC diagnostic pop
#undef main

namespace {
alignas(32) std::uint8_t command_buffer[0x28]{};
unsigned software_context=0,finish_calls=0,command_allocations=0;
unsigned texture_load_calls=0;
void texture_load(void*,unsigned,unsigned){++texture_load_calls;}
bool frame_end_seen=false;
void* command_alloc(std::uint64_t size){
    assert(frame_end_seen&&(size==0x20||size==0x28));++command_allocations;std::memset(command_buffer,0,sizeof(command_buffer));
    character_store(command_buffer,8,std::uint16_t(size));character_store(command_buffer,0xA,std::uint16_t(0));return command_buffer;
}
void* context_lookup(unsigned id){assert(id==0);return &software_context;}
void gpu_finish(void* gpu){assert(gpu==reinterpret_cast<void*>(0x12340000));++finish_calls;}
std::vector<void*> skin_chains,draw_chains;
std::vector<unsigned> freed_ids;
void* expected_actor=nullptr;
re4::CharacterNativeMatrix palette[255]{};
unsigned deformation_calls=0;
unsigned native_effect_draws=0;
void* native_palette(){return palette;}
void deform(void* out,const void* in,const void* indices,unsigned count){
    assert(count==1&&indices);assert(palette[0].value[0]==1);
    std::memcpy(out,in,12);++deformation_calls;
}
int skin_probe(void* actor,void* chain){assert(actor==expected_actor);skin_chains.push_back(chain);return 1;}
void draw_probe(void* actor,void* chain,const float*,unsigned){
    assert(actor==expected_actor);draw_chains.push_back(chain);
    // 4EDD64..78 invokes the auxiliary bone effect once with the real actor,
    // after traversing the supplied visual chain. Its owner is not that chain.
    if(character_value<std::uintptr_t>(actor,0x3A0)&&(character_value<unsigned>(actor,8)&0x10)){
        const auto owner=character_value<std::uintptr_t>(actor,0x3A0);
        assert(owner==main_base+0xAF4798);
        assert(character_value<unsigned>(reinterpret_cast<void*>(owner),0)==6);
        assert(character_value<std::uintptr_t>(actor,0x108)!=0);
        ++native_effect_draws;
    }
}
bool reservation_cancel=false;
unsigned reservation_calls=0;
std::uint8_t* reservation_metadata=nullptr;
void reserve_texture(void* out,const void* ref,unsigned width,unsigned height,unsigned format,unsigned ws,unsigned wt,unsigned mip){
    assert(width==4&&height==4&&format==0&&ws==0&&wt==0&&mip==0);
    assert(character_value<unsigned>(ref,0)==0x01000001);++reservation_calls;
    const auto before=command_allocations;CharacterFrameEnd::Callback(nullptr,0);
    assert(command_allocations==before); // No fence can precede this operation's end.
    character_store(reservation_metadata,4,0u);
    auto* descriptor=reinterpret_cast<void*>(main_base+0x11519A0);
    character_store(descriptor,0x2C,0x01000001u);character_store(descriptor,0x28,0u);character_store(descriptor,0x38,0u);
    // Native23D010/14 caches the descriptor IN the original TPL reference.
    character_store(const_cast<void*>(ref),4,0xFF000000u);
    character_store(out,0,reinterpret_cast<void*>(0x1000));
    if(reservation_cancel)character_appearance_request_retirement(character_banks[0]);
}
std::vector<unsigned> alias_creator_ids;
std::uint8_t* alias_metadata=nullptr;
void reserve_alias_texture(void* out,const void* ref,unsigned width,unsigned height,unsigned format,unsigned ws,unsigned wt,unsigned mip){
    assert(width==4&&height==4&&format==0&&ws==0&&wt==0&&mip==0);
    const auto id=character_value<unsigned>(ref,4);alias_creator_ids.push_back(id);
    if(id>>24!=0xFF){
        assert(id<32);
        character_store(alias_metadata+id*32,4,id);
        // Match native creator23D010 str descriptor at ref+4 and23D014 byte7=FF.
        character_store(const_cast<void*>(ref),4,id);
        static_cast<std::uint8_t*>(const_cast<void*>(ref))[7]=0xFF;
    }
    character_store(out,0,reinterpret_cast<void*>(0x1000));
}
struct AliasTpl {
    alignas(16) std::uint8_t tpl[16]{},table[4*16]{},images[4][0x28]{},refs[4][8]{};
    AliasTpl(std::initializer_list<unsigned> ids){
        character_store(tpl,4,unsigned(ids.size()));character_store(tpl,8,static_cast<void*>(table));
        unsigned i=0;for(const auto id:ids){
            character_store(table,i*16,static_cast<void*>(images[i]));
            character_store(images[i],0,std::uint16_t(4));character_store(images[i],2,std::uint16_t(4));
            character_store(images[i],8,static_cast<void*>(refs[i]));
            character_store(refs[i],0,0x0100000Au);character_store(refs[i],4,id);++i;
        }
    }
};
void reservation_alias_tests(Fixture& f){
    auto& bank=character_banks[0];
    alignas(16) std::uint8_t blob[8+32*8]{},metadata[32][32]{};
    character_store(blob,0,0x0100000Au);character_store(blob,4,32u);
    for(unsigned i=0;i<32;++i){
        character_store(blob,8+i*8,static_cast<void*>(metadata[i]));
        character_store(metadata[i],0,16u);character_store(metadata[i],4,0xffffffffu);character_store(metadata[i],8,0x0100000Au);
    }
    character_store(f.image.data(),0x11829B0,static_cast<void*>(blob));character_store(f.image.data(),0x11829B0+8,0x0100000Au);f.image[0x11829B0+15]=1;
    game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(blob),sizeof(blob),1});
    game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(metadata),sizeof(metadata),1});
    alias_metadata=metadata[0];character_native.texture_object=reserve_alias_texture;
    const auto prepare=[&](std::initializer_list<unsigned> ids,std::initializer_list<AliasTpl*> pieces){
        alias_creator_ids.clear();bank.ready={};bank.pack.store(0x0100000A);bank.phase.store(CharacterPhase::Reservations);
        for(unsigned i=0;i<32;++i)character_store(metadata[i],4,0xffffffffu);
        std::memset(f.image.data()+0x11519A0,0,3000*0x40);
        for(auto* piece:pieces)bank.ready.pieces[bank.ready.piece_count++].tpl={piece->tpl,sizeof(piece->tpl)};
        for(auto id:ids)bank.ready.texture_ids[bank.ready.texture_count++]=id;
    };
    {
        // Authenticated Krauser recipe: shared TPL5(3 DDS) and TPL13(2 DDS).
        // The old incremental key scan counts five rewritten refs again:15/10.
        AliasTpl body{0,1,2,3},head{4},upper{5,6,7},hands{17,18};
        prepare({0,1,2,3,4,5,6,7,17,18},{&body,&head,&upper,&upper,&hands,&hands});
        std::uint32_t diagnostic=0;
        assert(character_appearance_reserve_textures(bank,&diagnostic));
        assert(!diagnostic&&alias_creator_ids==std::vector<unsigned>({0,1,2,3,4,5,6,7,17,18}));
    }
    {
        AliasTpl first{0,1},other{1};prepare({1,0},{&first,&other});
        assert(character_appearance_reserve_textures(bank));
        assert(alias_creator_ids==std::vector<unsigned>({0,1})); // Distinct refs, same original DDS.
    }
    {
        AliasTpl wrong{0,2};prepare({0,1},{&wrong});std::uint32_t diagnostic=0;
        assert(!character_appearance_reserve_textures(bank,&diagnostic));
        assert(re4::character_loading_stage(diagnostic)==re4::CharacterLoadingStage::UniqueCount&&alias_creator_ids.empty());
    }
    {
        AliasTpl extra{0,1,2};prepare({0,1},{&extra});std::uint32_t diagnostic=0;
        assert(!character_appearance_reserve_textures(bank,&diagnostic));
        assert(re4::character_loading_observed(diagnostic)==3&&alias_creator_ids.empty());
    }
    bank.phase.store(CharacterPhase::Free);game_test::memory_maps.pop_back();game_test::memory_maps.pop_back();
}
}
int main(){
    Fixture f;
    character_appearance_reset();
    settings.character_appearance=re4::CharacterAppearance::Leon;
    character_appearance_update(f.actor);
    assert(control.appearance_status==unsigned(re4::CharacterAppearanceStatus::Native));
    assert(control.appearance_applied==unsigned(re4::CharacterAppearance::Leon));
    assert(!(control.appearance_generation&1));
    settings.character_appearance=re4::CharacterAppearance::Ashley;
    character_appearance_update(f.actor);
    assert(control.appearance_status==unsigned(re4::CharacterAppearanceStatus::Pending));
    assert(control.appearance_error==100); // Renderer has not initialized yet.
    assert(game_test::fs_opens==0); // A game scene and captured mount are required.
    auto& bank=character_banks[0];
    bank.token=10;bank.ack.store(9);bank.phase.store(CharacterPhase::Retiring);
    character_appearance_collect();
    assert(bank.phase.load()==CharacterPhase::Retiring);
    bank.ack.store(10);character_appearance_collect();
    assert(bank.phase.load()==CharacterPhase::Free);
    // A delayed probe acknowledgement may never release a newer retirement.
    bank.token=12;bank.ack.store(11);bank.phase.store(CharacterPhase::Retiring);
    character_appearance_collect();assert(bank.phase.load()==CharacterPhase::Retiring);
    character_appearance_ack(bank.ack,13);character_appearance_ack(bank.ack,11);
    assert(bank.ack.load()==13);
    character_appearance_collect();assert(bank.phase.load()==CharacterPhase::Free);
    // The marker executes after the native submission packet, and acknowledges
    // only after calling the actual GPU-finish backend.
    bank.token.store(20);bank.ack.store(0);bank.phase.store(CharacterPhase::Probe);
    character_native.command=command_alloc;character_native.context=context_lookup;character_native.finish=gpu_finish;
    game_test::memory_maps={{main_base,f.image.size(),1}};
    character_store(f.image.data(),0x1180D40+0x140,reinterpret_cast<void*>(0x12340000));
    character_frame_probe=+[]{frame_end_seen=true;};
    CharacterFrameEnd::Callback(nullptr,0);
    assert(bank.ack.load()==0&&finish_calls==0);
    assert(character_value<std::uint16_t>(command_buffer,8)==0x20);
    auto callback=character_value<void(*)(void*)>(command_buffer,0);callback(command_buffer);
    assert(finish_calls==1&&bank.ack.load()==20);
    character_appearance_retire(bank);const auto retirement=bank.token.load();
    assert(retirement!=20&&bank.phase.load()==CharacterPhase::Retiring);
    // Use a real monotonically allocated token in the fixture too.
    character_serial.store(30);bank.token.store(31);bank.ack.store(20);
    character_appearance_collect();assert(bank.phase.load()==CharacterPhase::Retiring);
    CharacterFrameEnd::Callback(nullptr,0);callback=character_value<void(*)(void*)>(command_buffer,0);callback(command_buffer);
    character_appearance_collect();assert(bank.phase.load()==CharacterPhase::Free);

    character_appearance_reset();f.view_mode();character_renderer_ready.store(true);
    alignas(32) std::uint8_t native_part[0x1A8]{},native_bin[0x98]{},hands[0x1A8]{},face[0x70]{},knife[0x1A8]{};
    native_bin[0x29]=119;
    character_store(f.actor,0x180,static_cast<void*>(native_part));character_store(native_part,0x18,static_cast<void*>(native_bin));
    character_store(f.actor,0x108,static_cast<void*>(native_bin));f.actor[0x11A]=119;
    character_store(f.actor,0x950,static_cast<void*>(face));character_store(face,0x38,static_cast<void*>(hands));
    character_store(native_part,0x28,static_cast<void*>(hands));
    game_test::memory_maps={{main_base,f.image.size(),1},{reinterpret_cast<std::uintptr_t>(f.actor),sizeof(f.actor),1},
        {reinterpret_cast<std::uintptr_t>(f.input_state.data()),f.input_state.size(),1},
        {reinterpret_cast<std::uintptr_t>(native_part),sizeof(native_part),1},{reinterpret_cast<std::uintptr_t>(native_bin),sizeof(native_bin),1},
        {reinterpret_cast<std::uintptr_t>(hands),sizeof(hands),1},{reinterpret_cast<std::uintptr_t>(face),sizeof(face),1}};
    CharacterIdentity identity;assert(character_identity(f.actor,identity));
    // Pending must name the blocking boundary without relaxing its guard.
    settings.character_appearance=character_desired=re4::CharacterAppearance::Ashley;
    const auto expect_wait=[&](unsigned reason){
        character_appearance_update(f.actor);
        assert(control.appearance_status==unsigned(re4::CharacterAppearanceStatus::Pending));
        assert(control.appearance_error==reason&&game_test::fs_opens==0);
        assert(character_error==re4::CharacterAppearanceError::None);
    };
    expect_wait(111); // No captured ROM mount.
    f.actor[0x118]=1;expect_wait(104|(2u<<16));
    assert(re4::character_appearance_reason(control.appearance_error)==re4::CharacterAppearanceError::WaitingModel);
    assert(re4::character_appearance_observed(control.appearance_error)==1);
    f.actor[0x118]=255;expect_wait(104|(256u<<16));
    assert(re4::character_appearance_observed(control.appearance_error)==255);f.actor[0x118]=0;
    f.actor[0x11A]=26;expect_wait(105|(27u<<16));
    assert(re4::character_appearance_reason(control.appearance_error)==re4::CharacterAppearanceError::WaitingBones);
    assert(re4::character_appearance_observed(control.appearance_error)==26);
    f.actor[0x11A]=0;expect_wait(105|(1u<<16));
    assert(re4::character_appearance_observed(control.appearance_error)==0);f.actor[0x11A]=119;
    character_store(f.actor,0x108,static_cast<void*>(nullptr));expect_wait(105); // Missing pointer has no observed byte.
    assert(re4::character_appearance_observed(control.appearance_error)==-1);
    character_store(f.actor,0x108,static_cast<void*>(native_bin));
    character_store(f.actor,0x188,static_cast<void*>(native_part));expect_wait(107);
    character_store(f.actor,0x188,static_cast<void*>(nullptr));
    character_store(f.actor,0x3B8,static_cast<void*>(native_part));expect_wait(108);
    character_store(f.actor,0x3B8,static_cast<void*>(nullptr));
    character_store(f.actor,0x3A0,static_cast<void*>(native_part));character_store(f.actor,8,0x10u);expect_wait(109);
    character_store(f.actor,0x3A0,static_cast<void*>(nullptr));character_store(f.actor,8,0u);
    // Ordinary player constructors install this native six-record bone-effect
    // owner, including while actor flag0x10 enables its unchanged draw branch.
    character_store(f.image.data(),0xAF4798,6u);
    character_store(f.image.data(),0xAF47A0,reinterpret_cast<void*>(main_base+0xAF4768));
    character_store(f.actor,0x3A0,reinterpret_cast<void*>(main_base+0xAF4798));
    character_store(f.actor,8,0x10u);expect_wait(111);
    character_store(f.image.data(),0xAF4798,7u);expect_wait(109);
    character_store(f.image.data(),0xAF4798,6u);
    character_store(f.image.data(),0xAF47A0,static_cast<void*>(native_part));expect_wait(109);
    character_store(f.image.data(),0xAF47A0,reinterpret_cast<void*>(main_base+0xAF4768));expect_wait(111);
    f.input_state[0x503C]=1;expect_wait(110);f.input_state[0x503C]=0;
    native_bin[0x29]=26;expect_wait(127|(27u<<16));
    assert(re4::character_appearance_reason(control.appearance_error)==re4::CharacterAppearanceError::WaitingBinBones);
    assert(re4::character_appearance_observed(control.appearance_error)==26);native_bin[0x29]=119;
    // Appearance ownership survives input-ineligible scripted actions and scope.
    f.actor[0x114]=1;assert(character_identity(f.actor,identity));f.actor[0x114]=0;
    f.image[0x1131718+0x54]=16;assert(character_identity(f.actor,identity));
    f.input_state[0x5092]|=4;assert(character_identity(f.actor,identity));
    f.input_state[0x5092]&=~4;f.view_mode();
    bank.identity=identity;bank.appearance=re4::CharacterAppearance::Ashley;bank.ready.piece_count=1;
    character_appearance_mount("rom");bank.pack.store(0x01000001);bank.token.store(40);bank.ack.store(0);
    bank.phase.store(CharacterPhase::Probe);bank.load_result.store(0);expect_wait(114);
    bank.load_result.store(5);expect_wait(115);
    bank.load_result.store(1);expect_wait(116);
    character_store(f.image.data(),0x11829B0,reinterpret_cast<void*>(0x1000));
    character_store(f.image.data(),0x11829B0+8,0x01000001u);f.image[0x11829B0+15]=1;expect_wait(117);
    bank.phase.store(CharacterPhase::UploadProbe);expect_wait(118);
    bank.phase.store(CharacterPhase::Reservations);expect_wait(119);
    bank.phase.store(CharacterPhase::Free);bank.load_result.store(0);
    std::memset(f.image.data()+0x11829B0,0,16);
    re4::CharacterWeight8 weight{{0,0,0},1,{100,0,0},0};std::uint16_t index=0;
    re4::ViewVec3 position{1,2,3},normal{0,1,0};
    auto& piece=bank.ready.pieces[0];piece.bin={native_bin,sizeof(native_bin)};
    piece.weights={reinterpret_cast<std::uint8_t*>(&weight),sizeof(weight)};piece.weight_count=1;piece.vertex_count=piece.normal_count=1;
    piece.position_weights=piece.normal_weights={reinterpret_cast<std::uint8_t*>(&index),sizeof(index)};
    character_store(native_bin,0x48,static_cast<const void*>(&position));character_store(native_bin,0x50,static_cast<const void*>(&normal));
    character_store(bank.parts[0],0xB0,static_cast<void*>(bank.deformed[0]));character_store(bank.parts[0],0xC8,static_cast<void*>(bank.deformed[0]+32));
    bank.map.count=26;const re4::ViewMatrix3x4 matrix{{{1,0,0,0},{0,1,0,0},{0,0,1,0}}};
    for(unsigned i=0;i<119;++i)std::memcpy(f.input_state.data()+0x1CC+i*sizeof(matrix),&matrix,sizeof(matrix));
    for(unsigned i=0;i<26;++i){bank.map.bones[i].source=std::uint8_t(i);bank.map.source_bind[i]={float(i),float(i),0};}
    character_native.output=native_palette;character_native.positions=character_native.normals=deform;
    bank.token.store(42);bank.phase.store(CharacterPhase::Active);expected_actor=f.actor;character_skin_probe=skin_probe;character_draw_probe=draw_probe;
    settings.character_appearance=character_desired=re4::CharacterAppearance::Ashley;
    character_appearance_update(f.actor);assert(control.appearance_status==unsigned(re4::CharacterAppearanceStatus::Pending));
    assert(control.appearance_error==120); // Active bank still awaits skin callback.
    const auto actor_before=std::vector<std::uint8_t>(f.actor,f.actor+sizeof(f.actor));
    const auto native_before=std::vector<std::uint8_t>(native_part,native_part+sizeof(native_part));
    assert(CharacterSkin::Callback(f.actor,native_part)==1);
    assert(skin_chains.size()==1&&skin_chains[0]==native_part);
    assert(deformation_calls==2&&character_value<float>(bank.deformed[0],0)==1);
    character_appearance_update(f.actor);assert(control.appearance_error==121); // Skinned, awaits own draw.
    view_frame.fetch_add(1); // The queued scene draws on the next simulation tick.
    CharacterDraw::Callback(f.actor,native_part,nullptr,0);
    assert(draw_chains.size()==1&&draw_chains[0]==bank.parts[0]);
    assert(native_effect_draws==1); // One native draw preserves one auxiliary-effect invocation.
    character_appearance_update(f.actor);assert(control.appearance_status==unsigned(re4::CharacterAppearanceStatus::Active));
    assert(control.appearance_error==0);
    assert(character_value<void*>(bank.parts[0],0x28)==nullptr);
    assert(std::memcmp(actor_before.data(),f.actor,actor_before.size())==0);
    assert(std::memcmp(native_before.data(),native_part,native_before.size())==0);
    const auto deformation_before=deformation_calls;
    CharacterSkin::Callback(f.actor,native_part);
    assert(deformation_calls==deformation_before); // A GPU-owned slot is immutable.
    CharacterDraw::Callback(f.actor,native_part,nullptr,0);assert(draw_chains.back()==native_part);
    character_native.command=command_alloc;
    const auto finishes_before_slot=finish_calls;
    CharacterFrameEnd::Callback(nullptr,0);character_value<void(*)(void*)>(command_buffer,0)(command_buffer);
    assert(bank.slot_ack[0].load()==bank.slot_token[0].load()&&finish_calls==finishes_before_slot);
    // A native special pose can collapse its translated arm edge. Natural FK
    // ignores that arm translation residual and keeps the donor resource.
    auto collapsed=matrix;collapsed.m[0][3]=-1;collapsed.m[1][3]=-1;
    std::memcpy(f.input_state.data()+0x1CC+8*sizeof(matrix),&collapsed,sizeof(collapsed));
    const auto collapse_before=deformation_calls;CharacterSkin::Callback(f.actor,native_part);
    assert(deformation_calls==collapse_before+2&&bank.phase.load()==CharacterPhase::Active);
    std::memcpy(f.input_state.data()+0x1CC+8*sizeof(matrix),&matrix,sizeof(matrix));
    // Fully owned body geometry does not borrow or validate native hand links.
    bank.drawn.store(0); // Diagnose a fresh generation's first-draw rejection.
    CharacterSkin::Callback(f.actor,native_part);
    character_store(hands,0x28,static_cast<void*>(native_part));
    CharacterDraw::Callback(f.actor,native_part,nullptr,0);assert(draw_chains.back()==bank.parts[0]);
    character_appearance_update(f.actor);assert(control.appearance_error==0);
    character_store(hands,0x28,static_cast<void*>(nullptr));
    character_store(f.actor,0x950,reinterpret_cast<void*>(0xDEAD0000));
    CharacterDraw::Callback(f.actor,native_part,nullptr,0);assert(draw_chains.back()==bank.parts[0]);
    character_store(f.actor,0x950,static_cast<void*>(face));
    bank.drawn.store(0);character_store(f.input_state.data(),0x2C,3u);
    CharacterDraw::Callback(f.actor,native_part,nullptr,0);assert(draw_chains.back()==native_part);
    character_appearance_update(f.actor);assert(control.appearance_error==123);
    character_store(f.input_state.data(),0x2C,0u);f.actor[0x118]=1;
    CharacterDraw::Callback(f.actor,native_part,nullptr,0);assert(draw_chains.back()==native_part);f.actor[0x118]=0;
    // Scope and scripted ladder ownership retain the active resource and render.
    const auto retained=bank.token.load();f.image[0x1131718+0x54]=16;f.actor[0x114]=1;f.input_state[0x5092]|=4;
    character_appearance_update(f.actor);assert(bank.phase.load()==CharacterPhase::Active&&bank.token.load()==retained);
    assert(character_identity(f.actor,identity));f.actor[0x114]=0;f.input_state[0x5092]&=~4;f.view_mode();
    // A real inventory cModel is only3C8 bytes: no actor-only offsets exist.
    alignas(32) std::uint8_t menu_model[0x3C8]{},menu_part[0x1A8]{},menu_hand[0x1A8]{},menu_right[0x1A8]{},menu_weapon_archive[0x100]{},native_archive[0x300]{};
    alignas(32) std::uint8_t bind_table[119*16]{};
    for(unsigned i=0;i<119;++i){
        bind_table[i*16]=std::uint8_t(i);bind_table[i*16+1]=i<26?re4::character_core_parents[i]:0;
        re4::ViewVec3 local{float(i+1),float(i+2),float(i+3)};
        std::memcpy(bind_table+i*16+4,&local,sizeof(local));
        if(i<26){bank.ready.body_bind[i].id=std::uint8_t(i);bank.ready.body_bind[i].parent=bind_table[i*16+1];bank.ready.body_bind[i].position=local;}
        std::memcpy(f.input_state.data()+0x1CC+i*sizeof(matrix),&matrix,sizeof(matrix));
    }
    bank.ready.body_bone_count=26;
    character_store(native_bin,0,static_cast<void*>(bind_table));
    character_native.initialize=+[](void*,void*,unsigned){return true;};
    character_native.attach=+[](void* part,void* bin,void* tpl){character_store(part,0x18,bin);character_store(part,0x20,tpl);character_store(part,8,8u);};
    game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(bind_table),sizeof(bind_table),1});
    // Explicit roles, including an interleaved blade, construct complete owned
    // hands and skip that blade in both gameplay and preview body chains.
    bank.ready.piece_count=4;
    for(unsigned i=1;i<4;++i)bank.ready.pieces[i]=bank.ready.pieces[0];
    bank.ready.pieces[1].role=re4::CharacterAssetRole::RightHand;
    bank.ready.pieces[2].role=re4::CharacterAssetRole::Knife;
    bank.ready.pieces[3].role=re4::CharacterAssetRole::LeftHand;
    assert(character_appearance_prepare(bank,bank.identity));
    const auto owned_links=[&](const CharacterBinding& binding){
        assert(character_value<void*>(binding.parts[0],0x28)==binding.parts[1]);
        assert(character_value<void*>(binding.parts[1],0x28)==binding.parts[3]);
        assert(character_value<void*>(binding.parts[2],0x28)==nullptr&&character_value<void*>(binding.parts[3],0x28)==nullptr);
    };
    owned_links(bank);
    assert(reinterpret_cast<std::uintptr_t>(bank.menu.deformed)>=reinterpret_cast<std::uintptr_t>(bank.raw)+re4::character_asset_recipe(bank.appearance)->size);
    const auto old_native_asset=character_value<void*>(f.input_state.data(),0x70);
    character_store(f.input_state.data(),0x70,static_cast<void*>(native_archive));
    character_store(native_archive,0x10,0x100u);character_store(native_archive,0x10+14*4,0x200u);character_store(native_archive,0x44,0x240u);
    std::memcpy(native_archive+0x100,native_bin,sizeof(native_bin));
    character_store(menu_part,0x18,static_cast<void*>(native_archive+0x100));character_store(menu_part,0x28,static_cast<void*>(menu_right));
    character_store(menu_right,0x18,static_cast<void*>(menu_weapon_archive));character_store(menu_right,0x20,static_cast<void*>(native_archive+0x240));
    character_store(menu_right,0x28,static_cast<void*>(menu_hand));
    character_store(menu_hand,0x18,static_cast<void*>(native_archive+0x200));character_store(menu_hand,0x20,static_cast<void*>(native_archive+0x240));
    character_store(menu_model,0,reinterpret_cast<void*>(main_base+0xAFF538));character_store(menu_model,8,7u);menu_model[0x11A]=119;
    character_store(menu_model,0x180,static_cast<void*>(menu_part));character_store(menu_model,0x108,static_cast<void*>(bind_table));
    character_store(f.image.data(),0xAFF4A8+8,static_cast<void*>(menu_model));character_store(f.image.data(),0xAFF4A8+0x10,1u);
    character_store(f.image.data(),0xAFF4A8+0x14,0x3C8u);character_store(f.image.data(),0x1272140,static_cast<void*>(menu_model));
    for(const auto& memory:std::vector<MemoryInfo>{{reinterpret_cast<std::uintptr_t>(menu_model),sizeof(menu_model),1},
            {reinterpret_cast<std::uintptr_t>(menu_part),sizeof(menu_part),1},{reinterpret_cast<std::uintptr_t>(menu_hand),sizeof(menu_hand),1},
            {reinterpret_cast<std::uintptr_t>(native_archive),sizeof(native_archive),1},{reinterpret_cast<std::uintptr_t>(menu_right),sizeof(menu_right),1},
            {reinterpret_cast<std::uintptr_t>(menu_weapon_archive),sizeof(menu_weapon_archive),1}})game_test::memory_maps.push_back(memory);
    CharacterIdentity menu_identity;assert(character_menu_identity(menu_model,menu_identity));
    assert(!character_identity(menu_model,identity));assert(!character_menu_identity(menu_model+8,menu_identity));
    const auto menu_before=std::vector<std::uint8_t>(menu_model,menu_model+sizeof(menu_model));
    const auto main_identity=bank.identity;const auto main_buffers=std::vector<std::uint8_t>(bank.deformed[0],bank.deformed[0]+64);
    expected_actor=menu_model;
    assert(CharacterSkin::Callback(menu_model,menu_part)==1);assert(bank.menu.prepared);owned_links(bank.menu);
    CharacterDraw::Callback(menu_model,menu_part,nullptr,0);assert(draw_chains.back()==bank.menu.parts[0]);
    assert(bank.menu.slot_token[0].load()>bank.menu.slot_ack[0].load());
    assert(bank.identity==main_identity&&std::memcmp(main_buffers.data(),bank.deformed[0],main_buffers.size())==0);
    assert(std::memcmp(menu_before.data(),menu_model,menu_before.size())==0);
    const auto menu_token=bank.menu.slot_token[0].load();
    CharacterMenuDestroy::Callback(nullptr);
    assert(bank.menu.identity.context!=character_menu_generation.load()&&bank.menu.slot_token[0].load()==menu_token&&bank.phase.load()==CharacterPhase::Active);
    CharacterSkin::Callback(menu_model,menu_part);assert(bank.menu.identity.context!=character_menu_generation.load()); // Pending consumer blocks reuse after same-address pool rebuild.
    CharacterFrameEnd::Callback(nullptr,0);character_value<void(*)(void*)>(command_buffer,0)(command_buffer);
    assert(bank.menu.slot_ack[0].load()==menu_token);
    CharacterSkin::Callback(menu_model,menu_part);assert(bank.menu.prepared&&bank.menu.identity.context==character_menu_generation.load());
    character_store(f.image.data(),0xAFF4A8+0x14,0x3C0u);assert(!character_menu_identity(menu_model,menu_identity));
    character_store(f.image.data(),0xAFF4A8+0x14,0x3C8u);
    expected_actor=f.actor;character_store(f.input_state.data(),0x70,old_native_asset);
    // A distinct native equipped-knife actor also exists; its guarded path remains supported.
    alignas(32) std::uint8_t weapon_actor[0x450]{},weapon_root[0x1F0]{},player_bones[11][0x1F0]{},weapon_owner[0x40]{},weapon_archive[0x300]{};
    for(unsigned i=0;i<11;++i){
        character_store(player_bones[i],0x18,matrix);
        if(i<10)character_store(player_bones[i],0x108,static_cast<void*>(player_bones[i+1]));
    }
    const re4::ViewMatrix3x4 player_world{{{0,-1,0,17},{1,0,0,-5},{0,0,1,9}}};
    const re4::ViewMatrix3x4 weapon_world{{{0,0,1,-3},{0,1,0,12},{-1,0,0,4}}};
    character_store(player_bones[0],0x18,player_world);character_store(weapon_root,0x18,weapon_world);
    character_store(weapon_root,0x78,static_cast<void*>(player_bones[10]));
    character_store(f.actor,0x108,static_cast<void*>(player_bones[0]));character_store(f.actor,0x938,static_cast<void*>(weapon_owner));
    character_store(weapon_owner,0x38,static_cast<void*>(weapon_actor));
    character_store(weapon_actor,0,reinterpret_cast<void*>(main_base+0xB0AAF8));weapon_actor[0x118]=0x24;weapon_actor[0x11A]=1;
    character_store(weapon_actor,0x3F8,static_cast<void*>(f.actor));character_store(weapon_actor,0x108,static_cast<void*>(weapon_root));
    character_store(weapon_actor,0x180,static_cast<void*>(knife));
    game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(knife),sizeof(knife),1});
    character_store(f.input_state.data(),0x58,static_cast<void*>(weapon_archive));
    character_store(weapon_archive,0x14,0x200u);character_store(weapon_archive,0x18,0x100u);weapon_archive[0x129]=1;
    character_store(knife,0x18,static_cast<void*>(weapon_archive+0x100));character_store(knife,0x20,static_cast<void*>(weapon_archive+0x200));
    character_store(knife,0x28,static_cast<void*>(nullptr));character_store(knife,8,8u);character_store(knife,0xA7,std::uint8_t(123));
    for(const auto& memory:std::vector<MemoryInfo>{{reinterpret_cast<std::uintptr_t>(weapon_actor),sizeof(weapon_actor),1},
        {reinterpret_cast<std::uintptr_t>(weapon_root),sizeof(weapon_root),1},{reinterpret_cast<std::uintptr_t>(player_bones),sizeof(player_bones),1},
        {reinterpret_cast<std::uintptr_t>(weapon_owner),sizeof(weapon_owner),1},{reinterpret_cast<std::uintptr_t>(weapon_archive),sizeof(weapon_archive),1}})game_test::memory_maps.push_back(memory);
    assert(character_identity(f.actor,bank.identity));bank.appearance=re4::CharacterAppearance::Krauser;bank.ready.piece_count=2;
    bank.ready.pieces[1]=bank.ready.pieces[0];bank.ready.pieces[1].role=re4::CharacterAssetRole::Knife;
    character_store(bank.parts[0],0x28,static_cast<void*>(nullptr));
    character_store(bank.parts[1],8,8u);character_store(bank.parts[1],0x28,static_cast<void*>(nullptr));
    bank.skinned[0].store(view_frame.load());bank.slot_ack[0].store(bank.slot_token[0].load());
    bank.knife_orientation={{0,0,0},{1,0,0}};bank.knife_orientation_valid=true;
    expected_actor=f.actor;assert(CharacterSkin::Callback(f.actor,native_part)==1);
    assert(bank.visual[0].valid&&bank.visual[0].knife_valid);
    const auto weapon_before=std::vector<std::uint8_t>(weapon_actor,weapon_actor+sizeof(weapon_actor));
    const auto weapon_root_before=std::vector<std::uint8_t>(weapon_root,weapon_root+sizeof(weapon_root));
    const auto knife_before=std::vector<std::uint8_t>(knife,knife+sizeof(knife));
    const auto body_token=bank.slot_token[0].load();
    expected_actor=weapon_actor;
    CharacterDraw::Callback(weapon_actor,knife,nullptr,0);assert(draw_chains.back()==bank.parts[1]);
    assert(bank.slot_token[0].load()>body_token&&bank.slot_ack[0].load()<bank.slot_token[0].load());
    assert(std::memcmp(weapon_before.data(),weapon_actor,weapon_before.size())==0&&std::memcmp(weapon_root_before.data(),weapon_root,weapon_root_before.size())==0);
    assert(std::memcmp(knife_before.data(),knife,knife_before.size())==0);
    assert((character_value<unsigned>(bank.parts[1],8)&8)&&character_value<std::uint8_t>(bank.parts[1],0xA7)==123);
    re4::ViewVec3 original_world,corrected_local,corrected_world;
    const re4::ViewVec3 sample{2,-7,4};
    re4::ViewVec3 flipped;
    assert(re4::view_transform_point(bank.visual[0].knife_half_turn,sample,flipped));
    assert(re4::view_transform_point(player_world,flipped,original_world));
    assert(re4::view_transform_point(character_value<re4::ViewMatrix3x4>(bank.parts[1],0x74),sample,corrected_local));
    assert(re4::view_transform_point(weapon_world,corrected_local,corrected_world));
    assert(std::fabs(original_world.x-corrected_world.x)<0.0001f&&std::fabs(original_world.y-corrected_world.y)<0.0001f&&std::fabs(original_world.z-corrected_world.z)<0.0001f);
    character_store(knife,8,0u);CharacterDraw::Callback(weapon_actor,knife,nullptr,0);
    assert(draw_chains.back()==bank.parts[1]&&!(character_value<unsigned>(bank.parts[1],8)&8)&&character_value<unsigned>(knife,8)==0);
    // Any unproved weapon owner/class/root/archive or stale pose stays native.
    const auto native_knife=[&]{const auto before=draw_chains.size();CharacterDraw::Callback(weapon_actor,knife,nullptr,0);assert(draw_chains.size()==before+1&&draw_chains.back()==knife);};
    character_store(weapon_owner,0x38,static_cast<void*>(weapon_root));native_knife();character_store(weapon_owner,0x38,static_cast<void*>(weapon_actor));
    CharacterDraw::Callback(weapon_actor,native_part,nullptr,0);assert(draw_chains.back()==native_part);
    character_store(knife,0x28,static_cast<void*>(hands));native_knife();character_store(knife,0x28,static_cast<void*>(nullptr));
    weapon_archive[0x129]=2;native_knife();weapon_archive[0x129]=1;
    character_store(weapon_actor,0,reinterpret_cast<void*>(main_base+0xB0AAF0));native_knife();character_store(weapon_actor,0,reinterpret_cast<void*>(main_base+0xB0AAF8));
    weapon_actor[0x118]=0;native_knife();weapon_actor[0x118]=0x24;
    character_store(weapon_actor,0x3F8,static_cast<void*>(weapon_root));native_knife();character_store(weapon_actor,0x3F8,static_cast<void*>(f.actor));
    character_store(weapon_root,0x78,static_cast<void*>(player_bones[9]));native_knife();character_store(weapon_root,0x78,static_cast<void*>(player_bones[10]));
    character_store(weapon_actor,0x188,static_cast<void*>(knife));native_knife();character_store(weapon_actor,0x188,static_cast<void*>(nullptr));
    character_store(weapon_actor,0x3B8,static_cast<void*>(knife));native_knife();character_store(weapon_actor,0x3B8,static_cast<void*>(nullptr));
    character_store(weapon_actor,8,0x4000u);native_knife();character_store(weapon_actor,8,0u);
    character_store(knife,0x20,static_cast<void*>(native_bin));native_knife();character_store(knife,0x20,static_cast<void*>(weapon_archive+0x200));
    auto singular=weapon_world;for(auto& row:singular.m)for(unsigned i=0;i<3;++i)row[i]=0;
    character_store(weapon_root,0x18,singular);native_knife();character_store(weapon_root,0x18,weapon_world);
    auto nonfinite=weapon_world;nonfinite.m[1][2]=std::numeric_limits<float>::quiet_NaN();
    character_store(weapon_root,0x18,nonfinite);native_knife();character_store(weapon_root,0x18,weapon_world);
    // Dense native bone mode uses stride1F0, just as2B4FF8..5000 does.
    const auto original_player_flags=character_value<unsigned>(f.actor,8);
    character_store(f.actor,8,original_player_flags|0x2000u);
    CharacterDraw::Callback(weapon_actor,knife,nullptr,0);assert(draw_chains.back()==bank.parts[1]);
    character_store(f.actor,8,original_player_flags);
    character_store(weapon_actor,0x3A0,reinterpret_cast<void*>(main_base+0xAF4798));character_store(weapon_actor,8,0x10u);
    const auto before_weapon_effect=native_effect_draws;const auto before_weapon_draw=draw_chains.size();
    CharacterDraw::Callback(weapon_actor,knife,nullptr,0);
    assert(draw_chains.size()==before_weapon_draw+1&&draw_chains.back()==bank.parts[1]&&native_effect_draws==before_weapon_effect+1);
    character_store(weapon_actor,0x3A0,static_cast<void*>(nullptr));character_store(weapon_actor,8,0u);
    bank.skinned[0].store(view_frame.load()-3);native_knife();bank.skinned[0].store(view_frame.load());
    bank.appearance=re4::CharacterAppearance::Ashley;native_knife();
    expected_actor=f.actor;bank.ready.piece_count=1;
    character_store(f.actor,0x108,static_cast<void*>(native_bin));character_store(f.actor,0x938,character_value<void*>(actor_before.data(),0x938));
    bank.pack.store(0x01000001);bank.pinned.store(true);
    character_free_probe=+[](void*,unsigned id){freed_ids.push_back(id);};
    CharacterTextureFree::Callback(&software_context,0x01000001);assert(freed_ids.empty());
    CharacterTextureFree::Callback(&software_context,0x01000002);assert(freed_ids==std::vector<unsigned>{0x01000002});
    CharacterRendererDestroy::Callback();assert(bank.phase.load()==CharacterPhase::Retiring);
    character_appearance_collect();assert(bank.phase.load()==CharacterPhase::Free&&!bank.pinned.load());
    static unsigned close_before_destroy=0;close_before_destroy=game_test::fs_closes;
    bank.phase.store(CharacterPhase::Reading);bank.file.value=3;bank.opened=true;
    character_destroy_probe=+[]{
        assert(character_banks[0].phase.load()==CharacterPhase::Free&&!character_banks[0].opened);
        assert(game_test::fs_closes==close_before_destroy+1);
    };
    CharacterRendererDestroy::Callback();assert(game_test::fs_closes==close_before_destroy+1);
    character_destroy_probe=nullptr;
    // A completed GPU marker cannot retire an outstanding native pack IO thread.
    bank.phase.store(CharacterPhase::Retiring);bank.token.store(300);bank.ack.store(300);bank.pinned.store(true);bank.load_result.store(1);
    f.image[0x11829B0+15]=0;
    character_appearance_collect();assert(bank.phase.load()==CharacterPhase::Retiring&&bank.pinned.load());
    auto* entry=f.image.data()+0x11829B0;
    auto* request=f.image.data()+0x1184300;auto* info=f.image.data()+0x11849E0;
    auto* wrapper=f.image.data()+0x20000;auto* sdk=f.image.data()+0x21000;
    character_store(entry,0,static_cast<void*>(native_bin));character_store(entry,8,0x01000001u);
    character_store(request,0,static_cast<void*>(info));character_store(request,8,static_cast<void*>(native_bin));
    character_store(request,0x18,reinterpret_cast<void*>(main_base+0x24E300));character_store(request,0x50,1u);
    character_store(info,0x40,static_cast<void*>(entry));character_store(wrapper,0x18,static_cast<void*>(sdk));
    character_store(f.image.data(),0x11852A0,static_cast<void*>(wrapper));
    static unsigned flushes=0,joins=0;flushes=joins=0;
    character_native.flush=+[]{++flushes;};
    character_native.wait_thread=+[](void* ptr){
        assert(flushes==2&&ptr==reinterpret_cast<void*>(main_base+0x21000));++joins;
        character_store(reinterpret_cast<void*>(main_base+0x1184300),0x50,0u);
        *reinterpret_cast<std::uint8_t*>(main_base+0x11829B0+15)=1;
    };
    character_destroy_probe=+[]{assert(flushes==2&&joins==1);assert(character_banks[0].phase.load()==CharacterPhase::Retiring);};
    CharacterRendererDestroy::Callback();character_appearance_collect();
    assert(bank.phase.load()==CharacterPhase::Free&&!bank.pinned.load());
    // New native loads must not enter the unchecked negative-index path when
    // all twenty IO thread handles are occupied.
    character_native.context=context_lookup;character_native.load_texture=texture_load;
    alignas(16) std::uint8_t load[0x28]{};
    bank.phase.store(CharacterPhase::Probe);bank.token.store(100);bank.pack.store(0x01000001);bank.load_result.store(0);
    character_store(load,0x10,&bank);character_store(load,0x18,std::uint64_t(100));
    character_store(load,0x20,0x22u);
    character_store(f.image.data(),0x11829B0+8,0xffffffffu);
    f.image[0x11807D4]=1;character_store(f.image.data(),0x11807D8+0x22*4,0x01000001u);
    character_store(f.image.data(),0x1180D40+0x158,reinterpret_cast<void*>(0x7000));
    character_pack_budget_native.close=+[](void*){};character_pack_budget_native.prefix=+[]{return "bio4";};
    character_pack_budget_native.reserve=+[](unsigned,const char*,unsigned)->void*{return nullptr;};
    character_pack_budget_native.release=+[](void*){};character_pack_budget_native.reap=+[]{return -1;};
    for(unsigned i=0;i<20;++i)character_store(f.image.data(),0x11852A0+i*8,reinterpret_cast<void*>(0x1000+i*8));
    character_appearance_load_command(load);assert(texture_load_calls==0&&bank.load_result.load()==5);
    // The active game process keeps its scheduler baton across both flushes.
    // Even a borrowed pending native load is joined before queue_load returns.
    static bool queue_owner=false,load_dispatched=false;queue_owner=true;load_dispatched=false;flushes=joins=0;
    character_store(entry,8,0x01000001u);entry[15]=0;character_store(request,0x50,1u);
    character_store(f.image.data(),0x11852A0,static_cast<void*>(wrapper));
    bank.phase.store(CharacterPhase::Probe);bank.token.store(350);bank.ack.store(0);bank.load_result.store(0);bank.pinned.store(true);character_serial.store(350);
    character_native.command=command_alloc;
    character_native.finish=gpu_finish;
    character_native.flush=+[]{
        assert(queue_owner);++flushes;
        if(!load_dispatched){load_dispatched=true;character_value<void(*)(void*)>(command_buffer,0)(command_buffer);}
    };
    character_native.wait_thread=+[](void* ptr){
        assert(queue_owner&&ptr==reinterpret_cast<void*>(main_base+0x21000));++joins;
        character_appearance_request_retirement(character_banks[0]);
        character_store(reinterpret_cast<void*>(main_base+0x1184300),0x50,0u);
        *reinterpret_cast<std::uint8_t*>(main_base+0x11829B0+15)=1;
    };
    assert(character_appearance_queue_load(bank,*re4::character_asset_recipe(re4::CharacterAppearance::Ashley)));
    queue_owner=false;assert(flushes==2&&joins==1&&bank.load_result.load()==1);
    character_appearance_collect();assert(bank.phase.load()==CharacterPhase::Retiring&&bank.pinned.load());
    CharacterFrameEnd::Callback(nullptr,0);character_value<void(*)(void*)>(command_buffer,0)(command_buffer);
    character_appearance_collect();assert(bank.phase.load()==CharacterPhase::Free);
    // Installed resources are chunked from the captured mounted ROM and must
    // authenticate before any native constructor, IO loader or texture call.
    character_appearance_reset();character_renderer_ready.store(true);
    settings.character_appearance=re4::CharacterAppearance::Ashley;
    character_appearance_mount("rom");
    game_test::resource_path="rom:/bio4/em/pl01.udas";
    game_test::resource_file.assign(re4::character_asset_recipe(re4::CharacterAppearance::Ashley)->size,0);
    game_test::fs_opens=game_test::fs_reads=game_test::fs_closes=0;game_test::resource_max_read=0;
    for(unsigned i=0;i<7;++i)character_appearance_update(f.actor);
    assert(game_test::fs_opens==1&&game_test::fs_reads==6&&game_test::fs_closes==1);
    assert(game_test::resource_max_read==256*1024);
    assert(control.appearance_status==unsigned(re4::CharacterAppearanceStatus::Failed));
    assert(control.appearance_error==unsigned(re4::CharacterAppearanceError::AssetMismatch));
    assert(texture_load_calls==0&&std::memcmp(actor_before.data(),f.actor,actor_before.size())==0);
    character_appearance_reset();character_renderer_ready.store(true);character_serial.store(201);
    settings.character_appearance=character_desired=re4::CharacterAppearance::Ashley;
    character_native.texture_object=reserve_texture;character_native.command=command_alloc;
    character_native.context=context_lookup;character_native.finish=gpu_finish;
    alignas(16) std::uint8_t blob[16]{},metadata[32]{},tpl[16]{},table[16]{},image[0x28]{},reference[8]{};
    character_store(blob,0,0x01000001u);character_store(blob,4,1u);character_store(blob,8,static_cast<void*>(metadata));
    character_store(metadata,0,16u);character_store(metadata,4,0xffffffffu);character_store(metadata,8,0x01000001u);
    character_store(tpl,4,1u);character_store(tpl,8,static_cast<void*>(table));character_store(table,0,static_cast<void*>(image));
    character_store(image,0,std::uint16_t(4));character_store(image,2,std::uint16_t(4));character_store(image,8,static_cast<void*>(reference));
    character_store(reference,0,0x01000001u);
    character_store(f.image.data(),0x11829B0,static_cast<void*>(blob));character_store(f.image.data(),0x11829B0+8,0x01000001u);f.image[0x11829B0+15]=1;
    game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(blob),sizeof(blob),1});
    game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(metadata),sizeof(metadata),1});
    assert(character_identity(f.actor,bank.identity));bank.appearance=re4::CharacterAppearance::Ashley;bank.pack.store(0x01000001);
    bank.ready.piece_count=1;bank.ready.pieces[0].tpl={tpl,sizeof(tpl)};bank.ready.texture_count=1;bank.ready.texture_ids[0]=0;
    bank.token.store(201);bank.ack.store(201);bank.phase.store(CharacterPhase::Probe);bank.load_result.store(1);bank.pinned.store(true);
    character_appearance_mount("rom");reservation_metadata=metadata;
    character_appearance_update(f.actor);
    assert(reservation_calls==1&&bank.phase.load()==CharacterPhase::UploadProbe);
    assert(bank.token.load()>201&&bank.ack.load()==201);
    CharacterFrameEnd::Callback(nullptr,0);callback=character_value<void(*)(void*)>(command_buffer,0);callback(command_buffer);
    character_appearance_update(f.actor);assert(bank.phase.load()==CharacterPhase::Active);
    character_appearance_update(f.actor);assert(control.appearance_status==unsigned(re4::CharacterAppearanceStatus::Pending));
    // Retirement requested while the native creator yields must get a NEW fence
    // after all producer uploads, never the earlier load/probe acknowledgement.
    reservation_cancel=true;bank.phase.store(CharacterPhase::Probe);bank.token.store(250);bank.ack.store(250);character_serial.store(250);
    character_store(metadata,4,0xffffffffu);character_store(reference,4,0u);character_appearance_update(f.actor);
    assert(bank.phase.load()==CharacterPhase::Retiring&&bank.token.load()>250);
    character_appearance_collect();assert(bank.phase.load()==CharacterPhase::Retiring&&bank.pinned.load());
    CharacterFrameEnd::Callback(nullptr,0);callback=character_value<void(*)(void*)>(command_buffer,0);callback(command_buffer);
    character_appearance_collect();assert(bank.phase.load()==CharacterPhase::Free&&!bank.pinned.load());
    reservation_alias_tests(f);
    std::puts("character appearance actual callbacks passed");
}
