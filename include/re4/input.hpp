#pragma once
#include <cstddef>
#include <cstdint>
#include "settings.hpp"

namespace re4 {
float shape_axis(float raw, float deadzone, float gain, StickCurve curve = StickCurve::Linear);
struct GyroSample {
    std::uint64_t number;
    std::uint64_t interval_ns;
    float yaw_radians_per_second;
    float pitch_radians_per_second;
};
struct AimDelta { float yaw = 0; float pitch = 0; };
class GyroIntegrator {
    std::uint64_t last_ = 0;
    bool seen_ = false;
public:
    void reset();
    AimDelta update(GyroSample sample, bool active, float gain_x, float gain_y);
    AimDelta update_batch(const GyroSample* oldest_first, std::size_t count,
                          bool active, float gain_x, float gain_y);
};
}
