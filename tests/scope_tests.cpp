#include "re4/scope.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>

bool close(float a,float b){return std::fabs(a-b)<1e-5f;}
int main(){
    // Scoped sticks use the ordinary aim acceleration, not an immediate native
    // digital step. Their integrals stay independent of presentation frequency.
    float previous_yaw=0,previous_zoom=0;
    for(unsigned hz:{30u,60u,120u}){
        re4::ScopeLook scope;float yaw=0,zoom=0;
        auto d=scope.update(1,1,true,1,1,1,0,false);
        assert(d.yaw==0&&d.pitch==0&&d.zoom==0);
        for(unsigned n=1;n<=hz/2;++n){
            d=scope.update(1,1+19200000ull*n/hz,true,1,1,1,0,false);
            yaw+=d.yaw;zoom+=d.zoom;
        }
        assert(close(yaw,-.94248255f));
        assert(close(zoom,.6750034f));
        if(previous_yaw){assert(close(yaw,previous_yaw));assert(close(zoom,previous_zoom));}
        previous_yaw=yaw;previous_zoom=zoom;
        d=scope.update(1,9600001,true,1,1,1,0,false);
        assert(d.yaw==0&&d.pitch==0&&d.zoom==0); // No duplicate frame integration.
        scope.reset();d=scope.update(1,10240001,true,1,1,1,0,false);assert(d.yaw==0&&d.zoom==0);
    }
    re4::ScopeLook normal,magnified;
    normal.update(3,1,true,1,1,0,0,true);magnified.update(3,1,true,1,1,0,1,true);
    const auto wide=normal.update(3,640001,true,1,1,0,0,true);
    const auto narrow=magnified.update(3,640001,true,1,1,0,1,true);
    assert(close(narrow.yaw,wide.yaw*.1f)&&close(narrow.pitch,wide.pitch*.1f));
    assert(close(re4::scope_pitch(0,.2f,-1.2f,1.2f),-.2f)); // Positive ordinary pitch looks up.
    assert(close(re4::scope_pitch(-1.19f,.1f,-1.2f,1.2f),-1.2f));
    assert(close(re4::scope_pitch(1.19f,-.1f,-1.2f,1.2f),1.2f));
    assert(re4::scope_zoom(.99f,.2f)==1&&re4::scope_zoom(.01f,-.2f)==0);
    auto d=normal.update(3,1280001,false,1,1,1,0,true);assert(d.yaw==0&&d.zoom==0);
    d=normal.update(3,1920001,true,1,1,1,0,true);assert(d.yaw==0&&d.zoom==0);
    d=normal.update(4,2560001,true,1,1,1,0,true);assert(d.yaw==0&&d.zoom==0);
    d=normal.update(4,5120002,true,1,1,1,0,true);assert(d.yaw==0&&d.zoom==0); // Long gap drains.
    d=normal.update(4,5760002,true,std::numeric_limits<float>::quiet_NaN(),1,1,0,true);assert(d.yaw==0&&d.zoom==0);
    assert(re4::scope_slowdown(0)==1&&close(re4::scope_slowdown(1),.1f));
    std::puts("scope math: eased sticks/zoom, frame cadence, bounds, inversion basis and drains PASS");
}
