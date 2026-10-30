#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace re4 {
// Native matrices use row-major rows; position translation is column 3.
struct ViewVec3 { float x=0,y=0,z=0; };
struct ViewMatrix3x4 { float m[3][4]; };
struct ViewMatrix4x4 { float m[4][4]; };
struct ViewQfpsOutput { ViewVec3 eye0,eye1,target;float roll=0,fov=0; };
struct ViewHudPoint { float x=0,y=0; };
struct ViewReticleRect { std::uint16_t x=0,y=0,width=0,height=0; };
static_assert(sizeof(ViewMatrix3x4)==0x30 && sizeof(ViewMatrix4x4)==0x40);
static_assert(sizeof(ViewQfpsOutput)==0x2C);

inline bool view_finite(ViewVec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
inline bool view_store_point(double x,double y,double z,ViewVec3& out){
    const ViewVec3 value{float(x),float(y),float(z)};
    if(!view_finite(value))return false;
    out=value;return true;
}
inline bool view_transform_point(const ViewMatrix3x4& matrix,ViewVec3 point,ViewVec3& out){
    if(!view_finite(point))return false;
    for(const auto& row:matrix.m)for(float v:row)if(!std::isfinite(v))return false;
    double value[3];
    for(unsigned i=0;i<3;++i)value[i]=double(matrix.m[i][0])*point.x
        +double(matrix.m[i][1])*point.y+double(matrix.m[i][2])*point.z+matrix.m[i][3];
    return view_store_point(value[0],value[1],value[2],out);
}
inline bool view_inverse_point(const ViewMatrix3x4& matrix,ViewVec3 point,ViewVec3& out){
    if(!view_finite(point))return false;
    double scale=0;
    for(unsigned i=0;i<3;++i)for(unsigned j=0;j<4;++j){
        if(!std::isfinite(matrix.m[i][j]))return false;
        if(j<3)scale=std::max(scale,std::fabs(double(matrix.m[i][j])));
    }
    if(scale==0)return false;
    double a[3][3];
    for(unsigned i=0;i<3;++i)for(unsigned j=0;j<3;++j)a[i][j]=matrix.m[i][j]/scale;
    const double c00=a[1][1]*a[2][2]-a[1][2]*a[2][1];
    const double c01=a[1][2]*a[2][0]-a[1][0]*a[2][2];
    const double c02=a[1][0]*a[2][1]-a[1][1]*a[2][0];
    const double determinant=a[0][0]*c00+a[0][1]*c01+a[0][2]*c02;
    // Relative to the linear basis scale, so uniformly tiny bases still work.
    if(!std::isfinite(determinant)||std::fabs(determinant)<=1e-12)return false;
    const double x=(double(point.x)-matrix.m[0][3])/scale;
    const double y=(double(point.y)-matrix.m[1][3])/scale;
    const double z=(double(point.z)-matrix.m[2][3])/scale;
    return view_store_point((c00*x+(a[0][2]*a[2][1]-a[0][1]*a[2][2])*y
            +(a[0][1]*a[1][2]-a[0][2]*a[1][1])*z)/determinant,
        (c01*x+(a[0][0]*a[2][2]-a[0][2]*a[2][0])*y
            +(a[0][2]*a[1][0]-a[0][0]*a[1][2])*z)/determinant,
        (c02*x+(a[0][1]*a[2][0]-a[0][0]*a[2][1])*y
            +(a[0][0]*a[1][1]-a[0][1]*a[1][0])*z)/determinant,out);
}
inline bool first_person_view(const ViewQfpsOutput& native,const ViewMatrix3x4& basis,
        ViewVec3 world_eye,ViewQfpsOutput& out){
    if(!view_finite(native.eye0)||!view_finite(native.eye1)||!view_finite(native.target)
        ||!std::isfinite(native.roll)||!std::isfinite(native.fov))return false;
    ViewVec3 eye;
    if(!view_inverse_point(basis,world_eye,eye))return false;
    const double dx=double(native.target.x)-native.eye0.x;
    const double dy=double(native.target.y)-native.eye0.y;
    const double dz=double(native.target.z)-native.eye0.z;
    const double distance=std::hypot(dx,dy,dz);
    if(!std::isfinite(distance)||distance<=1e-6)return false;
    auto value=native;
    value.eye0=value.eye1=eye;
    // Moving the origin while retaining this delta preserves both native view
    // direction and target distance, including a nonuniformly scaled basis.
    if(!view_store_point(eye.x+dx,eye.y+dy,eye.z+dz,value.target))return false;
    out=value;return true;
}
inline bool view_projection_valid(const ViewMatrix4x4& matrix){
    double a[4][4];
    for(unsigned i=0;i<4;++i){
        double scale=0;
        for(float v:matrix.m[i]){
            if(!std::isfinite(v))return false;
            scale=std::max(scale,std::fabs(double(v)));
        }
        if(scale==0)return false;
        for(unsigned j=0;j<4;++j)a[i][j]=matrix.m[i][j]/scale;
    }
    for(unsigned i=0;i<4;++i){
        unsigned pivot=i;
        for(unsigned j=i+1;j<4;++j)if(std::fabs(a[j][i])>std::fabs(a[pivot][i]))pivot=j;
        if(std::fabs(a[pivot][i])<=1e-12)return false;
        if(pivot!=i)for(unsigned j=0;j<4;++j)std::swap(a[i][j],a[pivot][j]);
        for(unsigned k=i+1;k<4;++k){
            const double factor=a[k][i]/a[i][i];
            for(unsigned j=i+1;j<4;++j)a[k][j]-=factor*a[i][j];
        }
    }
    return true;
}
// View is workspace+D8; projection is workspace+10C. Native perspective has
// clip.w=-view.z. The late native screen pass uses 512 by 448 logical units.
inline bool project_view_point(const ViewMatrix3x4& view,const ViewMatrix4x4& projection,
        ViewVec3 world,ViewHudPoint& out){
    ViewVec3 camera,inverse_check;
    if(!view_projection_valid(projection)||!view_inverse_point(view,{},inverse_check)
        ||!view_transform_point(view,world,camera))return false;
    const double v[4]={camera.x,camera.y,camera.z,1};
    double clip[4]{};
    for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j)clip[i]+=double(projection.m[i][j])*v[j];
    for(double value:clip)if(!std::isfinite(value))return false;
    if(camera.z>=0||clip[3]<=1e-6)return false;
    const double x=clip[0]/clip[3],y=clip[1]/clip[3];
    if(x < -1 || x > 1 || y < -1 || y > 1)return false;
    out={float((x+1)*256),float((1-y)*224)};return true;
}
inline bool view_reticle_rect(ViewHudPoint center,unsigned width,unsigned height,ViewReticleRect& out){
    if(!std::isfinite(center.x)||!std::isfinite(center.y)||!width||!height||width>512||height>448)return false;
    const double x=std::round(double(center.x)-width*0.5),y=std::round(double(center.y)-height*0.5);
    if(x<0||y<0||x+width>512||y+height>448)return false;
    out={std::uint16_t(x),std::uint16_t(y),std::uint16_t(width),std::uint16_t(height)};return true;
}
}
