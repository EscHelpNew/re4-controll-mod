#pragma once
#include "re4/reticle_draw.hpp"
#include <algorithm>
#include <cstdint>

namespace re4 {
inline constexpr unsigned kEnemyHealthAlpha=102;
inline constexpr unsigned kEnemyHealthPowerUnit=150;
enum class EnemyHealthBand {Green,Yellow,Red};
inline bool enemy_health_values(int current,int maximum){return current>0&&current<=32767&&maximum>0&&maximum<=32767;}
inline EnemyHealthBand enemy_health_band(int current,int maximum){
    if(std::int64_t(current)*4<=maximum)return EnemyHealthBand::Red;
    return std::int64_t(current)*2<=maximum?EnemyHealthBand::Yellow:EnemyHealthBand::Green;
}
inline bool enemy_health_target(unsigned flags,unsigned type,int current,int maximum){
    return (flags&0x201)==1&&!(flags&0x10000000)&&type>=0x10&&type<=0x4F&&enemy_health_values(current,maximum);
}
inline bool enemy_health_pool_member(std::uintptr_t base,unsigned count,unsigned stride,std::uintptr_t target){
    if(!base||count<2||count>256||stride!=0x10C8||target<=base)return false;
    const auto offset=target-base;
    return offset%stride==0&&offset/stride<count;
}
inline bool enemy_health_in_view(ViewHudPoint anchor){
    return std::isfinite(anchor.x)&&std::isfinite(anchor.y)&&anchor.x>=9&&anchor.x<=503&&anchor.y>=7&&anchor.y<=453;
}
// A single thin colored strip; no outline or black background. Coordinates
// refer to the displayed native512x448 HUD. Opacity is supplied by the fade model.
inline bool draw_enemy_health(const ReticleDrawBackend& backend,ViewHudPoint anchor,int current,int maximum,float opacity=1){
    if(!enemy_health_values(current,maximum)||!std::isfinite(anchor.x)||!std::isfinite(anchor.y)
        ||!backend.prepare_hud||!backend.blend||!backend.vertex_format||!backend.begin
        ||!backend.position||!backend.color||!backend.end||!std::isfinite(opacity)||opacity<0||opacity>1)return false;
    const unsigned alpha=unsigned(std::lround(float(kEnemyHealthAlpha)*opacity));if(!alpha)return false;
    if(!enemy_health_in_view(anchor))return false;
    const float left=anchor.x-9,top=anchor.y-7;
    const auto band=enemy_health_band(current,maximum);
    const unsigned red=band==EnemyHealthBand::Green?56:band==EnemyHealthBand::Yellow?238:232;
    const unsigned green=band==EnemyHealthBand::Green?212:band==EnemyHealthBand::Yellow?198:64;
    const unsigned blue=band==EnemyHealthBand::Green?96:band==EnemyHealthBand::Yellow?48:64;
    const float fill=18.f*float(std::min(current,maximum))/float(maximum);
    auto* context=backend.context;
    backend.prepare_hud(context);backend.blend(context,1,4,5,0);backend.vertex_format(context,0,9,1,4,0);
    const auto quad=[&](float x,float y,float width,float height,unsigned r,unsigned g,unsigned b,unsigned a){
        backend.begin(context,0x98,0,4);
        for(const auto& point:{ViewHudPoint{x,y},ViewHudPoint{x+width,y},ViewHudPoint{x,y+height},ViewHudPoint{x+width,y+height}}){
            backend.position(context,point.x,point.y,0);backend.color(context,r,g,b,a);
        }
        backend.end(context,0x34);
    };
    quad(left,top,fill,2,red,green,blue,alpha);
    backend.blend(context,1,1,0,0);backend.vertex_format(context,0,9,1,3,0);
    return true;
}
}
