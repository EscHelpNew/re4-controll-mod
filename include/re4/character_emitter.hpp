#pragma once
#include "re4/character_grip.hpp"

namespace re4 {
enum class CharacterEmitterMath : unsigned {Point,Product,Copy};
enum class CharacterEmitterSource : unsigned {None,Laser,Held,BodyHand,Emitter,HeldPart};
struct CharacterEmitterSite {
    CharacterEmitterSource source=CharacterEmitterSource::None;
    unsigned operand=0;
    unsigned part=0;
};
// LR is the original native BL return address, relative to the authenticated
// main image. Only consumers of an exact native world matrix are listed.
inline CharacterEmitterSite character_emitter_site(CharacterEmitterMath math,std::uintptr_t lr){
    using S=CharacterEmitterSource;
    if(math==CharacterEmitterMath::Point){
        switch(lr){
            case 0x2E8FB0:return {S::Laser,0};
            // Independent native BODY10 hit producers: ordinary handguns,
            // both automatic-fire branches, shotgun pellets and alternate
            // handgun fire. Scoped camera-origin rays have no entry here.
            case 0x31DF10:case 0x328288:case 0x3283BC:case 0x336898:case 0x346F04:
            case 0x351BB8: // Bow projectile origin, before native wall clipping.
            case 0x2D6310:case 0x2DA670:case 0x2DAF40:
            case 0x2E2A90:case 0x2E5594:case 0x2E7FD0:
            case 0x340190:case 0x340D00:case 0x3416F8:case 0x34212C:
            case 0x3483B0:case 0x34A4F8:case 0x34C110:case 0x34CBE0:
            case 0x34EEE0:case 0x350858:case 0x351234:case 0x352BB0:return {S::BodyHand,0};
            case 0x1D6A4C:case 0x1F4A48:case 0x1F68AC:case 0x1E0194:return {S::Held,0};
            // These older shell families consume held root0/part1, not BODY10.
            case 0x2DB2AC:case 0x2DBD94:return {S::HeldPart,0,0};
            case 0x2E3238:return {S::HeldPart,0,1};
            default:return {};
        }
    }
    if(math==CharacterEmitterMath::Product){
        switch(lr){
            case 0x1F4C8C:return {S::Held,0};
            case 0x1F4364:case 0x1F4424:case 0x1F4CF0:
            case 0x1F5284:case 0x1F59E0:case 0x1D7F48:return {S::Held,1};
            default:return {};
        }
    }
    if(math==CharacterEmitterMath::Copy){
        if(lr==0x1D7DA4)return {S::Held,0};
        if(lr==0x1E81F0)return {S::Emitter,0};
    }
    return {};
}
inline bool character_emitter_translate(const ViewMatrix3x4& matrix,ViewVec3 delta,ViewMatrix3x4& out){
    if(!view_finite(delta))return false;
    auto value=matrix;
    for(unsigned r=0;r<3;++r){
        for(float n:matrix.m[r])if(!std::isfinite(n))return false;
        const double d=r==0?delta.x:r==1?delta.y:delta.z;
        value.m[r][3]=float(double(matrix.m[r][3])+d);
        if(!std::isfinite(value.m[r][3]))return false;
    }
    out=value;return true;
}
namespace character_emitter_detail {
inline bool product(const ViewMatrix3x4& a,const ViewMatrix3x4& b,ViewMatrix3x4& out){
    for(const auto& row:a.m)for(float v:row)if(!std::isfinite(v))return false;
    for(const auto& row:b.m)for(float v:row)if(!std::isfinite(v))return false;
    ViewMatrix3x4 value{};
    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c){
        double v=c==3?a.m[r][3]:0;
        for(unsigned k=0;k<3;++k)v+=double(a.m[r][k])*b.m[k][c];
        value.m[r][c]=float(v);if(!std::isfinite(value.m[r][c]))return false;
    }
    out=value;return true;
}
inline bool inverse(const ViewMatrix3x4& a,ViewMatrix3x4& out){
    auto linear=a;for(auto& row:linear.m)row[3]=0;
    ViewMatrix3x4 value{};ViewVec3 p{};
    for(unsigned c=0;c<3;++c){
        const ViewVec3 axis{float(c==0),float(c==1),float(c==2)};
        if(!view_inverse_point(linear,axis,p))return false;
        value.m[0][c]=p.x;value.m[1][c]=p.y;value.m[2][c]=p.z;
    }
    if(!view_inverse_point(a,{},p))return false;
    value.m[0][3]=p.x;value.m[1][3]=p.y;value.m[2][3]=p.z;
    out=value;return true;
}
}
struct CharacterEmitterPose {
    ViewVec3 delta[4]{}; // BODY10, BODY16, BODY17, qualified Mine BODY9.
    ViewMatrix3x4 source10{},donor10{};
};
// Mirror native4EC0D0: inverse(rootWorld)*currentBoneWorld*inverseBind.
// The existing donor FK then supplies exactly the displacement used by the
// ordinary-size held render path, evaluated from the consumed CURRENT worlds.
inline bool character_emitter_pose(const CharacterRetargetMap& map,const ViewMatrix3x4* world,
        const ViewMatrix3x4* inverse_bind,CharacterEmitterPose& out,bool two_handed=false){
    if(!world||!inverse_bind
        ||character_retarget_detail::overlaps(&out,sizeof(out),world,26*sizeof(*world))
        ||character_retarget_detail::overlaps(&out,sizeof(out),inverse_bind,26*sizeof(*inverse_bind))
        ||character_retarget_detail::overlaps(&out,sizeof(out),&map,sizeof(map)))return false;
    ViewMatrix3x4 inverse_root{},palette[26]{};
    if(!character_emitter_detail::inverse(world[0],inverse_root))return false;
    for(unsigned i=0;i<26;++i){
        ViewMatrix3x4 normalized{};
        if(!character_emitter_detail::product(inverse_root,world[i],normalized)
            ||!character_emitter_detail::product(normalized,inverse_bind[i],palette[i]))return false;
    }
    CharacterRetargetPose natural{};
    if(!character_grip_pose(map,palette,26,two_handed,natural))return false;
    CharacterEmitterPose value{};auto linear=world[0];for(auto& row:linear.m)row[3]=0;
    constexpr unsigned parent[4]{10,16,17,9};
    for(unsigned i=0;i<4;++i)
        if(!view_transform_point(linear,natural.joint_shift[parent[i]],value.delta[i]))return false;
    value.source10=palette[10];value.donor10=natural.core[10];
    out=value;return true;
}
}
