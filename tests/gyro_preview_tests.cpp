#include "re4/gyro_preview.hpp"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <limits>
#define CHECK(c) do { if(!(c)){std::fprintf(stderr,"gyro preview line %d: %s\n",__LINE__,#c);std::exit(1);} } while(false)
namespace {
bool near(float a,float b,float tolerance=.000002f){return std::fabs(a-b)<tolerance;}
re4::Settings tuning(){
    re4::Settings s;s.gyro_enabled=true;s.invert_x=s.invert_y=false;
    s.gyro_speed=1;s.gyro_deadzone_dps=0;return s;
}
// Independent caller from module poll_sensors: rates enter before calibration.
struct GameBaseline {
    re4::GyroFilter filter;re4::GyroFrame frame;
    void begin(std::uint64_t tick,bool active){frame.begin_frame(tick,active);if(!active)filter.reset();}
    void push(re4::GyroSample sample,const re4::Settings& s,bool active){
        sample.yaw_radians_per_second-=s.gyro_bias_x;
        sample.pitch_radians_per_second-=s.gyro_bias_y;
        const auto delta=filter.update(sample,active,
            s.gyro_speed*s.gyro_gain_x*(s.invert_x?-1:1),
            s.gyro_speed*s.gyro_gain_y*(s.invert_y?-1:1),s.gyro_deadzone_dps);
        if(filter.discontinuity())frame.reset();
        if(filter.emitted_ns())frame.push(delta,filter.emitted_ns());
    }
};
void baseline_cadence(unsigned hz){
    re4::GyroPreview preview;GameBaseline baseline;auto s=tuning();
    s.gyro_bias_x=.15f;s.gyro_bias_y=-.2f;s.gyro_speed=.8f;
    s.gyro_gain_x=1.3f;s.gyro_gain_y=.7f;s.invert_y=true;s.gyro_deadzone_dps=.5f;
    unsigned number=0;double yaw=0;
    for(unsigned n=0;n<hz*4;++n){
        const auto tick=1000000+std::uint64_t(n)*19200000/hz;
        const bool active=n!=0;
        preview.begin_frame(tick,s,true);baseline.begin(tick,active);
        // Fixed200Hz dataset, including reversal, deadzone and a still tail.
        const unsigned end=(n+1)*200/hz;
        for(;number<end;++number){
            const float raw=number<150?1.f:number<300?-1.f:number<450?.154f:.15f;
            const re4::GyroSample sample{number+1,5000000,raw,-.5f};
            preview.push(sample,true);baseline.push(sample,s,active);
        }
        const auto actual=preview.finish_frame(),expected=baseline.frame.finish_frame();
        CHECK(near(actual.yaw,expected.yaw));CHECK(near(actual.pitch,expected.pitch));
        if(n==0)CHECK(actual.yaw==0&&actual.pitch==0);
        yaw+=actual.yaw;CHECK(std::isfinite(yaw));CHECK(preview.finish_frame().yaw==0);
    }
}
re4::AimDelta batch(re4::GyroPreview& p,const re4::Settings& s,std::uint64_t tick,
                   unsigned first,float yaw=1,float pitch=-2,bool connected=true,bool active=true){
    p.begin_frame(tick,s,active);
    for(unsigned i=first;i<first+7;++i)p.push({i,5000000,yaw,pitch},connected);
    return p.finish_frame();
}
void gains_bias_and_guards(){
    re4::GyroPreview p;auto s=tuning();
    CHECK(batch(p,s,1000000,1).yaw==0);
    auto d=batch(p,s,1640000,8);CHECK(d.yaw>0&&d.pitch<0);
    d=batch(p,s,2280000,15);CHECK(near(d.yaw,1.f/30));CHECK(near(d.pitch,-2.f/30));
    // Tuning changes discard the oldFIFO and delayed tail, including gainzero.
    s.gyro_gain_x=0;CHECK(batch(p,s,2920000,22).yaw==0);
    d=batch(p,s,3560000,29);CHECK(d.yaw==0&&d.pitch<0);
    s.gyro_gain_x=2;s.gyro_gain_y=.5f;s.gyro_speed=.5f;s.invert_x=true;s.invert_y=true;
    CHECK(batch(p,s,4200000,36).yaw==0);
    batch(p,s,4840000,43);d=batch(p,s,5480000,50);
    CHECK(near(d.yaw,-1.f/30));CHECK(near(d.pitch,.5f/30));
    // A bias equal to raw rates cancels with no double subtraction.
    s.gyro_bias_x=1;s.gyro_bias_y=-2;
    CHECK(batch(p,s,6120000,57).yaw==0);d=batch(p,s,6760000,64);
    CHECK(d.yaw==0&&d.pitch==0);
    s=tuning();s.gyro_deadzone_dps=1;
    CHECK(batch(p,s,7400000,71,.01f,-.01f).yaw==0);
    d=batch(p,s,8040000,78,.01f,-.01f);CHECK(d.yaw==0&&d.pitch==0);
    s.gyro_enabled=false;CHECK(batch(p,s,8680000,85).yaw==0);
    s.gyro_enabled=true;CHECK(batch(p,s,9320000,92).yaw==0);
    CHECK(batch(p,s,9960000,99).yaw>0);
    CHECK(batch(p,s,10600000,106,1,-2,false).yaw==0);
    p.reset();CHECK(batch(p,s,11240000,113).yaw==0);
    CHECK(batch(p,s,11880000,113).yaw==0); // duplicate batch
    CHECK(batch(p,s,12520000,106).yaw==0); // older batch
    CHECK(batch(p,s,13160000,120,std::numeric_limits<float>::quiet_NaN()).yaw==0);
    p.begin_frame(13800000,s,true);p.push({128,60000000,1,1},true);CHECK(p.finish_frame().yaw==0);
    // Wall interruption drains a whole batch; no intervals are synthesized.
    CHECK(batch(p,s,17000000,129).yaw==0);
    CHECK(batch(p,s,17640000,136).yaw>0);
    CHECK(batch(p,s,18280000,143,1,-2,true,false).yaw==0);
    CHECK(batch(p,s,18920000,150).yaw>0);
    // Center/controller reset discards both output and interpolation queue.
    p.reset();CHECK(batch(p,s,19560000,1,0,0).yaw==0);
    d=batch(p,s,20200000,8,0,0);CHECK(d.yaw==0&&d.pitch==0);
}
void bounded_position(){
    const auto bounded=re4::clamp_gyro_preview_position({1000,-1000});
    CHECK(near(810-bounded.yaw*270,390,.001f));CHECK(near(350-bounded.pitch*270,640,.001f));
    auto position=bounded;
    for(unsigned i=0;i<10000;++i)position=re4::clamp_gyro_preview_position({position.yaw+1,position.pitch-1});
    CHECK(position.yaw==bounded.yaw&&position.pitch==bounded.pitch);
    position=re4::clamp_gyro_preview_position({position.yaw-.01f,position.pitch+.02f});
    CHECK(near(position.yaw,bounded.yaw-.01f)&&near(position.pitch,bounded.pitch+.02f));
    const auto opposite=re4::clamp_gyro_preview_position({-1000,1000});
    CHECK(near(810-opposite.yaw*270,1244,.001f));CHECK(near(350-opposite.pitch*270,70,.001f));
    position=opposite;
    for(unsigned i=0;i<10000;++i)position=re4::clamp_gyro_preview_position({position.yaw-1,position.pitch+1});
    CHECK(position.yaw==opposite.yaw&&position.pitch==opposite.pitch);
    position=re4::clamp_gyro_preview_position({position.yaw+.01f,position.pitch-.02f});
    CHECK(near(810-position.yaw*270,1241.3f,.001f));
    CHECK(near(350-position.pitch*270,75.4f,.001f)); // Reversal leaves either edge immediately, without windup.
    const auto invalid=re4::clamp_gyro_preview_position({std::numeric_limits<float>::infinity(),1});
    CHECK(invalid.yaw==0&&invalid.pitch==0);
}
}
int main(){
    for(auto hz:{30u,60u,120u})baseline_cadence(hz);
    gains_bias_and_guards();bounded_position();
    std::puts("gyro preview: game baseline30/60/120Hz, tuning, bias, guards and bounds PASS");
}
