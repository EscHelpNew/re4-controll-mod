#include "re4/calibration.hpp"
#include <cassert>
#include <cmath>
#include <limits>
#include <cstdio>
int main(){
    re4::GyroCalibration c;
    assert(c.remaining()==0 && !c.add(1,1,2));
    c.begin();assert(c.remaining()==200);
    assert(!c.add(1,1,-2) && c.remaining()==199);
    assert(!c.add(1,99,99) && !c.add(0,99,99) && c.remaining()==199);
    assert(!c.add(2,std::numeric_limits<float>::infinity(),1) && c.remaining()==199);
    for(unsigned n=3;n<=200;++n)assert(!c.add(n,1,-2));
    assert(c.remaining()==1 && c.add(201,1,-2));
    assert(c.remaining()==0 && std::fabs(c.bias_x()-1)<0.00001f && std::fabs(c.bias_y()+2)<0.00001f);
    c.begin();c.add(1,4,5);c.cancel();assert(c.remaining()==0);
    c.begin();assert(!c.add(1,1,2) && c.remaining()==199); // Controller sampling numbers may restart.
    std::puts("calibration: 200 fresh finite samples, stale suppression, cancellation and reconnect PASS");
}
