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
void GyroFilter::reset() { last_=elapsed_ns_=0; seen_=anchored_=false; yaw_=pitch_=0; }
AimDelta GyroFilter::update(GyroSample s,bool active,float gx,float gy,float deadzone_dps) {
    if (!active) { anchored_=false;elapsed_ns_=0; }
    if (seen_ && s.number<=last_) return {};
    const auto skipped=seen_ ? s.number-last_ : 1;
    last_=s.number;seen_=true;
    if (!active || !s.interval_ns || s.interval_ns>50'000'000
        || skipped>50'000'000/s.interval_ns
        || !std::isfinite(s.yaw_radians_per_second) || !std::isfinite(s.pitch_radians_per_second)
        || !std::isfinite(gx) || !std::isfinite(gy) || !std::isfinite(deadzone_dps)
        || deadzone_dps<0 || deadzone_dps>20) {
        anchored_=false;elapsed_ns_=0;return {};
    }
    if (!anchored_) {
        yaw_=s.yaw_radians_per_second;pitch_=s.pitch_radians_per_second;
        anchored_=true;elapsed_ns_=0;return {};
    }
    elapsed_ns_+=s.interval_ns*skipped;
    if (elapsed_ns_<10'000'000) return {};
    const float seconds=float(elapsed_ns_)*1e-9f;
    const float threshold=deadzone_dps*0.01745329252f;
    const auto soften=[threshold](float rate) {
        return std::copysign(std::fmax(0.f,std::fabs(rate)-threshold),rate);
    };
    const AimDelta result{soften((yaw_+s.yaw_radians_per_second)*0.5f)*seconds*gx,
                          soften((pitch_+s.pitch_radians_per_second)*0.5f)*seconds*gy};
    yaw_=s.yaw_radians_per_second;pitch_=s.pitch_radians_per_second;elapsed_ns_=0;
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
