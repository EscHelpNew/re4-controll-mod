#pragma once
#include "re4/enemy_health.hpp"
#include <cstddef>

namespace re4 {
inline constexpr unsigned kEnemyHealthNumberAlpha=128;
// The caller supplies the verified raw-HP value of one weapon power unit.
// Decimal point is fixed in every interface language.
// Round positive halves upward without floating point or partial buffer writes.
inline bool format_enemy_health_number(int current,unsigned hp_per_power_unit,bool /*russian*/,char* out,std::size_t size){
    if(current<0||current>32767||!hp_per_power_unit||!out)return false;
    const auto unit=std::uint64_t(hp_per_power_unit);
    const auto tenths=(std::uint64_t(current)*20+unit)/(unit*2);
    auto whole=tenths/10;
    char reverse[5]{};
    std::size_t digits=0;
    do{reverse[digits++]=char('0'+whole%10);whole/=10;}while(whole);
    if(size<digits+3)return false;
    for(std::size_t i=0;i<digits;++i)out[i]=reverse[digits-1-i];
    out[digits]='.';out[digits+1]=char('0'+tenths%10);out[digits+2]='\0';
    return true;
}
// Original 3x5 pixel digits, each horizontal run emitted as a native float quad.
// Anchor is the bar's target point; the number starts eight pixels above its top.
inline bool draw_enemy_health_number(const ReticleDrawBackend& backend,ViewHudPoint anchor,
        int current,unsigned hp_per_power_unit,bool russian,float opacity=1){
    if(current<=0||!std::isfinite(anchor.x)||!std::isfinite(anchor.y)
        ||!backend.prepare_hud||!backend.blend||!backend.vertex_format||!backend.begin
        ||!backend.position||!backend.color||!backend.end
        ||!std::isfinite(opacity)||opacity<0||opacity>1)return false;
    const unsigned alpha=unsigned(std::lround(float(kEnemyHealthNumberAlpha)*opacity));
    if(!alpha)return false;
    char text[8]{};
    if(!format_enemy_health_number(current,hp_per_power_unit,russian,text,sizeof(text)))return false;
    unsigned width=0;
    for(const char* p=text;*p;++p){if(p!=text)++width;width+=*p=='.'?1u:3u;}
    const float left=anchor.x-float(width)*.5f,top=anchor.y-15;
    const float right=left+float(width),bottom=top+5;
    // Reject the complete text rectangle before enqueuing state or geometry.
    if(!(left>=0&&top>=0&&right<=512&&bottom<=448&&left<right&&top<bottom))return false;
    static constexpr unsigned char digits[10][5]={
        {7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},
        {7,4,7,1,7},{7,4,7,5,7},{7,1,2,2,2},{7,5,7,5,7},{7,5,7,1,7}};
    struct Run {float left,top,right,bottom;};
    Run runs[64]{};unsigned count=0,offset=0;
    for(const char* p=text;*p;++p){
        const unsigned glyph_width=*p=='.'?1u:3u;
        const unsigned glyph_height=5;
        for(unsigned y=0;y<glyph_height;++y){
            const unsigned bits=*p=='.'?(y==4?1u:0u):digits[unsigned(*p-'0')][y];
            unsigned x=0;
            while(x<glyph_width){
                if(!(bits&(1u<<(glyph_width-1-x)))){++x;continue;}
                const unsigned start=x++;
                while(x<glyph_width&&(bits&(1u<<(glyph_width-1-x))))++x;
                if(count==64)return false;
                runs[count++]={left+float(offset+start),top+float(y),left+float(offset+x),top+float(y+1)};
            }
        }
        offset+=glyph_width+1;
    }
    auto* const context=backend.context;
    backend.prepare_hud(context);backend.blend(context,1,4,5,0);backend.vertex_format(context,0,9,1,4,0);
    for(unsigned i=0;i<count;++i){
        const auto& run=runs[i];
        backend.begin(context,0x98,0,4);
        const ViewHudPoint points[]={{run.left,run.top},{run.right,run.top},{run.left,run.bottom},{run.right,run.bottom}};
        for(const auto& point:points){backend.position(context,point.x,point.y,0);backend.color(context,235,235,235,alpha);}
        backend.end(context,0x34);
    }
    backend.blend(context,1,1,0,0);backend.vertex_format(context,0,9,1,3,0);
    return true;
}
}
