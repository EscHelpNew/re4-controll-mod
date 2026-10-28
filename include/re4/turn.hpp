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
inline float turn_factor(float raw, bool dpad, float deadzone, float gain) {
    return dpad ? 1.f : std::fabs(shape_axis(raw, deadzone, 1)) * gain;
}
}
