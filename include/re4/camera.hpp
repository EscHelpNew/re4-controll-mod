#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace re4 {
struct CameraOffsets {float pitch=0,yaw=0;};
inline float camera_yaw(float radians){return std::remainder(radians,6.28318530718f);}
inline bool finite_camera(CameraOffsets value){return std::isfinite(value.pitch)&&std::isfinite(value.yaw);}
inline CameraOffsets camera_return(CameraOffsets before,CameraOffsets native,bool neutral,float speed){
    if(!neutral || speed==1 || !std::isfinite(speed) || speed<0 || speed>1 || !finite_camera(before))return native;
    if(speed==0)return before;
    const float factor=std::pow(0.9f,speed);
    return {before.pitch*factor,before.yaw*factor};
}
class FreeCamera {
    bool ready_=false;
    std::uintptr_t camera_=0;
    std::uint64_t tick_=0;
    CameraOffsets last_{};
public:
    void reset(){ready_=false;}
    CameraOffsets update(std::uintptr_t camera,std::uint64_t tick,CameraOffsets before,float x,float y){
        if(!finite_camera(before) || !std::isfinite(x) || !std::isfinite(y)){
            reset();return before;
        }
        const bool continuous=ready_ && camera==camera_ && tick>=tick_ && tick-tick_<=1'920'000
            && std::fabs(before.pitch-last_.pitch)<0.0001f
            && std::fabs(camera_yaw(before.yaw-last_.yaw))<0.0001f;
        const float seconds=continuous?float(tick-tick_)/19'200'000.f:0;
        CameraOffsets out{
            std::clamp(before.pitch+std::clamp(y,-1.f,1.f)*1.5f*seconds,-1.f,1.f),
            camera_yaw(before.yaw-std::clamp(x,-1.f,1.f)*2.09439510239f*seconds)};
        ready_=true;camera_=camera;tick_=tick;last_=out;
        return out;
    }
};
}
