#pragma once
#include <cstddef>
#include <cstdint>
#include "settings.hpp"

namespace re4 {
float shape_axis(float raw, float deadzone, float gain, StickCurve curve = StickCurve::Linear);
void process_stick_diagnostics(const float raw[4],const Settings&,float processed[4]);
struct GyroSample {
    std::uint64_t number;
    std::uint64_t interval_ns;
    float yaw_radians_per_second;
    float pitch_radians_per_second;
};
struct AimDelta { float yaw = 0; float pitch = 0; };
class GyroFilter {
    struct Point { std::uint64_t time; float yaw, pitch; } history_[16]{};
    std::uint64_t last_ = 0, time_ = 0;
    unsigned count_ = 0;
    bool seen_ = false, flowing_ = false, discontinuity_ = false;
    std::uint64_t emitted_ns_=0;
public:
    void reset();
    std::uint64_t emitted_ns()const{return emitted_ns_;}
    bool discontinuity()const{return discontinuity_;}
    AimDelta update(GyroSample sample, bool active, float gain_x, float gain_y, float deadzone_dps);
};
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
