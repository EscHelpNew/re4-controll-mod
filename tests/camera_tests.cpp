#include "re4/camera.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
bool near(float a,float b){return std::fabs(a-b)<0.00005f;}
int main(){
    using namespace re4;
    CameraOffsets old{0.8f,-1.f},native{0.72f,-0.9f};
    auto out=camera_return(old,native,true,1);
    assert(out.pitch==native.pitch && out.yaw==native.yaw);
    out=camera_return(old,native,true,0);assert(out.pitch==old.pitch && out.yaw==old.yaw);
    out=camera_return(old,native,true,0.5f);
    assert(near(out.pitch,old.pitch*std::sqrt(0.9f)) && near(out.yaw,-std::sqrt(0.9f)));
    out=camera_return(old,native,false,0);assert(out.pitch==native.pitch && out.yaw==native.yaw);
    out=camera_return(old,native,true,std::numeric_limits<float>::quiet_NaN());
    assert(out.pitch==native.pitch && out.yaw==native.yaw);
    CameraOffsets totals[2];
    for(unsigned i=0;i<2;++i){
        FreeCamera orbit;CameraOffsets at{};std::uint64_t tick=1;
        const unsigned fps=i?60:30;
        at=orbit.update(42,tick,at,1,0);
        for(unsigned n=0;n<fps;++n){tick+=19'200'000/fps;at=orbit.update(42,tick,at,1,0);}
        totals[i]=at;assert(near(at.yaw,-2.0943951f));
        const auto held=at;tick+=19'200'000/fps;at=orbit.update(42,tick,at,0,0);
        assert(at.yaw==held.yaw && at.pitch==held.pitch); // Neutral means no return.
        for(unsigned n=0;n<fps*2;++n){tick+=19'200'000/fps;at=orbit.update(42,tick,at,1,1);}
        assert(at.yaw>=-3.141593f && at.yaw<=3.141593f && at.pitch==1);
        tick+=19'200'000;at=orbit.update(42,tick,at,1,-1);
        assert(at.pitch==1); // A pause cannot add a second of movement.
        at={0,0};tick+=320'000;at=orbit.update(42,tick,at,1,1);
        assert(at.pitch==0 && at.yaw==0); // Native aim/reset clearing wins over cached orbit.
        tick+=320'000;at=orbit.update(43,tick,at,1,1);
        assert(at.pitch==0 && at.yaw==0); // Camera switch rebases.
        orbit.reset();tick+=320'000;at=orbit.update(43,tick,at,1,1);
        assert(at.pitch==0 && at.yaw==0);
        tick+=320'000;at=orbit.update(43,tick,at,1,-1);
        assert(at.pitch<0 && at.yaw<0);
    }
    assert(near(totals[0].yaw,totals[1].yaw));
    std::puts("camera math: native return, slowed/held return, 360 orbit, limits, timing and native reset PASS");
}
