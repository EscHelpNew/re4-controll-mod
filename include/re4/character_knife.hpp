#pragma once
#include "re4/character_assets.hpp"
#include <limits>

namespace re4 {
// Prepared from the authenticated, still-relative knife BIN and dense donor
// bind table before native attachment relocates either resource. This owns no
// geometry and carries no native pointers into later skin/draw callbacks.
struct CharacterKnifeOrientation {ViewVec3 bind_pivot{},bind_axis{};};
namespace character_knife_detail {
inline bool span(const void* data,std::size_t size){
    return data&&size&&size<=std::numeric_limits<std::uintptr_t>::max()-reinterpret_cast<std::uintptr_t>(data);
}
inline bool overlaps(const void* a,std::size_t as,const void* b,std::size_t bs){
    const auto x=reinterpret_cast<std::uintptr_t>(a),y=reinterpret_cast<std::uintptr_t>(b);
    return x<=y?y-x<as:x-y<bs;
}
inline bool axis(double x,double y,double z,ViewVec3& out){
    const double length=std::hypot(x,y,z);
    return std::isfinite(length)&&length>0&&view_store_point(x/length,y/length,z/length,out);
}
}
// This validates the complete raw BIN arrays through the shared asset parser,
// including finite positions/normals and the knife's single bone10 influence.
// Authentication belongs to the caller's existing archive-admission boundary.
// Reject aliases and malformed resources before publishing any output.
inline bool character_knife_prepare(CharacterBytes relative_bin,const CharacterBindBone* donor_bind,
        unsigned donor_count,CharacterKnifeOrientation& out){
    using namespace character_knife_detail;
    if(donor_count<26||donor_count>character_bone_limit||relative_bin.size>character_archive_limit
        ||!span(relative_bin.data,relative_bin.size)||!span(donor_bind,std::size_t(donor_count)*sizeof(*donor_bind))
        ||reinterpret_cast<std::uintptr_t>(donor_bind)%alignof(CharacterBindBone)
        ||overlaps(&out,sizeof(out),relative_bin.data,relative_bin.size)
        ||overlaps(&out,sizeof(out),donor_bind,std::size_t(donor_count)*sizeof(*donor_bind)))return false;
    CharacterAssetPiece knife;
    // bin/apply_role only read this view; the mutable wrapper matches the
    // existing asset parser's output type without relocating or editing bytes.
    if(!character_asset_detail::bin({const_cast<std::uint8_t*>(relative_bin.data),relative_bin.size},donor_count,knife)
        ||!character_asset_detail::apply_role(knife,CharacterAssetRole::Knife))return false;
    ViewVec3 bind[character_bone_limit];
    if(!character_bind_positions(donor_bind,donor_count,bind))return false;
    CharacterBytes positions;
    if(!character_asset_detail::array(relative_bin,0x48,knife.vertex_count,12,4,positions))return false;
    ViewVec3 minimum,maximum;
    if(!character_read(positions,0,minimum))return false;
    maximum=minimum;
    for(unsigned i=1;i<knife.vertex_count;++i){
        ViewVec3 p;if(!character_read(positions,std::size_t(i)*12,p))return false;
        minimum={std::min(minimum.x,p.x),std::min(minimum.y,p.y),std::min(minimum.z,p.z)};
        maximum={std::max(maximum.x,p.x),std::max(maximum.y,p.y),std::max(maximum.z,p.z)};
    }
    if(!(maximum.x>minimum.x&&maximum.y>minimum.y&&maximum.z>minimum.z))return false;
    CharacterKnifeOrientation value;
    // X/Z stay on the bind grasp. The cross-section's actual midpointY keeps
    // the handle in its original hand envelope when the blade reverses Z.
    if(!view_store_point(bind[10].x,(double(minimum.y)+maximum.y)*.5,bind[10].z,value.bind_pivot)
        ||!axis(double(bind[10].x)-bind[9].x,double(bind[10].y)-bind[9].y,
            double(bind[10].z)-bind[9].z,value.bind_axis))return false;
    out=value;return true;
}
// Construct a proper rigid half-turn IN THE CURRENT SKINNED coordinate space:
// p = skin10 * bindPivot, a = normalize(linear(skin10) * bindAxis),
// R = 2*a*a^T - I, t = p - R*p. The entire grasp axis is fixed; perpendicular
// blade directions reverse. Scaling/shear in the input never enters R.
// The caller snapshots this result per skin frame slot, then composes M*H for
// an attached part or inverse(weaponWorld)*playerWorld*H for an independent one.
inline bool character_knife_half_turn(const CharacterKnifeOrientation& orientation,
        const ViewMatrix3x4& current_skin_bone10,ViewMatrix3x4& out){
    using namespace character_knife_detail;
    if(overlaps(&out,sizeof(out),&orientation,sizeof(orientation))
        ||overlaps(&out,sizeof(out),&current_skin_bone10,sizeof(current_skin_bone10))
        ||!view_finite(orientation.bind_pivot)||!view_finite(orientation.bind_axis))return false;
    double scale=0;
    for(const auto& row:current_skin_bone10.m)for(unsigned c=0;c<4;++c){
        if(!std::isfinite(row[c]))return false;
        if(c<3)scale=std::max(scale,std::fabs(double(row[c])));
    }
    if(!(scale>0))return false;
    double linear[3][3];
    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)linear[r][c]=current_skin_bone10.m[r][c]/scale;
    const double determinant=linear[0][0]*(linear[1][1]*linear[2][2]-linear[1][2]*linear[2][1])
        -linear[0][1]*(linear[1][0]*linear[2][2]-linear[1][2]*linear[2][0])
        +linear[0][2]*(linear[1][0]*linear[2][1]-linear[1][1]*linear[2][0]);
    if(!std::isfinite(determinant)||std::fabs(determinant)<=1e-12)return false;
    const auto v=orientation.bind_axis;ViewVec3 direction,pivot;
    if(!axis(linear[0][0]*v.x+linear[0][1]*v.y+linear[0][2]*v.z,
             linear[1][0]*v.x+linear[1][1]*v.y+linear[1][2]*v.z,
             linear[2][0]*v.x+linear[2][1]*v.y+linear[2][2]*v.z,direction)
        ||!view_transform_point(current_skin_bone10,orientation.bind_pivot,pivot))return false;
    // Normalize once more in double after the float direction boundary.
    const double length=std::hypot(double(direction.x),double(direction.y),double(direction.z));
    const double a[]{direction.x/length,direction.y/length,direction.z/length};
    const double p[]{pivot.x,pivot.y,pivot.z};ViewMatrix3x4 value{};
    for(unsigned r=0;r<3;++r){
        double translation=p[r];
        for(unsigned c=0;c<3;++c){
            const double rotation=2*a[r]*a[c]-(r==c?1:0);
            value.m[r][c]=float(rotation);translation-=rotation*p[c];
        }
        value.m[r][3]=float(translation);
        if(!std::isfinite(value.m[r][3]))return false;
    }
    out=value;return true;
}
}
