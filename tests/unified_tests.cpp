#include "re4/unified.hpp"
#include <cassert>
#include <cmath>
#include <limits>
#include <cstdio>
using namespace re4;
bool near(float a,float b){return std::fabs(a-b)<1e-4f;}
int main(){
 for(unsigned hz:{30u,60u,120u}){
  UnifiedLook measured;measured.update(9,1,1,true,1,1,true);
  UnifiedDelta step;
  for(unsigned n=1;n<=hz*2;++n)step=measured.update(9,1,1+19200000ull*n/hz,true,1,1,true);
  constexpr float to_degrees=57.2957795131f;
  assert(std::fabs(-step.yaw*hz*to_degrees-171.6f)<.03f);
  assert(std::fabs(step.pitch*hz*to_degrees-90.f)<.03f);
  assert(measured.update(9,2,38400001,true,1,1,true).yaw==0); // Raising/route change drains overview speed.
  for(unsigned n=1;n<=hz*2;++n)step=measured.update(9,2,38400001+19200000ull*n/hz,true,1,1,true);
  assert(std::fabs(-step.yaw*hz*to_degrees-57.3f)<.03f);
  assert(std::fabs(step.pitch*hz*to_degrees-53.f)<.03f);
  float coast=0;
  for(unsigned n=1;n<=hz/5;++n)coast-=measured.update(9,2,76800001+19200000ull*n/hz,true,0,0,true).yaw;
  const float expected=(57.3f/to_degrees)*.047f*(1-std::exp(-.2f/.047f));
  assert(std::fabs(coast-expected)<.00002f);
  assert(measured.update(9,2,80640001,true,1,1,false).yaw==0); // Live preset change never replays old velocity.
 }
 UnifiedLook look;auto d=look.update(1,1,1,true,1,1);assert(d.yaw==0&&d.pitch==0);
 // A50ms step must ease into motion instead of instantly taking full speed.
 d=look.update(1,1,960001,true,1,1);assert(near(d.yaw,-.038524245f));assert(near(d.pitch,.028893184f));
 assert(near(unified_pitch_weight(d.pitch),.018393972f));
 d=look.update(1,1,960001,true,1,1);assert(d.yaw==0&&d.pitch==0);
 d=look.update(1,2,1920001,true,1,1);assert(d.yaw==0&&d.pitch==0);
 d=look.update(1,2,100000000,true,1,1);assert(d.yaw==0&&d.pitch==0);
 look.update(1,2,100100000,false,1,1);d=look.update(1,2,100200000,true,1,1);assert(d.yaw==0);
 // The analytic velocity integral gives the same half-second turn at each rate.
 for(unsigned hz:{30u,60u,120u}){
  UnifiedLook motion;motion.update(7,1,1,true,1,1);float yaw=0,pitch=0;
  for(unsigned n=1;n<=hz/2;++n){auto step=motion.update(7,1,1+19200000ull*n/hz,true,1,1);yaw+=step.yaw;pitch+=step.pitch;}
  assert(near(yaw,-.94248255f)&&near(pitch,.70686191f));
  float coast=0;
  for(unsigned n=1;n<=hz/2;++n){auto step=motion.update(7,1,9600001+19200000ull*n/hz,true,0,0);assert(step.yaw<=0);coast-=step.yaw;d=step;}
  assert(coast<.02514f&&std::fabs(d.yaw)<1e-6f); // Stop tail is below1.45degrees at full speed.
 }
 for(unsigned hz:{30u,60u,120u}){
  UnifiedLook motion;motion.update(1,1,1,true,1,0);
  for(unsigned n=1;n<=hz;++n)motion.update(1,1,1+19200000ull*n/hz,true,1,0);
  float reversed=0;
  for(unsigned n=1;n<=hz/10;++n)reversed+=motion.update(1,1,19200001+19200000ull*n/hz,true,-1,0).yaw;
  assert(std::fabs(reversed-.09632437f)<.000002f); // Same100ms flick after the brake-to-acceleration crossing.
 }
 {
  UnifiedLook motion;motion.update(1,1,1,true,1,0);
  for(unsigned n=1;n<=10;++n)motion.update(1,1,1+960000ull*n,true,1,0);
  d=motion.update(1,1,10560001,true,-1,0);assert(d.yaw>0); // A reversal must brake promptly.
  motion.reset();assert(motion.update(1,1,11520001,true,1,0).yaw==0);
  d=motion.update(1,1,12480001,true,1,0);assert(near(d.yaw,-.038524245f));
 }
 // Stop stick-driven pitch at50degrees without snapping a gyro-created angle.
 assert(near(unified_stick_pitch_delta(.85f,.1f),.022664626f));
 assert(near(unified_stick_pitch_delta(-.85f,-.1f),-.022664626f));
 assert(unified_stick_pitch_delta(1.2f,.1f)==0);
 assert(near(unified_stick_pitch_delta(1.2f,-.02f),-.02f));
 assert(unified_stick_pitch_delta(-1.2f,-.1f)==0);
 assert(near(unified_stick_pitch_delta(-1.2f,.02f),.02f));
 assert(unified_stick_pitch_delta(1.2f,0)==0);
 assert(unified_stick_pitch_delta(std::numeric_limits<float>::quiet_NaN(),.1f)==0);
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
