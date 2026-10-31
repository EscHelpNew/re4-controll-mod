#pragma once
#include "re4/input.hpp"
#include <cmath>
#include <cstdint>
#include <limits>

namespace re4 {
// Present the cumulative filtered sensor displacement on the game frame clock.
// The first nonempty batch anchors its newest endpoint to the current frame;
// that anchor is retained, so 6/7-sample batches cannot modulate a steady turn.
// Interpolation adds a fixed 10ms delay; missing data is clamped, never predicted.
// Call begin_frame before pushes and finish_frame once afterward. The caller
// must reset on actor/controller/aim-session/gain changes, including gain zero.
// Inactive, interrupted or discontinuous frames reject pushes and emit zero;
// the next active frame can anchor a fresh batch.
class GyroFrame {
    struct Point {std::uint64_t ns=0;double yaw=0,pitch=0;};
    static constexpr unsigned capacity=64;
    static constexpr std::uint64_t max_interval_ns=50000000;
    static constexpr std::uint64_t max_gap_ticks=1920000;
    Point points_[capacity]{};
    unsigned first_=0,count_=0;
    std::uint64_t sensor_ns_=0,batch_ns_=0,anchor_tick_=0,anchor_ns_=0,last_tick_=0,frame_tick_=0;
    double emitted_yaw_=0,emitted_pitch_=0;
    bool seen_frame_=false,anchored_=false,open_=false;
    const Point& point(unsigned offset)const{return points_[(first_+offset)%capacity];}
    void discard_frame(){reset();}
public:
    void reset(){
        first_=count_=0;sensor_ns_=batch_ns_=anchor_tick_=anchor_ns_=last_tick_=frame_tick_=0;
        emitted_yaw_=emitted_pitch_=0;seen_frame_=anchored_=open_=false;
    }
    void begin_frame(std::uint64_t tick,bool active){
        if(!active){reset();return;}
        if(open_||(seen_frame_&&(tick<last_tick_||tick-last_tick_>max_gap_ticks))){reset();return;}
        frame_tick_=tick;last_tick_=tick;batch_ns_=0;seen_frame_=open_=true;
    }
    bool push(AimDelta delta,std::uint64_t interval_ns){
        if(!open_)return false;
        if(!interval_ns||interval_ns>max_interval_ns||!std::isfinite(delta.yaw)||!std::isfinite(delta.pitch)
            ||sensor_ns_>std::numeric_limits<std::uint64_t>::max()-interval_ns
            ||batch_ns_>100000000-interval_ns){discard_frame();return false;}
        if(!count_){points_[0]={};first_=0;count_=1;}
        if(count_==capacity){discard_frame();return false;}
        const auto& previous=point(count_-1);
        const Point next{sensor_ns_+interval_ns,previous.yaw+delta.yaw,previous.pitch+delta.pitch};
        sensor_ns_=next.ns;batch_ns_+=interval_ns;points_[(first_+count_)%capacity]=next;++count_;
        return true;
    }
    AimDelta finish_frame(){
        if(!open_)return {};
        open_=false;
        if(count_<2)return {};
        if(!anchored_){anchor_tick_=frame_tick_;anchor_ns_=sensor_ns_;anchored_=true;}
        const double wall_ns=double(frame_tick_-anchor_tick_)*(1000000000.0/19200000.0)+double(anchor_ns_);
        // A clock/sample discontinuity cannot accumulate an unbounded queue.
        // Normal asynchronous phase is under one sensor interval; 100ms also
        // matches the caller's pause boundary and drains malformed timelines.
        if(std::fabs(double(sensor_ns_)-wall_ns)>100000000.0){reset();return {};}
        double target=wall_ns-10000000.0;
        if(target<0)target=0;
        if(target>double(sensor_ns_))target=double(sensor_ns_);
        while(count_>1&&double(point(1).ns)<=target){first_=(first_+1)%capacity;--count_;}
        const auto& a=point(0);
        double yaw=a.yaw,pitch=a.pitch;
        if(count_>1&&target>double(a.ns)){
            const auto& b=point(1);
            const double fraction=(target-double(a.ns))/double(b.ns-a.ns);
            yaw+=(b.yaw-a.yaw)*fraction;pitch+=(b.pitch-a.pitch)*fraction;
        }
        const AimDelta result{float(yaw-emitted_yaw_),float(pitch-emitted_pitch_)};
        if(!std::isfinite(result.yaw)||!std::isfinite(result.pitch)){reset();return {};}
        emitted_yaw_=yaw;emitted_pitch_=pitch;
        return result;
    }
};
}
