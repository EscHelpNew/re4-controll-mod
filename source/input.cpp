#include "re4/input.hpp"
#include <cmath>

namespace re4 {
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
