#include "re4/button_map.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
int main() {
    using namespace re4;
    constexpr unsigned bits[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,24,25,26,27};
    constexpr const char* names[] = {"A","B","X","Y","L3","R3","L","R","ZL","ZR","PLUS","MINUS","D-left","D-up","D-right","D-down","SL(L)","SR(L)","SL(R)","SR(R)"};
    const auto identity = identity_button_map();
    assert(valid_button_map(identity));
    for (unsigned i=0;i<20;++i) {
        assert(identity[i]==i && button_mask(i)==(std::uint64_t{1}<<bits[i]));
        assert(std::strcmp(button_name(i),names[i])==0);
        for (unsigned j=0;j<20;++j) {
            auto map=identity;map[i]=std::uint8_t(j);
            assert(remap_buttons(std::uint64_t{1}<<bits[i],map)==(std::uint64_t{1}<<bits[j]));
        }
    }
    for (std::uint64_t packed=0;packed<(std::uint64_t{1}<<20);++packed) {
        const auto state=(packed&0xffff)|((packed&0xf0000)<<8);
        assert(remap_buttons(state,identity)==state);
    }
    auto map=identity;map[0]=1;map[1]=0;
    assert(remap_buttons(1,map)==2 && remap_buttons(2,map)==1 && remap_buttons(3,map)==3);
    map[1]=2;map[2]=0;
    assert(remap_buttons(3,map)==6 && remap_buttons(7,map)==7);
    map[0]=map[1]=2;
    assert(remap_buttons(3,map)==4);
    map.fill(kUnboundButton);
    assert(valid_button_map(map));
    constexpr std::uint64_t known=0x0f00ffff;
    assert(remap_buttons(~std::uint64_t{0},map)==~known);
    assert(remap_buttons(~std::uint64_t{0},map,false)==~std::uint64_t{0});
    map[0]=20;assert(!valid_button_map(map));
    map[0]=254;assert(!valid_button_map(map));
    assert(button_mask(20)==0 && button_mask(255)==0);
    std::puts("button mapping: all physical bits, exhaustive identity, swaps, cycles, duplicate and unbound PASS");
}
