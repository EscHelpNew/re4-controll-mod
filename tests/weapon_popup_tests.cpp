#include "re4/weapon_popup.hpp"
#include <cassert>
#include <cmath>

int main(){
    using re4::weapon_popup_opacity;
    constexpr std::uint64_t start=100;
    assert(weapon_popup_opacity(start,start-1)==0);
    assert(weapon_popup_opacity(start,start)==0);
    assert(std::fabs(weapon_popup_opacity(start,start+4'800'000)-.5f)<.0001f);
    assert(weapon_popup_opacity(start,start+9'600'000)==1);
    assert(weapon_popup_opacity(start,start+48'000'000)==1);
    assert(std::fabs(weapon_popup_opacity(start,start+52'800'000)-.5f)<.0001f);
    assert(weapon_popup_opacity(start,start+57'600'000)==0);
    assert(weapon_popup_opacity(start,UINT64_MAX)==0);
    // Latest equip gets its own full timeline; old fade cannot carry over.
    const auto replacement=start+52'800'000;
    assert(weapon_popup_opacity(replacement,replacement)==0);
    assert(weapon_popup_opacity(replacement,replacement+9'600'000)==1);
    // Tick zero is a valid successful equip, not an inactive sentinel.
    assert(weapon_popup_opacity(0,9'600'000)==1);
}
