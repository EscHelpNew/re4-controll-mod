#pragma once
#include "re4/character_retarget.hpp"
#include <limits>

namespace re4 {
// Owned metadata only. The caller proves the immutable native/donor resource
// and its single hand-bone influence before selecting the actual mesh tip.
struct CharacterBladeTip {ViewVec3 bind_point{};unsigned vertex=0;};
inline bool character_blade_tip_prepare(const ViewVec3* vertices,unsigned count,
        ViewVec3 bind_hand,CharacterBladeTip& out){
    const auto address=reinterpret_cast<std::uintptr_t>(vertices);
    if(!vertices||!count||count>65535||address%alignof(ViewVec3)
        ||std::size_t(count)*sizeof(*vertices)>std::numeric_limits<std::uintptr_t>::max()-address
        ||!view_finite(bind_hand)
        ||character_retarget_detail::overlaps(&out,sizeof(out),vertices,std::size_t(count)*sizeof(*vertices)))return false;
    CharacterBladeTip value{};double farthest=0;
    for(unsigned i=0;i<count;++i){
        const auto p=vertices[i];if(!view_finite(p))return false;
        const double x=double(p.x)-bind_hand.x,y=double(p.y)-bind_hand.y,z=double(p.z)-bind_hand.z;
        const double distance=x*x+y*y+z*z;
        if(!std::isfinite(distance))return false;
        if(distance>farthest){farthest=distance;value={p,i};}
    }
    if(!(farthest>0))return false;
    out=value;return true;
}
// Compose the exact owned blade route, including its posed half-turn if that
// is part of the supplied mesh matrix. No guessed tip or height correction.
inline bool character_blade_tip_world(const CharacterBladeTip& tip,const ViewMatrix3x4& skin,
        const ViewMatrix3x4& part,const ViewMatrix3x4& root,ViewVec3& out){
    if(character_retarget_detail::overlaps(&out,sizeof(out),&tip,sizeof(tip))
        ||character_retarget_detail::overlaps(&out,sizeof(out),&skin,sizeof(skin))
        ||character_retarget_detail::overlaps(&out,sizeof(out),&part,sizeof(part))
        ||character_retarget_detail::overlaps(&out,sizeof(out),&root,sizeof(root)))return false;
    ViewVec3 value{};
    if(!view_transform_point(skin,tip.bind_point,value)||!view_transform_point(part,value,value)
        ||!view_transform_point(root,value,value))return false;
    out=value;return true;
}
}
