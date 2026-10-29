#pragma once
#include <cstdint>
#include <cmath>
namespace re4 {
class GyroCalibration {
    unsigned count_=0;
    bool active_=false,seen_=false;
    std::uint64_t last_=0;
    double sum_x_=0,sum_y_=0;
public:
    void begin(){count_=0;active_=true;seen_=false;sum_x_=sum_y_=0;}
    void cancel(){active_=false;count_=0;seen_=false;}
    bool add(std::uint64_t number,float x,float y){
        if(!active_ || (seen_ && number<=last_))return false;
        seen_=true;last_=number;
        if(!std::isfinite(x)||!std::isfinite(y))return false;
        sum_x_+=x;sum_y_+=y;
        if(++count_==200){active_=false;return true;}
        return false;
    }
    unsigned remaining()const{return active_?200-count_:0;}
    float bias_x()const{return count_==200?float(sum_x_/200):0;}
    float bias_y()const{return count_==200?float(sum_y_/200):0;}
};
}
