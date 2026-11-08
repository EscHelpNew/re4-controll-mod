// Actual post-name inline callback, with only engine/SDK boundaries substituted.
#include "lib.hpp"
#include "re4/hook_context.hpp"
#include <array>
#include <atomic>
#include <cstring>
#include <limits>
#include <cstdio>
namespace {void test_InventoryDamageName(exl::hook::InlineFloatCtx*){}}
namespace {
std::uintptr_t main_base=0;
float extended_upgrade_ratio_seen=1.f;
float extended_upgrade_display_ratio(std::uintptr_t){return extended_upgrade_ratio_seen;}
#include "../source/game/inventory_damage.inc"
unsigned getter_calls=0,resolver_calls=0,item_class=1;
unsigned received_id=0,received_level=0;
unsigned native_weapon=4;
void* hover_node=nullptr;
float native_value=6.5f;
void classify(unsigned,void* out){static_cast<std::uint8_t*>(out)[2]=std::uint8_t(item_class);}
unsigned weapon(unsigned){return native_weapon;}
float power(unsigned id,int level){++getter_calls;received_id=id;received_level=unsigned(level);return native_value;}
void* hovered(void*,void*){++resolver_calls;return hover_node;}
void require(bool c,unsigned line){if(!c){std::fprintf(stderr,"inventory damage line %u\n",line);std::exit(1);}}
#define CHECK(c) require((c),__LINE__)
struct Fixture {
    std::vector<std::uint8_t> image=std::vector<std::uint8_t>(0x1272000);
    alignas(8) std::array<std::uint8_t,0x60> grid{},cursor{},node{};
    std::array<std::uint8_t,28> slots{};
    exl::hook::InlineFloatCtx context{};
    template<class T>void put(void* p,std::size_t offset,T v){std::memcpy(static_cast<std::uint8_t*>(p)+offset,&v,sizeof(v));}
    std::uint8_t* menu(){return image.data()+0x1270F70;}
    std::uint8_t* manager(){return image.data()+0x124D918;}
    Fixture(){
        main_base=reinterpret_cast<std::uintptr_t>(image.data());
        put(menu(),0x338,grid.data());put(grid.data(),0x58,cursor.data());put(node.data(),0x30,slots.data()+14);
        put(manager(),0x20,slots.data());put(manager(),0x30,std::int32_t(2));
        put(image.data(),0xB00648+0x1C,std::uint32_t(3));
        // The equipped duplicate has level1. The hovered duplicate has level7.
        put(manager(),0x10,slots.data());put(slots.data(),0,std::uint16_t(4));put(slots.data(),2,std::uint16_t(1));slots[4]=1;
        put(slots.data(),14,std::uint16_t(4));put(slots.data(),16,std::uint16_t(1));slots[18]=1;slots[20]=6;
        game_test::memory_maps={{main_base,image.size(),3},
            {reinterpret_cast<std::uintptr_t>(grid.data()),grid.size(),3},
            {reinterpret_cast<std::uintptr_t>(cursor.data()),cursor.size(),3},
            {reinterpret_cast<std::uintptr_t>(node.data()),node.size(),3},
            {reinterpret_cast<std::uintptr_t>(slots.data()),slots.size(),3}};
        put(context.bytes,0x200+20*8,reinterpret_cast<std::uintptr_t>(menu()));
        put(context.bytes,0x200+22*8,std::uintptr_t(320));put(context.bytes,0x200+23*8,std::uintptr_t(390));
        inventory_damage_native={hovered,classify,weapon,power};inventory_damage_enabled.store(true);
        inventory_damage_clear();getter_calls=resolver_calls=0;item_class=1;native_weapon=4;native_value=6.5f;hover_node=node.data();
    }
    void invoke(){InventoryDamageName::Callback(&context);}
};
}
int main(){
    Fixture f;const auto before_context=f.context;const auto before_slots=f.slots;
    f.invoke();InventoryDamageReadout out;
    CHECK(inventory_damage_snapshot(out));CHECK(out.tenths==65&&out.x==320&&out.y==390);
    CHECK(received_id==4&&received_level==7&&getter_calls==1);
    CHECK(f.slots==before_slots&&!std::memcmp(&f.context,&before_context,sizeof(f.context)));
    f.put(f.image.data(),0xB00648+0x1C,std::uint32_t(2));
    CHECK(!inventory_damage_snapshot(out)); // Native close slot0 must suppress cached damage.
    f.put(f.image.data(),0xB00648+0x1C,std::uint32_t(3));
    hover_node=nullptr;f.invoke();CHECK(!inventory_damage_snapshot(out)); // Empty cells clear prior firearm.
    hover_node=f.node.data();
    for(unsigned non_firearm:{0u,3u,6u,9u}){
        item_class=non_firearm;getter_calls=0;f.invoke();CHECK(!inventory_damage_snapshot(out)&&getter_calls==0);
    }
    item_class=1;native_weapon=255;getter_calls=0;f.invoke();CHECK(!inventory_damage_snapshot(out)&&getter_calls==0);native_weapon=4;
    f.slots[20]=7;f.invoke();CHECK(!inventory_damage_snapshot(out)&&getter_calls==0); // Invalid table level8.
    f.slots[20]=0;native_value=1.f;f.invoke();CHECK(inventory_damage_snapshot(out)&&out.tenths==10&&received_level==1);
    f.slots[20]=1;CHECK(!inventory_damage_snapshot(out)); // No stale damage after an upgrade changes.
    native_value=2.15f;f.invoke();CHECK(inventory_damage_snapshot(out)&&out.tenths==22&&received_level==2);
    for(float invalid:{0.f,-1.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
        native_value=invalid;f.invoke();CHECK(!inventory_damage_snapshot(out));
    }
    native_value=2.f;f.put(f.slots.data(),14,std::uint16_t(0x36));f.slots[20]=4;getter_calls=0;f.invoke();
    CHECK(!inventory_damage_snapshot(out)&&getter_calls==0); // Special getter only accepts one-based1..4.
    f.put(f.slots.data(),14,std::uint16_t(4));f.slots[20]=0;
    f.put(f.node.data(),0x30,f.slots.data()+15);f.invoke();CHECK(!inventory_damage_snapshot(out)); // Not a record boundary.
    f.put(f.node.data(),0x30,f.slots.data()+14);
    f.slots[19]=2;f.invoke();CHECK(!inventory_damage_snapshot(out));f.slots[19]=0; // Different owner.
    f.slots[18]=0;f.invoke();CHECK(!inventory_damage_snapshot(out));f.slots[18]=1; // Inactive record.
    f.put(f.slots.data(),16,std::uint16_t(0));f.invoke();CHECK(!inventory_damage_snapshot(out));
    f.put(f.slots.data(),16,std::uint16_t(1));
    f.put(f.grid.data(),0x20,f.node.data());hover_node=nullptr;resolver_calls=0;f.invoke();
    CHECK(inventory_damage_snapshot(out)&&out.tenths==20&&resolver_calls==0); // Dragged item follows native name choice.
    f.put(f.menu(),0x338,std::uintptr_t(0));CHECK(!inventory_damage_snapshot(out)); // Native menu cleanup.
    f.put(f.menu(),0x338,f.grid.data());
    inventory_damage_enabled.store(false);f.invoke();CHECK(!inventory_damage_snapshot(out));
    inventory_damage_enabled.store(true);f.invoke();CHECK(inventory_damage_snapshot(out));
    game_test::memory_maps.pop_back();f.invoke();CHECK(!inventory_damage_snapshot(out)); // Unreadable inventory span.
    std::puts("inventory damage module tests passed");
}
