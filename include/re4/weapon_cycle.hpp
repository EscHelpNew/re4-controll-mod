#pragma once
#include <cstddef>
#include <cstdint>
namespace re4 {
constexpr std::size_t weapon_cycle_none=static_cast<std::size_t>(-1);
constexpr std::size_t weapon_cycle_capacity=384;
enum class WeaponCycleGroup { Guns, Grenades };
struct WeaponCycleSlot {
    std::uint16_t raw_id=0,quantity=0;
    std::uint8_t flags=0,owner=0,item_class=0;
};
constexpr bool weapon_cycle_grenade(std::uint16_t raw){return raw==1||raw==2||raw==14;}
constexpr bool weapon_cycle_candidate(const WeaponCycleSlot& slot,std::uint8_t owner,WeaponCycleGroup group){
    return owner!=1&&slot.quantity&&(slot.flags&1)&&slot.owner==owner
        &&(group==WeaponCycleGroup::Grenades?weapon_cycle_grenade(slot.raw_id)
            :!weapon_cycle_grenade(slot.raw_id)&&(slot.item_class==1||slot.item_class==3));
}
// Current is an exact slot index: duplicate guns/upgrades remain separate
// choices. Grenades cycle available types, using the first active stack of
// each type in inventory order and retaining the current stack for a no-op.
inline std::size_t weapon_cycle_next(const WeaponCycleSlot* slots,std::size_t count,std::size_t current,
                                   std::uint8_t owner,WeaponCycleGroup group,int direction){
    if(!slots||!count||count>weapon_cycle_capacity||owner==1||(direction!=1&&direction!=-1))return weapon_cycle_none;
    const bool has_current=current<count&&weapon_cycle_candidate(slots[current],owner,group);
    if(group==WeaponCycleGroup::Grenades){
        std::size_t types[3]{},used=0,current_type=weapon_cycle_none;
        for(std::size_t i=0;i<count;++i){
            if(!weapon_cycle_candidate(slots[i],owner,group))continue;
            bool seen=false;
            for(std::size_t n=0;n<used;++n)if(slots[types[n]].raw_id==slots[i].raw_id)seen=true;
            if(!seen)types[used++]=i;
        }
        if(!used)return weapon_cycle_none;
        if(has_current)for(std::size_t n=0;n<used;++n)
            if(slots[types[n]].raw_id==slots[current].raw_id)current_type=n;
        if(current_type==weapon_cycle_none)return types[direction>0?0:used-1];
        if(used==1)return current;
        return types[direction>0?(current_type+1)%used:(current_type+used-1)%used];
    }
    std::size_t index=has_current?current:(direction>0?count-1:0);
    for(std::size_t n=0;n<count;++n){
        index=direction>0?(index+1)%count:(index+count-1)%count;
        if(weapon_cycle_candidate(slots[index],owner,group))return index;
    }
    return weapon_cycle_none;
}
}
