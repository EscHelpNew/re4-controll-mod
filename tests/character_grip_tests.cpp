#include "re4/character_grip.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <cstdlib>
#undef assert
#define assert(condition) do { if(!(condition)){std::fprintf(stderr,"grip line %d: %s\n",__LINE__,#condition);std::exit(1);} } while(false)

namespace {
using namespace re4;
constexpr ViewMatrix3x4 identity{{{1,0,0,0},{0,1,0,0},{0,0,1,0}}};
ViewVec3 add(ViewVec3 a,ViewVec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
ViewVec3 sub(ViewVec3 a,ViewVec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
double length(ViewVec3 p){return std::hypot(double(p.x),double(p.y),double(p.z));}
double distance(ViewVec3 a,ViewVec3 b){return length(sub(a,b));}
bool near(ViewVec3 a,ViewVec3 b,double tolerance=2e-5){return distance(a,b)<tolerance;}
ViewVec3 point(const ViewMatrix3x4& matrix,ViewVec3 bind){ViewVec3 out{};assert(view_transform_point(matrix,bind,out));return out;}
struct Rig {
    CharacterRetargetMap map{};
    std::array<ViewVec3,26> source{},donor{};
    std::array<ViewMatrix3x4,26> palette{};
    Rig(ViewVec3 contact={0,4,0},bool asymmetric=false){
        donor[7]={0,0,0};donor[8]={1.6f,1.2f,0};donor[9]={2.1f,.8f,0};donor[10]={2.6f,.4f,0};
        donor[13]={0,3,0};donor[14]={1.6f,1.8f,0};donor[15]={2.1f,2.2f,0};donor[16]={2.6f,2.6f,0};
        if(asymmetric){donor[14]={4,0,0};donor[15]={4.32f,.24f,0};donor[16]={4.8f,.6f,0};}
        source[10]={2,0,0};source[16]=add(source[10],contact);
        rebuild();
    }
    void rebuild(){
        std::array<CharacterBindBone,26> native{},model{};
        for(unsigned i=0;i<26;++i){
            native[i].id=model[i].id=std::uint8_t(i);
            native[i].parent=model[i].parent=character_core_parents[i];
            const unsigned parent=character_core_parents[i];
            native[i].position=i?sub(source[i],source[parent]):source[i];
            model[i].position=i?sub(donor[i],donor[parent]):donor[i];
            palette[i]=identity;
        }
        assert(character_retarget_map(native.data(),26,model.data(),26,map));
    }
    ViewVec3 joint(const CharacterRetargetPose& pose,unsigned i)const{return point(pose.core[i],donor[i]);}
    CharacterRetargetPose natural()const{CharacterRetargetPose out{};assert(character_retarget_pose(map,palette.data(),26,out));return out;}
    CharacterRetargetPose fit(bool enabled=true)const{CharacterRetargetPose out{};assert(character_grip_pose(map,palette.data(),26,enabled,out));return out;}
};
void contact_and_lengths(const Rig& rig,const CharacterRetargetPose& natural,const CharacterRetargetPose& fitted){
    const auto native_right=point(rig.palette[10],rig.map.source_bind[10]);
    const auto native_left=point(rig.palette[16],rig.map.source_bind[16]);
    assert(near(sub(rig.joint(fitted,16),rig.joint(fitted,10)),sub(native_left,native_right)));
    assert(!std::memcmp(&fitted.joint_shift[10],&fitted.joint_shift[16],sizeof(ViewVec3)));
    for(unsigned start:{7u,13u}){
        assert(near(rig.joint(natural,start),rig.joint(fitted,start)));
        for(unsigned i=start+1;i<=start+3;++i)
            assert(std::fabs(distance(rig.joint(natural,i-1),rig.joint(natural,i))-distance(rig.joint(fitted,i-1),rig.joint(fitted,i)))<2e-5);
        const auto old_compound=sub(rig.joint(natural,start+3),rig.joint(natural,start+1));
        const auto new_compound=sub(rig.joint(fitted,start+3),rig.joint(fitted,start+1));
        assert(std::fabs(length(old_compound)-length(new_compound))<2e-5);
        const auto shoulder=rig.joint(fitted,start),end=sub(rig.joint(fitted,start+3),shoulder);
        if(length(end)>1e-5){
            const auto elbow=sub(rig.joint(fitted,start+1),shoulder),old_elbow=sub(rig.joint(natural,start+1),shoulder);
            const double n=length(end),along=(double(elbow.x)*end.x+double(elbow.y)*end.y+double(elbow.z)*end.z)/(n*n);
            const double old_along=(double(old_elbow.x)*end.x+double(old_elbow.y)*end.y+double(old_elbow.z)*end.z)/(n*n);
            const ViewVec3 bend{float(elbow.x-along*end.x),float(elbow.y-along*end.y),float(elbow.z-along*end.z)};
            const ViewVec3 pole{float(old_elbow.x-old_along*end.x),float(old_elbow.y-old_along*end.y),float(old_elbow.z-old_along*end.z)};
            if(length(bend)>1e-3){
                assert(length(pole)>1e-7);
                assert(near({float(bend.x/length(bend)),float(bend.y/length(bend)),float(bend.z/length(bend))},
                    {float(pole.x/length(pole)),float(pole.y/length(pole)),float(pole.z/length(pole))},2e-4));
            }
        }
        // Proper rotations preserve basis inner products, scale and handedness.
        for(unsigned i=start;i<start+3;++i)for(unsigned a=0;a<3;++a)for(unsigned b=0;b<3;++b){
            double before=0,after=0;
            for(unsigned r=0;r<3;++r){before+=double(natural.core[i].m[r][a])*natural.core[i].m[r][b];after+=double(fitted.core[i].m[r][a])*fitted.core[i].m[r][b];}
            assert(std::fabs(before-after)<2e-5);
        }
    }
    for(unsigned palm:{10u,16u})for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)
        assert(fitted.core[palm].m[r][c]==natural.core[palm].m[r][c]);
    for(unsigned i=0;i<26;++i)if(!((i>=7&&i<=10)||(i>=13&&i<=16))){
        assert(!std::memcmp(&natural.core[i],&fitted.core[i],sizeof(ViewMatrix3x4)));
        assert(!std::memcmp(&natural.joint_shift[i],&fitted.joint_shift[i],sizeof(ViewVec3)));
    }
}
void minimality(const Rig& rig,const CharacterRetargetPose& before,const CharacterRetargetPose& fitted){
    const auto p=rig.joint(before,10),answer=rig.joint(fitted,10),right=rig.joint(before,7),left=rig.joint(before,13);
    const auto d=sub(point(rig.palette[16],rig.map.source_bind[16]),point(rig.palette[10],rig.map.source_bind[10]));
    const double ur=distance(right,rig.joint(before,8)),fr=distance(rig.joint(before,8),p);
    const double ul=distance(left,rig.joint(before,14)),fl=distance(rig.joint(before,14),rig.joint(before,16));
    const double best=distance(answer,p);unsigned feasible=0;
    // Independent Cartesian search over the arm volume, including both signs
    // of Z; this does not enumerate the solver's boundary candidates.
    for(int x=-22;x<=22;++x)for(int y=-22;y<=22;++y)for(int z=-10;z<=10;++z){
        const ViewVec3 candidate{float(x)*.2f,float(y)*.2f,float(z)*.25f};
        const double r=distance(candidate,right),l=distance(add(candidate,d),left);
        if(r<std::fabs(ur-fr)||r>ur+fr||l<std::fabs(ul-fl)||l>ul+fl)continue;
        ++feasible;assert(best<=distance(candidate,p)+2e-5);
    }
    assert(feasible>0);
}
void reachable(){
    for(const auto d:{ViewVec3{0,4,0},ViewVec3{0,6.5f,0},ViewVec3{4,-2,0}}){
        Rig rig(d);const auto palette=rig.palette;const auto map=rig.map;
        const auto before=rig.natural(),fitted=rig.fit();contact_and_lengths(rig,before,fitted);minimality(rig,before,fitted);
        assert(!std::memcmp(&palette,&rig.palette,sizeof(palette))&&!std::memcmp(&map,&rig.map,sizeof(map)));
    }
    Rig unchanged;const auto before=unchanged.natural(),fitted=unchanged.fit();
    assert(near(unchanged.joint(before,10),unchanged.joint(fitted,10))); // Already feasible: zero weapon movement.
    Rig inner({-2.6f,2.6f,0},true);const auto inner_before=inner.natural(),inner_fit=inner.fit();
    contact_and_lengths(inner,inner_before,inner_fit);minimality(inner,inner_before,inner_fit);
    assert(std::fabs(distance(inner.joint(inner_fit,10),inner.joint(inner_before,10))-4)<2e-5);
}
void covariance(){
    Rig original({4,-2,0});const auto fitted=original.fit();
    Rig moved=original;const ViewMatrix3x4 transform{{{0,-1,0,120},{1,0,0,-90},{0,0,1,70}}};
    moved.palette.fill(transform);const auto result=moved.fit();contact_and_lengths(moved,moved.natural(),result);
    // Float palettes translated120 units quantize the2-unit arms; allow the
    // resulting near-extension elbow sensitivity, while checking lengths above.
    for(unsigned i=0;i<26;++i)assert(near(moved.joint(result,i),point(transform,original.joint(fitted,i)),1e-4));
    Rig scaled;const ViewMatrix3x4 scale{{{1.3f,.1f,0,0},{0,.8f,0,0},{0,0,1.1f,0}}};
    scaled.palette.fill(scale);contact_and_lengths(scaled,scaled.natural(),scaled.fit());
}
void degenerate_boundaries(){
    Rig concentric({0,3,0});contact_and_lengths(concentric,concentric.natural(),concentric.fit());
    Rig tangent({-10,3,0});
    tangent.donor[8]={0,2,0};tangent.donor[9]={1.2f,2,0};tangent.donor[10]={3,2,0};
    tangent.donor[14]={0,5,0};tangent.donor[15]={1.2f,5,0};tangent.donor[16]={3,5,0};tangent.rebuild();
    const auto extended=tangent.fit();contact_and_lengths(tangent,tangent.natural(),extended);
    assert(near(tangent.joint(extended,10),{5,0,0}));
    // The left lower edge reverses direction at this unique tangent; its
    // proper180-degree rotation must not reflect the helper or its basis.
    const auto& matrix=extended.core[14];
    const double determinant=matrix.m[0][0]*(matrix.m[1][1]*matrix.m[2][2]-matrix.m[1][2]*matrix.m[2][1])
        -matrix.m[0][1]*(matrix.m[1][0]*matrix.m[2][2]-matrix.m[1][2]*matrix.m[2][0])
        +matrix.m[0][2]*(matrix.m[1][0]*matrix.m[2][1]-matrix.m[1][1]*matrix.m[2][0]);
    assert(std::fabs(determinant-1)<1e-5);
    Rig singular({-3,3,0});
    singular.donor[8]={2,0,0};singular.donor[9]={3,0,0};singular.donor[10]={5,0,0};
    singular.donor[14]={.3f,3.4f,0};singular.donor[15]={.45f,3.6f,0};singular.donor[16]={.6f,3.8f,0};singular.rebuild();
    const auto natural=singular.natural(),fallback=singular.fit();assert(!std::memcmp(&natural,&fallback,sizeof(natural)));
    // A straight upper/forearm axis is valid when the new elbow height is0.
    singular.source[16]=add(singular.source[10],{-4,3,0});singular.rebuild();
    contact_and_lengths(singular,singular.natural(),singular.fit());
    Rig folded({1,3,0});
    folded.donor[8]={1,std::sqrt(3.f),0};folded.donor[9]={1.4f,std::sqrt(3.f)*.6f,0};folded.donor[10]={2,0,0};
    folded.donor[14]={.3f,3.4f,0};folded.donor[15]={.45f,3.6f,0};folded.donor[16]={.6f,3.8f,0};folded.rebuild();
    const auto full_fold=folded.fit();contact_and_lengths(folded,folded.natural(),full_fold);assert(near(folded.joint(full_fold,10),{}));
    // Both inner boundaries are active and P lies on their centers' axis.
    // Every circle point ties for minimality; the posed anatomical seed picks
    // a covariant direction instead of inventing a global pole.
    Rig circle({-3,3,0});circle.donor[8]={3,0,0};circle.donor[9]={2.5f,0,0};circle.donor[10]={2,0,0};
    circle.donor[14]={0,6,0};circle.donor[15]={.4f,6,0};circle.donor[16]={1,6,0};circle.rebuild();
    const auto circle_fit=circle.fit();contact_and_lengths(circle,circle.natural(),circle_fit);
    assert(std::fabs(distance(circle.joint(circle_fit,10),circle.donor[10])-std::sqrt(2.0))<2e-5);
    // The remaining mixed boundary case: the left outer sphere clips the
    // right inner hole, so neither single-sphere projection is admissible.
    Rig mixed({0,2,0});mixed.donor[8]={3,0,0};mixed.donor[9]={2.5f,0,0};mixed.donor[10]={2,0,0};
    mixed.donor[14]={.45f,3.6f,0};mixed.donor[15]={.675f,3.9f,0};mixed.donor[16]={.9f,4.2f,0};mixed.rebuild();
    const auto mixed_fit=mixed.fit();contact_and_lengths(mixed,mixed.natural(),mixed_fit);minimality(mixed,mixed.natural(),mixed_fit);
    assert(std::fabs(distance(mixed.joint(mixed_fit,10),{})-2)<2e-5);
    assert(std::fabs(distance(mixed.joint(mixed_fit,16),mixed.donor[13])-1.5)<2e-5);
    Rig empty({0,3,0});empty.donor[8]={3,0,0};empty.donor[9]={2.5f,0,0};empty.donor[10]={2,0,0};
    empty.donor[14]={.15f,3.2f,0};empty.donor[15]={.225f,3.3f,0};empty.donor[16]={.3f,3.4f,0};empty.rebuild();
    const auto empty_before=empty.natural(),empty_after=empty.fit();assert(!std::memcmp(&empty_before,&empty_after,sizeof(empty_before)));
    Rig helper;helper.donor[9]={2,.9f,.25f};helper.donor[15]={2,2.1f,-.3f};helper.rebuild();
    // Independently rotated palms retain their native linear orientation;
    // non-collinear helper pivots still move with their rigid forearms.
    for(unsigned i:{10u,16u}){
        const float angle=i==10?.6f:-.8f,c=std::cos(angle),s=std::sin(angle);
        helper.palette[i]={{{c,-s,0,0},{s,c,0,0},{0,0,1,0}}};
        const auto rotated=point(helper.palette[i],helper.source[i]);
        helper.palette[i].m[0][3]=helper.source[i].x-rotated.x;helper.palette[i].m[1][3]=helper.source[i].y-rotated.y;
    }
    contact_and_lengths(helper,helper.natural(),helper.fit());
}
void native_unit_scales(){
    // Rounded Ashley/Ramon neutral segment ranges, with an explicitly
    // synthetic two-handed posed contact. These are not live-aim captures.
    for(const auto dimensions:{ViewVec3{241,235,265},ViewVec3{172,169,307}}){
        Rig rig({0,700,0});const float upper=dimensions.x,forearm=dimensions.y,separation=dimensions.z;
        rig.donor[8]={.8f*upper,.6f*upper,0};rig.donor[10]={.8f*(upper+forearm),.6f*(upper-forearm),0};
        rig.donor[9]=add(rig.donor[8],{.8f*forearm*.55f,-.6f*forearm*.55f,0});
        rig.donor[13]={0,separation,0};rig.donor[14]={.8f*upper,separation-.6f*upper,0};
        rig.donor[16]={.8f*(upper+forearm),separation-.6f*(upper-forearm),0};
        rig.donor[15]=add(rig.donor[14],{.8f*forearm*.55f,.6f*forearm*.55f,0});rig.rebuild();
        const auto before=rig.natural(),fitted=rig.fit();
        assert(near(sub(rig.joint(fitted,16),rig.joint(fitted,10)),{0,700,0},.001));
        assert(!std::memcmp(&fitted.joint_shift[10],&fitted.joint_shift[16],sizeof(ViewVec3)));
        assert(distance(rig.joint(fitted,10),rig.joint(before,10))>1);
        for(unsigned first:{7u,13u})for(unsigned i=first+1;i<=first+3;++i)
            assert(std::fabs(distance(rig.joint(before,i-1),rig.joint(before,i))-distance(rig.joint(fitted,i-1),rig.joint(fitted,i)))<.001);
    }
}
void fallback_and_invalid(){
    Rig rig;const auto natural=rig.natural(),disabled=rig.fit(false);
    assert(!std::memcmp(&natural,&disabled,sizeof(natural)));
    Rig far({100,100,0});const auto a=far.natural(),b=far.fit();assert(!std::memcmp(&a,&b,sizeof(a)));
    auto out=natural;const auto saved=out;
    assert(!character_grip_pose(rig.map,nullptr,26,true,out));assert(!std::memcmp(&saved,&out,sizeof(out)));
    assert(!character_grip_pose(rig.map,rig.palette.data(),25,true,out));assert(!std::memcmp(&saved,&out,sizeof(out)));
    assert(!character_grip_pose(rig.map,rig.palette.data(),256,true,out));assert(!std::memcmp(&saved,&out,sizeof(out)));
    rig.palette[4].m[0][0]=std::numeric_limits<float>::quiet_NaN();
    assert(!character_grip_pose(rig.map,rig.palette.data(),26,true,out));assert(!std::memcmp(&saved,&out,sizeof(out)));
    rig.palette[4]=identity;auto corrupt=rig.map;corrupt.bones[9].source=255;
    assert(!character_grip_pose(corrupt,rig.palette.data(),26,true,out));assert(!std::memcmp(&saved,&out,sizeof(out)));
    std::array<ViewMatrix3x4,119> actor_palette{};std::copy(rig.palette.begin(),rig.palette.end(),actor_palette.begin());
    assert(character_grip_pose(rig.map,actor_palette.data(),119,true,out));contact_and_lengths(rig,natural,out);
    // A full-sized backing allocation makes rejected partial alias spans safe
    // to express without ever dereferencing beyond the actual input storage.
    alignas(CharacterRetargetPose) std::array<std::uint8_t,sizeof(CharacterRetargetPose)> aliased{};
    std::memcpy(aliased.data(),rig.palette.data(),sizeof(rig.palette));const auto previous=aliased;
    auto* source=reinterpret_cast<ViewMatrix3x4*>(aliased.data());auto* output=reinterpret_cast<CharacterRetargetPose*>(aliased.data());
    assert(!character_grip_pose(rig.map,source,26,true,*output));assert(aliased==previous);
    assert(!character_grip_pose(rig.map,source,26,false,*output));assert(aliased==previous);
    alignas(CharacterRetargetPose) std::array<std::uint8_t,sizeof(CharacterRetargetMap)> map_alias{};
    std::memcpy(map_alias.data(),&rig.map,sizeof(rig.map));const auto old_map=map_alias;
    assert(!character_grip_pose(*reinterpret_cast<CharacterRetargetMap*>(map_alias.data()),rig.palette.data(),26,true,*reinterpret_cast<CharacterRetargetPose*>(map_alias.data())));
    assert(map_alias==old_map);
}
}
int main(){reachable();covariance();degenerate_boundaries();native_unit_scales();fallback_and_invalid();std::puts("grip: shared contacts, posed limb lengths, nearest fit, covariance and safe fallback PASS");return 0;}
