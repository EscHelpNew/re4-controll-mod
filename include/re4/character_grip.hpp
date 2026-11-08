#pragma once
#include "character_retarget.hpp"
#include <array>
#include <limits>

namespace re4 {
namespace character_grip_detail {
struct Vec {double x=0,y=0,z=0;};
inline Vec vec(ViewVec3 p){return {p.x,p.y,p.z};}
inline Vec operator+(Vec a,Vec b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline Vec operator-(Vec a,Vec b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline Vec operator*(Vec a,double b){return {a.x*b,a.y*b,a.z*b};}
inline double dot(Vec a,Vec b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Vec cross(Vec a,Vec b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline double norm(Vec a){return std::sqrt(dot(a,a));}
inline bool unit(Vec a,double tolerance,Vec& out){const double n=norm(a);if(!std::isfinite(n)||n<=tolerance)return false;out=a*(1/n);return true;}
using Rotation=std::array<std::array<double,3>,3>;
inline Vec rotate(const Rotation& r,Vec v){return {r[0][0]*v.x+r[0][1]*v.y+r[0][2]*v.z,r[1][0]*v.x+r[1][1]*v.y+r[1][2]*v.z,r[2][0]*v.x+r[2][1]*v.y+r[2][2]*v.z};}
inline Vec column(const ViewMatrix3x4& m,unsigned c){return {m.m[0][c],m.m[1][c],m.m[2][c]};}
inline bool perpendicular(Vec axis,const std::array<Vec,5>& seeds,double tolerance,Vec& out){
    for(auto seed:seeds)if(unit(seed-axis*dot(seed,axis),tolerance,out))return true;
    return false;
}
// A proper rotation changes direction only: no scaling or reflection. At an
// antiparallel direction use the posed arm's plane/basis, never a global pole.
inline bool alignment(Vec from,Vec to,const std::array<Vec,5>& seeds,double tolerance,Rotation& out){
    Vec a,b;if(!unit(from,tolerance,a)||!unit(to,tolerance,b))return false;
    const double cosine=std::clamp(dot(a,b),-1.0,1.0);
    Rotation value{{{1,0,0},{0,1,0},{0,0,1}}};
    if(cosine<-1+1e-10){
        Vec axis;if(!perpendicular(a,seeds,tolerance,axis))return false;
        const double v[]{axis.x,axis.y,axis.z};
        for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)value[r][c]=2*v[r]*v[c]-(r==c?1:0);
    }else {
        const Vec v=cross(a,b);
        const Rotation k{{{0,-v.z,v.y},{v.z,0,-v.x},{-v.y,v.x,0}}};
        for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c){
            double squared=0;for(unsigned j=0;j<3;++j)squared+=k[r][j]*k[j][c];
            value[r][c]+=k[r][c]+squared/(1+cosine);
        }
    }
    out=value;return true;
}
struct Annulus {Vec center;double inner=0,outer=0;};
inline bool contains(const Annulus& a,Vec p,double tolerance){const double d=norm(p-a.center);return std::isfinite(d)&&d>=a.inner-tolerance&&d<=a.outer+tolerance;}
// A closest point has zero, one or two active sphere constraints. Enumerate
// those cases directly; no iterative projection or arm-length relaxation.
inline bool nearest(Vec p,const Annulus& a,const Annulus& b,const std::array<Vec,5>& seeds,Vec& out){
    const double scale=std::max({a.outer,b.outer,norm(a.center-b.center)}),tolerance=scale*1e-9;
    if(!std::isfinite(scale)||scale<=0)return false;
    bool found=false;double best=std::numeric_limits<double>::infinity();Vec answer;
    const auto offer=[&](Vec q){
        const double cost=dot(q-p,q-p);
        if(contains(a,q,tolerance)&&contains(b,q,tolerance)&&std::isfinite(cost)&&cost<best){found=true;best=cost;answer=q;}
    };
    offer(p);
    for(const auto& sphere:{a,b})for(double radius:{sphere.inner,sphere.outer}){
        Vec direction;
        if(radius==0)offer(sphere.center);
        else if(unit(p-sphere.center,tolerance,direction)||unit(seeds[0],tolerance,direction))offer(sphere.center+direction*radius);
    }
    Vec axis;const double separation=norm(b.center-a.center);
    if(unit(b.center-a.center,tolerance,axis))for(double ra:{a.inner,a.outer})for(double rb:{b.inner,b.outer}){
        if(separation>ra+rb+tolerance||separation<std::fabs(ra-rb)-tolerance)continue;
        const double along=(ra*ra-rb*rb+separation*separation)/(2*separation);
        double square=ra*ra-along*along;if(square<-tolerance*scale)continue;
        square=std::max(0.0,square);const Vec center=a.center+axis*along;
        if(square<=tolerance*tolerance){offer(center);continue;}
        Vec radial;
        if(unit((p-center)-axis*dot(p-center,axis),tolerance,radial)||perpendicular(axis,seeds,tolerance,radial))offer(center+radial*std::sqrt(square));
    }
    if(found)out=answer;
    return found;
}
struct Arm {
    Vec shoulder,elbow,helper,wrist;
    Rotation upper{},lower{};
};
inline bool solve(const Vec* old,unsigned first,Vec target,const ViewMatrix3x4& basis,Arm& out){
    const Vec s=old[first],e=old[first+1],w=old[first+3],u=e-s,f=w-e;
    const double upper=norm(u),lower=norm(f),distance=norm(target-s),scale=std::max(upper,lower),tolerance=scale*2e-6;
    if(!std::isfinite(scale)||upper<=scale*1e-10||lower<=scale*1e-10
        ||distance>upper+lower+tolerance||distance<std::fabs(upper-lower)-tolerance)return false;
    Vec elbow;
    if(distance<=scale*1e-10){
        if(std::fabs(upper-lower)>tolerance)return false;
        elbow=e; // Equal-length full fold: keep its already defined upper edge.
    }else {
        const Vec axis=(target-s)*(1/distance);
        const double along=(upper*upper-lower*lower+distance*distance)/(2*distance);
        const double square=upper*upper-along*along;
        if(square<-tolerance*scale)return false;
        const double height=std::sqrt(std::max(0.0,square));Vec pole;
        if(height>scale*1e-8&&!unit(u-axis*dot(u,axis),scale*1e-10,pole))return false;
        elbow=s+axis*along+pole*height;
    }
    const std::array<Vec,5> seeds{cross(u,f),u,column(basis,0),column(basis,1),column(basis,2)};
    Arm value;value.shoulder=s;value.elbow=elbow;value.wrist=target;
    if(!alignment(u,elbow-s,seeds,scale*1e-10,value.upper)
        ||!alignment(f,target-elbow,seeds,scale*1e-10,value.lower))return false;
    value.helper=elbow+rotate(value.lower,old[first+2]-e);
    out=value;return true;
}
inline bool write_joint(const CharacterRetargetMap& map,const ViewVec3* native,unsigned index,Vec point,
        const Rotation& rotation,CharacterRetargetPose& pose){
    auto matrix=pose.core[index];
    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c){
        double n=0;for(unsigned k=0;k<3;++k)n+=rotation[r][k]*pose.core[index].m[k][c];
        matrix.m[r][c]=float(n);if(!std::isfinite(matrix.m[r][c]))return false;
    }
    const auto bind=vec(map.source_bind[index])-vec(map.bones[index].translation);
    const double q[]{point.x,point.y,point.z};
    for(unsigned r=0;r<3;++r){
        matrix.m[r][3]=float(q[r]-matrix.m[r][0]*bind.x-matrix.m[r][1]*bind.y-matrix.m[r][2]*bind.z);
        if(!std::isfinite(matrix.m[r][3]))return false;
    }
    ViewVec3 shift;
    if(!view_store_point(point.x-native[index].x,point.y-native[index].y,point.z-native[index].z,shift))return false;
    pose.core[index]=matrix;pose.joint_shift[index]=shift;return true;
}
}
// Source palettes use the same root-local convention as character_retarget_pose.
// Invalid input/alias rejects without writing. Disabled, unreachable, or singular
// elbow-pole fits return the valid natural pose rather than stretching a limb.
inline bool character_grip_pose(const CharacterRetargetMap& map,const ViewMatrix3x4* source,
        unsigned count,bool two_handed,CharacterRetargetPose& out){
    if(!source||count<26||count>character_bone_limit
        ||character_retarget_detail::overlaps(&out,sizeof(out),source,std::size_t(count)*sizeof(*source))
        ||character_retarget_detail::overlaps(&out,sizeof(out),&map,sizeof(map)))return false;
    CharacterRetargetPose natural;
    if(!character_retarget_pose(map,source,count,natural))return false;
    if(!two_handed){out=natural;return true;}
    using namespace character_grip_detail;
    Vec joint[26]{};ViewVec3 native[26]{};
    for(unsigned i=0;i<26;++i){
        ViewVec3 bind,q;
        if(!view_store_point(double(map.source_bind[i].x)-map.bones[i].translation.x,
                double(map.source_bind[i].y)-map.bones[i].translation.y,double(map.source_bind[i].z)-map.bones[i].translation.z,bind)
            ||!view_transform_point(natural.core[i],bind,q)||!view_transform_point(source[i],map.source_bind[i],native[i]))return false;
        joint[i]=vec(q);
    }
    const Vec contact=vec(native[16])-vec(native[10]);
    const double ur=norm(joint[8]-joint[7]),fr=norm(joint[10]-joint[8]),ul=norm(joint[14]-joint[13]),fl=norm(joint[16]-joint[14]);
    const Annulus right{joint[7],std::fabs(ur-fr),ur+fr},left{joint[13]-contact,std::fabs(ul-fl),ul+fl};
    const std::array<Vec,5> seeds{joint[8]-joint[7],joint[14]-joint[13],column(natural.core[7],0),column(natural.core[7],1),column(natural.core[7],2)};
    Vec target;ViewVec3 common;Arm a,b;
    if(!nearest(joint[10],right,left,seeds,target)
        ||!view_store_point(target.x-native[10].x,target.y-native[10].y,target.z-native[10].z,common)
        ||!solve(joint,7,vec(native[10])+vec(common),natural.core[7],a)
        ||!solve(joint,13,vec(native[16])+vec(common),natural.core[13],b)){out=natural;return true;}
    const Rotation identity{{{1,0,0},{0,1,0},{0,0,1}}};auto value=natural;
    for(unsigned side=0;side<2;++side){
        const auto& arm=side?b:a;const unsigned first=side?13:7;
        if(!write_joint(map,native,first,arm.shoulder,arm.upper,value)
            ||!write_joint(map,native,first+1,arm.elbow,arm.lower,value)
            ||!write_joint(map,native,first+2,arm.helper,arm.lower,value)
            ||!write_joint(map,native,first+3,arm.wrist,identity,value)){out=natural;return true;}
    }
    value.joint_shift[10]=value.joint_shift[16]=common;
    out=value;return true;
}
}
