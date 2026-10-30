#include "re4/reticle_preview.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
struct Rect {int x,y,w,h;unsigned color;};
int main(){
    using namespace re4;
    assert(reticle_marks(ReticleStyle::Off,ReticleSize::Small).count==0);
    assert(reticle_marks(static_cast<ReticleStyle>(255),ReticleSize::Small).count==0);
    assert(reticle_marks(ReticleStyle::Dot,static_cast<ReticleSize>(255)).count==0);
    for(unsigned size=0;size<3;++size){
        const auto dot=reticle_marks(ReticleStyle::Dot,static_cast<ReticleSize>(size));
        assert(dot.count==1 && dot.rects[0].width==2+size && dot.rects[0].height==2+size);
        const auto cross=reticle_marks(ReticleStyle::Crosshair,static_cast<ReticleSize>(size));
        assert(cross.count==4 && cross.rects[0].width==6+2*size && cross.rects[0].height==(size?2u:1u));
        for(auto style:{ReticleStyle::Off,ReticleStyle::Dot,ReticleStyle::Crosshair}){
            Settings s;s.reticle=style;s.reticle_size=static_cast<ReticleSize>(size);
            std::vector<Rect> draws;
            assert(draw_reticle_preview(s,10,20,344,160,[&](int x,int y,int w,int h,unsigned color){draws.push_back({x,y,w,h,color});}));
            unsigned markers=0;
            for(const auto& r:draws){
                assert(r.w>0 && r.h>0 && r.x>=10 && r.y>=20 && r.x+r.w<=354 && r.y+r.h<=180);
                if(r.color==kReticlePreviewWhite){++markers;assert((r.color>>24)==128);}
                else assert((r.color>>24)==255);
            }
            assert(markers==(style==ReticleStyle::Off?0u:style==ReticleStyle::Dot?1u:4u));
        }
    }
    unsigned called=0;
    assert(!draw_reticle_preview(Settings{},0,0,0,0,[&](int,int,int,int,unsigned){++called;}));
    assert(called==0);
    std::puts("reticle preview: native shapes, every style/size, off and panel bounds PASS");
}
