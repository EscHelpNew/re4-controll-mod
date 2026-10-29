#pragma once
#include "input.hpp"
#include <cmath>
#include <cstdint>

namespace re4 {
// Limit correction to small input deltas; weapon changes, resets and scripted
// pose changes must not be multiplied as though they were stick movement.
inline float scaled_axis_state(float before, float after, float factor,
                               float bound, float maximum_delta) {
    if (!std::isfinite(before) || !std::isfinite(after) || !std::isfinite(factor)
        || factor < 0 || factor > 4 || factor == 1
        || std::fabs(before) > bound || std::fabs(after) > bound
        || std::fabs(after - before) > maximum_delta) return after;
    return std::fmax(-bound, std::fmin(bound, before + (after - before) * factor));
}
inline std::int8_t camera_axis(float raw, float deadzone, float gain) {
    return static_cast<std::int8_t>(std::lround(shape_axis(raw, deadzone, gain) * 63));
}
inline float gyro_pitch(float before, float delta) {
    if (!std::isfinite(before) || !std::isfinite(delta) || std::fabs(before) > 1) return before;
    return std::fmax(-1, std::fmin(1, before + delta));
}
inline float gyro_yaw(float before, float delta) {
    constexpr float pi = 3.14159265359f, tau = 2 * pi;
    if (!std::isfinite(before) || !std::isfinite(delta) || std::fabs(before) > tau) return before;
    // Fresh high-gain motion can exceed a radian per render frame. Wrap that
    // motion instead of dropping it; sampling/timing gates handle stale input.
    float result = before + std::remainder(delta, tau);
    if (result > pi) result -= tau;
    if (result < -pi) result += tau;
    return result;
}
}
