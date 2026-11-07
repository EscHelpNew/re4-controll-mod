#pragma once
#include "re4/enemy_health_numbers.hpp"
#include "re4/settings.hpp"
#include "graphics_preview_images.hpp"
#include <algorithm>
#include <array>

namespace re4::health_preview {
constexpr int width=256,height=222;
static_assert(sizeof(graphics_preview::health_pixels)/sizeof(graphics_preview::health_pixels[0])==width*height);
// Use the actual in-game glyph/bar geometry on the approved close-up. Only
// scaled, clipped rectangles and copied colors enter the row compositor.
struct Quad {float left=0,top=0,right=0,bottom=0;unsigned r=0,g=0,b=0,a=0;};
struct Canvas {
    std::array<Quad,65> quads{};
    unsigned count=0,vertex=0;
    Quad pending{};
    ReticleDrawBackend backend(){return {this,[](void*){},
        [](void*,unsigned,unsigned,unsigned,unsigned){},
        [](void*,unsigned,unsigned,unsigned,unsigned,unsigned){},
        [](void* p,unsigned,unsigned,unsigned){static_cast<Canvas*>(p)->vertex=0;},
        [](void* p,float x,float y,float){
            auto& c=*static_cast<Canvas*>(p);
            const float px=(x*2.5f-415)*float(width)/225;
            const float py=(y*(720.f/448.f)-105)*float(height)/195;
            if(!c.vertex){c.pending.left=c.pending.right=px;c.pending.top=c.pending.bottom=py;}
            else{c.pending.left=std::min(c.pending.left,px);c.pending.right=std::max(c.pending.right,px);
                c.pending.top=std::min(c.pending.top,py);c.pending.bottom=std::max(c.pending.bottom,py);}
            ++c.vertex;
        },
        [](void* p,unsigned r,unsigned g,unsigned b,unsigned a){auto& q=static_cast<Canvas*>(p)->pending;q.r=r;q.g=g;q.b=b;q.a=a;},
        [](void* p,unsigned){auto& c=*static_cast<Canvas*>(p);if(c.vertex==4&&c.count<c.quads.size())c.quads[c.count++]=c.pending;}
    };}
};
template<class Draw>void draw_rows(const Settings& settings,int left,int top,Draw draw){
    Canvas canvas;
    if(settings.enemy_health_bars){
        const ViewHudPoint anchor{520.f/2.5f,156.f/(720.f/448.f)+6};
        draw_enemy_health(canvas.backend(),anchor,750,750);
        if(settings.enemy_health_numbers)draw_enemy_health_number(canvas.backend(),anchor,750,kEnemyHealthPowerUnit,false);
    }
    std::array<std::uint8_t,width*4> row{};
    std::array<float,width> coverage{};
    std::array<unsigned,width> colors{};
    for(int y=0;y<height;++y){
        coverage.fill(0);
        for(unsigned i=0;i<canvas.count;++i){
            const auto& q=canvas.quads[i];
            const float vertical=std::max(0.f,std::min(float(y+1),q.bottom)-std::max(float(y),q.top));
            if(!vertical)continue;
            for(int x=std::max(0,int(std::floor(q.left)));x<std::min(width,int(std::ceil(q.right)));++x){
                const float horizontal=std::max(0.f,std::min(float(x+1),q.right)-std::max(float(x),q.left));
                // Adjacent pixel-font rows share fractional output pixels.
                // Accumulate their coverage rather than overwriting the edge.
                coverage[x]=std::min(float(q.a),coverage[x]+float(q.a)*horizontal*vertical);
                colors[x]=(q.r<<16)|(q.g<<8)|q.b;
            }
        }
        for(int x=0;x<width;++x){
            const auto pixel=graphics_preview::health_pixels[y*width+x];
            const unsigned r=(pixel>>11)&31,g=(pixel>>5)&63,b=pixel&31;
            const unsigned base[]={r*255/31,g*255/63,b*255/31};
            const unsigned rgb=colors[x];const unsigned front[]={rgb>>16,(rgb>>8)&255,rgb&255};
            for(unsigned c=0;c<3;++c)row[x*4+c]=std::uint8_t(std::lround((float(base[c])*(255-coverage[x])+float(front[c])*coverage[x])/255));
            row[x*4+3]=255;
        }
        draw(left,top+y,width,1,row.data());
    }
}
}
