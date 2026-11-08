#include "re4/character_retarget.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace {
using namespace re4;
std::array<CharacterBindBone,28> skeleton(float scale){
    std::array<CharacterBindBone,28> b{};
    for(unsigned i=0;i<b.size();++i){
        b[i].id=std::uint8_t(i);
        b[i].parent=i<26?character_core_parents[i]:std::uint8_t(i-1);
        b[i].position={scale*float(i%3),scale*float(i+1),-scale*float(i%2)};
    }
    return b;
}
ViewMatrix3x4 identity(){return {{{1,0,0,0},{0,1,0,0},{0,0,1,0}}};}
void near(float a,float b){assert(std::fabs(a-b)<0.0001f);}
ViewVec3 matrix_point(const ViewMatrix3x4& m,ViewVec3 p){
    ViewVec3 out{};assert(view_transform_point(m,p,out));return out;
}
float distance(ViewVec3 a,ViewVec3 b){
    return std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z));
}
void natural_proportions(){
    auto source_bind=skeleton(2),donor_bind=skeleton(.7f);
    CharacterRetargetMap map;
    assert(character_retarget_map(source_bind.data(),28,donor_bind.data(),28,map));
    std::array<ViewVec3,28> lp{},dp{};
    assert(character_bind_positions(source_bind.data(),28,lp.data()));
    assert(character_bind_positions(donor_bind.data(),28,dp.data()));
    std::array<ViewMatrix3x4,28> source;
    for(auto& m:source)m=identity();
    const auto before=source;CharacterRetargetPose pose;
    assert(character_retarget_pose(map,source.data(),28,pose));
    assert(!std::memcmp(source.data(),before.data(),sizeof(source)));
    // Natural root height and every limb recover the actual donor rest rig.
    for(unsigned i=0;i<26;++i){
        const auto q=matrix_point(pose.core[i],dp[i]);
        near(q.x,dp[i].x);near(q.y,dp[i].y);near(q.z,dp[i].z);
    }
    // Native animated torso/pelvis translations survive. Deliberately large
    // arm/leg translation residuals must never stretch a donor limb.
    for(unsigned sample=0;sample<80;++sample){
        ViewVec3 source_q[26]{},expected[26]{};
        for(unsigned i=0;i<26;++i){
            const float angle=float(sample+i)*.027f,c=std::cos(angle),sn=std::sin(angle);
            source[i]={{{c,-sn,0,0},{sn,c,0,0},{0,0,1,0}}};
            const unsigned parent=character_core_parents[i];
            const ViewVec3 residual{float(i%3)*13,float(i%5)*7,float(i%2)*11};
            if(!i){source_q[i]={17,-29,31};expected[i]=source_q[i];
                const auto offset=matrix_point(source[0],{dp[0].x-lp[0].x,dp[0].y-lp[0].y,dp[0].z-lp[0].z});
                expected[0]={expected[0].x+offset.x,expected[0].y+offset.y,expected[0].z+offset.z};
            }else{
                auto rotation=source[parent];for(auto& row:rotation.m)row[3]=0;
                const auto l=matrix_point(rotation,source_bind[i].position),d=matrix_point(rotation,donor_bind[i].position);
                source_q[i]={source_q[parent].x+l.x+residual.x,source_q[parent].y+l.y+residual.y,source_q[parent].z+l.z+residual.z};
                const auto keep=(i==1||i==2||i==17)?residual:ViewVec3{};
                expected[i]={expected[parent].x+d.x+keep.x,expected[parent].y+d.y+keep.y,expected[parent].z+d.z+keep.z};
            }
            const auto origin=matrix_point(source[i],lp[i]);
            source[i].m[0][3]=source_q[i].x-origin.x;source[i].m[1][3]=source_q[i].y-origin.y;source[i].m[2][3]=source_q[i].z-origin.z;
        }
        const auto immutable=source;
        assert(character_retarget_pose(map,source.data(),28,pose));
        assert(!std::memcmp(source.data(),immutable.data(),sizeof(source)));
        for(unsigned i=0;i<26;++i){
            const auto q=matrix_point(pose.core[i],dp[i]);
            assert(distance(q,expected[i])<.0002f);
            if(i&&i!=1&&i!=2&&i!=17){
                const auto p=matrix_point(pose.core[character_core_parents[i]],dp[character_core_parents[i]]);
                assert(std::fabs(distance(p,q)-distance({},donor_bind[i].position))<.0002f);
            }
        }
    }
    const CharacterWeight8 secondary{{27,0,0},1,{100,0,0},0};CharacterNativeMatrix value{};
    assert(character_retarget_blend(map,pose,&secondary,1,&value,1)==1);
    const auto core=map.bones[27].source;
    for(unsigned row=0;row<3;++row)for(unsigned col=0;col<4;++col)near(value.value[col*3+row],pose.core[core].m[row][col]);
    const auto saved=pose;const auto old_value=value;
    source[4].m[1][2]=std::numeric_limits<float>::quiet_NaN();
    assert(!character_retarget_pose(map,source.data(),28,pose));assert(!std::memcmp(&saved,&pose,sizeof(pose)));
    source=before;assert(!character_retarget_pose(map,source.data(),25,pose));
    auto corrupt=map;corrupt.bones[0].source=25;assert(!character_retarget_pose(corrupt,source.data(),28,pose));
    auto invalid=secondary;invalid.bone[0]=250;
    assert(character_retarget_blend(map,saved,&invalid,1,&value,1)==-1);assert(!std::memcmp(&old_value,&value,sizeof(value)));
    auto bad_pose=saved;bad_pose.core[0].m[0][0]=std::numeric_limits<float>::infinity();
    assert(character_retarget_blend(map,bad_pose,&secondary,1,&value,1)==-1);
    auto aliased=saved;assert(character_retarget_blend(map,aliased,&secondary,1,reinterpret_cast<CharacterNativeMatrix*>(&aliased),1)==-1);
    assert(!std::memcmp(&aliased,&saved,sizeof(saved)));
    assert(!character_retarget_pose(map,source.data(),28,*reinterpret_cast<CharacterRetargetPose*>(source.data())));
    assert(!std::memcmp(source.data(),before.data(),sizeof(source)));
    CharacterWeight8 mixed{{3,7,9},3,{20,30,255},0};CharacterNativeMatrix mixed_a{},mixed_b{};
    assert(character_retarget_blend(map,saved,&mixed,1,&mixed_a,1)==3);mixed.percent[2]=0;
    assert(character_retarget_blend(map,saved,&mixed,1,&mixed_b,1)==3);assert(!std::memcmp(&mixed_a,&mixed_b,sizeof(mixed_a)));
}
}
int main(){
    natural_proportions();
    auto leon=skeleton(2),donor=skeleton(1);
    CharacterRetargetMap map{};
    assert(character_retarget_map(leon.data(),leon.size(),donor.data(),donor.size(),map));
    assert(map.count==28&&map.bones[27].source==25);
    // Source hip26/27 are deliberately unrelated: donor secondary joints must
    // follow their nearest canonical ancestor rather than source numeric ID.
    std::array<ViewMatrix3x4,28> source;
    for(auto& m:source)m=identity();
    source[25]={{{0,-1,0,20},{1,0,0,30},{0,0,1,40}}};
    const CharacterWeight8 weights[]{{{27,0,0},1,{0,0,0},0},{{4,25,0},2,{25,99,0},0}};
    std::array<CharacterNativeMatrix,2> result{};
    assert(character_retarget_blend(map,weights,2,source.data(),source.size(),result.data(),result.size())==3);
    ViewVec3 p{7,8,9},expected{};
    const auto delta=map.bones[27].translation;
    assert(view_transform_point(source[25],{p.x+delta.x,p.y+delta.y,p.z+delta.z},expected));
    const auto& r=result[0].value;
    near(r[0]*p.x+r[3]*p.y+r[6]*p.z+r[9],expected.x);
    near(r[1]*p.x+r[4]*p.y+r[7]*p.z+r[10],expected.y);
    near(r[2]*p.x+r[5]*p.y+r[8]*p.z+r[11],expected.z);
    // The final stored percentage is ignored by the engine: last=1-sum(prior).
    auto changed=weights[1];changed.percent[1]=0;
    CharacterNativeMatrix same{};
    assert(character_retarget_blend(map,&changed,1,source.data(),source.size(),&same,1)==2);
    for(unsigned i=0;i<12;++i)near(same.value[i],result[1].value[i]);

    const auto saved=map;
    donor[27].parent=27;
    assert(!character_retarget_map(leon.data(),28,donor.data(),28,map));
    assert(map.count==saved.count&&map.bones[27].source==saved.bones[27].source);
    donor=skeleton(1);donor[7].parent=6;
    assert(!character_retarget_map(leon.data(),28,donor.data(),28,map));
    donor=skeleton(1);donor[27].position.x=std::numeric_limits<float>::infinity();
    assert(!character_retarget_map(leon.data(),28,donor.data(),28,map));
    donor=skeleton(1);donor[27].id=26;
    assert(!character_retarget_map(leon.data(),28,donor.data(),28,map));
    assert(!character_retarget_map(nullptr,28,donor.data(),28,map));
    assert(!character_retarget_map(leon.data(),25,donor.data(),28,map));

    auto invalid=weights[0];invalid.bone[0]=99;
    const auto old=result;
    CharacterWeight8 batch[]{weights[0],invalid};
    assert(character_retarget_blend(saved,batch,2,source.data(),28,result.data(),2)==-1);
    for(unsigned j=0;j<2;++j)for(unsigned i=0;i<12;++i)near(old[j].value[i],result[j].value[i]);
    invalid=weights[0];invalid.links=0;
    assert(character_retarget_blend(saved,&invalid,1,source.data(),28,result.data(),2)==-1);
    invalid={{0,1,2},3,{80,30,0},0};
    assert(character_retarget_blend(saved,&invalid,1,source.data(),28,result.data(),2)==-1);
    assert(character_retarget_blend(saved,weights,2,source.data(),28,result.data(),1)==-1);
    const auto before_alias=source;
    assert(character_retarget_blend(saved,weights,2,source.data(),28,
        reinterpret_cast<CharacterNativeMatrix*>(source.data()),28)==-1);
    assert(!std::memcmp(before_alias.data(),source.data(),sizeof(source)));
    source[0].m[0][0]=std::numeric_limits<float>::quiet_NaN();
    invalid={{0,0,0},1,{100,0,0},0};
    assert(character_retarget_blend(saved,&invalid,1,source.data(),28,result.data(),2)==-1);
    std::puts("character retarget: natural donor proportions, strict limb lengths, native blends and rejection PASS");
}
