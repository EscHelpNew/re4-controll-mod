// Loading boundaries use the real module callbacks and native table layouts.
#define main game_module_baseline_main
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#include "game_module_tests.cpp"
#pragma GCC diagnostic pop
#undef main
#include "re4/character_loading.hpp"

namespace {
struct LoadingFixture : Fixture {
    alignas(32) std::uint8_t part[0x1A8]{},bin[0x98]{};
    LoadingFixture(){
        character_appearance_reset();view_mode();character_renderer_ready.store(true);
        bin[0x29]=119;actor[0x11A]=119;
        character_store(actor,0x180,static_cast<void*>(part));character_store(part,0x18,static_cast<void*>(bin));
        character_store(actor,0x108,static_cast<void*>(bin));
        game_test::memory_maps={{main_base,image.size(),1},{reinterpret_cast<std::uintptr_t>(actor),sizeof(actor),1},
            {reinterpret_cast<std::uintptr_t>(input_state.data()),input_state.size(),1},
            {reinterpret_cast<std::uintptr_t>(part),sizeof(part),1},{reinterpret_cast<std::uintptr_t>(bin),sizeof(bin),1}};
        character_appearance_mount("rom");
        settings.character_appearance=character_desired=re4::CharacterAppearance::Ashley;
    }
    CharacterBank& probe(){
        auto& b=character_banks[0];assert(character_identity(actor,b.identity));
        b.appearance=re4::CharacterAppearance::Ashley;b.pack.store(0x01000001);b.pinned.store(true);
        b.token.store(100);b.ack.store(0);b.load_result.store(0);b.phase.store(CharacterPhase::Probe);
        return b;
    }
    void failed_texture(unsigned stage){
        character_appearance_update(actor);character_appearance_update(actor);
        assert(control.appearance_status==unsigned(re4::CharacterAppearanceStatus::Failed));
        assert(re4::character_appearance_reason(control.appearance_error)==re4::CharacterAppearanceError::TextureUnavailable);
        if(((control.appearance_error>>16)&255)!=stage)
            std::fprintf(stderr,"loading boundary: expected stage%u, actual stage%u (observed%u)\n",stage,
                unsigned((control.appearance_error>>16)&255),unsigned(control.appearance_error>>24));
        assert(((control.appearance_error>>16)&255)==stage);
    }
};
unsigned loading_context=0,loading_native_calls=0,loading_creators=0;
void* loading_context_lookup(unsigned id){assert(id==0);return &loading_context;}
void loading_native(void*,unsigned,unsigned){++loading_native_calls;}
alignas(16) std::uint8_t loading_command[0x28]{};
void make_load_command(CharacterBank& b){
    std::memset(loading_command,0,sizeof(loading_command));
    character_store(loading_command,0x10,&b);character_store(loading_command,0x18,b.token.load());character_store(loading_command,0x20,0x22u);
    character_native.context=loading_context_lookup;character_native.load_texture=loading_native;
}
void configure_mapping(LoadingFixture& f){
    character_store(f.image.data(),0x11807D4,std::uint8_t(1));
    character_store(f.image.data(),0x11807D8+0x22*4,0x01000001u);
    character_store(f.image.data(),0x1180D40+0x158,reinterpret_cast<void*>(0x7000));
    for(unsigned i=0;i<64;++i)character_store(f.image.data(),0x11829B0+i*16+8,0xffffffffu);
    character_pack_budget_native.close=+[](void*){};character_pack_budget_native.prefix=+[]{return "bio4";};
    character_pack_budget_native.reserve=+[](unsigned,const char*,unsigned)->void*{return nullptr;};
    character_pack_budget_native.release=+[](void*){};character_pack_budget_native.reap=+[]{return 0;};
}
struct ReservationFixture : LoadingFixture {
    alignas(16) std::uint8_t blob[16]{},metadata[32]{},tpl[16]{},table[16]{},texture[0x28]{},reference[8]{};
    ReservationFixture(){
        auto& b=probe();b.load_result.store(1);b.ack.store(100);
        character_store(blob,0,0x01000001u);character_store(blob,4,1u);character_store(blob,8,static_cast<void*>(metadata));
        character_store(metadata,0,16u);character_store(metadata,4,0xffffffffu);character_store(metadata,8,0x01000001u);
        character_store(image.data(),0x11829B0,static_cast<void*>(blob));
        character_store(image.data(),0x11829B0+8,0x01000001u);image[0x11829B0+15]=1;
        game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(blob),sizeof(blob),1});
        game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(metadata),sizeof(metadata),1});
        b.ready.texture_count=1;b.ready.texture_ids[0]=0;b.ready.piece_count=1;b.ready.pieces[0].tpl={tpl,sizeof(tpl)};
        character_store(tpl,4,1u);character_store(tpl,8,static_cast<void*>(table));character_store(table,0,static_cast<void*>(texture));
        character_store(texture,0,std::uint16_t(4));character_store(texture,2,std::uint16_t(4));
        character_store(texture,8,static_cast<void*>(reference));character_store(reference,0,0x01000001u);
        character_native.texture_object=+[](void*,const void*,unsigned,unsigned,unsigned,unsigned,unsigned,unsigned){++loading_creators;};
    }
};
}
int main(){
    assert(re4::character_loading_percent(0,1312768)==0);
    assert(re4::character_loading_percent(262144,1312768)==19);
    assert(re4::character_loading_percent(524288,1312768)==39);
    assert(re4::character_loading_percent(1312768,1312768)==100);
    assert(re4::character_loading_percent(4,0)==0);
    assert(re4::character_loading_mebibytes(std::size_t(128)*1024*1024-1)==127);
    const auto heap_reason=re4::character_loading_diagnostic(re4::CharacterLoadingStage::Headroom,500);
    assert(re4::character_loading_stage(heap_reason)==re4::CharacterLoadingStage::Headroom);
    assert(re4::character_loading_observed(heap_reason)==255);
    {
        LoadingFixture f;
        const auto* recipe=re4::character_asset_recipe(re4::CharacterAppearance::Ashley);
        game_test::resource_path="rom:/bio4/em/pl01.udas";game_test::resource_file.assign(recipe->size,0);
        character_appearance_update(f.actor);
        assert(game_test::fs_reads==1&&character_banks[0].read==256*1024);
        assert(re4::character_appearance_reason(control.appearance_error)==re4::CharacterAppearanceError::ReadingAsset);
        assert(re4::character_appearance_observed(control.appearance_error)==19);
        // Draw/fence collection can continue without a player-update callback.
        // This is the relevant contract when UI pauses the simulation.
        for(unsigned i=0;i<20;++i){CharacterFrameEnd::Callback(nullptr,0);character_appearance_collect();}
        assert(game_test::fs_reads==1&&character_banks[0].read==256*1024);
        character_appearance_update(f.actor);
        assert(control.appearance_status==unsigned(re4::CharacterAppearanceStatus::Pending));
        assert(re4::character_appearance_reason(control.appearance_error)==re4::CharacterAppearanceError::ReadingAsset);
        assert(re4::character_appearance_observed(control.appearance_error)==39);
        assert(game_test::fs_reads==2&&character_banks[0].read==512*1024);
        // Selecting a different skin mid-read closes the old file before a new
        // native pack can be admitted; partial archives are never constructed.
        settings.character_appearance=re4::CharacterAppearance::Leon;
        character_appearance_update(f.actor);
        assert(game_test::fs_closes==1&&character_banks[0].phase.load()==CharacterPhase::Free);
        assert(control.appearance_status==unsigned(re4::CharacterAppearanceStatus::Native));
    }
    {
        LoadingFixture f;auto& b=f.probe();make_load_command(b);
        character_native.context=nullptr;
        character_appearance_load_command(loading_command);
        assert(b.load_result.load()==2&&loading_native_calls==0);f.failed_texture(1);
    }
    {
        LoadingFixture f;auto& b=f.probe();configure_mapping(f);make_load_command(b);
        // Full native registry is a permanent admission refusal, not IO busy.
        for(unsigned i=0;i<64;++i)character_store(f.image.data(),0x11829B0+i*16+8,0x01000100u+i);
        character_appearance_load_command(loading_command);
        assert(b.load_result.load()==2&&loading_native_calls==0);f.failed_texture(3);
    }
    {
        LoadingFixture f;auto& b=f.probe();configure_mapping(f);make_load_command(b);
        // Busy IO is already retryable and must not be converted to error10.
        character_store(f.image.data(),0x1184300+0x50,1u);
        character_appearance_load_command(loading_command);
        assert(b.load_result.load()==5&&loading_native_calls==0);
        character_appearance_update(f.actor);
        assert(control.appearance_status==unsigned(re4::CharacterAppearanceStatus::Pending));
        assert(control.appearance_error==115&&character_error==re4::CharacterAppearanceError::None);
    }
    {
        LoadingFixture f;auto& b=f.probe();configure_mapping(f);make_load_command(b);
        auto* entry=f.image.data()+0x11829B0;auto* request=f.image.data()+0x1184300;auto* info=f.image.data()+0x11849E0;
        character_store(entry,0,static_cast<void*>(f.bin));character_store(entry,8,0x01000001u);entry[15]=0;
        character_store(request,0,static_cast<void*>(info));character_store(info,0x40,static_cast<void*>(entry));
        character_store(request,8,static_cast<void*>(f.bin));character_store(request,0x18,reinterpret_cast<void*>(main_base+0x24E300));
        character_store(request,0x50,0u); // Exact borrowed request ended without ready.
        character_appearance_load_command(loading_command);
        assert(b.load_result.load()==3&&loading_native_calls==0);f.failed_texture(16);
    }
    {
        ReservationFixture f;
        character_store(f.blob,8,static_cast<void*>(nullptr));
        // The ready pack's missing metadata pointer is a reservation failure,
        // independently of native loading/admission.
        f.failed_texture(18);assert(loading_creators==0);
    }
    {
        ReservationFixture f;
        for(unsigned i=0;i<3000;++i)character_store(f.image.data()+0x11519A0+i*0x40,8,1u);
        f.failed_texture(21);assert(re4::character_loading_observed(control.appearance_error)==1);
        assert(loading_creators==0);
    }
    {
        ReservationFixture f;character_store(f.metadata,4,0u);
        character_store(f.image.data()+0x11519A0,0x2C,0x01000002u);
        f.failed_texture(20);assert(loading_creators==0);
    }
    {
        ReservationFixture f;f.failed_texture(22);assert(loading_creators==1);
    }
    std::puts("character loading: chunked reads, cancellation, admission, busy retry, IO and reservation boundaries PASS");
}
