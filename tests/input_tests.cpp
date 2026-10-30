#include "re4/input.hpp"
#include "re4/settings.hpp"
#include "re4/turn.hpp"
#include "re4/profile.hpp"
#include "re4/hook_context.hpp"
#include "re4/shared.hpp"
#include "re4/response.hpp"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <vector>

static bool close(float a, float b) { return std::fabs(a - b) < 0.0001f; }

int main() {
    using namespace re4;
    assert(close(scaled_axis_state(0.2f, 0.24f, 0, 1, 0.25f), 0.2f));
    assert(close(scaled_axis_state(0.2f, 0.24f, 0.5f, 1, 0.25f), 0.22f));
    assert(close(scaled_axis_state(0.96f, 0.99f, 4, 1, 0.25f), 1));
    assert(scaled_axis_state(0, 0.9f, 0, 1, 0.25f) == 0.9f); // Preserve state resets.
    assert(scaled_axis_state(0, 0.1f, 1, 1, 0.25f) == 0.1f);
    assert(camera_axis(0.1f, 0.15f, 1) == 0);
    assert(camera_axis(1, 0.15f, 1) == 63);
    assert(camera_axis(-1, 0.15f, 1) == -63);
    assert(camera_axis(1, 0.15f, 0) == 0);
    assert(camera_axis(0.575f, 0.15f, 1) >= 31 && camera_axis(0.575f, 0.15f, 1) <= 32);
    assert(close(gyro_pitch(0, 0.2f), 0.2f));
    assert(gyro_pitch(0.99f, 0.2f) == 1);
    assert(gyro_pitch(-0.99f, -0.2f) == -1);
    assert(close(gyro_pitch(0.3f, 0.5f), 0.8f)); // Fresh fast motion is valid; timestamps reject backlog.
    assert(close(gyro_yaw(0, 2.f * 10 / 30), 2.f * 10 / 30));
    float yaw30 = 0, yaw60 = 0;
    for (unsigned i = 0; i < 30; ++i) yaw30 = gyro_yaw(yaw30, 2.f * 10 / 30);
    for (unsigned i = 0; i < 60; ++i) yaw60 = gyro_yaw(yaw60, 2.f * 10 / 60);
    assert(close(yaw30, yaw60));
    assert(close(gyro_yaw(3.13f, 0.04f), -3.1131853f));
    assert(gyro_yaw(0.2f, std::numeric_limits<float>::infinity()) == 0.2f);
    SharedControl shared{};
    shared.sequence = 4;
    Settings acknowledged;
    acknowledged.turn_gain = 0;
    assert(acknowledge_settings(shared, acknowledged, 4));
    assert(shared.applied_sequence == 4 && shared.applied_turn_gain == 0);
    shared.sequence = 5;
    acknowledged.turn_gain = 2;
    assert(!acknowledge_settings(shared, acknowledged, 5));
    assert(shared.applied_sequence == 4 && shared.applied_turn_gain == 0);
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
    assert(close(corrected_turn_yaw(0, -0.04f, 0), 0));
    assert(close(corrected_turn_yaw(0, -0.04f, 0.5f), -0.02f));
    assert(close(corrected_turn_yaw(0, 0.04f, 2), 0.08f));
    assert(close(std::fabs(corrected_turn_yaw(3.13f, -3.13f, 0.5f)), 3.14159265f));
    assert(corrected_turn_yaw(0, 3.14159265f, 0) == 3.14159265f); // Preserve quick-turn/script jumps.
    assert(corrected_turn_yaw(0.2f, 0.24f, 1) == 0.24f); // Full-scale leaves original bits untouched.
    assert(yaw_delta(std::numeric_limits<float>::infinity(), 0) == 0);
    assert(yaw_delta(1e30f, 0) == 0);
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
    assert(settings.invert_x && settings.invert_y);
    for (auto member : {&Settings::turn_gain, &Settings::camera_gain, &Settings::aim_gain_x,
                        &Settings::aim_gain_y, &Settings::movement_gain}) {
        Settings too_fast;
        too_fast.*member = 1.01f;
        assert(!valid_settings(too_fast));
    }
    settings.movement_gain = 1;
    std::uint8_t encoded[kSettingsBytes];
    assert(encode_settings(settings, encoded, sizeof(encoded)));
    Settings decoded;
    decoded.turn_gain = 2;
    assert(decode_settings(encoded, sizeof(encoded), decoded));
    assert(decoded.turn_gain == 1 && !decoded.gyro_enabled);
    assert(decoded.movement_gain == 1);
    // Version 1's reserved slot was zero. Preserve existing SD settings and
    // give old files the original full walking speed.
    auto legacy = std::vector<std::uint8_t>(encoded, encoded + kLegacySettingsBytes);
    legacy[4] = 1;
    legacy[8]&=15;legacy[9]=legacy[10]=legacy[11]=0;
    std::memset(legacy.data() + 56, 0, 4);
    std::uint32_t hash = 2166136261u;
    for (unsigned i = 0; i < 60; ++i) { hash ^= legacy[i]; hash *= 16777619u; }
    for (unsigned i = 0; i < 4; ++i) legacy[60 + i] = std::uint8_t(hash >> (8 * i));
    assert(decode_settings(legacy.data(), legacy.size(), decoded));
    assert(decoded.movement_gain == 1 && decoded.turn_gain == 1);
    // Older releases allowed speed-up to4. Migrate each old stick gain down to
    // the original game limit while preserving the remaining configuration.
    legacy.assign(encoded, encoded + kLegacySettingsBytes);
    legacy[4] = 2;
    legacy[8]&=15;legacy[9]=legacy[10]=legacy[11]=0;
    const float old_gain = 3;
    for (auto offset : {16,20,24,28,56}) std::memcpy(legacy.data()+offset,&old_gain,4);
    hash = 2166136261u;
    for (unsigned i=0;i<60;++i) { hash^=legacy[i];hash*=16777619u; }
    for (unsigned i=0;i<4;++i) legacy[60+i]=std::uint8_t(hash>>(8*i));
    assert(decode_settings(legacy.data(),legacy.size(),decoded));
    assert(decoded.turn_gain==1 && decoded.camera_gain==1 && decoded.aim_gain_x==1
           && decoded.aim_gain_y==1 && decoded.movement_gain==1);
    assert(decoded.left_deadzone==settings.left_deadzone);
    settings.movement_gain = std::numeric_limits<float>::quiet_NaN();
    assert(!encode_settings(settings, encoded, sizeof(encoded)));
    settings.movement_gain = 1;
    assert(encode_settings(settings, encoded, sizeof(encoded)));
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
    Settings diagnostic_settings;diagnostic_settings.aim_gain_x=0;diagnostic_settings.right_aim_gain_x=0.5f;
    diagnostic_settings.right_aim_gain_y=0.25f;diagnostic_settings.right_deadzone=0;
    const float raw[]={0,0,1,-1};float processed[4]{};
    process_stick_diagnostics(raw,diagnostic_settings,processed);
    assert(processed[2]==0.5f && processed[3]==-0.25f);
    std::puts("input/settings: all checks passed");
}
