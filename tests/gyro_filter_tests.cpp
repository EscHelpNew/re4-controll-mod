#include "re4/input.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
namespace { bool near(float a,float b){return std::fabs(a-b)<0.000001f;} }
int main(){
    using namespace re4;
    GyroFilter filter;
    assert(filter.update({1,5'000'000,1,2},true,1,1,0).yaw==0);
    assert(filter.update({2,5'000'000,99,99},true,1,1,0).yaw==0);
    auto d=filter.update({3,5'000'000,3,4},true,2,-1,0);
    assert(near(d.yaw,0.04f) && near(d.pitch,-0.03f)); // Mean of endpoints10ms apart.
    assert(filter.update({3,5'000'000,3,4},true,2,-1,0).yaw==0); // No replay.
    filter.reset();
    constexpr float rad=3.141592653589793f/180;
    filter.update({1,10'000'000,0.4f*rad,-2*rad},true,1,1,0.5f);
    d=filter.update({2,10'000'000,0.4f*rad,-2*rad},true,1,1,0.5f);
    assert(d.yaw==0 && near(d.pitch,-1.5f*rad*0.01f));
    filter.update({3,10'000'000,100,100},false,1,1,0);
    assert(filter.update({4,10'000'000,1,1},true,1,1,0).yaw==0);
    assert(near(filter.update({5,10'000'000,1,1},true,1,1,0).yaw,0.01f));
    filter.update({5,10'000'000,1,1},false,1,1,0); // Inactive even with no fresh sample.
    assert(filter.update({6,10'000'000,1,1},true,1,1,0).yaw==0);
    assert(filter.update({50,10'000'000,100,100},true,1,1,0).yaw==0); // Missing history.
    assert(filter.update({51,0,1,1},true,1,1,0).yaw==0);
    assert(filter.update({52,5'000'000,std::numeric_limits<float>::quiet_NaN(),1},true,1,1,0).yaw==0);
    filter.reset();
    float totals[2]{};
    for(int grouping=0;grouping<2;++grouping){
        filter.reset(); const unsigned batch=grouping?3:7;
        for(unsigned start=1;start<=201;start+=batch)
            for(unsigned i=start;i<start+batch && i<=201;++i)
                totals[grouping]+=filter.update({i,5'000'000,1,0},true,3,1,0).yaw;
    }
    assert(std::fabs(totals[0]-3)<0.00001f && near(totals[0],totals[1]));
    Settings settings; settings.gyro_speed=2.5f;settings.gyro_deadzone_dps=0.75f;
    settings.gyro_enabled=true;settings.language=Language::Russian;settings.stick_curve=StickCurve::Hybrid;
    std::uint8_t bytes[kSettingsBytes];
    assert(encode_settings(settings,bytes,sizeof(bytes)) && bytes[4]==8);
    Settings decoded;assert(decode_settings(bytes,sizeof(bytes),decoded));
    assert(decoded.gyro_speed==2.5f && decoded.gyro_deadzone_dps==0.75f && decoded.gyro_enabled);
    assert(decoded.language==Language::Russian && decoded.stick_curve==StickCurve::Hybrid);
    for(unsigned version=1;version<=4;++version){
        bytes[4]=std::uint8_t(version);bytes[8]=version<4?15:127;bytes[9]=bytes[10]=bytes[11]=0;
        const float old_walk=version==1?0.f:0.3f;
        std::memcpy(bytes+56,&old_walk,4);std::memset(bytes+12,0,4);
        std::uint32_t hash=2166136261u;
        for(unsigned i=0;i<60;++i){hash^=bytes[i];hash*=16777619u;}
        std::memcpy(bytes+60,&hash,4);
        assert(decode_settings(bytes,kLegacySettingsBytes,decoded));
        assert(decoded.gyro_enabled && decoded.gyro_speed==0.8f && decoded.gyro_deadzone_dps==0.5f);
        assert(decoded.movement_gain==1);
        assert(decoded.language==(version==4?Language::Russian:Language::English));
    }
    assert(encode_settings(settings,bytes,sizeof(bytes)));bytes[4]=5;bytes[9]=bytes[10]=bytes[11]=0;
    std::uint32_t old_hash=2166136261u;
    for(unsigned i=0;i<60;++i){old_hash^=bytes[i];old_hash*=16777619u;}
    std::memcpy(bytes+60,&old_hash,4);assert(decode_settings(bytes,kLegacySettingsBytes,decoded));
    assert(decoded.gyro_speed==2.5f && decoded.gyro_deadzone_dps==0.75f && decoded.right_aim_gain_x==1);
    settings.gyro_deadzone_dps=-1;assert(!valid_settings(settings));
    settings.gyro_deadzone_dps=0.5f;settings.gyro_speed=10.1f;assert(!valid_settings(settings));
    std::puts("gyro filter: 10ms endpoint mean, soft deadzone, speed, no replay and wirev5 PASS");
}
