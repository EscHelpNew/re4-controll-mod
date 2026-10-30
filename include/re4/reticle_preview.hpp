#pragma once
#include "settings.hpp"
#include "view.hpp"
#include <array>

namespace re4 {
struct ReticleMarks {std::array<ViewReticleRect,4> rects{};unsigned count=0;};
inline ReticleMarks reticle_marks(ReticleStyle style,ReticleSize extent){
    ReticleMarks result;
    const unsigned size=unsigned(extent);
    if(unsigned(style)>2 || size>2 || style==ReticleStyle::Off)return result;
    auto add=[&](float x,float y,unsigned w,unsigned h){
        ViewReticleRect rectangle;
        if(view_reticle_rect({x,y},w,h,rectangle))result.rects[result.count++]=rectangle;
    };
    if(style==ReticleStyle::Dot){add(256,224,2+size,2+size);return result;}
    const unsigned length=6+size*2,thickness=size?2:1,gap=2+size;
    const float offset=float(gap)+float(length)*0.5f;
    add(256-offset,224,length,thickness);add(256+offset,224,length,thickness);
    add(256,224-offset,thickness,length);add(256,224+offset,thickness,length);
    return result;
}
// Packed ARGB, matching the game's alpha128 marker over an opaque target.
constexpr unsigned kReticlePreviewWhite=0x80FFFFFF;
// A small target scene, using the same logical rectangles as the native HUD.
// Magnification is 2x at the standard720p display scale, preserving HUD aspect.
template<class Draw> bool draw_reticle_preview(const Settings& settings,int x,int y,int width,int height,Draw draw){
    if(x<0 || y<0 || x>16384 || y>16384 || width<160 || height<112 || width>4096 || height>4096)return false;
    auto fill=[&](int left,int top,int w,int h,unsigned color){
        const int right=std::min(x+width,left+w),bottom=std::min(y+height,top+h);
        left=std::max(x,left);top=std::max(y,top);
        if(right>left && bottom>top)draw(left,top,right-left,bottom-top,color>0xFFFFFF?color:color|0xFF000000u);
    };
    fill(x,y,width,height,0x252A25);
    fill(x,y,width/5,height,0x30362D);
    fill(x+width*4/5,y,width/5,height,0x1A211D);
    fill(x,y+height*3/4,width,height/4,0x161C18);
    for(int column=1;column<4;++column)fill(x+width*column/4,y,1,height*3/4,0x3A4035);
    fill(x,y+height*3/4,width,1,0x4A4C3D);
    const int cx=x+width/2,cy=y+height/2;
    fill(cx-11,cy-39,22,22,0x596052);
    fill(cx-24,cy-13,48,55,0x4A5145);
    fill(cx-36,cy-8,10,44,0x42493E);fill(cx+26,cy-8,10,44,0x42493E);
    fill(cx-22,cy+44,18,35,0x343C31);fill(cx+4,cy+44,18,35,0x343C31);
    const auto marks=reticle_marks(settings.reticle,settings.reticle_size);
    for(unsigned i=0;i<marks.count;++i){
        const auto& r=marks.rects[i];
        const int left=cx+int(std::lround((int(r.x)-256)*5.f));
        const int top=cy+int(std::lround((int(r.y)-224)*(720.f/224.f)));
        const int right=cx+int(std::lround((int(r.x)+r.width-256)*5.f));
        const int bottom=cy+int(std::lround((int(r.y)+r.height-224)*(720.f/224.f)));
        fill(left,top,right-left,bottom-top,kReticlePreviewWhite);
    }
    return true;
}
}
