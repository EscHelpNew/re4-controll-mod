#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "re4/view.hpp"

namespace re4 {
// Slanted cyan seven-segment quantity centered in the original three-digit
// field above the native name plate, without an icon or leading zeros. Geometry is
// produced as independent four-vertex strips (native 0x98, TL,TR,BL,BR) in the
// 512x448 late-HUD space; X is corrected for the 16:9 display of that space.
struct AmmoCounterQuad {ViewHudPoint p[4];};
inline constexpr float kAmmoCounterAspect=9.f/14.f;
inline constexpr unsigned kAmmoCounterMaxQuads=5*7;

namespace ammo_counter_detail {
// Segment endpoints in a unit glyph (width .5, height 1, Y down).
struct Segment {float x0,y0,x1,y1;};
inline constexpr Segment kSegments[7]={
    {0,0,.5f,0},{.5f,0,.5f,.5f},{.5f,.5f,.5f,1},{0,1,.5f,1},{0,.5f,0,1},{0,0,0,.5f},{0,.5f,.5f,.5f}};
inline constexpr std::uint8_t kDigits[10]={0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F}; // bit i = segment a..g
// Thick line between a and b (glyph units), skewed and mapped to HUD space.
inline AmmoCounterQuad stroke(float ax,float ay,float bx,float by,float thickness,float inset,
        float origin_x,float top,float height,float slant){
    const float dx=bx-ax,dy=by-ay,length=std::hypot(dx,dy);
    const float ux=length>0?dx/length:1,uy=length>0?dy/length:0;
    ax+=ux*inset;ay+=uy*inset;bx-=ux*inset;by-=uy*inset;
    const float nx=-uy*thickness*.5f,ny=ux*thickness*.5f;
    auto map=[&](float x,float y){
        const float skewed=x+slant*(1.f-y);
        return ViewHudPoint{origin_x+skewed*height*kAmmoCounterAspect,top+y*height};
    };
    return {{map(ax+nx,ay+ny),map(bx+nx,by+ny),map(ax-nx,ay-ny),map(bx-nx,by-ny)}};
}
}
inline float ammo_counter_digit_advance(float height){return (.5f+.18f)*height*kAmmoCounterAspect;}
inline float ammo_counter_width(unsigned digits,float height){
    return digits?digits*ammo_counter_digit_advance(height)-.18f*height*kAmmoCounterAspect+.12f*height*kAmmoCounterAspect:0;
}
inline unsigned ammo_counter_digits(unsigned value){unsigned n=1;while(value>=10&&n<5){value/=10;++n;}return n;}
// right/bottom: original three-digit field right edge and baseline.
// Shorter and longer quantities share that field center. Returns quad count.
inline unsigned ammo_counter_quads(unsigned value,float right,float bottom,float height,
        AmmoCounterQuad (&out)[kAmmoCounterMaxQuads]){
    using namespace ammo_counter_detail;
    if(!std::isfinite(right)||!std::isfinite(bottom)||!std::isfinite(height)||height<4||height>80)return 0;
    if(value>65535)value=65535;
    const unsigned digits=ammo_counter_digits(value);
    const float top=bottom-height,thickness=.11f,slant=.12f;
    const float center=right-ammo_counter_width(3,height)*.5f;
    const float left=center-ammo_counter_width(digits,height)*.5f;
    unsigned count=0,divisor=1;for(unsigned i=1;i<digits;++i)divisor*=10;
    for(unsigned i=0;i<digits;++i,divisor/=10){
        const unsigned digit=value/divisor%10;
        const float origin=left+i*ammo_counter_digit_advance(height);
        for(unsigned s=0;s<7;++s)if(kDigits[digit]>>s&1u){
            const auto& g=kSegments[s];
            out[count++]=stroke(g.x0,g.y0,g.x1,g.y1,thickness,thickness*.7f,origin,top,height,slant);
        }
    }
    for(unsigned i=0;i<count;++i)for(const auto& p:out[i].p)
        if(!(p.x>=0&&p.x<=512&&p.y>=0&&p.y<=448))return 0;
    return count;
}

// Native spare-ammo rule (CountItem 298310 / reload 299320): every valid stack
// of the weapon's ammo id with the inventory owner, summed as u16.
struct AmmoCounterRecord {std::uint16_t raw=0,quantity=0;std::uint8_t flags=0,owner=0;};
inline std::uint16_t ammo_counter_total(const AmmoCounterRecord* records,std::size_t count,
        std::uint16_t ammo,std::uint8_t owner){
    std::uint16_t total=0;
    for(std::size_t i=0;i<count;++i){
        const auto& r=records[i];
        if((r.flags&1)&&r.owner==owner&&r.raw==ammo)total=std::uint16_t(total+r.quantity);
    }
    return total;
}
// Weapons whose native gauge has no spare-ammo meaning: bolts/arrows are
// already the loaded count (Bowgun 0x10, Krauser's bow 0x52).
inline bool ammo_counter_supported(std::uint16_t effective,std::uint16_t ammo,unsigned capacity){
    return effective!=0x10&&effective!=0x52&&ammo!=0xFF&&ammo!=0xFFFF&&capacity!=0x8000;
}
}
