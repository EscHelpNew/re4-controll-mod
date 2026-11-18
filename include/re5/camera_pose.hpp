#pragma once
#include <array>
#include <cmath>
#include <cstdint>
namespace re5 {
struct CameraPose {std::array<float,3> eye{},up{},target{};};
namespace camera_detail {
using Vec=std::array<float,3>;
inline Vec add(Vec a,Vec b){for(unsigned i=0;i<3;++i)a[i]+=b[i];return a;}
inline Vec subtract(Vec a,Vec b){for(unsigned i=0;i<3;++i)a[i]-=b[i];return a;}
inline Vec scale(Vec v,float factor){for(float& x:v)x*=factor;return v;}
inline float dot(Vec a,Vec b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
inline Vec cross(Vec a,Vec b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
inline bool finite(Vec v){for(float x:v)if(!std::isfinite(x)||std::fabs(x)>1000000.f)return false;return true;}
inline Vec yaw(Vec v,float angle){const float c=std::cos(angle),s=std::sin(angle);return {c*v[0]+s*v[2],v[1],-s*v[0]+c*v[2]};}
inline Vec rotate(Vec v,Vec axis,float angle){const float c=std::cos(angle),s=std::sin(angle);return add(add(scale(v,c),scale(cross(axis,v),s)),scale(axis,dot(axis,v)*(1-c)));}
}
// Transform the complete native arm around a root-relative upper-body anchor.
// This retains its screen composition, native arm dimensions and roll, while
// body heading and overview pitch own the view before native wall collision.
inline bool anchored_camera_pose(const CameraPose& native,std::array<float,3> anchor,float forward_x,float forward_z,
 float pitch,float distance,CameraPose& out){
 using namespace camera_detail;
 if(!finite(native.eye)||!finite(native.target)||!finite(native.up)||!finite(anchor)||!std::isfinite(pitch)||std::fabs(pitch)>.610866f
  ||!std::isfinite(distance)||distance<1||distance>2||!std::isfinite(forward_x)||!std::isfinite(forward_z))return false;
 const auto direction=subtract(native.target,native.eye);const float length=std::sqrt(dot(direction,direction)),horizontal=std::hypot(direction[0],direction[2]);
 const float forward_length=std::hypot(forward_x,forward_z),up_cross=std::sqrt(dot(cross(direction,native.up),cross(direction,native.up)));
 if(!std::isfinite(length)||length<.001f||horizontal<.001f||forward_length<.001f||!std::isfinite(up_cross)||up_cross<=length*.000001f)return false;
 forward_x/=forward_length;forward_z/=forward_length;
 const float turn=std::atan2(forward_x,forward_z)-std::atan2(direction[0],direction[2]);
 const float elevation=std::atan2(direction[1],horizontal),tilt=pitch-elevation;
 const Vec right{-forward_z,0,forward_x};
 auto rotate_rig=[&](Vec v){return rotate(yaw(v,turn),right,tilt);};
 const auto arm=rotate_rig(subtract(native.eye,anchor)),look=rotate_rig(direction),up=rotate_rig(native.up);
 CameraPose result;result.eye=add(anchor,scale(arm,distance));result.target=add(result.eye,look);result.up=up;
 if(!finite(result.eye)||!finite(result.target)||!finite(result.up))return false;
 out=result;return true;
}
struct CameraPoseIdentity {std::uintptr_t camera=0,actor=0,command=0;unsigned local_player=0;};
// A one-use native desired-pose baseline. The caller restores before native
// desired/history evaluation, independently of current overlay/input gates.
class CameraPoseRestore {
 bool live_=false;CameraPoseIdentity owner_{};CameraPose native_{};std::uint64_t tick_=0;
 static bool valid(CameraPoseIdentity owner,const CameraPose& pose){
  if(!owner.camera||!owner.actor||!owner.command||owner.local_player>=2)return false;
  for(const auto* values:{&pose.eye,&pose.up,&pose.target})for(float x:*values)if(!std::isfinite(x)||std::fabs(x)>1000000.f)return false;
  return true;
 }
public:
 bool pending(std::uintptr_t camera)const{return live_&&owner_.camera==camera;}
 bool remember(CameraPoseIdentity owner,const CameraPose& pose,std::uint64_t tick){
  // A missed try-lock restoration leaves the only genuine native baseline
  // pending. Skip another overlay instead of capturing its customized output.
  if(live_&&owner.camera==owner_.camera)return false;
  clear();if(!valid(owner,pose)||!tick)return false;
  owner_=owner;native_=pose;tick_=tick;live_=true;return true;
 }
 bool take(CameraPoseIdentity owner,std::uint64_t tick,CameraPose& out){
  const bool usable=live_&&owner.camera==owner_.camera&&owner.actor==owner_.actor&&owner.command==owner_.command
   &&owner.local_player==owner_.local_player&&tick>=tick_&&tick-tick_<=1920000;
  clear();if(!usable)return false;
  out=native_;return true;
 }
 void clear(){live_=false;}
};
}
