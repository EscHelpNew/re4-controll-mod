#include "re4/input.hpp"
#include <cmath>

namespace re4 {
float shape_axis(float raw, float deadzone, float gain) {
    if (!std::isfinite(raw) || !std::isfinite(deadzone) || !std::isfinite(gain)
        || deadzone < 0 || deadzone >= 1 || gain < 0) return 0;
    const float magnitude = std::fmin(std::fabs(raw), 1.f);
    if (magnitude <= deadzone) return 0;
    const float shaped = std::fmin((magnitude - deadzone) / (1 - deadzone) * gain, 1.f);
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
