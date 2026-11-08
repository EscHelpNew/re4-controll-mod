#include "re4/weapon_cycle.hpp"
#include <array>
#include <cassert>
#include <cstdio>
int main(){
    using namespace re4;
    std::array<WeaponCycleSlot,8> s{{
        {4,1,1,0,1},{4,1,1,0,1},{1,2,1,0,3},{2,1,1,0,3},
        {14,1,1,0,3},{99,1,1,0,3},{7,1,1,0,6},{5,1,1,1,1}}};
    assert(weapon_cycle_next(s.data(),s.size(),0,0,WeaponCycleGroup::Guns,1)==1);
    assert(weapon_cycle_next(s.data(),s.size(),1,0,WeaponCycleGroup::Guns,1)==5);
    assert(weapon_cycle_next(s.data(),s.size(),0,0,WeaponCycleGroup::Guns,-1)==5);
    assert(weapon_cycle_next(s.data(),s.size(),5,0,WeaponCycleGroup::Guns,1)==0);
    assert(weapon_cycle_next(s.data(),s.size(),0,0,WeaponCycleGroup::Grenades,1)==2);
    assert(weapon_cycle_next(s.data(),s.size(),0,0,WeaponCycleGroup::Grenades,-1)==4);
    assert(weapon_cycle_next(s.data(),s.size(),4,0,WeaponCycleGroup::Grenades,1)==2);
    s[2].quantity=0;s[3].flags=0;
    assert(weapon_cycle_next(s.data(),s.size(),4,0,WeaponCycleGroup::Grenades,1)==4);
    s[4].owner=1;
    assert(weapon_cycle_next(s.data(),s.size(),0,0,WeaponCycleGroup::Grenades,1)==weapon_cycle_none);
    assert(weapon_cycle_next(nullptr,8,0,0,WeaponCycleGroup::Guns,1)==weapon_cycle_none);
    assert(weapon_cycle_next(s.data(),385,0,0,WeaponCycleGroup::Guns,1)==weapon_cycle_none);
    assert(weapon_cycle_next(s.data(),0,0,0,WeaponCycleGroup::Guns,1)==weapon_cycle_none);
    assert(weapon_cycle_next(s.data(),8,0,1,WeaponCycleGroup::Guns,1)==weapon_cycle_none);
    assert(weapon_cycle_next(s.data(),8,0,0,WeaponCycleGroup::Guns,0)==weapon_cycle_none);
    std::array<WeaponCycleSlot,384> full{};full[383]={6,1,1,0,1};
    assert(weapon_cycle_next(full.data(),full.size(),weapon_cycle_none,0,WeaponCycleGroup::Guns,1)==383);
    full[383].flags=2;
    assert(weapon_cycle_next(full.data(),full.size(),weapon_cycle_none,0,WeaponCycleGroup::Guns,1)==weapon_cycle_none);
    std::array<WeaponCycleSlot,7> stacks{{{4,1,1,0,1},{1,2,1,0,3},{1,1,1,0,3},{2,1,1,0,3},
        {1,1,1,0,3},{14,1,1,0,3},{2,1,1,0,3}}};
    assert(weapon_cycle_next(stacks.data(),7,1,0,WeaponCycleGroup::Grenades,1)==3);
    assert(weapon_cycle_next(stacks.data(),7,3,0,WeaponCycleGroup::Grenades,1)==5);
    assert(weapon_cycle_next(stacks.data(),7,5,0,WeaponCycleGroup::Grenades,1)==1);
    assert(weapon_cycle_next(stacks.data(),7,2,0,WeaponCycleGroup::Grenades,1)==3);
    assert(weapon_cycle_next(stacks.data(),7,3,0,WeaponCycleGroup::Grenades,-1)==1);
    assert(weapon_cycle_next(stacks.data(),7,0,0,WeaponCycleGroup::Grenades,1)==1);
    assert(weapon_cycle_next(stacks.data(),7,0,0,WeaponCycleGroup::Grenades,-1)==5);
    stacks[3].quantity=stacks[5].quantity=stacks[6].quantity=0;
    assert(weapon_cycle_next(stacks.data(),7,2,0,WeaponCycleGroup::Grenades,1)==2);
    std::puts("weapon cycle tests passed");
}
