#include "re4/view.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

#define CHECK(c) do { if(!(c)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#c);std::exit(1);} } while(0)
bool close(float a,float b){return std::fabs(a-b)<0.001f;}
void same(re4::ViewVec3 a,re4::ViewVec3 b){CHECK(close(a.x,b.x)&&close(a.y,b.y)&&close(a.z,b.z));}
int main(){
    using namespace re4;
    const float nan=std::numeric_limits<float>::quiet_NaN();
    const ViewMatrix3x4 basis{{{0,0,2,100},{0,3,0,-20},{-4,0,0,50}}};
    ViewVec3 world{},local{};
    CHECK(view_transform_point(basis,{10,20,30},world));same(world,{160,40,10});
    CHECK(view_inverse_point(basis,world,local));same(local,{10,20,30});
    // Aliasing and a reflected, nonuniformly scaled basis are supported.
    CHECK(view_inverse_point(basis,world,world));same(world,{10,20,30});
    const ViewMatrix3x4 mirrored{{{-2,0,0,7},{0,3,0,8},{0,0,4,9}}};
    CHECK(view_transform_point(mirrored,local,world));
    CHECK(view_inverse_point(mirrored,world,local));same(local,{10,20,30});
    const ViewMatrix3x4 shear{{{2,0.3f,0,100},{0,3,0.2f,200},{0.1f,0,4,-300}}};
    CHECK(view_transform_point(shear,local,world));
    CHECK(view_inverse_point(shear,world,local));same(local,{10,20,30});
    const ViewVec3 sentinel{91,92,93};local=sentinel;
    auto broken=basis;broken.m[2][0]=0;
    CHECK(!view_inverse_point(broken,{0,0,0},local));same(local,sentinel);
    broken=basis;broken.m[1][1]=nan;
    CHECK(!view_inverse_point(broken,{0,0,0},local));same(local,sentinel);
    CHECK(!view_transform_point(basis,{nan,0,0},local));same(local,sentinel);
    auto huge=basis;huge.m[0][0]=std::numeric_limits<float>::max();
    CHECK(!view_transform_point(huge,{2,0,0},local));same(local,sentinel);
    // Uniformly tiny but invertible bases must not fail an absolute determinant test.
    const ViewMatrix3x4 tiny{{{0.0001f,0,0,0},{0,0.0001f,0,0},{0,0,0.0001f,0}}};
    CHECK(view_inverse_point(tiny,{0.001f,0.002f,0.003f},local));same(local,{10,20,30});

    const ViewQfpsOutput native{{-500,1765,-1190},{-180,1700,-110},{0,1340,1480},0,50};
    ViewQfpsOutput fps=native;
    CHECK(view_transform_point(basis,{0,1700,0},world));
    CHECK(first_person_view(native,basis,world,fps));
    same(fps.eye0,{0,1700,0});same(fps.eye1,fps.eye0);
    same({fps.target.x-fps.eye0.x,fps.target.y-fps.eye0.y,fps.target.z-fps.eye0.z},
         {native.target.x-native.eye0.x,native.target.y-native.eye0.y,native.target.z-native.eye0.z});
    CHECK(fps.roll==native.roll&&fps.fov==native.fov);
    auto alias=native;
    CHECK(first_person_view(alias,basis,world,alias));same(alias.eye0,fps.eye0);same(alias.target,fps.target);
    const auto saved=fps;
    broken=basis;broken.m[2][0]=0;
    CHECK(!first_person_view(native,broken,world,fps));CHECK(std::memcmp(&fps,&saved,sizeof(fps))==0);
    auto zero=native;zero.target=zero.eye0;
    CHECK(!first_person_view(zero,basis,world,fps));CHECK(std::memcmp(&fps,&saved,sizeof(fps))==0);
    zero=native;zero.fov=nan;
    CHECK(!first_person_view(zero,basis,world,fps));CHECK(std::memcmp(&fps,&saved,sizeof(fps))==0);

    // A farther shoulder eye retains the collision pivot and native look delta.
    const ViewQfpsOutput shoulder{{-4,6,-8},{2,3,4},{1,8,12},0.2f,50};
    ViewQfpsOutput framed{};
    CHECK(third_person_view(shoulder,1.35f,framed));
    same(framed.eye0,{-6.1f,7.05f,-12.2f});same(framed.target,{-1.1f,9.05f,7.8f});
    CHECK(std::memcmp(&framed.eye1,&shoulder.eye1,sizeof(ViewVec3))==0);
    CHECK(framed.roll==shoulder.roll&&framed.fov==shoulder.fov);
    CHECK(third_person_view(shoulder,2,framed));
    same(framed.eye0,{-10,9,-20});same(framed.target,{-5,11,0});
    // The displacement remains correct through rotated, nonuniformly scaled bases.
    ViewQfpsOutput transformed=shoulder;
    CHECK(view_transform_point(basis,shoulder.eye0,transformed.eye0));
    CHECK(view_transform_point(basis,shoulder.eye1,transformed.eye1));
    CHECK(view_transform_point(basis,shoulder.target,transformed.target));
    CHECK(third_person_view(transformed,2,transformed));
    same(transformed.eye0,{60,7,90});same(transformed.eye1,{108,-11,42});same(transformed.target,{100,13,70});
    auto shoulder_alias=shoulder;
    CHECK(third_person_view(shoulder_alias,2,shoulder_alias));same(shoulder_alias.eye0,framed.eye0);
    const auto untouched=framed;
    for(float distance:{1.f,0.f,-1.f,0.99f,2.01f,nan,std::numeric_limits<float>::infinity()}){
        CHECK(!third_person_view(shoulder,distance,framed));CHECK(std::memcmp(&framed,&untouched,sizeof(framed))==0);
    }
    for(unsigned invalid=0;invalid<8;++invalid){
        auto bad=shoulder;
        if(invalid==0)bad.eye0.x=nan;
        if(invalid==1)bad.eye1.y=nan;
        if(invalid==2)bad.target.z=nan;
        if(invalid==3)bad.roll=nan;
        if(invalid==4)bad.fov=nan;
        if(invalid==5)bad.eye0=bad.eye1;
        if(invalid==6)bad.target=bad.eye0;
        if(invalid==7){bad.eye0.x=std::numeric_limits<float>::max();bad.eye1.x=-bad.eye0.x;}
        CHECK(!third_person_view(bad,2,framed));CHECK(std::memcmp(&framed,&untouched,sizeof(framed))==0);
    }

    const ViewMatrix3x4 identity{{{1,0,0,0},{0,1,0,0},{0,0,1,0}}};
    ViewQfpsOutput locked;
    CHECK(re6_shoulder_view(native,identity,{100,200,300},0,0,1.15f,locked));
    same(locked.eye0,{-448,1974.75f,-1052});same(locked.eye1,native.eye1);
    CHECK(locked.roll==native.roll&&locked.fov==native.fov);
    for(float heading:{-3.14f,-1.57f,0.f,1.57f,3.14f})for(float pitch:{-1.5707963f,-.6f,0.f,.6f,1.5707963f}){
        CHECK(re6_shoulder_view(native,basis,{100,200,300},heading,pitch,1.15f,locked));
        ViewVec3 e,t;CHECK(view_transform_point(basis,locked.eye0,e)&&view_transform_point(basis,locked.target,t));
        const float sh=std::sin(heading),ch=std::cos(heading),sp=std::sin(pitch),cp=std::cos(pitch);
        const ViewVec3 forward{sh*cp,sp,ch*cp},up{-sh*sp,cp,-ch*sp},right{-ch,0,sh};
        const ViewVec3 relative{100-e.x,1900-e.y,300-e.z};
        const auto dot=[](ViewVec3 a,ViewVec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;};
        // Same torso position in camera coordinates through turns and pitch,
        // including the poles. No time history is needed to keep the anchor.
        CHECK(close(dot(relative,right),-548)&&close(dot(relative,up),-74.75f)&&close(dot(relative,forward),1352));
        const float length=std::hypot(t.x-e.x,t.y-e.y,t.z-e.z);
        CHECK(close((t.x-e.x)/length,forward.x)&&close((t.y-e.y)/length,forward.y)&&close((t.z-e.z)/length,forward.z));
        same(locked.eye1,native.eye1);
    }
    auto alias_locked=native;
    CHECK(re6_shoulder_view(alias_locked,identity,{},0,0,1,alias_locked));same(alias_locked.eye0,native.eye0);
    const auto locked_saved=locked;
    for(unsigned invalid=0;invalid<12;++invalid){
        auto n=native;auto m=identity;ViewVec3 root{};float h=0,p=0,d=1.15f;
        if(invalid==0)n.eye0.x=nan;
        if(invalid==1)n.eye1.y=nan;
        if(invalid==2)n.target.z=nan;
        if(invalid==3)n.fov=nan;
        if(invalid==4)n.target=n.eye0;
        if(invalid==5)m.m[2][2]=0;
        if(invalid==6)root.x=nan;
        if(invalid==7)h=nan;
        if(invalid==8)p=1.58f;
        if(invalid==9)d=.99f;
        if(invalid==10)d=2.01f;
        if(invalid==11)m.m[1][1]=nan;
        CHECK(!re6_shoulder_view(n,m,root,h,p,d,locked));CHECK(std::memcmp(&locked,&locked_saved,sizeof(locked))==0);
    }
    // Native row-major perspective: cot(fov/2)/aspect, cot, depth, -z clip w.
    const ViewMatrix4x4 projection{{{0.75f,0,0,0},{0,1,0,0},{0,0,-0.001001f,-100.1001f},{0,0,-1,0}}};
    ViewHudPoint point{91,92};
    CHECK(project_view_point(identity,projection,{0,0,-1000},point));CHECK(close(point.x,256)&&close(point.y,224));
    CHECK(project_view_point(identity,projection,{100,-100,-1000},point));CHECK(close(point.x,275.2f)&&close(point.y,246.4f));
    const ViewMatrix3x4 view{{{1,0,0,-100},{0,1,0,20},{0,0,1,-50}}};
    CHECK(project_view_point(view,projection,{100,-20,-950},point));CHECK(close(point.x,256)&&close(point.y,224));
    const auto old=point;
    for(auto p:{ViewVec3{0,0,100},ViewVec3{0,0,0},ViewVec3{10000,0,-1000},ViewVec3{0,10000,-1000},ViewVec3{nan,0,-1000}}){
        CHECK(!project_view_point(identity,projection,p,point));CHECK(point.x==old.x&&point.y==old.y);
    }
    auto bad_projection=projection;bad_projection.m[0][0]=0;
    CHECK(!project_view_point(identity,bad_projection,{0,0,-1000},point));
    CHECK(!project_view_point(broken,projection,{0,0,-1000},point));
    bad_projection=projection;bad_projection.m[3][2]=nan;
    CHECK(!project_view_point(identity,bad_projection,{0,0,-1000},point));
    // Partial offscreen markers must be rejected before signed->u16 conversion.
    ViewReticleRect rect{};
    CHECK(view_reticle_rect({256,224},6,2,rect));CHECK(rect.x==253&&rect.y==223&&rect.width==6&&rect.height==2);
    const auto old_rect=rect;
    CHECK(!view_reticle_rect({0,0},6,2,rect));CHECK(std::memcmp(&rect,&old_rect,sizeof(rect))==0);
    CHECK(!view_reticle_rect({nan,224},6,2,rect));
    CHECK(!view_reticle_rect({256,224},0,2,rect));
    std::puts("view math: scaled basis, FPS eye/target, native projection and reticle bounds PASS");
}
