#include "re5/camera_pose.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#define CHECK(c) do{if(!(c)){std::fprintf(stderr,"camera pose line %d: %s\n",__LINE__,#c);std::exit(1);}}while(false)
bool equal(const re5::CameraPose& a,const re5::CameraPose& b){return std::memcmp(&a,&b,sizeof(a))==0;}
namespace {
using Vec=std::array<float,3>;
Vec projection(const re5::CameraPose& pose,Vec point){
 using namespace re5::camera_detail;
 auto forward=subtract(pose.target,pose.eye);forward=scale(forward,1/std::sqrt(dot(forward,forward)));
 auto right=cross(forward,pose.up);right=scale(right,1/std::sqrt(dot(right,right)));const auto up=cross(right,forward);
 const auto delta=subtract(point,pose.eye);const float depth=dot(delta,forward);
 return {dot(delta,right)/depth,dot(delta,up)/depth,depth};
}
void native_arm_keeps_anchor_composition(){
 const re5::CameraPose native{{-0.5f,1.8f,1.2f},{0,1,0},{-.5f,1.8f,-8.8f}};const Vec anchor{0,1.7f,0};
 const auto original=projection(native,anchor);
 for(float heading:{-3.f,-1.f,0.f,1.f,3.f})for(float pitch:{-.6108652f,0.f,.6108652f})for(float distance:{1.f,1.5f,2.f}){
  re5::CameraPose out;CHECK(re5::anchored_camera_pose(native,anchor,std::sin(heading),std::cos(heading),pitch,distance,out));
  const auto screen=projection(out,anchor);CHECK(std::fabs(screen[0]-original[0])<.00001f&&std::fabs(screen[1]-original[1])<.00001f);
  using namespace re5::camera_detail;const auto look=subtract(out.target,out.eye);const float horizontal=std::hypot(look[0],look[2]);
  CHECK(std::fabs(std::atan2(look[1],horizontal)-pitch)<.00001f);
  CHECK(std::fabs(std::remainder(std::atan2(look[0],look[2])-heading,6.28318530718f))<.00001f);
 }
 re5::CameraPose out=native;const auto before=out;
 CHECK(!re5::anchored_camera_pose(native,anchor,0,0,0,1,out)&&equal(out,before));
 CHECK(!re5::anchored_camera_pose(native,anchor,0,1,.7f,1,out)&&equal(out,before));
 auto invalid=native;invalid.up={0,0,-1};CHECK(!re5::anchored_camera_pose(invalid,anchor,0,1,0,1,out)&&equal(out,before));
 invalid=native;invalid.eye[0]=std::numeric_limits<float>::quiet_NaN();CHECK(!re5::anchored_camera_pose(invalid,anchor,0,1,0,1,out)&&equal(out,before));
}
}
int main(){
 native_arm_keeps_anchor_composition();
 const re5::CameraPoseIdentity identity{0x1000,0x2000,0x3000,0};
 const re5::CameraPose original{{0,2,10},{0,1,0},{0,2,0}};
 re5::CameraPoseRestore state;auto camera=original;std::uint64_t tick=1;
 CHECK(!state.pending(identity.camera));
 CHECK(state.remember(identity,original,tick));auto customized=original;customized.eye[0]=25;
 CHECK(!state.remember(identity,customized,tick+1));CHECK(state.take(identity,tick+1,camera)&&equal(camera,original));
 // Native desired can skip updating persistent fields. Each overlay must
 // start from the original pose, including up, before native history reads it.
 for(unsigned frame=0;frame<120;++frame){
  if(frame){CHECK(state.pending(identity.camera));CHECK(state.take(identity,tick,camera));CHECK(equal(camera,original));}
  const auto transition_history=camera;CHECK(equal(transition_history,original));
  CHECK(state.remember(identity,camera,tick));
  for(unsigned i=0;i<3;++i)camera.eye[i]=camera.target[i]+(camera.eye[i]-camera.target[i])*1.75f;
  camera.up={.2f,.9f,.1f}; // Simulated post-overlay native collision/view basis.
  CHECK(camera.eye[2]==17.5f);tick+=640000;
 }
 // Cleanup happens before any current control/settings/scoped/fresh-frame
 // guard; declining to apply another overlay must leave the native pose.
 CHECK(state.take(identity,tick,camera));CHECK(equal(camera,original)&&!state.pending(identity.camera));
 auto unchanged=camera;CHECK(!state.take(identity,tick,camera)&&equal(camera,unchanged));
 for(unsigned mismatch=0;mismatch<4;++mismatch){
  CHECK(state.remember(identity,original,tick));auto other=identity;
  if(mismatch==0)++other.camera;
  if(mismatch==1)++other.actor;
  if(mismatch==2)++other.command;
  if(mismatch==3)++other.local_player;
  camera.up={3,4,5};unchanged=camera;CHECK(!state.take(other,tick+1,camera)&&equal(camera,unchanged));
  CHECK(!state.pending(identity.camera));
 }
 CHECK(state.remember(identity,original,tick));camera.up={3,4,5};unchanged=camera;
 CHECK(!state.take(identity,tick+1920001,camera)&&equal(camera,unchanged));
 CHECK(state.remember(identity,original,tick));CHECK(!state.take(identity,tick-1,camera));
 auto invalid=original;invalid.eye[0]=std::numeric_limits<float>::infinity();CHECK(!state.remember(identity,invalid,tick));
 CHECK(!state.pending(identity.camera));
 auto moving=original;moving.eye[0]=5;CHECK(state.remember(identity,moving,tick));CHECK(state.take(identity,tick+640000,camera)&&equal(camera,moving));
 CHECK(state.remember(identity,original,tick));state.clear();CHECK(!state.pending(identity.camera));
 std::puts("Camera pose: repeated skipped desired frames, clean native history/up, cleanup, identity and lifetime guards PASS");
}
