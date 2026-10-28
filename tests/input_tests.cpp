#include "re4/input.hpp"
#include "re4/settings.hpp"
#include "re4/turn.hpp"
#include "re4/profile.hpp"
#include "re4/hook_context.hpp"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <vector>

static bool close(float a, float b) { return std::fabs(a - b) < 0.0001f; }

int main() {
    using namespace re4;
    assert(shape_axis(0.1f, 0.2f, 1) == 0);
    assert(close(shape_axis(0.6f, 0.2f, 1), 0.5f));
    assert(close(shape_axis(-0.6f, 0.2f, 1), -0.5f));
    assert(shape_axis(2, 0.2f, 1) == 1);
    assert(shape_axis(0.6f, 0.2f, 4) == 1);
    assert(shape_axis(std::numeric_limits<float>::quiet_NaN(), 0.2f, 1) == 0);
    assert(shape_axis(1, 1, 1) == 0);
    assert(turn_flags(0xF0C, 0.3f, false, true, 0.15f) == 0xF04);
    assert(turn_flags(0xF0C, -0.3f, false, true, 0.15f) == 0xF08);
    assert(turn_flags(0xF0C, 0.1f, false, true, 0.15f) == 0xF00);
    assert(turn_flags(0xF0C, 0.1f, true, true, 0.15f) == 0xF0C);
    assert(turn_flags(0xF0C, 0.1f, false, false, 0.15f) == 0xF0C);
    assert(close(turn_factor(1, false, 0.15f, 3), 3));
    assert(turn_factor(0, true, 0.15f, 3) == 1);
    alignas(16) std::uint8_t saved_context[0x300];
    std::memset(saved_context, 0xA5, sizeof(saved_context));
    const float s0 = 2, s16 = 7;
    std::memcpy(saved_context, &s0, sizeof(s0));
    std::memcpy(saved_context + 0x100, &s16, sizeof(s16));
    std::uint8_t expected_context[sizeof(saved_context)];
    std::memcpy(expected_context, saved_context, sizeof(saved_context));
    const float scaled_s0 = 0.5f;
    std::memcpy(expected_context, &scaled_s0, sizeof(scaled_s0));
    scale_saved_s0(saved_context, 0.25f);
    assert(!std::memcmp(saved_context, expected_context, sizeof(saved_context)));
    std::vector<std::uint8_t> image(profile::build_id_offset + 32, 0);
    assert(!profile::matches(image.data(), image.size()));
    std::memcpy(image.data() + profile::build_id_offset, profile::build_id, 32);
    for (const auto site : profile::sites)
        std::memcpy(image.data() + site.offset, &site.instruction, 4);
    assert(profile::matches(image.data(), image.size()));
    for (const auto site : profile::sites) {
        image[site.offset] ^= 1;
        assert(!profile::matches(image.data(), image.size()));
        image[site.offset] ^= 1;
    }
    assert(!profile::matches(image.data(), 256));
    float previous = 0;
    for (int i = 0; i <= 100; ++i) {
        float value = shape_axis(i / 100.f, 0.15f, 1);
        assert(value >= previous); previous = value;
    }

    Settings settings;
    std::uint8_t encoded[kSettingsBytes];
    assert(encode_settings(settings, encoded, sizeof(encoded)));
    Settings decoded;
    decoded.turn_gain = 2;
    assert(decode_settings(encoded, sizeof(encoded), decoded));
    assert(decoded.turn_gain == 1 && !decoded.gyro_enabled);
    encoded[24] ^= 1;
    assert(!decode_settings(encoded, sizeof(encoded), decoded));
    assert(decoded.turn_gain == 1); // Failed reads cannot publish a partial snapshot.
    assert(!decode_settings(encoded, 20, decoded));
    settings.left_deadzone = 0.99f;
    assert(!encode_settings(settings, encoded, sizeof(encoded)));

    GyroIntegrator gyro;
    auto delta = gyro.update({1, 5'000'000, 1, 2}, true, 1, 2);
    assert(close(delta.yaw, 0.005f) && close(delta.pitch, 0.02f));
    delta = gyro.update({1, 5'000'000, 1, 2}, true, 1, 2);
    assert(delta.yaw == 0 && delta.pitch == 0);
    assert(gyro.update({0, 5'000'000, 1, 2}, true, 1, 2).yaw == 0);
    assert(gyro.update({2, 5'000'000, 1, 2}, false, 1, 2).yaw == 0);
    assert(gyro.update({2, 5'000'000, 1, 2}, true, 1, 2).yaw == 0);
    assert(gyro.update({3, 200'000'000, 1, 2}, true, 1, 2).yaw == 0);
    gyro.reset();
    assert(close(gyro.update({1, 5'000'000, 1, 0}, true, 1, 1).yaw, 0.005f));
    // Grouping the same 200 Hz samples into 30 or 60 render frames changes nothing.
    float totals[2]{};
    GyroSample samples[200];
    for (int i = 0; i < 200; ++i) samples[i] = {std::uint64_t(i + 1), 5'000'000, 1, 0};
    for (int group = 0; group < 2; ++group) {
        gyro.reset();
        const int batch = group ? 3 : 7;
        for (int offset = 0; offset < 200; offset += batch) {
            const int count = offset + batch <= 200 ? batch : 200 - offset;
            totals[group] += gyro.update_batch(samples + offset, count, true, 1, 1).yaw;
        }
    }
    assert(close(totals[0], 1) && close(totals[0], totals[1]));
    std::puts("input/settings: all checks passed");
}
