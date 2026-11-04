#pragma once
#include "gyro_frame.hpp"
#include <algorithm>

namespace re4 {
namespace gyro_preview {
constexpr int left=390,right=1244,top=70,bottom=640,origin_x=810,origin_y=350;
constexpr float pixels_per_radian=270.f;
}
// Native positive yaw turns left; presentation maps X as origin_x-yaw*scale.
inline AimDelta clamp_gyro_preview_position(AimDelta position){
    using namespace gyro_preview;
    if(!std::isfinite(position.yaw)||!std::isfinite(position.pitch))return {};
    return {std::clamp(position.yaw,float(origin_x-right)/pixels_per_radian,float(origin_x-left)/pixels_per_radian),
            std::clamp(position.pitch,float(origin_y-bottom)/pixels_per_radian,float(origin_y-top)/pixels_per_radian)};
}
class GyroPreview {
    GyroFilter filter_;
    GyroFrame frame_;
    Settings tuning_;
    std::uint64_t last_tick_=0;
    bool seen_=false,active_=false;
public:
    void reset(){filter_.reset();frame_.reset();last_tick_=0;seen_=active_=false;}
    void begin_frame(std::uint64_t tick,const Settings& settings,bool active){
        const bool changed=!seen_ || settings.gyro_enabled!=tuning_.gyro_enabled
            || settings.gyro_speed!=tuning_.gyro_speed || settings.gyro_gain_x!=tuning_.gyro_gain_x
            || settings.gyro_gain_y!=tuning_.gyro_gain_y || settings.gyro_deadzone_dps!=tuning_.gyro_deadzone_dps
            || settings.gyro_bias_x!=tuning_.gyro_bias_x || settings.gyro_bias_y!=tuning_.gyro_bias_y
            || settings.invert_x!=tuning_.invert_x || settings.invert_y!=tuning_.invert_y;
        const bool continuous=seen_ && tick>=last_tick_ && tick-last_tick_<=1920000;
        active_=active && settings.gyro_enabled && continuous && !changed;
        tuning_=settings;seen_=true;last_tick_=tick;
        if(!active_)filter_.reset();
        frame_.begin_frame(tick,active_);
    }
    // Rates are axis-mapped raw radians/second. Bias is subtracted here once.
    void push(GyroSample raw,bool connected){
        raw.yaw_radians_per_second-=tuning_.gyro_bias_x;
        raw.pitch_radians_per_second-=tuning_.gyro_bias_y;
        const auto delta=filter_.update(raw,active_&&connected,
            tuning_.gyro_speed*tuning_.gyro_gain_x*(tuning_.invert_x?-1:1),
            tuning_.gyro_speed*tuning_.gyro_gain_y*(tuning_.invert_y?-1:1),tuning_.gyro_deadzone_dps);
        if(!connected || filter_.discontinuity())frame_.reset();
        if(filter_.emitted_ns())frame_.push(delta,filter_.emitted_ns());
    }
    AimDelta finish_frame(){return frame_.finish_frame();}
};
}
