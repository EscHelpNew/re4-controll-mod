#pragma once
#include "re4/unified.hpp"

namespace re4 {
struct ScopeDelta {float yaw=0,pitch=0,zoom=0;};
inline float scope_slowdown(float zoom){return 1-.9f*std::clamp(zoom,0.f,1.f);}
inline float scope_pitch(float current,float positive_up,float lower,float upper){
    return std::clamp(current-positive_up,lower,upper);
}
inline float scope_zoom(float current,float delta){return std::clamp(current+delta,0.f,1.f);}
// The original scope owns separate pitch and zoom storage. Reuse ordinary aim
// easing, including its measured RE6 response, without writing ordinary pitch.
class ScopeLook {
    UnifiedLook aim_,zoom_;
public:
    void reset(){aim_.reset();zoom_.reset();}
    ScopeDelta update(std::uintptr_t owner,std::uint64_t tick,bool allowed,
            float x,float y,float zoom_input,float position,bool measured){
        if(!allowed||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(zoom_input)
                ||!std::isfinite(position)){reset();return {};}
        const auto aim=aim_.update(owner,2,tick,true,x,y,measured);
        const auto zoom=zoom_.update(owner,2,tick,true,zoom_input,0);
        const float scale=scope_slowdown(position);
        // 1.5 normalized zoom units/s keeps the native digital zoom rate at
        // 30Hz, with the same short acceleration/brake as the standard sticks.
        return {aim.yaw*scale,aim.pitch*scale,-zoom.yaw*(1.5f/2.09439510239f)};
    }
};
}
