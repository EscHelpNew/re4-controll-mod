// Atmosphere dmnt:cht ABI:
// https://github.com/Atmosphere-NX/Atmosphere/blob/master/docs/components/modules/dmnt.md
#include "game_link.hpp"
#include "re4/profile.hpp"
#include <algorithm>
#include <array>
#include <cstring>

namespace {
struct Extents { u64 base, size; };
struct Metadata {
    u64 pid, title;
    Extents main, heap, alias, space;
    u8 build_id[32];
};
static_assert(sizeof(Metadata) == 112);
bool supported(const Metadata& metadata) {
    return metadata.title == re4::profile::title_id
        && !std::memcmp(metadata.build_id, re4::profile::build_id, 32);
}
Result read_memory(Service* service, u64 address, void* out, size_t size) {
    const struct { u64 address, size; } in = {address, size};
    return serviceDispatchIn(service, 65102, in,
        .buffer_attrs = {SfBufferAttr_HipcMapAlias | SfBufferAttr_Out},
        .buffers = {{out, size}});
}
Result write_memory(Service* service, u64 address, const void* data, size_t size) {
    const struct { u64 address, size; } in = {address, size};
    return serviceDispatchIn(service, 65103, in,
        .buffer_attrs = {SfBufferAttr_HipcMapAlias | SfBufferAttr_In},
        .buffers = {{data, size}});
}
}

