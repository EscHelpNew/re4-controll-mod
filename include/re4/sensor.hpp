#pragma once
#include <cstddef>
#include <cstdint>

namespace re4 {
struct SensorState {
    std::uint64_t interval_ns, number;
    float acceleration[3], angular_velocity[3], angle[3], direction[9];
    std::uint32_t attributes, reserved;
};
static_assert(sizeof(SensorState) == 96);
static_assert(offsetof(SensorState, angular_velocity) == 28);
// Backend functions return Result values instead of SDK public wrappers that
// abort on service failures. Handles are valid only for the selected pad/style.
template<class Backend> class SensorReader {
    std::uint32_t handles_[2]{};
    std::uint32_t id_ = 0, style_ = 0, attributes_ = 0;
    bool enabled_ = false, started_ = false;
    unsigned selected_ = 0;
public:
    std::uint32_t result = 0;
    void stop() {
        if (started_) Backend::stop(handles_[selected_]);
        started_ = false;
    }
    bool select(std::uint32_t id, std::uint32_t style, std::uint32_t attributes, bool enabled) {
        if (id == id_ && style == style_ && attributes == attributes_ && enabled == enabled_) return false;
        stop(); id_ = id; style_ = style; attributes_ = attributes; enabled_ = enabled;
        result = 0;
        if (!enabled || !(attributes & 1) || (id != 0 && id != 32) || (style != 1 && style != 2 && style != 4)) return true;
        int count = 0;
        result = Backend::handles(handles_, &count, 2, id, style);
        if (result || count < 1 || count > 2) return true;
        selected_ = style == 4 && count == 2 && (attributes & 8) ? 1 : 0;
        result = Backend::start(handles_[selected_]);
        if (result && count == 2) { selected_ ^= 1; result = Backend::start(handles_[selected_]); }
        started_ = result == 0;
        return true;
    }
    int read(SensorState* samples, int capacity) {
        if (!started_ || !samples || capacity < 1) return 0;
        int count = 0;
        result = Backend::read(&count, samples, capacity, handles_[selected_]);
        if (result || count < 0 || count > capacity) {
            stop();
            if (!result) result = 0xFFFFFFFFu;
            return 0;
        }
        return !result && count >= 0 && count <= capacity ? count : 0;
    }
    bool running() const { return started_; }
};
}
