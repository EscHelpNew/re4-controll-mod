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

    const ViewMatrix3x4 identity{{{1,0,0,0},{0,1,0,0},{0,0,1,0}}};
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
