#pragma once
#include <cstdint>

namespace re4 {
// Switch monotonic counter: 19.2 MHz. One shared opacity drives text and mist.
inline float weapon_popup_opacity(std::uint64_t start,std::uint64_t now){
    constexpr std::uint64_t fade=9'600'000,hold=38'400'000;
    if(now<start)return 0;
    const auto elapsed=now-start;
    if(elapsed>=2*fade+hold)return 0;
    float t=1;
    if(elapsed<fade)t=float(elapsed)/float(fade);
    else if(elapsed>fade+hold)t=float(2*fade+hold-elapsed)/float(fade);
    return t*t*(3-2*t);
}
}