void GameLink::initialize() {
    info_ready_ = R_SUCCEEDED(pminfoInitialize());
    if (R_FAILED(smGetService(&service_, "dmnt:cht")))
        message = tr("Debug service unavailable","Сервис отладки недоступен");
}
void GameLink::close() {
    if (serviceIsActive(&service_)) serviceClose(&service_);
    if (info_ready_) pminfoExit();
    info_ready_ = false;
}
bool GameLink::connect() {
    address_ = process_ = 0;
    details = "RE4 process unavailable";
    u64 pid = 0, title = 0;
    if (!info_ready_ || R_FAILED(pmdmntGetApplicationProcessId(&pid))
        || R_FAILED(pminfoGetProgramId(&title, pid)) || title != re4::profile::title_id) {
        message = tr("Start Resident Evil 4","Запусти Resident Evil 4"); return false;
    }
    if (!serviceIsActive(&service_) || R_FAILED(serviceDispatch(&service_, 65003))) {
        details = "RE4 found, dmnt attach unavailable";
        message = tr("Could not connect to game","Не удалось подключиться к игре"); return false;
    }
    Metadata metadata{};
    const auto metadata_result = serviceDispatchOut(&service_, 65002, metadata);
    char diagnostic[256];
    std::snprintf(diagnostic, sizeof(diagnostic), "metadata_rc=%08X pid=%llX title=%016llX main=%llX build=",
        metadata_result, (unsigned long long)metadata.pid, (unsigned long long)metadata.title,
        (unsigned long long)metadata.main.base);
    details = diagnostic;
    for (const auto byte : metadata.build_id) {
        std::snprintf(diagnostic, sizeof(diagnostic), "%02X", byte); details += diagnostic;
    }
    if (R_FAILED(metadata_result) || !supported(metadata)) {
        details += "\nunsupported profile";
        message = tr("Unsupported RE4 build","Эта сборка RE4 пока не поддерживается"); return false;
    }
    std::array<MemoryInfo, 32> mappings;
    std::array<std::uint8_t, 0x1000 + sizeof(re4::SharedControl)> buffer;
    for (u64 offset = 0; offset < 4096;) {
        u64 count = 0;
        const Result rc = serviceDispatchInOut(&service_, 65101, offset, count,
            .buffer_attrs = {SfBufferAttr_HipcMapAlias | SfBufferAttr_Out},
            .buffers = {{mappings.data(), sizeof(mappings)}});
        if (R_FAILED(rc) || !count || count > mappings.size()) break;
        for (u64 i = 0; i < count; ++i) {
            const auto& mapping = mappings[i];
            if ((mapping.type & 0xFF) != MemType_CodeMutable || mapping.perm != Perm_Rw) continue;
            const auto limit = std::min<u64>(mapping.size, 0x20000);
            for (u64 page = 0; page < limit; page += 0x1000) {
                const auto bytes = std::min<u64>(buffer.size(), mapping.size - page);
                if (R_FAILED(read_memory(&service_, mapping.addr + page, buffer.data(), bytes))) break;
                for (size_t j = 0; j + sizeof(re4::SharedControl) <= bytes; j += 8) {
                    re4::SharedControl block{};
                    if (std::memcmp(buffer.data() + j, re4::kSharedMarker, sizeof(re4::kSharedMarker))) continue;
                    std::memcpy(&block, buffer.data() + j, sizeof(block));
                    std::snprintf(diagnostic, sizeof(diagnostic), "\nmodule=%llX abi=%u status=%u frames=%llu raw_lx=%.3f",
                        (unsigned long long)(mapping.addr + page + j), block.abi, block.status,
                        (unsigned long long)block.frames, block.raw_lx);
                    details += diagnostic;
                    std::snprintf(diagnostic, sizeof(diagnostic), "\nsensor_rc=%08X running=%u samples=%u gyro_delta=%.6f,%.6f",
                        block.sensor_result, block.sensor_running, block.sensor_samples, block.gyro_yaw, block.gyro_pitch);
                    details += diagnostic;
                    std::snprintf(diagnostic,sizeof(diagnostic),
                        "\nsettings_startup_source=%u attempts=%u mount_rc=%08X read_rc=%08X",
                        block.settings_source,block.settings_attempts,block.settings_mount_result,block.settings_read_result);
                    details += diagnostic;
                    re4::Settings applied_settings;
                    std::snprintf(diagnostic,sizeof(diagnostic),
                        "\ngraphics_ready=%u sampler=%u changed=%u filter=%u aa=%u submit=%u draw=%u cull=%u far=%.1f/%.1f resolution=%d/%d ssaa_maps=%u",
                        block.graphics_ready,block.graphics_sampler_calls,block.graphics_sampler_overrides,
                        block.graphics_filter_latched,block.graphics_aa_latched,block.graphics_scene_submits,
                        block.graphics_scene_draws,block.graphics_cull_updates,block.graphics_native_far,
                        block.graphics_effective_far,block.graphics_display_index,block.graphics_back_index,
                        block.graphics_resolution_requests);
                    details += diagnostic;
                    if(re4::decode_settings(block.settings,sizeof(block.settings),applied_settings)){
                        std::snprintf(diagnostic,sizeof(diagnostic),
                            "\ngyro_enabled=%u speed=%.2f gain=%.2f,%.2f deadzone_dps=%.2f active=%u fresh=%u restarts=%u rate=%.4f,%.4f style=%u",
                            unsigned(applied_settings.gyro_enabled),applied_settings.gyro_speed,
                            applied_settings.gyro_gain_x,applied_settings.gyro_gain_y,applied_settings.gyro_deadzone_dps,
                            block.gyro_active,block.sensor_fresh_samples,block.sensor_restarts,
                            block.gyro_rate_yaw,block.gyro_rate_pitch,block.pad_style);
                        details+=diagnostic;
                    }
                    std::snprintf(diagnostic, sizeof(diagnostic), "\nmovement_calls=%u factor=%.3f camera_calls=%u aim_calls=%u",
                        block.movement_calls, block.movement_factor, block.camera_calls, block.aim_calls);
                    details += diagnostic;
                    std::snprintf(diagnostic, sizeof(diagnostic), "\nyaw_delta_original=%.6f corrected=%.6f",
                        block.original_yaw_delta, block.corrected_yaw_delta);
                    details += diagnostic;
                    std::snprintf(diagnostic, sizeof(diagnostic), "\nsequence=%u applied=%u gain=%.2f direction_calls=%u speed_calls=%u",
                        block.sequence, block.applied_sequence, block.applied_turn_gain, block.direction_calls, block.speed_calls);
                    details += diagnostic;
                    std::snprintf(diagnostic, sizeof(diagnostic), "\npad_samples=%u pad_id=%u max_raw=%.3f last_factor=%.3f",
                        block.pad_samples, block.last_pad_id, block.maximum_raw_lx, block.last_turn_factor);
                    details += diagnostic;
                    if (block.abi != re4::kSharedAbi || block.status != 1 || !(block.capabilities & re4::kCapabilityTurning)) continue;
                    address_ = mapping.addr + page + j; process_ = metadata.pid;
                    message = tr("Connected to game module","Подключено к игровому модулю");
                    return true;
                }
            }
        }
        offset += count;
    }
    message = tr("Mod not found. Install and restart RE4","Мод не найден. Проверь установку и перезапусти RE4");
    return false;
}
bool GameLink::apply(const re4::Settings& settings) {
    if (!address_ && !connect()) return false;
    Metadata metadata{};
    if (R_FAILED(serviceDispatchOut(&service_, 65002, metadata)) || !supported(metadata)
        || metadata.pid != process_) {
        address_ = process_ = 0; message = tr("Game restarted. Reconnect","Игра перезапущена. Подключись снова"); return false;
    }
    re4::SharedControl block{};
    if (R_FAILED(read_memory(&service_, address_, &block, sizeof(block)))
        || std::memcmp(block.marker, re4::kSharedMarker, 16) || block.abi != re4::kSharedAbi || block.status != 1) {
        address_ = 0; message = tr("Connection to mod lost","Связь с модом потеряна"); return false;
    }
    std::uint8_t bytes[re4::kSettingsBytes];
    if (!re4::encode_settings(settings, bytes, sizeof(bytes))) return false;
    const u32 odd = (block.sequence + 1) | 1u, even = odd + 1;
    const u64 sequence = address_ + offsetof(re4::SharedControl, sequence);
    const bool applied = R_SUCCEEDED(write_memory(&service_, sequence, &odd, sizeof(odd)))
        && R_SUCCEEDED(write_memory(&service_, address_ + offsetof(re4::SharedControl, settings), bytes, sizeof(bytes)))
        && R_SUCCEEDED(write_memory(&service_, sequence, &even, sizeof(even)));
    message = applied ? tr("Settings sent to game","Настройки отправлены в игру")
                      : tr("Could not send settings","Не удалось отправить настройки");
    return applied;
}
