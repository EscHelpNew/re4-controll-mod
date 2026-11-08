#include "re4/character_textures.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#define CHECK(c) do { if (!(c)) { std::fprintf(stderr,"character textures line %d: %s\n",__LINE__,#c); std::exit(1); } } while(false)
namespace {
using namespace re4;
constexpr std::uint32_t ashley=0x01000001,other=0x01000002,luis=0x00000014;
template<class T> void put(void* p,std::size_t o,T v){std::memcpy(static_cast<unsigned char*>(p)+o,&v,sizeof(v));}
template<class T> T get(const void* p,std::size_t o){T v{};std::memcpy(&v,static_cast<const unsigned char*>(p)+o,sizeof(v));return v;}
struct Fixture {
    std::array<unsigned char,64*16> registry{};
    std::array<unsigned char,3000*64> descriptors{};
    std::array<unsigned char,3000*24> gpu_objects{};
    std::vector<std::uint32_t> released;
    unsigned resets=0,cleanups=0;
    Fixture(){for(unsigned i=0;i<64;++i)put(registry.data(),i*16+8,0xffffffffu);
        for(unsigned i=0;i<3000;++i)put(descriptors.data(),i*64+44,0xffffffffu);}
    void entry(unsigned i,std::uint32_t id,std::uint16_t options=1,std::uint8_t delay=0){
        put(registry.data(),i*16,std::uintptr_t(0x1000+i*0x100));put(registry.data(),i*16+8,id);
        put(registry.data(),i*16+12,options);registry[i*16+14]=delay;registry[i*16+15]=1;}
    void cache(unsigned i,std::uint32_t id){put(descriptors.data(),i*64+44,id);descriptors[i*64]=0x55;}
    CharacterTextureTables tables(std::size_t high_water=3000){return {registry.data(),64,descriptors.data(),3000,high_water,gpu_objects.data(),3000};}
    static void reset(void* p){++static_cast<Fixture*>(p)->resets;}
    static void cleanup(void* p){++static_cast<Fixture*>(p)->cleanups;}
    static void exact(void* p,std::uint32_t id){auto& f=*static_cast<Fixture*>(p);f.released.push_back(id);
        for(unsigned i=0;i<64;++i)if(get<std::uint32_t>(f.registry.data(),i*16+8)==id){
            put(f.registry.data(),i*16+8,0xffffffffu);f.registry[i*16+14]=f.registry[i*16+15]=0;}
        for(unsigned i=0;i<3000;++i)if(get<std::uint32_t>(f.descriptors.data(),i*64+44)==id)
            std::memset(f.descriptors.data()+i*64,0,64);}
    CharacterTextureCallbacks callbacks(){return {this,exact,reset,cleanup};}
    bool resident(unsigned i)const{return registry[i*16+15]!=0;}
};
CharacterTexturePins pins(std::uint32_t a=ashley){return {{{a,0}},1};}
CharacterTextureRequest group(std::uint32_t g=1,std::uint32_t delay=0){return {CharacterTextureOperation::Group,g,delay,0};}
bool has(const CharacterTexturePlan& p,std::uint32_t id){for(std::size_t i=0;i<p.free_count;++i)if(p.free_ids[i]==id)return true;return false;}

// A bulk free that ignores pins would destroy Ashley's live GPU objects.
void group_keeps_shared_pinned_pack_and_frees_registry_and_orphans(){
    Fixture f;f.entry(0,ashley);f.entry(1,other);f.entry(2,luis);f.cache(0,ashley);f.cache(1,other);f.cache(2,0x01000003);
    const auto before=f.registry;const auto p=plan_character_textures(f.tables(),pins(),group());
    CHECK(p.action==CharacterTextureAction::Apply&&p.free_count==2&&!has(p,ashley));
    CHECK(has(p,other)&&has(p,0x01000003)&&p.reset_cache&&!p.sweep_cleanup);
    CHECK(apply_character_texture_plan(p,f.tables(),f.callbacks()));
    CHECK(f.resident(0)&&!f.resident(1)&&f.resident(2)&&f.descriptors[0]==0x55&&f.descriptors[64]==0);
    CHECK(std::memcmp(before.data(),f.registry.data(),16)==0&&f.resets==1&&f.cleanups==0);
}
// Enumeration must snapshot/deduplicate before native deletion mutates the tables.
void duplicates_release_once_and_two_generations_can_pin_one_shared_id(){
    Fixture f;f.entry(0,other);f.entry(1,other);f.cache(0,other);f.cache(1,other);
    CharacterTexturePins shared{{{ashley,ashley}},2};
    const auto p=plan_character_textures(f.tables(),shared,group());CHECK(p.free_count==1&&p.free_ids[0]==other);
    CHECK(apply_character_texture_plan(p,f.tables(),f.callbacks()));CHECK(f.released.size()==1);
}
// Native delay is uint8: 256 is immediate; 257 is one tick, not 257 ticks.
void delay_truncates_to_byte_and_never_updates_pinned_countdown(){
    Fixture f;f.entry(0,ashley,1,9);f.entry(1,other,1,7);f.entry(2,luis,1,8);
    auto p=plan_character_textures(f.tables(),pins(),group(0x101,257));
    CHECK(p.action==CharacterTextureAction::Apply&&p.free_count==0&&p.delay==1&&p.delay_count==1&&!p.reset_cache);
    CHECK(apply_character_texture_plan(p,f.tables(),f.callbacks()));
    CHECK(f.registry[14]==9&&f.registry[30]==1&&f.registry[46]==8&&f.released.empty()&&f.resets==0);
    p=plan_character_textures(f.tables(),pins(),group(1,256));CHECK(has(p,other)&&p.delay_count==0&&p.reset_cache);
}
// Mask selection comes from the registry; orphan cache IDs have no option bits.
void mask_filters_options_without_freeing_descriptor_only_orphans(){
    Fixture f;f.entry(0,ashley,2);f.entry(1,other,2);f.entry(2,0x01000003,1);f.cache(0,0x01000004);
    CharacterTextureRequest r{CharacterTextureOperation::MaskedGroup,1,0,0x10002};
    auto p=plan_character_textures(f.tables(),pins(),r);CHECK(p.free_count==1&&p.free_ids[0]==other);
    r.delay=0x1ff;p=plan_character_textures(f.tables(),pins(),r);CHECK(p.delay==255&&p.delay_count==1&&p.delay_slots[0]==1);
    CHECK(apply_character_texture_plan(p,f.tables(),f.callbacks()));CHECK(f.registry[30]==255&&f.registry[14]==0);
    r.delay=0;r.option_mask=0;p=plan_character_textures(f.tables(),pins(),r);CHECK(p.free_count==0&&p.reset_cache);
}
// A masked duplicate with inconsistent options cannot be faithfully freed per ID.
void conflicting_masked_duplicates_require_retirement_without_side_effects(){
    Fixture f;f.entry(0,other,1);f.entry(1,other,2);const auto before=f.registry;
    CharacterTextureRequest r{CharacterTextureOperation::MaskedGroup,1,0,1};
    const auto p=plan_character_textures(f.tables(),pins(),r);
    CHECK(p.action==CharacterTextureAction::RetireRenderer);
    CHECK(!apply_character_texture_plan(p,f.tables(),f.callbacks())&&f.registry==before&&f.released.empty());
}
// Treating -1 as ALL would free unrelated groups and accidentally remove the pin.
void exact_and_group_minus_one_are_literal_and_unrelated_calls_stay_native(){
    Fixture f;f.entry(0,ashley);f.entry(1,luis);
    auto p=plan_character_textures(f.tables(),pins(),{CharacterTextureOperation::Exact,ashley,0,0});
    CHECK(p.action==CharacterTextureAction::Apply&&p.free_count==0&&!p.reset_cache);
    p=plan_character_textures(f.tables(),pins(),{CharacterTextureOperation::Exact,0xffffffffu,0,0});CHECK(p.action==CharacterTextureAction::Native);
    p=plan_character_textures(f.tables(),pins(),group(0xffffffffu,255));CHECK(p.action==CharacterTextureAction::Native);
    p=plan_character_textures(f.tables(),pins(),group(0,0));CHECK(p.action==CharacterTextureAction::Native);
}
// 222060 preserves five specific native groups; retaining every group >=15 is wrong.
void selective_sweep_keeps_whitelist_and_pins_and_runs_remaining_cleanup(){
    Fixture f;f.entry(0,ashley);f.entry(1,0x0f000001);f.entry(2,0x18000001);f.entry(3,0x46000001);
    f.entry(4,0x49000001);f.entry(5,0x4c000001);f.entry(6,0x10000001);f.entry(7,0x4d000001);
    f.cache(0,0x11000001);f.cache(1,0x12000001);f.cache(2,0x13000001);
    auto p=plan_character_textures(f.tables(2),pins(),{CharacterTextureOperation::SelectiveSweep,0,0,0});
    CHECK(p.free_count==4&&has(p,0x10000001)&&has(p,0x4d000001)&&has(p,0x11000001)&&has(p,0x12000001));
    CHECK(!has(p,0x13000001)&&!has(p,ashley)&&p.sweep_cleanup&&p.reset_cache);
    CHECK(apply_character_texture_plan(p,f.tables(2),f.callbacks()));
    CHECK(f.resident(0)&&f.resident(1)&&f.resident(2)&&f.resident(3)&&f.resident(4)&&f.resident(5));
    CHECK(!f.resident(6)&&!f.resident(7)&&f.cleanups==1&&f.resets==1);
}
// Missing callbacks or invalid native bounds must never partially release ownership.
void invalid_views_pins_and_teardown_request_retirement(){
    Fixture f;auto t=f.tables();t.registry_count=65;
    auto p=plan_character_textures(t,pins(),group());CHECK(p.action==CharacterTextureAction::RetireRenderer);
    t=f.tables();t.descriptor_count=3001;p=plan_character_textures(t,pins(),group());CHECK(p.action==CharacterTextureAction::RetireRenderer);
    t=f.tables();t.sweep_descriptor_count=3001;p=plan_character_textures(t,pins(),{CharacterTextureOperation::SelectiveSweep,0,0,0});CHECK(p.action==CharacterTextureAction::RetireRenderer);
    t=f.tables();t.registry=nullptr;p=plan_character_textures(t,pins(),group());CHECK(p.action==CharacterTextureAction::RetireRenderer);
    auto invalid=pins();invalid.count=3;p=plan_character_textures(f.tables(),invalid,group());CHECK(p.action==CharacterTextureAction::RetireRenderer);
    invalid=pins(0xffffffffu);p=plan_character_textures(f.tables(),invalid,group());CHECK(p.action==CharacterTextureAction::RetireRenderer);
    p=plan_character_textures(f.tables(),pins(),{CharacterTextureOperation::RendererTeardown,0,0,0});
    CHECK(p.action==CharacterTextureAction::RetireRenderer&&!apply_character_texture_plan(p,f.tables(),f.callbacks()));
    f.entry(0,other);p=plan_character_textures(f.tables(),pins(),group());auto cb=f.callbacks();cb.exact_free=nullptr;
    CHECK(!apply_character_texture_plan(p,f.tables(),cb)&&f.resident(0)&&f.resets==0);
}
// Full registry/cache coverage must not truncate deletion at 64 IDs.
void maximum_tables_and_plan_snapshot_are_bounded(){
    Fixture f;for(unsigned i=0;i<64;++i)f.entry(i,0x01001000+i);
    for(unsigned i=0;i<3000;++i)f.cache(i,0x01010000+i);
    const auto p=plan_character_textures(f.tables(),pins(),group());CHECK(p.free_count==3064);
    CHECK(has(p,0x0100103f)&&has(p,0x01010bb7));
    auto changed=f.tables();changed.registry_count=63;CHECK(!apply_character_texture_plan(p,changed,f.callbacks())&&f.resets==0);
}
// Pending and shared ownership must not depend on ptr/ready, or a second pin.
void pending_pins_and_two_distinct_ids_remain_protected(){
    Fixture f;f.entry(0,ashley);f.entry(1,other);f.entry(2,0x01000003);
    put(f.registry.data(),0,std::uintptr_t(0));f.registry[15]=0;
    CharacterTexturePins pair{{{ashley,other}},2};
    auto p=plan_character_textures(f.tables(),pair,group());
    CHECK(p.free_count==1&&p.free_ids[0]==0x01000003);
    CHECK(apply_character_texture_plan(p,f.tables(),f.callbacks()));
    CHECK(get<std::uint32_t>(f.registry.data(),8)==ashley&&f.resident(1));
    CharacterTexturePins none{};p=plan_character_textures(f.tables(),none,group());CHECK(p.action==CharacterTextureAction::Native);
    p=plan_character_textures(f.tables(),none,{CharacterTextureOperation::RendererTeardown,0,0,0});
    CHECK(p.action==CharacterTextureAction::RetireRenderer);
}
// FF is a literal group, including native absent slots for delayed writes.
void ff_delay_and_missing_sweep_cleanup_cannot_partially_delete(){
    Fixture f;f.entry(0,0xff000001,1,7);f.entry(1,0xff000002,1,8);
    auto p=plan_character_textures(f.tables(),pins(0xff000001),group(0xffffffffu,255));
    CHECK(p.action==CharacterTextureAction::Apply&&p.delay_count==63&&p.delay==255);
    CHECK(apply_character_texture_plan(p,f.tables(),f.callbacks()));CHECK(f.registry[14]==7&&f.registry[30]==255&&f.registry[46]==255);
    p=plan_character_textures(f.tables(),pins(0xff000001),group(255,0));CHECK(p.action==CharacterTextureAction::RetireRenderer);
    Fixture sweep;sweep.entry(0,other);p=plan_character_textures(sweep.tables(),pins(),{CharacterTextureOperation::SelectiveSweep,0,0,0});
    auto cb=sweep.callbacks();cb.sweep_cleanup=nullptr;
    CHECK(!apply_character_texture_plan(p,sweep.tables(),cb)&&sweep.resident(0)&&sweep.resets==0&&sweep.released.empty());
}
// Exact free scans all descriptors, whereas native sweep stops at high-water.
void sweep_rejects_selected_ids_with_cache_records_beyond_high_water(){
    Fixture f;f.entry(0,other);f.cache(0,other);f.cache(2,other);
    const auto registry=f.registry;const auto descriptors=f.descriptors;
    const auto p=plan_character_textures(f.tables(2),pins(),{CharacterTextureOperation::SelectiveSweep});
    CHECK(p.action==CharacterTextureAction::RetireRenderer);
    CHECK(p.free_count==0&&p.delay_count==0&&!p.reset_cache&&!p.sweep_cleanup);
    CHECK(!apply_character_texture_plan(p,f.tables(2),f.callbacks()));
    CHECK(f.registry==registry&&f.descriptors==descriptors&&f.released.empty()&&f.resets==0&&f.cleanups==0);
    Fixture orphan;orphan.cache(0,other);orphan.cache(2,other);
    const auto q=plan_character_textures(orphan.tables(2),pins(),{CharacterTextureOperation::SelectiveSweep});
    CHECK(q.action==CharacterTextureAction::RetireRenderer&&q.free_count==0);
}
// FFFFFFFF cannot be passed to exact free: absent registry slots retain old pointers.
void sweep_rejects_absent_owner_with_live_gpu_object(){
    Fixture f;put(f.gpu_objects.data(),8,std::uint64_t(0x1234));
    const auto p=plan_character_textures(f.tables(1),pins(),{CharacterTextureOperation::SelectiveSweep});
    CHECK(p.action==CharacterTextureAction::RetireRenderer);
    CHECK(!apply_character_texture_plan(p,f.tables(1),f.callbacks())&&f.released.empty()&&f.resets==0&&f.cleanups==0);
}
// Zeroed native holes and empty invalid-owner sentinels own no GPU resource.
void sweep_keeps_unused_holes_but_requires_proof_for_invalid_owner(){
    Fixture f;f.entry(0,other);f.cache(0,other);
    // The normal native free/init representation is all 64 descriptor bytes zero.
    std::memset(f.descriptors.data()+64,0,f.descriptors.size()-64);
    put(f.gpu_objects.data(),24,std::uint64_t(0x1111));
    put(f.gpu_objects.data(),24+16,std::uint64_t(0x2222)); // +8 remains null: native no-op.
    auto p=plan_character_textures(f.tables(2),pins(),{CharacterTextureOperation::SelectiveSweep});
    CHECK(p.action==CharacterTextureAction::Apply&&has(p,other)&&has(p,0));
    CHECK(apply_character_texture_plan(p,f.tables(2),f.callbacks()));
    CHECK(get<std::uint64_t>(f.gpu_objects.data(),24)==0x1111&&get<std::uint64_t>(f.gpu_objects.data(),24+16)==0x2222);
    put(f.gpu_objects.data(),2*24+8,std::uint64_t(0x3333));
    p=plan_character_textures(f.tables(2),pins(),{CharacterTextureOperation::SelectiveSweep});
    CHECK(p.action==CharacterTextureAction::RetireRenderer&&p.free_count==0);

    Fixture empty;auto t=empty.tables(1);
    p=plan_character_textures(t,pins(),{CharacterTextureOperation::SelectiveSweep});
    CHECK(p.action==CharacterTextureAction::Apply&&p.free_count==0);
    t.gpu_objects=nullptr;t.gpu_object_count=0;
    p=plan_character_textures(t,pins(),{CharacterTextureOperation::SelectiveSweep});
    CHECK(p.action==CharacterTextureAction::RetireRenderer);
    empty.descriptors[0]=1;
    p=plan_character_textures(empty.tables(1),pins(),{CharacterTextureOperation::SelectiveSweep});
    CHECK(p.action==CharacterTextureAction::RetireRenderer);
}
void sweep_gpu_view_is_bounded_and_part_of_execution_snapshot(){
    Fixture f;auto t=f.tables(1);t.gpu_object_count=3001;
    auto p=plan_character_textures(t,pins(),{CharacterTextureOperation::SelectiveSweep});
    CHECK(p.action==CharacterTextureAction::RetireRenderer);
    t=f.tables(1);t.gpu_objects=nullptr;
    p=plan_character_textures(t,pins(),{CharacterTextureOperation::SelectiveSweep});
    CHECK(p.action==CharacterTextureAction::RetireRenderer);
    p=plan_character_textures(f.tables(1),pins(),{CharacterTextureOperation::SelectiveSweep});
    t=f.tables(1);--t.gpu_object_count;
    CHECK(!apply_character_texture_plan(p,t,f.callbacks())&&f.resets==0&&f.cleanups==0&&f.released.empty());
    // A missing proof for a prefix FF record is irrelevant to unrelated groups.
    t=f.tables(1);t.gpu_objects=nullptr;t.gpu_object_count=0;
    p=plan_character_textures(t,pins(),group(2));CHECK(p.action==CharacterTextureAction::Native);
}
}
int main(){
    sweep_rejects_selected_ids_with_cache_records_beyond_high_water();sweep_rejects_absent_owner_with_live_gpu_object();
    sweep_keeps_unused_holes_but_requires_proof_for_invalid_owner();sweep_gpu_view_is_bounded_and_part_of_execution_snapshot();
    group_keeps_shared_pinned_pack_and_frees_registry_and_orphans();duplicates_release_once_and_two_generations_can_pin_one_shared_id();
    delay_truncates_to_byte_and_never_updates_pinned_countdown();mask_filters_options_without_freeing_descriptor_only_orphans();
    conflicting_masked_duplicates_require_retirement_without_side_effects();exact_and_group_minus_one_are_literal_and_unrelated_calls_stay_native();
    selective_sweep_keeps_whitelist_and_pins_and_runs_remaining_cleanup();invalid_views_pins_and_teardown_request_retirement();
    maximum_tables_and_plan_snapshot_are_bounded();pending_pins_and_two_distinct_ids_remain_protected();
    ff_delay_and_missing_sweep_cleanup_cannot_partially_delete();
    std::puts("character textures: pin-aware native selection, delays, sweep, bounded ownership PASS");
}
