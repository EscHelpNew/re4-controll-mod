#pragma once
#include "input.hpp"
#include <cmath>
#include <cstdint>

namespace re4 {
inline std::uint32_t turn_flags(std::uint32_t original, float raw,
                               bool dpad, bool enabled, float deadzone) {
    if (!enabled || dpad) return original;
    const float axis = shape_axis(raw, deadzone, 1);
    return (original & ~12u) | (axis > 0 ? 4u : axis < 0 ? 8u : 0u);
}
inline float turn_factor(float raw, bool dpad, float deadzone, float gain, StickCurve curve = StickCurve::Linear) {
    return dpad ? 1.f : std::fabs(shape_axis(raw, deadzone, 1, curve)) * gain;
}
inline float yaw_delta(float before, float after) {
    constexpr float pi = 3.14159265359f, tau = 2 * pi;
    if (!std::isfinite(before) || !std::isfinite(after) || std::fabs(before) > tau || std::fabs(after) > tau) return 0;
    float delta = after - before;
    while (delta > pi) delta -= tau;
    while (delta < -pi) delta += tau;
    return delta;
}
inline float corrected_turn_yaw(float before, float after, float factor) {
    constexpr float pi = 3.14159265359f, tau = 2 * pi;
    if (!std::isfinite(before) || !std::isfinite(after) || !std::isfinite(factor)
        || std::fabs(before) > tau || std::fabs(after) > tau || factor < 0 || factor > 4 || factor == 1)
        return after;
    const float delta = yaw_delta(before, after);
    if (std::fabs(delta) > 0.35f) return after;
    float result = before + delta * factor;
    while (result > pi) result -= tau;
    while (result < -pi) result += tau;
    return result;
}
}
