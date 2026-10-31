#pragma once
#include "re6/settings.hpp"
#include <cstring>

namespace re6 {
// RE6 v131072's native constructor copies exactly 0x38 bytes synchronously.
// Filter modes7/8 share the ordinary trilinear min/mag pair. Mode8 additionally
// reads the native AF lookup using bits17..21. All comparison/point modes pass
// through. This descriptor cannot distinguish world textures from render targets.
inline bool filter_sampler_descriptor(const std::uint8_t* source,std::uint8_t* output,
    std::size_t size,TextureFilter filter) {
    if(!source||!output||size<0x38)return false;
    std::uint8_t value[0x38];std::memcpy(value,source,sizeof(value));
    std::uint32_t flags;std::memcpy(&flags,value,sizeof(flags));
    const unsigned mode=flags&0xff;
    if((mode==7||mode==8)&&filter!=TextureFilter::Native&&unsigned(filter)<=unsigned(TextureFilter::Af16x)) {
        const unsigned index=filter==TextureFilter::Af2x?2:filter==TextureFilter::Af4x?4:
            filter==TextureFilter::Af8x?9:16;
        constexpr std::uint32_t mask=0xffu|(0x1fu<<17);
        flags=(flags&~mask)|8u|(index<<17);
        std::memcpy(value,&flags,sizeof(flags));
    }
    const bool changed=std::memcmp(value,source,sizeof(value))!=0;
    std::memcpy(output,value,sizeof(value));return changed;
}
}
