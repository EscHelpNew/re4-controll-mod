#pragma once
#include "re4/reticle_draw.hpp"
#include <algorithm>

namespace re4 {
// A single irregular feathered cloud, centered below the weapon line. Spread
// contracts on appearance and expands on disappearance; opacity follows text.
inline bool draw_weapon_mist(const ReticleDrawBackend& b,ViewHudPoint center,float text_width,float opacity,float spread){
    if(!b.prepare_hud||!b.blend||!b.vertex_format||!b.begin||!b.position||!b.color||!b.end
        ||!std::isfinite(center.x)||!std::isfinite(center.y)||!std::isfinite(text_width)
        ||!std::isfinite(opacity)||!std::isfinite(spread)||opacity<=0||opacity>1
        ||text_width<=0||text_width>400||spread<1||spread>1.3f)return false;
    const unsigned peak=unsigned(std::lround(46*opacity));if(!peak)return false;
    constexpr unsigned sectors=24,rings=4;
    ViewHudPoint points[rings+1][sectors+1]{};
    const float rx=(text_width*.5f+48)*spread,ry=22*spread;
    for(unsigned ring=0;ring<=rings;++ring)for(unsigned sector=0;sector<=sectors;++sector){
        const double angle=double(sector%sectors)*6.283185307179586/sectors;
        const float edge=1+.045f*float(std::sin(3*angle))+.025f*float(std::sin(7*angle+.5));
        const float radius=float(ring)/rings*edge;
        points[ring][sector]={std::clamp(center.x+rx*radius*float(std::cos(angle)),0.f,512.f),
            std::clamp(center.y+8+ry*radius*float(std::sin(angle)),0.f,448.f)};
    }
    constexpr unsigned falloff[]={46,35,16,4,0};
    // A pale source-over cloud disappears against similarly pale ground.
    // Add its feathered light (at most 46/255 * RGB155), preserving opacity.
    b.prepare_hud(b.context);b.blend(b.context,1,4,1,0);b.vertex_format(b.context,0,9,1,4,0);
    b.begin(b.context,0x90,0,rings*sectors*6);
    for(unsigned ring=0;ring<rings;++ring)for(unsigned sector=0;sector<sectors;++sector){
        const ViewHudPoint vertices[]={points[ring][sector],points[ring+1][sector],points[ring][sector+1],
            points[ring+1][sector],points[ring+1][sector+1],points[ring][sector+1]};
        constexpr unsigned outer[]={0,1,0,1,1,0};
        for(unsigned i=0;i<6;++i){b.position(b.context,vertices[i].x,vertices[i].y,0);
            b.color(b.context,155,153,149,unsigned(std::lround(falloff[ring+outer[i]]*opacity)));}
    }
    b.end(b.context,0x34);
    b.blend(b.context,1,1,0,0);b.vertex_format(b.context,0,9,1,3,0);return true;
}
}
