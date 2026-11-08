#pragma once
#include "re4/view.hpp"
#include <cstddef>
#include <cstdint>

namespace re4 {
// This kernel owns no native resources and changes no actor or input data.
// Bind tables and source skin matrices must remain immutable during a call.
constexpr unsigned character_bone_limit=255;
constexpr std::uint8_t character_core_parents[26]{
    255,0,1,2,3,2,5,5,7,8,9,2,11,11,13,14,15,0,17,18,19,20,17,22,23,24};
struct CharacterBindBone {
    std::uint8_t id=0,parent=255;
    ViewVec3 position{};
};
struct CharacterRetargetBone {
    std::uint8_t source=0;
    ViewVec3 translation{};
};
struct CharacterRetargetMap {
    unsigned count=0;
    CharacterRetargetBone bones[character_bone_limit]{};
    ViewVec3 source_bind[26]{},local_difference[26]{},donor_local[26]{};
};
struct CharacterRetargetPose {
    ViewMatrix3x4 core[26]{};
    ViewVec3 joint_shift[26]{};
};
struct CharacterWeight8 {
    std::uint8_t bone[3]{},links=0,percent[3]{},reserved=0;
};
struct CharacterNativeMatrix {float value[12]{};};
static_assert(sizeof(CharacterWeight8)==8&&sizeof(CharacterNativeMatrix)==0x30);

inline bool character_bind_positions(const CharacterBindBone* bones,unsigned count,
                                     ViewVec3* positions){
    if(!bones||!positions||count<26||count>character_bone_limit)return false;
    for(unsigned i=0;i<count;++i){
        const auto& b=bones[i];
        if(b.id!=i||!view_finite(b.position))return false;
        if(i<26&&b.parent!=character_core_parents[i])return false;
        if(i){
            if(b.parent>=i)return false;
            const auto p=positions[b.parent];
            if(!view_store_point(double(p.x)+b.position.x,double(p.y)+b.position.y,
                                 double(p.z)+b.position.z,positions[i]))return false;
        }else positions[i]=b.position;
    }
    return true;
}
inline bool character_retarget_map(const CharacterBindBone* source,unsigned source_count,
        const CharacterBindBone* donor,unsigned donor_count,CharacterRetargetMap& out){
    ViewVec3 source_bind[character_bone_limit],donor_bind[character_bone_limit];
    if(!character_bind_positions(source,source_count,source_bind)
        ||!character_bind_positions(donor,donor_count,donor_bind))return false;
    CharacterRetargetMap value;value.count=donor_count;
    for(unsigned i=0;i<26;++i){
        value.source_bind[i]=source_bind[i];
        value.donor_local[i]=donor[i].position;
        if(!view_store_point(double(donor[i].position.x)-source[i].position.x,
            double(donor[i].position.y)-source[i].position.y,
            double(donor[i].position.z)-source[i].position.z,value.local_difference[i]))return false;
    }
    for(unsigned i=0;i<donor_count;++i){
        unsigned core=i;
        while(core>=26)core=donor[core].parent; // strictly decreasing, validated above
        value.bones[i].source=std::uint8_t(core);
        if(!view_store_point(double(source_bind[core].x)-donor_bind[core].x,
            double(source_bind[core].y)-donor_bind[core].y,
            double(source_bind[core].z)-donor_bind[core].z,value.bones[i].translation))return false;
    }
    out=value;return true;
}
namespace character_retarget_detail {
inline bool overlaps(const void* a,std::size_t as,const void* b,std::size_t bs){
    const auto x=reinterpret_cast<std::uintptr_t>(a),y=reinterpret_cast<std::uintptr_t>(b);
    return x<=y?y-x<as:x-y<bs;
}
}
// Virtual donor FK. Native actor bones/source palettes remain immutable. All
// anatomical edges use donor local offsets; only spine1/2 and pelvis17 retain
// native animated local translation residuals. Native root motion and every
// source rotation survive; arms/legs never inherit Leon's translation stretch.
inline bool character_retarget_pose(const CharacterRetargetMap& map,
        const ViewMatrix3x4* source,unsigned source_count,CharacterRetargetPose& out){
    if(!source||source_count<26||source_count>character_bone_limit||map.count<26
        ||map.count>character_bone_limit
        ||character_retarget_detail::overlaps(&out,sizeof(out),&map,sizeof(map))
        ||character_retarget_detail::overlaps(&out,sizeof(out),source,std::size_t(source_count)*sizeof(*source)))return false;
    for(unsigned i=0;i<map.count;++i)
        if(map.bones[i].source>=26||(i<26&&map.bones[i].source!=i)
            ||!view_finite(map.bones[i].translation))return false;
    ViewVec3 native[26]{},joint[26]{};
    for(unsigned i=0;i<26;++i){
        if(!view_finite(map.source_bind[i])||!view_finite(map.local_difference[i])
            ||!view_finite(map.donor_local[i])
            ||!view_transform_point(source[i],map.source_bind[i],native[i]))return false;
    }
    CharacterRetargetPose value;
    for(unsigned i=0;i<26;++i){
        if(!i){
            const auto& m=source[0];const auto e=map.local_difference[0];
            if(!view_store_point(double(native[0].x)+double(m.m[0][0])*e.x+double(m.m[0][1])*e.y+double(m.m[0][2])*e.z,
                double(native[0].y)+double(m.m[1][0])*e.x+double(m.m[1][1])*e.y+double(m.m[1][2])*e.z,
                double(native[0].z)+double(m.m[2][0])*e.x+double(m.m[2][1])*e.y+double(m.m[2][2])*e.z,joint[0]))return false;
        }else {
            const unsigned parent=character_core_parents[i];const auto& m=source[parent];
            const bool residual=i==1||i==2||i==17;
            // On translated torso joints, native edge + rotated bind delta
            // retains the native animation residual without requiring inversion.
            const auto e=residual?map.local_difference[i]:map.donor_local[i];
            const double x=double(joint[parent].x)+(residual?double(native[i].x)-native[parent].x:0);
            const double y=double(joint[parent].y)+(residual?double(native[i].y)-native[parent].y:0);
            const double z=double(joint[parent].z)+(residual?double(native[i].z)-native[parent].z:0);
            if(!view_store_point(x+double(m.m[0][0])*e.x+double(m.m[0][1])*e.y+double(m.m[0][2])*e.z,
                y+double(m.m[1][0])*e.x+double(m.m[1][1])*e.y+double(m.m[1][2])*e.z,
                z+double(m.m[2][0])*e.x+double(m.m[2][1])*e.y+double(m.m[2][2])*e.z,joint[i]))return false;
        }
        if(!view_store_point(double(joint[i].x)-native[i].x,double(joint[i].y)-native[i].y,
            double(joint[i].z)-native[i].z,value.joint_shift[i]))return false;
        const auto b=map.source_bind[i],t=map.bones[i].translation;
        ViewVec3 donor;if(!view_store_point(double(b.x)-t.x,double(b.y)-t.y,double(b.z)-t.z,donor))return false;
        value.core[i]=source[i];
        for(unsigned r=0;r<3;++r){
            const auto* row=source[i].m[r];const double q=r==0?joint[i].x:r==1?joint[i].y:joint[i].z;
            const float translated=float(q-double(row[0])*donor.x-double(row[1])*donor.y-double(row[2])*donor.z);
            if(!std::isfinite(translated))return false;
            value.core[i].m[r][3]=translated;
        }
    }
    out=value;return true;
}
inline int character_retarget_blend(const CharacterRetargetMap& map,const CharacterRetargetPose& pose,
        const CharacterWeight8* weights,unsigned count,CharacterNativeMatrix* output,unsigned capacity){
    if(!weights||!output||!count||count>255||capacity<count||map.count<26||map.count>character_bone_limit)return -1;
    const auto bytes=std::size_t(count)*sizeof(*output);
    if(character_retarget_detail::overlaps(output,bytes,&map,sizeof(map))
        ||character_retarget_detail::overlaps(output,bytes,&pose,sizeof(pose))
        ||character_retarget_detail::overlaps(output,bytes,weights,std::size_t(count)*sizeof(*weights)))return -1;
    for(const auto& m:pose.core)for(const auto& row:m.m)for(float v:row)if(!std::isfinite(v))return -1;
    for(unsigned i=0;i<count;++i){
        const auto& w=weights[i];if(w.links<1||w.links>3)return -1;
        unsigned used=0;
        for(unsigned j=0;j<w.links;++j){
            if(w.bone[j]>=map.count||map.bones[w.bone[j]].source>=26)return -1;
            if(j+1<w.links){if(w.percent[j]>100-used)return -1;used+=w.percent[j];}
        }
    }
    // Convex combinations of finite float matrices cannot overflow float;
    // validate before the first write, then perform the bounded output pass.
    int links=0;
    for(unsigned i=0;i<count;++i){
        CharacterNativeMatrix value;const auto& w=weights[i];unsigned used=0;
        double result[12]{};
        for(unsigned j=0;j<w.links;++j){
            const unsigned percent=j+1==w.links?100-used:w.percent[j];used+=percent;
            const auto& m=pose.core[map.bones[w.bone[j]].source];const double fraction=double(percent)/100;
            for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)result[c*3+r]+=fraction*m.m[r][c];
        }
        for(unsigned k=0;k<12;++k)value.value[k]=float(result[k]);
        output[i]=value;links+=w.links;
    }
    return links;
}
inline bool character_retarget_one(const CharacterRetargetMap& map,const CharacterWeight8& weight,
        const ViewMatrix3x4* source,unsigned source_count,CharacterNativeMatrix& out){
    if(!map.count||map.count>character_bone_limit||!source||source_count<26
        ||source_count>character_bone_limit||weight.links<1||weight.links>3)return false;
    double result[3][4]{};unsigned used=0;
    for(unsigned link=0;link<weight.links;++link){
        const unsigned id=weight.bone[link];
        if(id>=map.count)return false;
        const auto& b=map.bones[id];
        if(b.source>=26||b.source>=source_count||!view_finite(b.translation))return false;
        // Native 4EC750 uses the remainder for the last link, even if its
        // stored percent differs. Reject negative remainders before writing.
        unsigned percent=100-used;
        if(link+1<weight.links){
            percent=weight.percent[link];
            if(percent>100-used)return false;
            used+=percent;
        }
        const auto& s=source[b.source];
        for(const auto& row:s.m)for(float v:row)if(!std::isfinite(v))return false;
        const double fraction=double(percent)/100;
        for(unsigned row=0;row<3;++row){
            for(unsigned col=0;col<3;++col)result[row][col]+=fraction*s.m[row][col];
            const double corrected=double(s.m[row][0])*b.translation.x
                +double(s.m[row][1])*b.translation.y+double(s.m[row][2])*b.translation.z+s.m[row][3];
            result[row][3]+=fraction*corrected;
        }
    }
    CharacterNativeMatrix value;
    for(unsigned row=0;row<3;++row)for(unsigned col=0;col<4;++col){
        const float v=float(result[row][col]);
        if(!std::isfinite(v))return false;
        value.value[col*3+row]=v;
    }
    out=value;return true;
}
// Validate the entire batch before touching native output. A second bounded
// pass avoids a 12 KiB temporary palette on the native render-task stack.
inline int character_retarget_blend(const CharacterRetargetMap& map,const CharacterWeight8* weights,
        unsigned count,const ViewMatrix3x4* source,unsigned source_count,
        CharacterNativeMatrix* output,unsigned capacity){
    if(!weights||!source||!output||!count||count>255||capacity<count
        ||source_count<26||source_count>character_bone_limit)return -1;
    const auto overlaps=[](const void* a,std::size_t as,const void* b,std::size_t bs){
        const auto x=reinterpret_cast<std::uintptr_t>(a),y=reinterpret_cast<std::uintptr_t>(b);
        return x<=y?y-x<as:x-y<bs;
    };
    const auto bytes=std::size_t(count)*sizeof(*output);
    if(overlaps(output,bytes,weights,std::size_t(count)*sizeof(*weights))
        ||overlaps(output,bytes,source,std::size_t(source_count)*sizeof(*source))
        ||overlaps(output,bytes,&map,sizeof(map)))return -1;
    int links=0;CharacterNativeMatrix value;
    for(unsigned i=0;i<count;++i){
        if(!character_retarget_one(map,weights[i],source,source_count,value))return -1;
        links+=weights[i].links;
    }
    for(unsigned i=0;i<count;++i){
        character_retarget_one(map,weights[i],source,source_count,value);
        output[i]=value;
    }
    return links;
}
}
