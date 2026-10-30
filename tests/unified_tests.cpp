#include "re4/unified.hpp"
#include <cassert>
#include <cmath>
#include <limits>
#include <cstdio>
using namespace re4;
bool near(float a,float b){return std::fabs(a-b)<1e-4f;}
int main(){
 UnifiedLook look;auto d=look.update(1,1,1,true,1,1);assert(d.yaw==0&&d.pitch==0);
 d=look.update(1,1,960001,true,1,1);assert(near(d.yaw,-2.09439510239f*.05f));assert(near(d.pitch,1.57079632679f*.05f));
 assert(near(unified_pitch_weight(d.pitch),.05f));
 d=look.update(1,1,960001,true,1,1);assert(d.yaw==0&&d.pitch==0);
 d=look.update(1,2,1920001,true,1,1);assert(d.yaw==0&&d.pitch==0);
 d=look.update(1,2,100000000,true,1,1);assert(d.yaw==0&&d.pitch==0);
 look.update(1,2,100100000,false,1,1);d=look.update(1,2,100200000,true,1,1);assert(d.yaw==0);
 ViewMatrix3x4 basis{{{2,.1f,0,44},{0,3,.2f,55},{.3f,0,4,66}}};ViewVec3 in{2,3,4},out,world0,world1;
 assert(unified_redirect_root(basis,in,1.2f,out));auto linear=basis;for(auto& row:linear.m)row[3]=0;
 assert(view_transform_point(linear,in,world0)&&view_transform_point(linear,out,world1));
 assert(near(world0.y,world1.y));assert(near(std::hypot(world0.x,world0.z),std::hypot(world1.x,world1.z)));
 ViewVec3 secondary{-1,4,2},a,b,blend{.25f*in.x+.75f*secondary.x,.25f*in.y+.75f*secondary.y,.25f*in.z+.75f*secondary.z},r;
 assert(unified_redirect_root(basis,secondary,1.2f,b)&&unified_redirect_root(basis,in,1.2f,a)&&unified_redirect_root(basis,blend,1.2f,r));assert(near(r.x,.25f*a.x+.75f*b.x)&&near(r.y,.25f*a.y+.75f*b.y)&&near(r.z,.25f*a.z+.75f*b.z));
 auto bad=basis;bad.m[0][0]=std::numeric_limits<float>::quiet_NaN();out={1,2,3};assert(!unified_redirect_root(bad,in,1,out)&&out.x==1&&out.y==2&&out.z==3);
 assert(unified_direction_flags(15,1,0)==1&&unified_direction_flags(15,0,-1)==2&&unified_direction_flags(15,0,0)==0);
 ViewVec3 target{1,2,3};float distance=4;assert(unified_target({1,2,0},{1,3,10},{0,1,0},target,distance));assert(target.z==10&&near(distance,std::sqrt(101.f)));
 assert(!unified_target({0,0,0},{0,10,0},{0,1,0},target,distance));
 std::puts("unified tests passed");
}
