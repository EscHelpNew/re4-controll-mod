#pragma once
#include "settings.hpp"
#include <cstddef>
#include <cstdint>
#include <atomic>

namespace re4 {
constexpr char kSharedMarker[16] = "RE4CTL_SHARED_7";
constexpr unsigned kSharedAbi = 7;
// Wire layout is shared by the module and overlay, with no runtime pointers.
// The overlay publishes an odd sequence, payload, then the next even sequence.
// The module validates both sequence reads and the payload checksum.
struct alignas(8) SharedControl {
    char marker[16];
    std::uint32_t abi;
    std::uint32_t status;
    std::uint32_t sequence;
    std::uint32_t capabilities;
    std::uint8_t settings[kSettingsBytes];
    std::uint64_t frames;
    float raw_lx;
    float raw_ly;
    float raw_rx;
    float raw_ry;
    std::uint32_t applied_sequence;
    std::uint32_t direction_calls;
    std::uint32_t speed_calls;
    float applied_turn_gain;
    std::uint32_t pad_samples;
    std::uint32_t last_pad_id;
    float maximum_raw_lx;
    float last_turn_factor;
    float original_yaw_delta;
    float corrected_yaw_delta;
    std::uint32_t movement_calls;
    std::uint32_t camera_calls;
    std::uint32_t aim_calls;
    float movement_factor;
    std::uint32_t sensor_result;
    std::uint32_t sensor_running;
    std::uint32_t sensor_samples;
    float gyro_yaw;
    float gyro_pitch;
};
static_assert(offsetof(SharedControl, settings) == 32);
static_assert(offsetof(SharedControl, frames) == 96);
static_assert(offsetof(SharedControl, applied_sequence) == 120);
static_assert(sizeof(SharedControl) == 200);
constexpr unsigned kCapabilityTurning = 1;
inline bool acknowledge_settings(volatile SharedControl& control, const Settings& settings,
                                 std::uint32_t sequence) {
    if ((sequence & 1) || control.sequence != sequence || !valid_settings(settings)) return false;
    control.applied_turn_gain = settings.turn_gain;
    std::atomic_thread_fence(std::memory_order_release);
    control.applied_sequence = sequence;
    return true;
}
}
