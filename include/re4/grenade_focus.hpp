#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace re4 {
// A camera lens transition, independent of prediction, collision or look input.
// Native ticks are19.2MHz. Critical damping preserves a continuous velocity
// when aim is released or resumed; roughly98% of a change settles in200ms.
class GrenadeAimZoom {
    std::uintptr_t actor_=0;
    std::uint64_t tick_=0;
    double amount_=0,velocity_=0;
public:
    void reset(){actor_=0;tick_=0;amount_=velocity_=0;}
    float update(std::uintptr_t actor,std::uint64_t tick,bool held,float native_fov){
        if(!actor||!std::isfinite(native_fov)||native_fov<=0||native_fov>=180){reset();return native_fov;}
        if(actor_!=actor||tick<tick_||tick-tick_>19'200'000){
            reset();actor_=actor;tick_=tick;
        }else{
            const double seconds=double(tick-tick_)/19'200'000.;tick_=tick;
            const double target=held?1.:0.,error=amount_-target;
            const double decay=std::exp(-30*seconds),coefficient=velocity_+30*error;
            amount_=std::clamp(target+(error+coefficient*seconds)*decay,0.,1.);
            velocity_=(velocity_-30*coefficient*seconds)*decay;
        }
        // Preserve an already wider native lens instead of narrowing it.
        return float(native_fov+(std::max(native_fov,68.f)-native_fov)*amount_);
    }
};
}
