#pragma once
#include "re6/settings.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace re6 {
constexpr char kSharedMarker[16] = "RE6CTL_SHARED1";
constexpr std::uint32_t kSharedAbi = 1;
constexpr std::uint32_t kCapabilityFiltering = 1;
struct alignas(8) SharedControl {
    char marker[16];
    std::uint32_t abi;
    std::uint32_t status;
    std::uint32_t sequence;
    std::uint32_t capabilities;
    std::uint8_t settings[kSettingsBytes];
    std::uint32_t applied_sequence;
    std::uint32_t settings_source;
    std::uint64_t frames;
    std::uint32_t graphics_filter_latched;
    std::uint32_t sampler_calls;
    std::uint32_t sampler_overrides;
    std::uint32_t reserved;
};
static_assert(alignof(SharedControl) == 8);
static_assert(offsetof(SharedControl, settings) == 32);
static_assert(offsetof(SharedControl, applied_sequence) == 96);
static_assert(offsetof(SharedControl, settings_source) == 100);
static_assert(offsetof(SharedControl, frames) == 104);
static_assert(offsetof(SharedControl, graphics_filter_latched) == 112);
static_assert(offsetof(SharedControl, sampler_calls) == 116);
static_assert(offsetof(SharedControl, sampler_overrides) == 120);
static_assert(offsetof(SharedControl, reserved) == 124);
static_assert(sizeof(SharedControl) == 128);
inline SharedControl initial_shared_control() {
    SharedControl control{};
    for (unsigned i = 0; i < sizeof(kSharedMarker); ++i) control.marker[i] = kSharedMarker[i];
    control.abi = kSharedAbi;
    control.graphics_filter_latched = 255;
    encode_settings(Settings{}, control.settings, sizeof(control.settings));
    return control;
}
namespace detail {
inline bool valid_shared_header(const volatile SharedControl& control) {
    for (unsigned i = 0; i < sizeof(kSharedMarker); ++i)
        if (control.marker[i] != kSharedMarker[i]) return false;
    return control.abi == kSharedAbi && control.status <= 2 && control.reserved == 0
        && !(control.capabilities & ~kCapabilityFiltering);
}
}
// The module uses this during bootstrap; a live producer follows the same
// odd / payload / even transaction. Only one producer may own an odd sequence.
inline bool publish_settings(volatile SharedControl& control, const Settings& settings,
                             std::uint32_t& last_sequence) {
    std::uint8_t bytes[kSettingsBytes];
    if (!detail::valid_shared_header(control) || !encode_settings(settings, bytes, sizeof(bytes))) return false;
    auto sequence = __atomic_load_n(&control.sequence, __ATOMIC_ACQUIRE);
    if ((sequence & 1) || sequence != last_sequence) return false;
    if (!__atomic_compare_exchange_n(&control.sequence, &sequence, sequence + 1,
                                    false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) return false;
    for (unsigned i = 0; i < sizeof(bytes); ++i)
        __atomic_store_n(&control.settings[i], bytes[i], __ATOMIC_RELAXED);
    const auto next = sequence + 2;
    __atomic_store_n(&control.sequence, next, __ATOMIC_RELEASE);
    last_sequence = next;
    return true;
}
inline bool consume_settings(volatile SharedControl& control, Settings& out,
                             std::uint32_t& last_sequence) {
    if (!detail::valid_shared_header(control)) return false;
    const auto sequence = __atomic_load_n(&control.sequence, __ATOMIC_ACQUIRE);
    const auto delta = sequence - last_sequence;
    // Modular ordering accepts wraparound, but not a duplicate, an old value,
    // or the ambiguous point halfway through the 32-bit sequence space.
    if ((sequence & 1) || delta == 0 || delta >= 0x80000000u) return false;
    std::uint8_t bytes[kSettingsBytes];
    for (unsigned i = 0; i < sizeof(bytes); ++i)
        bytes[i] = __atomic_load_n(&control.settings[i], __ATOMIC_RELAXED);
    std::atomic_thread_fence(std::memory_order_acquire);
    if (__atomic_load_n(&control.sequence, __ATOMIC_ACQUIRE) != sequence) return false;
    Settings settings;
    if (!decode_settings(bytes, sizeof(bytes), settings)
        || __atomic_load_n(&control.sequence, __ATOMIC_ACQUIRE) != sequence) return false;
    out = settings;
    last_sequence = sequence;
    __atomic_store_n(&control.applied_sequence, sequence, __ATOMIC_RELEASE);
    return true;
}
}
