#pragma once
#include "re4/character_retarget.hpp"
namespace re4 {
// Bone4 is the shared head/neck pivot. The retarget map stores source minus
// donor bind position, so its opposite is the measured donor height change.
// This neutral anatomical reference excludes hats/hair and animated bending.
inline bool character_height_difference(const CharacterRetargetMap& map,float& out){
    if(map.count<26||map.count>character_bone_limit||map.bones[4].source!=4
        ||!view_finite(map.source_bind[4])||!view_finite(map.bones[4].translation))return false;
    out=-map.bones[4].translation.y;return true;
}
// Move the entire camera rig in worldY before native wall collision. Converting
// the displacement through the linear basis also supports rotated/scaled bases;
// changing all three points together retains the look direction and wall arm.
inline bool character_height_camera(const ViewQfpsOutput& native,const ViewMatrix3x4& basis,
        float difference,ViewQfpsOutput& out){
    if(!std::isfinite(difference)||!view_finite(native.eye0)||!view_finite(native.eye1)
        ||!view_finite(native.target)||!std::isfinite(native.roll)||!std::isfinite(native.fov))return false;
    for(const auto& row:basis.m)for(float value:row)if(!std::isfinite(value))return false;
    auto linear=basis;for(auto& row:linear.m)row[3]=0;
    ViewVec3 shift;if(!view_inverse_point(linear,{0,difference,0},shift))return false;
    if(difference==0){out=native;return true;}
    auto value=native;
    const auto translate=[&](ViewVec3 p,ViewVec3& result){
        return view_store_point(double(p.x)+shift.x,double(p.y)+shift.y,double(p.z)+shift.z,result);
    };
    if(!translate(native.eye0,value.eye0)||!translate(native.eye1,value.eye1)
        ||!translate(native.target,value.target))return false;
    out=value;return true;
}
// Native4ED694 multiplies this view by each part's world matrix; postmultiply
// by a worldY translation to lift the entire short preview, including hands,
// knife and separate held actors. Native actor/bones/descriptors stay untouched.
inline bool character_height_case_view(const ViewMatrix3x4& native,float difference,ViewMatrix3x4& out){
    ViewVec3 check;
    if(!std::isfinite(difference)||!view_inverse_point(native,{},check))return false;
    auto value=native;const double lift=std::max(-double(difference),0.0);
    if(lift==0){out=native;return true;}
    for(unsigned row=0;row<3;++row){
        const float translated=float(double(native.m[row][3])+double(native.m[row][1])*lift);
        if(!std::isfinite(translated))return false;
        value.m[row][3]=translated;
    }
    out=value;return true;
}
}
