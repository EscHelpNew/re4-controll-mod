#pragma once
#include "re4/settings.hpp"
#include "graphics_preview_images.hpp"
#include <array>
#include <cmath>

namespace re4::grenade_preview {
constexpr int width=256,height=171;
static_assert(sizeof(graphics_preview::grenade_pixels)/sizeof(graphics_preview::grenade_pixels[0])==width*height);
template<class Draw>void draw_rows(const Settings& settings,int left,int top,Draw draw){
    std::array<std::uint8_t,width*4> row{};
    for(int y=0;y<height;++y){
        for(int x=0;x<width;++x){
            const auto pixel=graphics_preview::grenade_pixels[y*width+x];
            const unsigned r=(pixel>>11)&31,g=(pixel>>5)&63,b=pixel&31;
            const unsigned base[]={r*255/31,g*255/63,b*255/31};
            // Enlarged style sample on the approved floor crop, not trajectory data.
            const double dx=((x+.5)*270/width-183)/51,dy=((y+.5)*180/height-139)/16;
            const double squared=dx*dx+dy*dy;
            const double center=squared<1?1-std::sqrt(squared):0;
            const unsigned front[]={unsigned(std::lround(232+23*center)),unsigned(std::lround(48-36*center)),unsigned(std::lround(40-32*center))};
            double alpha=0;
            if(settings.grenade_landing_spot&&squared<1){
                constexpr unsigned falloff[]={92,59,27,7,0};
                const double ring=std::sqrt(squared)*4;
                const unsigned index=unsigned(ring);const double t=ring-index;
                alpha=falloff[index]*(1-t)+falloff[index+1]*t;
            }
            for(unsigned c=0;c<3;++c)row[x*4+c]=std::uint8_t(std::lround((base[c]*(255-alpha)+front[c]*alpha)/255));
            row[x*4+3]=255;
        }
        draw(left,top+y,width,1,row.data());
    }
}
}
