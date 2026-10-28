#pragma once
#include "settings.hpp"
#include <cstddef>
#include <cstdint>

namespace re4 {
constexpr char kSharedMarker[16] = "RE4CTL_SHARED_1";
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
};
static_assert(offsetof(SharedControl, settings) == 32);
static_assert(offsetof(SharedControl, frames) == 96);
static_assert(sizeof(SharedControl) == 120);
constexpr unsigned kCapabilityTurning = 1;
}
