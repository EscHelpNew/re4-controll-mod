#pragma once
#include "settings.hpp"
#include "version.hpp"
#include <cstddef>
#include <cstdint>
#include <atomic>

namespace re4 {
constexpr char kSharedMarker[16] = "RE4CTL_SHARED24";
constexpr unsigned kSharedAbi = 24;
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
    std::uint32_t sensor_fresh_samples;
    std::uint32_t sensor_restarts;
    std::uint32_t gyro_active;
    std::uint32_t pad_style;
    float gyro_rate_yaw;
    float gyro_rate_pitch;
    std::uint32_t aim_source;
    std::int32_t applied_aim_x;
    std::int32_t applied_aim_y;
    float aim_full_scale;
    std::uint32_t settings_attempts;
    std::uint32_t settings_mount_result;
    std::uint32_t settings_read_result;
    std::uint32_t settings_source; // 0 pending, 1 primary, 2 backup, 3 defaults, 4 live IPC.
    std::uint32_t graphics_sampler_calls;
    std::uint32_t graphics_sampler_overrides;
    std::uint32_t graphics_filter_latched;
    std::uint32_t graphics_scene_submits;
    std::uint32_t graphics_scene_draws;
    std::uint32_t graphics_cull_updates;
    float graphics_native_far;
    float graphics_effective_far;
    std::uint32_t graphics_resolution_requests;
    std::int32_t graphics_display_index;
    std::int32_t graphics_back_index;
    std::uint32_t graphics_ready;
    std::uint32_t graphics_aa_latched;
    std::uint32_t graphics_reserved; // ABI22: weapon popup producer/HUD/event/frame diagnostic bytes.
    std::uint32_t fps_overrides;
    std::uint32_t reticle_draws;
    std::uint32_t modern_move_steps;
    std::uint32_t modern_pose_steps;
    std::uint32_t modern_move_status;
    float modern_move_distance;
    alignas(8) std::uint32_t appearance_applied; // Keep ABI21's trailing padding.
    std::uint32_t appearance_status;
    std::uint32_t appearance_error;
    std::uint32_t appearance_generation; // Odd while publishing; even when stable.
    std::uint32_t popup_capture_sequence; // Ordered native command writer, odd while publishing.
    std::uint32_t popup_capture_status;
    float popup_viewport[4];
    std::int32_t popup_scissor[4];
    std::uint64_t popup_tick;
    std::uint64_t popup_event;
    char module_version[16];
    std::uint32_t money_request;
    std::uint32_t money_ack;
    std::uint32_t money_result;
    std::uint32_t money_availability;
};
static_assert(offsetof(SharedControl, settings) == 32);
static_assert(offsetof(SharedControl, frames) == 160);
static_assert(offsetof(SharedControl, applied_sequence) == 184);
static_assert(offsetof(SharedControl, appearance_applied) == 400);
static_assert(offsetof(SharedControl, appearance_generation) == 412);
static_assert(offsetof(SharedControl,popup_capture_sequence)==416);
static_assert(offsetof(SharedControl,popup_viewport)==424);
static_assert(offsetof(SharedControl,popup_scissor)==440);
static_assert(offsetof(SharedControl,popup_tick)==456);
static_assert(offsetof(SharedControl,popup_event)==464);
static_assert(offsetof(SharedControl,module_version)==472);
static_assert(offsetof(SharedControl,money_request)==488);
static_assert(offsetof(SharedControl,money_ack)==492);
static_assert(sizeof(SharedControl) == 504);
// Native64E084..B0 represents disabled scissor by this exact integer rect.
// Keep it distinct from ordinary bounded rectangles and from framebuffer size.
constexpr bool native_scissor_disabled(const std::int32_t* rect){
    return rect&&rect[0]==0&&rect[1]==0&&rect[2]==0x7FFFFFFF&&rect[3]==0x7FFFFFFF;
}
constexpr unsigned kCapabilityTurning = 1;
constexpr SharedControl initial_shared_control() {
    SharedControl result{};
    for(unsigned i=0;i<16;++i) result.marker[i]=kSharedMarker[i];
    result.abi=kSharedAbi;result.capabilities=kCapabilityTurning;
    result.graphics_filter_latched=255;
    result.graphics_aa_latched=255;
    result.graphics_display_index=result.graphics_back_index=-1;
    for(unsigned i=0;i<15&&kVersion[i];++i)result.module_version[i]=kVersion[i];
    return result;
}
inline bool acknowledge_settings(volatile SharedControl& control, const Settings& settings,
                                 std::uint32_t sequence) {
    if ((sequence & 1) || control.sequence != sequence || !valid_settings(settings)) return false;
    control.applied_turn_gain = settings.turn_gain;
    std::atomic_thread_fence(std::memory_order_release);
    control.applied_sequence = sequence;
    return true;
}
}
