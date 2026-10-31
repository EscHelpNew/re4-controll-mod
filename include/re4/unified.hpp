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
 std::uintptr_t actor_=0;unsigned route_=0;std::uint64_t tick_=0;bool ready_=false,re6_=false;
 float x_=0,y_=0;
 static float segment(float& velocity,float target,float dt,float tau){
  const float change=-std::expm1(-dt/tau);
  const float integral=target*dt+(velocity-target)*tau*change;
  velocity+=(target-velocity)*change;
  return integral;
 }
 static float advance(float& velocity,float target,float dt,float rise,float brake){
  target=std::clamp(target,-1.f,1.f);
  if(target*velocity<0){
   const float crossing=brake*std::log1p(-velocity/target);
   if(crossing<dt){
    const float first=segment(velocity,target,crossing,brake);
    velocity=0;
    return first+segment(velocity,target,dt-crossing,rise);
   }
   return segment(velocity,target,dt,brake);
  }
  return segment(velocity,target,dt,std::fabs(target)<std::fabs(velocity)?brake:rise);
 }
public:
 void reset(){ready_=false;x_=y_=0;}
 UnifiedDelta update(std::uintptr_t actor,unsigned route,std::uint64_t tick,bool allowed,float x,float y,bool re6_response=false){
  if(!allowed||!actor||!std::isfinite(x)||!std::isfinite(y)){reset();return {};}
  const bool continuous=ready_&&actor==actor_&&route==route_&&re6_==re6_response&&tick>=tick_&&tick-tick_<=1920000;
  const float dt=continuous?float(tick-tick_)/19200000.f:0;
  if(!continuous)x_=y_=0;
  actor_=actor;route_=route;tick_=tick;re6_=re6_response;ready_=true;
  // RE6 constants are a first fit to the synchronized console journal. Pitch
  // uses a provisional aim rate; overview vertical response was not isolated.
  const bool aim=route==2;
  const float rise=re6_response&&aim?.220f:.050f,brake=re6_response?.047f:.012f;
  constexpr float radians=.0174532925199433f;
  const float yaw_rate=re6_response?(aim?57.3f:171.6f)*radians:2.09439510239f;
  const float pitch_rate=re6_response&&aim?53.f*radians:1.57079632679f;
  return {-advance(x_,x,dt,rise,brake)*yaw_rate,advance(y_,y,dt,rise,brake)*pitch_rate};
 }
};
inline float unified_pitch_weight(float radians){return radians*.636619772368f;}
// Only the stick contribution is bounded. A gyro angle outside the band must
// allow gradual inward movement, without snapping or further outward motion.
inline float unified_stick_pitch_delta(float current,float delta){
 if(!std::isfinite(current)||!std::isfinite(delta))return 0;
 constexpr float limit=.610865238f;
 return std::clamp(delta,std::min(0.f,-limit-current),std::max(0.f,limit-current));
}
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
