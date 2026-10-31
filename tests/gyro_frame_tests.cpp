#include "re4/gyro_frame.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <initializer_list>

namespace {
bool near(float a,float b,float tolerance=0.000002f){return std::fabs(a-b)<tolerance;}
constexpr std::uint64_t frame30=640000,frame60=320000;
void feed(re4::GyroFrame& frame,unsigned count,float yaw=1,float pitch=-2){
    for(unsigned i=0;i<count;++i)assert(frame.push({yaw*.005f,pitch*.005f},5000000));
}
void cadence(unsigned hz){
    re4::GyroFrame frame;
    std::uint64_t tick=1000000;
    const auto step=hz==30?frame30:frame60;
    for(unsigned n=0;n<120;++n){
        const unsigned count=(n+1)*200/hz-n*200/hz;
        frame.begin_frame(tick,true);feed(frame,count);
        const auto d=frame.finish_frame();
        if(n>0){assert(near(d.yaw,1.f/hz));assert(near(d.pitch,-2.f/hz));}
        assert(frame.finish_frame().yaw==0); // A frame can only be consumed once.
        tick+=step;
    }
}
void area_and_dropout(){
    re4::GyroFrame frame;double yaw=0,pitch=0;
    std::uint64_t tick=1000000;
    // A50ms rectangular pulse, then freshzero samples. Startup retains all
    // genuine motion, including the firstbatch's delayed10ms tail.
    for(unsigned n=0;n<8;++n){
        frame.begin_frame(tick,true);
        const unsigned start=n*200/30,count=(n+1)*200/30-start;
        for(unsigned i=0;i<count;++i){const bool pulse=start+i<10;
            assert(frame.push({pulse?.005f:0,pulse?-.01f:0},5000000));}
        const auto d=frame.finish_frame();yaw+=d.yaw;pitch+=d.pitch;
        if(n>=3)assert(d.yaw==0&&d.pitch==0);
        tick+=frame30;
    }
    assert(std::fabs(yaw-.05)<.000001&&std::fabs(pitch+.1)<.000001);
    frame.reset();frame.begin_frame(tick,true);feed(frame,6);yaw=frame.finish_frame().yaw;
    // Without new measurements, present only the existing tail; never invent
    // a continuing velocity. Duplicated ticks cannot consume it twice.
    frame.begin_frame(tick,true);assert(frame.finish_frame().yaw==0);
    for(unsigned n=0;n<8;++n){tick+=frame30;frame.begin_frame(tick,true);
        const auto d=frame.finish_frame();yaw+=d.yaw;if(n>0)assert(d.yaw==0);}
    assert(std::fabs(yaw-.03)<.000001);
}
void changing_frames(){
    re4::GyroFrame frame;std::uint64_t tick=1000000,elapsed=0,samples=0;
    frame.begin_frame(tick,true);feed(frame,6);frame.finish_frame();
    for(auto step:{frame30,frame60,frame60,frame30,frame60,frame30}){
        elapsed+=step;tick+=step;
        const auto next=elapsed/96000; //5ms sensorperiod in native ticks.
        frame.begin_frame(tick,true);feed(frame,unsigned(next-samples));samples=next;
        assert(near(frame.finish_frame().yaw,float(step)/19200000));
    }
}
void clock_skew(){
    re4::GyroFrame frame;std::uint64_t tick=1000000;
    frame.begin_frame(tick,true);feed(frame,6);frame.finish_frame();
    // A malformed sensor timeline running150% of walltime must drain before
    // it can retain a future-motion backlog beyond100ms.
    for(unsigned n=1;n<=7;++n){tick+=frame30;frame.begin_frame(tick,true);feed(frame,10);
        const auto d=frame.finish_frame();if(n<7)assert(near(d.yaw,1.f/30));else assert(d.yaw==0);}
}
void guards(){
    re4::GyroFrame frame;
    assert(!frame.push({1,1},5000000));assert(frame.finish_frame().yaw==0);
    frame.begin_frame(1000000,false);assert(!frame.push({1,1},5000000));assert(frame.finish_frame().yaw==0);
    // Reset/gate discards the pending tail and cumulative history.
    frame.begin_frame(1000000,true);feed(frame,6);frame.finish_frame();
    frame.begin_frame(1640000,false);assert(frame.finish_frame().yaw==0);
    frame.begin_frame(2280000,true);assert(frame.finish_frame().yaw==0);
    frame.begin_frame(2920000,true);feed(frame,1);assert(frame.finish_frame().yaw==0);
    frame.reset();frame.begin_frame(1000000,true);feed(frame,6);frame.finish_frame();
    frame.begin_frame(1000000+1920001,true);assert(!frame.push({.4f,.4f},5000000));
    assert(frame.finish_frame().yaw==0); // Pause flushes oldtail and this potentially stale batch.
    frame.begin_frame(500000,true);assert(frame.finish_frame().yaw==0); // Backward clock.
    for(auto interval:{std::uint64_t(0),std::uint64_t(50000001)}){
        frame.begin_frame(1000000,true);assert(!frame.push({.01f,.02f},interval));assert(frame.finish_frame().yaw==0);}
    for(auto value:{std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
        frame.begin_frame(1000000,true);assert(!frame.push({value,0},5000000));assert(frame.finish_frame().yaw==0);}
    // Excessive input without presenting cannot overwrite the interpolation
    // bracket or deliver accumulated old motion as one burst.
    frame.reset();frame.begin_frame(1000000,true);
    for(unsigned i=0;i<63;++i)assert(frame.push({0,0},1000000));
    assert(!frame.push({0,0},1000000));assert(frame.finish_frame().yaw==0);
    frame.begin_frame(1000000,true);
    assert(frame.push({.01f,0},50000000));assert(frame.push({.01f,0},50000000));
    assert(!frame.push({.01f,0},1));assert(frame.finish_frame().yaw==0);
    // Aggregate displacement can overflow float even with finite input.
    frame.begin_frame(1000000,true);
    for(unsigned i=0;i<3;++i)assert(frame.push({std::numeric_limits<float>::max(),0},10000000));
    assert(frame.finish_frame().yaw==0);
}
}
int main(){
    cadence(30);cadence(60);area_and_dropout();changing_frames();clock_skew();guards();
    std::puts("gyro frame:30/60Hz cadence, pulse area, delayed tail, timing changes and drain guards PASS");
}
