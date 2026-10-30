#pragma once
#include "re4/view.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace re4 {
struct UnifiedDelta {float yaw=0,pitch=0;};
// Monotonic time is consumed once by the player update. State transitions and
// gaps over100ms drain their first sample; no pause/raise motion is caught up.
class UnifiedLook {
 std::uintptr_t actor_=0;unsigned route_=0;std::uint64_t tick_=0;bool ready_=false;
public:
 void reset(){ready_=false;}
 UnifiedDelta update(std::uintptr_t actor,unsigned route,std::uint64_t tick,bool allowed,float x,float y){
  if(!allowed||!actor||!std::isfinite(x)||!std::isfinite(y)){reset();return {};}
  const bool continuous=ready_&&actor==actor_&&route==route_&&tick>=tick_&&tick-tick_<=1920000;
  const float dt=continuous?float(tick-tick_)/19200000.f:0;
  actor_=actor;route_=route;tick_=tick;ready_=true;
  return {-std::clamp(x,-1.f,1.f)*2.09439510239f*dt,std::clamp(y,-1.f,1.f)*1.57079632679f*dt};
 }
};
inline float unified_pitch_weight(float radians){return radians*.636619772368f;}
inline std::uint64_t unified_direction_flags(std::uint64_t flags,float x,float y){
 return (flags&~std::uint64_t(15))|((std::isfinite(x)&&std::isfinite(y)&&std::hypot(x,y)>0)?(y<0?2u:1u):0u);
}
// Rotate native world displacement, preserving its worldY and planar budget,
// then return it in the exact native basis. The same linear operator must be
// used for primary and secondary roots before their native blend.
inline bool unified_redirect_root(ViewMatrix3x4 basis,ViewVec3 in,float yaw,ViewVec3& out){
 if(!std::isfinite(yaw))return false;
 for(auto& row:basis.m)row[3]=0;
 ViewVec3 world,result;if(!view_transform_point(basis,in,world))return false;
 const double c=std::cos(double(yaw)),s=std::sin(double(yaw));
 if(!view_store_point(c*world.x+s*world.z,world.y,-s*world.x+c*world.z,result))return false;
 return view_inverse_point(basis,result,out);
}
inline bool unified_target(ViewVec3 eye,ViewVec3 endpoint,ViewVec3 up,ViewVec3& target,float& distance){
 if(!view_finite(eye)||!view_finite(endpoint)||!view_finite(up))return false;
 const double x=double(endpoint.x)-eye.x,y=double(endpoint.y)-eye.y,z=double(endpoint.z)-eye.z;
 const double length=std::hypot(x,y,z),cross=std::hypot(up.y*z-up.z*y,up.z*x-up.x*z,up.x*y-up.y*x);
 if(!std::isfinite(length)||length<=1e-4||!std::isfinite(cross)||cross<=length*1e-6)return false;
 const float value=float(length);if(!std::isfinite(value))return false;
 target=endpoint;distance=value;return true;
}
}
