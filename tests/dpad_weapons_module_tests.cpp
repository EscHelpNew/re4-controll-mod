#define main game_module_baseline_main
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#include "game_module_tests.cpp"
#pragma GCC diagnostic pop
#undef main
#include "re4/weapon_cycle.hpp"
#include "native_popup_memory.hpp"

namespace {
std::vector<unsigned> equip_calls;
void* equip_expected_actor=nullptr;
void* equip_expected_slot=nullptr;
bool equip_select_success=true,equip_mapping_valid=true,equip_reenter=false,equip_load_changes_scene=false;
unsigned equip_effective_override=0,equip_post_id=0,equip_preflight_change=0;
void* equip_rollback_slot=nullptr;
unsigned equip_readonly_calls=0;
float* equip_pose_root=nullptr;
float* equip_pose_leg=nullptr;
void equip_classify(unsigned raw,void* scratch){
    ++equip_readonly_calls;
    std::memset(scratch,0,6);static_cast<std::uint8_t*>(scratch)[2]=raw==50?6:raw==99||re4::weapon_cycle_grenade(raw)?3:1;
}
unsigned equip_effective(void* manager,const void* slot){
    ++equip_readonly_calls;
    if(slot==equip_expected_slot&&equip_preflight_change){
        if(equip_preflight_change==1)static_cast<std::uint8_t*>(equip_expected_actor)[0x115]=6;
        if(equip_preflight_change==2)static_cast<std::uint8_t*>(const_cast<void*>(slot))[6]^=1;
        if(equip_preflight_change==3)*reinterpret_cast<void**>(static_cast<std::uint8_t*>(manager)+0x10)=nullptr;
    }
    return equip_effective_override?equip_effective_override:*static_cast<const std::uint16_t*>(slot);
}
unsigned equip_weapon(unsigned raw){++equip_readonly_calls;return !equip_mapping_valid||raw==0x7FF?255:raw==1?0x13:raw==2?0x16:raw==14?0x17:raw;}
unsigned equip_variant(unsigned){++equip_readonly_calls;return equip_mapping_valid?0:255;}
int equip_select(void* manager,void* slot){
    equip_calls.push_back(1);assert(slot==equip_expected_slot||slot==equip_rollback_slot);
    assert(!modern_body_pose.pending()&&!modern_pose.pending());
    if(equip_pose_root)assert(*equip_pose_root==0);
    if(equip_pose_leg)assert(*equip_pose_leg==0);
    if(!equip_select_success)return 0;
    auto* m=static_cast<std::uint8_t*>(manager);*reinterpret_cast<void**>(m+0x10)=slot;
    if(!slot){*reinterpret_cast<std::uint16_t*>(m+0x18)=0x800;return 1;}
    *reinterpret_cast<std::uint16_t*>(m+0x18)=std::uint16_t(slot==equip_expected_slot&&equip_post_id?equip_post_id:equip_effective(manager,slot));
    //294600 owns these packed bits; preserving them over native selection would
    // override the native inventory owner. The adapter must save +8&7 afterward.
    static_cast<std::uint8_t*>(slot)[8]&=0xF8;
    return 1;
}
void equip_prepare(unsigned mode){assert(mode==2);equip_calls.push_back(2);}
void equip_teardown(void* actor){assert(actor==equip_expected_actor);equip_calls.push_back(3);}
void equip_load(void* actor,unsigned weapon,unsigned variant){
    assert(actor==equip_expected_actor&&weapon==equip_weapon(equip_post_id?equip_post_id:equip_effective(nullptr,equip_expected_slot))&&variant==0);equip_calls.push_back(4);
    auto* state=*reinterpret_cast<std::uint8_t**>(main_base+0xB1AE08);state[0x5034]=std::uint8_t(weapon);state[0x5036]=0xFF;
    if(equip_load_changes_scene){settings.dpad_weapon_switching=false;state[0x86FF]=1;*reinterpret_cast<std::uint8_t*>(main_base+0x1131718+0x54)=16;}
    if(equip_reenter){dpad_weapons_before_update(actor);dpad_weapons_update(actor);}
}
void equip_initialize(void* actor){
    assert(actor==equip_expected_actor);equip_calls.push_back(5);
    auto* state=*reinterpret_cast<std::uint8_t**>(main_base+0xB1AE08);
    assert(state[0x5036]==(static_cast<const std::uint8_t*>(equip_expected_slot)[8]&7));
}
void equip_reset(void* actor){assert(actor==equip_expected_actor);equip_calls.push_back(6);}
struct EquipFixture {
    Fixture f;
    std::array<std::uint8_t,14*8> slots{};
    EquipFixture(){
        f.view_mode();sensor_attributes=1;settings.dpad_weapon_switching=true;
        weapon_popup_clear();publish_view_settings();
        *reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;
        auto* m=manager();*reinterpret_cast<void**>(m+0x20)=slots.data();*reinterpret_cast<int*>(m+0x30)=8;
        put(0,4);put(1,4,1,0x12345678);put(2,99);put(3,1);put(4,2);put(5,14);put(6,50);put(7,7,0);
        *reinterpret_cast<void**>(m+0x10)=slot(0);*reinterpret_cast<std::uint16_t*>(m+0x18)=4;
        game_test::memory_maps={{main_base,f.image.size(),3},{reinterpret_cast<std::uintptr_t>(f.actor),sizeof(f.actor),3},
            {reinterpret_cast<std::uintptr_t>(f.input_state.data()),f.input_state.size(),3},
            {reinterpret_cast<std::uintptr_t>(slots.data()),slots.size(),3}};
        dpad_weapons_native={equip_classify,equip_effective,equip_weapon,equip_variant,equip_select,equip_prepare,
            equip_teardown,equip_load,equip_initialize,equip_reset};
        equip_calls.clear();equip_expected_actor=f.actor;equip_expected_slot=slot(1);equip_select_success=equip_mapping_valid=true;equip_reenter=equip_load_changes_scene=false;
        equip_effective_override=equip_post_id=equip_preflight_change=0;equip_rollback_slot=nullptr;
        equip_readonly_calls=0;equip_pose_root=equip_pose_leg=nullptr;
        dpad_weapons_filter(0,false);dpad_weapons_filter(0,true);
    }
    std::uint8_t* manager(){return f.image.data()+0x124D918;}
    std::uint8_t* slot(unsigned i){return slots.data()+14*i;}
    void put(unsigned i,unsigned id,unsigned qty=1,unsigned params=0){
        auto* p=slot(i);std::uint16_t raw=std::uint16_t(id),quantity=std::uint16_t(qty);
        std::memcpy(p,&raw,2);std::memcpy(p+2,&quantity,2);p[4]=1;p[5]=0;std::memcpy(p+6,&params,4);
    }
    void edge(std::uint64_t key=1u<<14){assert(dpad_weapons_filter(key|1,true)==1);}
    void tick(){ModernPlayerUpdate::Callback(f.actor);}
    void* selected(){return *reinterpret_cast<void**>(manager()+0x10);}
    void expect_transaction(){assert((equip_calls==std::vector<unsigned>{1,2,3,4,5,6}));assert(selected()==equip_expected_slot);}
};
}
int main(){
    {
        // End to end: a real weapon item ID must survive the actual D-pad
        // transaction, publish a label and render after the native HUD list.
        EquipFixture h;NativePopupMemory popup_memory;popup_memory.bind(h.f);h.put(0,35);h.put(1,37);
        *reinterpret_cast<std::uint16_t*>(h.manager()+0x18)=35;
        game_test::tick=100;h.edge();h.tick();h.expect_transaction();
        WeaponPopupEvent event;assert(weapon_popup_read(event)&&event.raw_id==37);
        native_gx_begin=+[](unsigned primitive,unsigned format,unsigned count){
            assert(hud_native_calls&&primitive==0x90&&format==0&&count%6==0);};
        native_gx_position=+[](float,float,float){++gx_vertices;};
        native_gx_color=+[](unsigned,unsigned,unsigned,unsigned){};
        native_gx_end=+[](unsigned token){assert(token==0x34);};
        game_test::tick+=9'600'000;HudDraw::Callback(nullptr);assert(gx_vertices>0);
    }
    {
        EquipFixture h;WeaponPopupEvent event;
        assert(!weapon_popup_read(event));
        game_test::tick=100;h.edge();h.tick();h.expect_transaction();
        assert(weapon_popup_read(event)&&event.raw_id==4&&event.tick==100
            &&event.actor==reinterpret_cast<std::uintptr_t>(h.f.actor));
        dpad_weapons_filter(0,true);equip_expected_slot=h.slot(2);
        game_test::tick=200;equip_calls.clear();h.edge();h.tick();h.expect_transaction();
        assert(weapon_popup_read(event)&&event.raw_id==99&&event.tick==200);
        settings.dpad_weapon_switching=false;publish_view_settings();
        assert(!weapon_popup_read(event));
        settings.dpad_weapon_switching=true;publish_view_settings();
        assert(!weapon_popup_read(event));
    }
    for(bool failed:{false,true}){
        EquipFixture h;WeaponPopupEvent event;
        equip_select_success=!failed;equip_load_changes_scene=!failed;
        h.edge();h.tick();assert(!weapon_popup_read(event));
    }
    {
        EquipFixture h;WeaponPopupEvent event;
        equip_expected_slot=h.slot(3);h.edge(1u<<13);h.tick();h.expect_transaction();
        assert(weapon_popup_read(event)&&event.raw_id==1);
        h.f.image[0x1131718+0x54]=16;h.tick();
        assert(!weapon_popup_read(event));
    }
    for(float pitch:{-.37f,.37f})for(bool grenade:{false,true}){
        EquipFixture h;settings.unified_controls=true;
        *reinterpret_cast<float*>(h.f.input_state.data()+0xA4)=1;
        publish_view_settings();
        unified_pitch_actor=reinterpret_cast<std::uintptr_t>(h.f.actor);
        unified_pitch_character=0;unified_pitch_route=1;unified_pitch=pitch;
        free_orbit={};const re4::CameraOffsets orbit{.2f,.45f};
        free_orbit.update(1234,1920000,orbit,.5f,0);
        if(grenade)equip_expected_slot=h.slot(3);
        h.edge(1u<<(grenade?13:14));h.tick();h.expect_transaction();
        // Equipping replaces weapon/animation owners, not the player's view.
        assert(unified_pitch_actor==reinterpret_cast<std::uintptr_t>(h.f.actor));
        assert(unified_pitch_route==1&&unified_pitch==pitch);
        equip_calls.clear();h.tick();
        assert(unified_pitch==pitch&&equip_calls.empty());
        const auto continued=free_orbit.update(1234,2112000,orbit,.5f,0);
        assert(continued.yaw<orbit.yaw-.01f&&continued.pitch==orbit.pitch);
    }
    {
        EquipFixture h;auto before=h.slots;before[14+8]&=0xF8;
        h.edge();assert(equip_calls.empty()&&!equip_readonly_calls);h.tick();h.expect_transaction();
        assert(before==h.slots); // Only the native setter's documented packed-bit clear.
        equip_calls.clear();h.edge();h.tick();assert(equip_calls.empty()); // Held direction.
        dpad_weapons_filter(0,true);equip_expected_slot=h.slot(2);h.edge();h.tick();h.expect_transaction(); // Launcher class3.
    }
    for(auto raw:{1u,2u,14u}){
        EquipFixture h;equip_expected_slot=h.slot(raw==1?3:raw==2?4:5);
        if(raw!=1){*reinterpret_cast<void**>(h.manager()+0x10)=h.slot(raw==2?3:4);*reinterpret_cast<std::uint16_t*>(h.manager()+0x18)=std::uint16_t(raw==2?1:2);}
        const auto before=h.slots;h.edge(1u<<13);h.tick();h.expect_transaction();assert(before==h.slots);
    }
    for(bool other_type:{true,false}){
        EquipFixture h;h.put(4,1);h.put(5,2,other_type?1:0);
        *reinterpret_cast<void**>(h.manager()+0x10)=h.slot(3);*reinterpret_cast<std::uint16_t*>(h.manager()+0x18)=1;
        equip_expected_slot=h.slot(5);h.edge(1u<<13);h.tick();
        if(other_type)h.expect_transaction();else assert(equip_calls.empty());
    }
    {
        EquipFixture h;equip_expected_slot=h.slot(2);h.edge(1u<<12);h.tick();h.expect_transaction();
    }
    {
        EquipFixture h;equip_expected_slot=h.slot(5);h.edge(1u<<15);h.tick();h.expect_transaction();
    }
    for(unsigned gate=0;gate<16;++gate){
        EquipFixture h;h.edge();
        if(gate==0)settings.dpad_weapon_switching=false;
        if(gate==1)h.f.input_state[0x5092]=4;
        if(gate==2)h.f.input_state[0x509E]=0x80;
        if(gate==3)h.f.input_state[0x1AB]=0x40;
        if(gate==4)h.f.input_state[0x86FF]=1;
        if(gate==5)h.f.image[0x1131718+0x54]=16;
        if(gate==6)h.f.image[0x1131718+0x48]=0;
        if(gate==7)h.f.actor[0x114]=1;
        if(gate==8)h.f.input_state[0x503C]=1;
        if(gate==9)*reinterpret_cast<std::uint32_t*>(h.f.input_state.data()+0x5090)=0x60004000;
        if(gate==10)h.f.input_state[0x5090]=0x40;
        if(gate==11)sensor_attributes=0;
        if(gate==12)h.f.image[0x11C7D55]=1;
        if(gate==13)*reinterpret_cast<int*>(h.f.image.data()+0xAFF39C)=0;
        if(gate==14)*reinterpret_cast<void**>(h.manager()+0x10)=h.slot(2);
        if(gate==15)h.manager()[0x1B]=1;
        h.tick();assert(equip_calls.empty());
    }
    {
        EquipFixture h;h.f.actor[0x115]=6;h.edge();h.tick();assert(equip_calls.empty());
        h.f.actor[0x115]=1;h.tick();h.expect_transaction();
    }
    {
        EquipFixture h;h.f.actor[0x115]=6;h.edge();dpad_weapons_before_update(h.f.actor);
        h.f.actor[0x115]=1;dpad_weapons_update(h.f.actor);assert(equip_calls.empty());
        h.tick();h.expect_transaction(); // Both sides of native update must be ordinary.
    }
    {
        EquipFixture h;h.edge();dpad_weapons_before_update(h.f.actor);
        h.f.actor[0x115]=6;dpad_weapons_update(h.f.actor);assert(equip_calls.empty());
        assert(!dpad_weapons_queue_lock.test_and_set());dpad_weapons_before_update(h.f.actor);
        dpad_weapons_queue_lock.clear();h.f.actor[0x115]=1;dpad_weapons_update(h.f.actor);
        assert(equip_calls.empty()); // Lock contention must not reuse an earlier safe pre-update.
        h.tick();h.expect_transaction();
    }
    for(auto mask:{0x4000u,0x20000000u,0x40000000u,0x40u}){
        EquipFixture h;h.edge();dpad_weapons_before_update(h.f.actor);
        *reinterpret_cast<std::uint32_t*>(h.f.input_state.data()+0x5090)=mask;
        dpad_weapons_update(h.f.actor);assert(equip_calls.empty());
        *reinterpret_cast<std::uint32_t*>(h.f.input_state.data()+0x5090)=0;
        h.tick();assert(equip_calls.empty()); // Same-frame menu/scope request discards the edge.
    }
    {
        EquipFixture h;h.f.actor[0x115]=6;h.edge();for(unsigned i=0;i<31;++i)h.tick();
        h.f.actor[0x115]=1;h.tick();assert(equip_calls.empty());
    }
    {
        EquipFixture h;h.edge();update_state_change=1;h.tick();assert(equip_calls.empty());update_state_change=0;
    }
    for(unsigned gate=0;gate<8;++gate){
        EquipFixture h;h.edge();
        if(gate==0)*reinterpret_cast<int*>(h.manager()+0x30)=385;
        if(gate==1)*reinterpret_cast<int*>(h.manager()+0x30)=-1;
        if(gate==2)*reinterpret_cast<void**>(h.manager()+0x20)=nullptr;
        if(gate==3)game_test::memory_maps.back().size=13;
        if(gate==4)game_test::memory_maps.back().perm=1;
        if(gate==5)equip_mapping_valid=false;
        if(gate==6)dpad_weapons_native.effective_id=nullptr;
        if(gate==7)*reinterpret_cast<void**>(h.manager()+0x10)=h.slot(0)+1;
        h.tick();assert(equip_calls.empty());
    }
    {
        EquipFixture h;assert(dpad_weapons_filter((1u<<14)|(1u<<13),true)==0);h.tick();assert(equip_calls.empty());
        assert(dpad_weapons_filter(1u<<14,true)==0);h.tick();assert(equip_calls.empty()); // Release one diagonal key is not a new edge.
    }
    {
        EquipFixture h;h.edge();assert(dpad_weapons_filter((1u<<14)|(1u<<13),true)==0);
        h.tick();assert(equip_calls.empty()); // A later diagonal also cancels a queued request.
    }
    {
        EquipFixture h;settings.dpad_weapon_switching=false;assert(dpad_weapons_filter(1u<<14,true)==1u<<14);
        settings.dpad_weapon_switching=true;h.edge();h.tick();assert(equip_calls.empty()); // Option change never invents an edge.
    }
    {
        EquipFixture h;h.f.image[0x1131718+0x54]=16;assert(dpad_weapons_filter(1u<<14,true)==1u<<14);
        h.f.image[0x1131718+0x54]=10;h.edge();h.tick();assert(equip_calls.empty()); // Menu-held edge stays native.
    }
    {
        EquipFixture h;h.edge();dpad_weapons_filter(0,false);h.tick();assert(equip_calls.empty());
    }
    {
        EquipFixture h;h.edge();equip_select_success=false;h.tick();assert((equip_calls==std::vector<unsigned>{1}));
    }
    {
        EquipFixture h;equip_reenter=true;h.edge();h.tick();h.expect_transaction();
    }
    {
        EquipFixture h;equip_reenter=equip_load_changes_scene=true;h.edge();h.tick();h.expect_transaction();
    }
    for(unsigned change=0;change<5;++change){
        EquipFixture h;h.edge();
        if(change==0)h.slot(0)[0]=8; // Same address was replaced with a different item.
        if(change==1)h.slot(0)[6]^=1; // Same item/instance parameters changed while queued.
        if(change==2)h.slot(0)[4]=0;
        if(change==3)h.slot(0)[5]=1;
        if(change==4)*reinterpret_cast<int*>(h.manager()+0x30)=7;
        h.tick();assert(equip_calls.empty());
    }
    for(unsigned change=1;change<=3;++change){
        EquipFixture h;h.edge();equip_preflight_change=change;h.tick();assert(equip_calls.empty());
    }
    {
        EquipFixture h;equip_post_id=99;h.edge();h.tick();h.expect_transaction();
    }
    {
        EquipFixture h;equip_post_id=0x7FF;equip_rollback_slot=h.slot(0);h.edge();h.tick();
        assert((equip_calls==std::vector<unsigned>{1,1}));assert(h.selected()==h.slot(0));
    }
    {
        EquipFixture h;h.put(1,4,0);h.put(2,99,0);h.edge();h.tick();assert(equip_calls.empty());
    }
    {
        EquipFixture h;std::array<std::uint8_t,384*14> full{};
        std::memcpy(full.data(),h.slot(0),14);std::memcpy(full.data()+383*14,h.slot(2),14);
        *reinterpret_cast<void**>(h.manager()+0x20)=full.data();*reinterpret_cast<int*>(h.manager()+0x30)=384;
        *reinterpret_cast<void**>(h.manager()+0x10)=full.data();
        game_test::memory_maps.back()={reinterpret_cast<std::uintptr_t>(full.data()),full.size(),3};
        equip_expected_slot=full.data()+383*14;h.edge();h.tick();h.expect_transaction();
    }
    for(bool rollback:{false,true}){
        EquipFixture h;*reinterpret_cast<void**>(h.manager()+0x10)=nullptr;
        *reinterpret_cast<std::uint16_t*>(h.manager()+0x18)=0x800;
        equip_expected_slot=h.slot(0);if(rollback)equip_post_id=0x7FF;
        h.edge();h.tick();
        if(rollback){assert((equip_calls==std::vector<unsigned>{1,1}));assert(h.selected()==nullptr);assert(*reinterpret_cast<std::uint16_t*>(h.manager()+0x18)==0x800);}
        else h.expect_transaction();
    }
    for(unsigned change=0;change<4;++change){
        EquipFixture h;
        if(change==0)h.put(0,4,0);
        if(change==1)h.slot(0)[4]=0;
        if(change==2){h.put(0,0x7FF);*reinterpret_cast<std::uint16_t*>(h.manager()+0x18)=0x7FF;}
        if(change==3){*reinterpret_cast<void**>(h.manager()+0x10)=nullptr;*reinterpret_cast<std::uint16_t*>(h.manager()+0x18)=4;}
        h.edge();h.tick();assert(equip_calls.empty());
    }
    for(bool valid_owner:{true,false}){
        EquipFixture h;
        alignas(16) std::uint8_t bones[26][0x1E0]{};
        const auto store=[]<class T>(std::uint8_t* p,unsigned offset,T value){std::memcpy(p+offset,&value,sizeof(value));};
        store(h.f.actor,0x108,reinterpret_cast<std::uintptr_t>(bones));
        store(h.f.actor,0x428,reinterpret_cast<std::uintptr_t>(bones));
        store(h.f.actor,0x548,reinterpret_cast<std::uintptr_t>(bones));h.f.actor[0x11A]=26;
        const int parents[]={-1,0,1,2,3,2,5,5,7,8,9,2,11,11,13,14,15,0,17,18,19,20,17,22,23,24};
        for(unsigned i=0;i<26;++i){
            store(bones[i],0x78,reinterpret_cast<std::uintptr_t>(i?bones[parents[i]]:h.f.actor));
            store(bones[i],0x108,i<25?reinterpret_cast<std::uintptr_t>(bones[i+1]):std::uintptr_t(0));
        }
        game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(bones),sizeof(bones),3});
        h.edge();dpad_weapons_before_update(h.f.actor);
        re4::ModernMoveIdentity owner;std::array<re4::ModernMoveBone,9> legs;re4::ModernMoveBone root;
        assert(modern_move_collect(h.f.actor,owner,legs,&root));
        assert(modern_body_pose.begin(owner,{root}));root.local[0]=17;assert(modern_body_pose.finish());
        assert(modern_pose.begin(owner,legs));legs[0].local[0]=24;assert(modern_pose.finish());
        if(valid_owner){equip_pose_root=root.local;equip_pose_leg=legs[0].local;}
        else store(h.f.actor,0x548,reinterpret_cast<std::uintptr_t>(bones)+16);
        dpad_weapons_update(h.f.actor);h.expect_transaction();
        assert(root.local[0]==(valid_owner?0:17)&&legs[0].local[0]==(valid_owner?0:24));
        assert(!modern_body_pose.pending()&&!modern_pose.pending());
    }
    {
        EquipFixture h;settings.remap_enabled=true;settings.button_map[14]=0;settings.button_map[0]=14;
        NpadState pad{};pad.attributes=1;capture(pad,0,1);
        pad.buttons=1;capture(pad,0,1);assert(pad.buttons==1u<<14);h.tick();assert(equip_calls.empty());
        pad.buttons=0;capture(pad,0,1);
        pad.buttons=1u<<14;capture(pad,0,1);assert(pad.buttons==0);h.tick();h.expect_transaction();
        equip_calls.clear();
        for(unsigned i=0;i<3;++i){
            NpadState inactive{};capture(inactive,0,4);capture(inactive,32,2);capture(inactive,1,1);
            pad.buttons=1u<<14;capture(pad,0,1);assert(pad.buttons==0);h.tick();assert(equip_calls.empty());
        }
        pad.attributes=0;capture(pad,0,1);
        pad.attributes=1;pad.buttons=1u<<14;capture(pad,0,1);h.tick();assert(equip_calls.empty());
        pad.buttons=0;capture(pad,0,1);equip_expected_slot=h.slot(2);
        pad.buttons=1u<<14;capture(pad,0,1);h.tick();h.expect_transaction();
    }
    std::puts("dpad weapons module tests passed");
}
