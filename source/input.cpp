#include "re4/input.hpp"
#include <cmath>

namespace re4 {
void process_stick_diagnostics(const float raw[4],const Settings& s,float out[4]){
    if(!raw || !out)return;
    out[0]=shape_axis(raw[0],s.left_deadzone,s.turn_gain,s.stick_curve);
    out[1]=shape_axis(raw[1],s.left_deadzone,1,s.stick_curve);
    out[2]=shape_axis(raw[2],s.right_deadzone,s.right_aim_gain_x,s.stick_curve);
    out[3]=shape_axis(raw[3],s.right_deadzone,s.right_aim_gain_y,s.stick_curve);
}
float shape_axis(float raw, float deadzone, float gain, StickCurve curve) {
    if (!std::isfinite(raw) || !std::isfinite(deadzone) || !std::isfinite(gain)
        || deadzone < 0 || deadzone >= 1 || gain < 0 || unsigned(curve)>3) return 0;
    const float magnitude = std::fmin(std::fabs(raw), 1.f);
    if (magnitude <= deadzone) return 0;
    const float x = (magnitude - deadzone) / (1 - deadzone);
    float response = x;
    if (curve == StickCurve::Exponential) response = x*x;
    else if (curve == StickCurve::Logarithmic || curve == StickCurve::Hybrid) {
        const float logarithmic = std::log(1+9*x) / 2.302585093f;
        if (curve == StickCurve::Logarithmic) response = logarithmic;
        else {
            const float u = std::fmax(0, std::fmin(1, (x-0.25f)*2));
            const float blend = u*u*(3-2*u);
            response = (1-blend)*x*x + blend*logarithmic;
        }
    }
    const float shaped = std::fmin(response * gain, 1.f);
    return std::copysign(shaped, raw);
}
void GyroIntegrator::reset() { last_ = 0; seen_ = false; }
void GyroFilter::reset() { last_=time_=emitted_ns_=0; count_=0; seen_=flowing_=discontinuity_=false; }
AimDelta GyroFilter::update(GyroSample s,bool active,float gx,float gy,float deadzone_dps) {
    emitted_ns_=0;
    discontinuity_=false;
    if (!active) { count_=0;time_=0;flowing_=false; }
    if (seen_ && s.number<=last_) return {};
    const auto skipped=seen_ ? s.number-last_ : 1;
    last_=s.number;seen_=true;
    if (!active || !s.interval_ns || s.interval_ns>50'000'000
        || skipped>50'000'000/s.interval_ns
        || !std::isfinite(s.yaw_radians_per_second) || !std::isfinite(s.pitch_radians_per_second)
        || !std::isfinite(gx) || !std::isfinite(gy) || !std::isfinite(deadzone_dps)
        || deadzone_dps<0 || deadzone_dps>20) {
        discontinuity_=active;count_=0;time_=0;flowing_=false;return {};
    }
    if (!count_) {
        history_[0]={0,s.yaw_radians_per_second,s.pitch_radians_per_second};
        count_=1;time_=0;return {};
    }
    const auto dt=s.interval_ns*skipped;
    time_+=dt;
    const auto boundary=time_>10'000'000?time_-10'000'000:0;
    // Keep the two points bracketing t-10ms. Interpolate the older endpoint
    // when sensor timing varies; integrate every fresh sample, not 10ms bursts.
    while(count_>1 && history_[1].time<=boundary){
        for(unsigned i=1;i<count_;++i)history_[i-1]=history_[i];
        --count_;
    }
    if(count_==16){discontinuity_=true;count_=0;time_=0;flowing_=false;return {};}
    history_[count_++]={time_,s.yaw_radians_per_second,s.pitch_radians_per_second};
    if(time_<10'000'000)return {};
    const auto& a=history_[0];const auto& b=history_[1];
    const float fraction=float(boundary-a.time)/float(b.time-a.time);
    const float old_yaw=a.yaw+(b.yaw-a.yaw)*fraction;
    const float old_pitch=a.pitch+(b.pitch-a.pitch)*fraction;
    emitted_ns_=flowing_?dt:time_;
    const float seconds=float(emitted_ns_)*1e-9f;
    const float threshold=deadzone_dps*0.01745329252f;
    const auto soften=[threshold](float rate) {
        return std::copysign(std::fmax(0.f,std::fabs(rate)-threshold),rate);
    };
    const AimDelta result{soften((old_yaw+s.yaw_radians_per_second)*0.5f)*seconds*gx,
                          soften((old_pitch+s.pitch_radians_per_second)*0.5f)*seconds*gy};
    flowing_=true;
    return result;
}
AimDelta GyroIntegrator::update(GyroSample sample, bool active, float gain_x, float gain_y) {
    if (seen_ && sample.number <= last_) return {};
    last_ = sample.number;
    seen_ = true;
    if (!active || !sample.interval_ns || sample.interval_ns > 50'000'000
        || !std::isfinite(sample.yaw_radians_per_second)
        || !std::isfinite(sample.pitch_radians_per_second)
        || !std::isfinite(gain_x) || !std::isfinite(gain_y)) return {};
    const float seconds = float(sample.interval_ns) * 1e-9f;
    return {sample.yaw_radians_per_second * seconds * gain_x,
            sample.pitch_radians_per_second * seconds * gain_y};
}
AimDelta GyroIntegrator::update_batch(const GyroSample* samples, std::size_t count,
                                     bool active, float gain_x, float gain_y) {
    AimDelta result;
    if (!samples) return result;
    for (std::size_t i = 0; i < count; ++i) {
        const auto delta = update(samples[i], active, gain_x, gain_y);
        result.yaw += delta.yaw;
        result.pitch += delta.pitch;
    }
    return result;
}
}
