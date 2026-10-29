#include "re4/settings.hpp"
#include "re4/input.hpp"
#include <cassert>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <initializer_list>

void seal(std::uint8_t* data){
    std::uint32_t hash=2166136261u;
    for(unsigned i=0;i<60;++i){hash^=data[i];hash*=16777619u;}
    for(unsigned i=0;i<4;++i)data[60+i]=std::uint8_t(hash>>(i*8));
}
int main(){
    using namespace re4;
    Settings defaults;
    assert(defaults.language==Language::English && defaults.stick_curve==StickCurve::Linear);
    assert(defaults.left_deadzone==0.05f && defaults.right_deadzone==0.05f);
    assert(defaults.invert_x && defaults.invert_y);
    for(auto mode:{StickCurve::Linear,StickCurve::Exponential,StickCurve::Logarithmic,StickCurve::Hybrid}){
        float previous=0;
        for(int i=0;i<=1000;++i){
            const float x=i/1000.f,y=shape_axis(x,0,1,mode);
            assert(std::isfinite(y) && y>=previous && y>=0 && y<=1);
            assert(std::fabs(y+shape_axis(-x,0,1,mode))<0.00001f);
            previous=y;
        }
        assert(shape_axis(0,0.05f,1,mode)==0);
        assert(shape_axis(0.04f,0.05f,1,mode)==0);
        assert(shape_axis(1,0.05f,1,mode)==1);
        assert(shape_axis(1,0.05f,0,mode)==0);
    }
    assert(shape_axis(0.5f,0,1,StickCurve::Exponential)<0.5f);
    assert(shape_axis(0.5f,0,1,StickCurve::Logarithmic)>0.5f);
    for(float edge:{0.25f,0.75f})
        assert(std::fabs(shape_axis(edge-0.00001f,0,1,StickCurve::Hybrid)
                        -shape_axis(edge+0.00001f,0,1,StickCurve::Hybrid))<0.001f);
    assert(shape_axis(0.5f,0,1,static_cast<StickCurve>(255))==0);
    std::uint8_t bytes[kSettingsBytes];
    Settings s;s.language=Language::Russian;s.stick_curve=StickCurve::Hybrid;
    assert(encode_settings(s,bytes,sizeof(bytes)) && bytes[4]==4);
    Settings decoded;assert(decode_settings(bytes,sizeof(bytes),decoded));
    assert(decoded.language==Language::Russian && decoded.stick_curve==StickCurve::Hybrid);
    s.language=static_cast<Language>(2);assert(!valid_settings(s));
    s={};s.stick_curve=static_cast<StickCurve>(4);assert(!valid_settings(s));
    // Legacy preset: keep custom turn gain, migrate the old default deadzone,
    // reset the removed manual walk-speed control to native full scale.
    s={};s.turn_gain=0.3f;assert(encode_settings(s,bytes,sizeof(bytes)));
    bytes[4]=3;bytes[8]&=15;
    const float old_zone=0.15f,old_walk=0.3f;
    std::memcpy(bytes+32,&old_zone,4);std::memcpy(bytes+36,&old_zone,4);
    std::memcpy(bytes+56,&old_walk,4);seal(bytes);
    assert(decode_settings(bytes,sizeof(bytes),decoded));
    assert(decoded.turn_gain==0.3f && decoded.movement_gain==1);
    assert(decoded.left_deadzone==0.05f && decoded.right_deadzone==0.05f);
    assert(decoded.language==Language::English && decoded.stick_curve==StickCurve::Linear);
    std::puts("response curves/defaults/migration: four monotone bounded curves and wirev4 PASS");
}
