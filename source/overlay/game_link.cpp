// Atmosphere dmnt:cht ABI:
// https://github.com/Atmosphere-NX/Atmosphere/blob/master/docs/components/modules/dmnt.md
#include "game_link.hpp"
#include "re4/profile.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <cmath>
#include <limits>

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
// Optional observations from the authenticated main image's context0 pointer.
// These are cached native renderer bounds, not measured popup clipping bounds.
bool valid_popup_bounds(const float* v,const std::int32_t* s){
    return re4::live_diagnostic_bounds_readable(v,s);
}
bool main_address(const Metadata& metadata,u64 offset,std::size_t size,u64& result){
    constexpr auto maximum=std::numeric_limits<u64>::max();
    if(!metadata.main.base||(metadata.main.base&0xFFF)||metadata.main.size>maximum-metadata.main.base
        ||offset>metadata.main.size||size>metadata.main.size-offset)return false;
    result=metadata.main.base+offset;return true;
}
bool read_main_pointer(Service* service,const Metadata& metadata,u64 offset,u64& pointer){
    u64 address=0;
    if(!main_address(metadata,offset,sizeof(pointer),address)||R_FAILED(read_memory(service,address,&pointer,sizeof(pointer))))return false;
    return pointer>=0x1000&&!(pointer&7);
}
bool read_popup_bounds(Service* service,const Metadata& metadata,
        std::array<float,4>& viewport,std::array<std::int32_t,4>& scissor){
    constexpr u64 pointer_offset=0x1180D40+0x140;
    constexpr u64 cache_offset=0xF80,cache_bytes=0x30;
    constexpr auto maximum=std::numeric_limits<u64>::max();
    if(!metadata.main.base||(metadata.main.base&0xFFF)||metadata.main.size>maximum-metadata.main.base
        ||pointer_offset>metadata.main.size||sizeof(u64)>metadata.main.size-pointer_offset)return false;
    u64 gfx=0;
    if(R_FAILED(read_memory(service,metadata.main.base+pointer_offset,&gfx,sizeof(gfx)))
        ||gfx<0x1000||(gfx&7)||gfx>maximum-cache_offset-cache_bytes)return false;
    std::array<u8,cache_bytes> cache{};
    if(R_FAILED(read_memory(service,gfx+cache_offset,cache.data(),cache.size())))return false;
    std::array<float,4> v{};std::array<std::int32_t,4> s{};
    std::memcpy(v.data(),cache.data(),sizeof(v));std::memcpy(s.data(),cache.data()+0x20,sizeof(s));
    if(!valid_popup_bounds(v.data(),s.data()))return false; // Empty scissor is useful evidence.
    viewport=v;scissor=s;return true;
}
}

#include "live_knife.inc"
#include "live_renderer.inc"

void GameLink::initialize() {
    info_ready_ = R_SUCCEEDED(pminfoInitialize());
    if (R_FAILED(smGetService(&service_, "dmnt:cht")))
        message = tr("Debug service unavailable","Сервис отладки недоступен");
}
void GameLink::initialize_shared(const GameLink& owner) {
    info_ready_ = R_SUCCEEDED(pminfoInitialize());
    service_ = owner.service_;borrowed_ = true;
    if (!serviceIsActive(&service_))
        message = tr("Debug service unavailable","Сервис отладки недоступен");
}
void GameLink::close() {
    appearance={};address_=process_=0;
    popup_diagnostic=0;popup_bounds_valid=false;popup_viewport={};popup_scissor={};
    if (!borrowed_ && serviceIsActive(&service_)) serviceClose(&service_);
    service_ = {};borrowed_ = false;
    if (info_ready_) pminfoExit();
    info_ready_ = false;
}
bool GameLink::connect() {
    appearance={};
    popup_diagnostic=0;popup_bounds_valid=false;popup_viewport={};popup_scissor={};
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
                    std::snprintf(diagnostic,sizeof(diagnostic),
                        "\nmodern: fps=%u reticle=%u move=%u pose=%u status=%u distance=%.3f",
                        block.fps_overrides,block.reticle_draws,block.modern_move_steps,block.modern_pose_steps,
                        block.modern_move_status,block.modern_move_distance);
                    details+=diagnostic;
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
bool GameLink::refresh_appearance(){
    appearance={};
    if(!address_)return false;
    Metadata metadata{};
    if(R_FAILED(serviceDispatchOut(&service_,65002,metadata))||!supported(metadata)||metadata.pid!=process_){
        address_=process_=0;return false;
    }
    u32 before=0,after=0;re4::SharedControl block{};
    const auto generation_address=address_+offsetof(re4::SharedControl,appearance_generation);
    if(R_FAILED(read_memory(&service_,generation_address,&before,sizeof(before)))||(before&1)
       ||R_FAILED(read_memory(&service_,address_,&block,sizeof(block)))
       ||R_FAILED(read_memory(&service_,generation_address,&after,sizeof(after)))
       ||before!=after||block.appearance_generation!=after||(after&1)
       ||std::memcmp(block.marker,re4::kSharedMarker,16)||block.abi!=re4::kSharedAbi||block.status!=1
       ||block.appearance_applied>=8||block.appearance_status>3)return false;
    appearance.applied=static_cast<re4::CharacterAppearance>(block.appearance_applied);
    appearance.status=static_cast<re4::CharacterAppearanceStatus>(block.appearance_status);
    appearance.error=block.appearance_error;appearance.generation=after;appearance.valid=true;return true;
}
bool GameLink::refresh_popup(){
    popup_diagnostic=0;
    popup_bounds_valid=false;popup_viewport={};popup_scissor={};
    if(!address_)return false;
    Metadata metadata{};re4::SharedControl block{};
    if(R_FAILED(serviceDispatchOut(&service_,65002,metadata))||!supported(metadata)||metadata.pid!=process_){
        address_=process_=0;return false;
    }
    if(R_FAILED(read_memory(&service_,address_,&block,sizeof(block)))
       ||std::memcmp(block.marker,re4::kSharedMarker,16)||block.abi!=re4::kSharedAbi||block.status!=1)return false;
    popup_diagnostic=block.graphics_reserved;
    popup_bounds_valid=read_popup_bounds(&service_,metadata,popup_viewport,popup_scissor);
    return true;
}
bool GameLink::read_diagnostic_snapshot(re4::LiveDiagnosticSnapshot& out){
    out={};using S=re4::LiveDiagnosticStatus;
    if(!info_ready_||!serviceIsActive(&service_)){out.status=S::ServiceUnavailable;return false;}
    u64 pid=0,title=0;
    if(R_FAILED(pmdmntGetApplicationProcessId(&pid))||!pid
        ||R_FAILED(pminfoGetProgramId(&title,pid))||title!=re4::profile::title_id){
        address_=process_=0;out.status=S::NoGame;return false;
    }
    out.process_found=true;
    if(!address_||process_!=pid){address_=process_=0;connect();} // One bounded scan at most per request.
    Metadata metadata{};
    if(R_FAILED(serviceDispatchOut(&service_,65002,metadata))){out.status=S::ReadFailed;address_=process_=0;return false;}
    if(!supported(metadata)){out.status=S::UnsupportedBuild;address_=process_=0;return false;}
    out.supported=true;
    if(metadata.pid!=pid){out.status=S::ReadFailed;address_=process_=0;return false;}
    if(!address_||process_!=pid){out.status=S::ModuleMissing;return false;}
    if(address_>std::numeric_limits<u64>::max()-sizeof(re4::SharedControl)){out.status=S::ReadFailed;address_=process_=0;return false;}
    const std::size_t offsets[]{offsetof(re4::SharedControl,sequence),offsetof(re4::SharedControl,appearance_generation),
        offsetof(re4::SharedControl,popup_capture_sequence)};
    u32 before[3]{};bool readable[3]{};
    for(unsigned i=0;i<3;++i)readable[i]=R_SUCCEEDED(read_memory(&service_,address_+offsets[i],&before[i],sizeof(before[i])));
    re4::SharedControl block{};
    if(R_FAILED(read_memory(&service_,address_,&block,sizeof(block)))
        ||std::memcmp(block.marker,re4::kSharedMarker,16)||block.abi!=re4::kSharedAbi||block.status!=1
        ||!(block.capabilities&re4::kCapabilityTurning)){
        out.status=S::ReadFailed;address_=process_=0;return false;
    }
    const u32 captured[]{block.sequence,block.appearance_generation,block.popup_capture_sequence};
    bool stable[3]{};
    for(unsigned i=0;i<3;++i){
        u32 after=0;
        stable[i]=readable[i]&&!(before[i]&1)
            &&R_SUCCEEDED(read_memory(&service_,address_+offsets[i],&after,sizeof(after)))
            &&before[i]==after&&captured[i]==after;
    }
    out.status=S::Connected;out.connected=true;out.shared=block;
    out.settings_valid=stable[0]&&re4::decode_settings(block.settings,sizeof(block.settings),out.settings);
    out.appearance_valid=stable[1]&&block.appearance_applied<8&&block.appearance_status<=3;
    out.popup_capture_stable=stable[2];
    out.popup_capture_bounds_valid=stable[2]&&(block.popup_capture_status==1||block.popup_capture_status==5)
        &&valid_popup_bounds(block.popup_viewport,block.popup_scissor);
    out.popup_cached_valid=read_popup_bounds(&service_,metadata,out.popup_cached_viewport,out.popup_cached_scissor);
    u64 player=0;
    if(read_main_pointer(&service_,metadata,re4::profile::player_slot,player)
        &&player<=std::numeric_limits<u64>::max()-0x11C){
        std::array<u8,8> bytes{};
        if(R_SUCCEEDED(read_memory(&service_,player+0x114,bytes.data(),bytes.size()))){
            std::copy_n(bytes.data(),4,out.player_routine.data());out.player_type=bytes[4];out.player_bones=bytes[6];out.player_valid=true;
        }
    }
    u64 state=0;
    if(read_main_pointer(&service_,metadata,0xB1AE08,state)&&state<=std::numeric_limits<u64>::max()-0x14C){
        std::array<u8,0x74> matrices{};
        if(R_SUCCEEDED(read_memory(&service_,state+0xD8,matrices.data(),matrices.size()))){
            std::memcpy(out.camera_view.data(),matrices.data(),48);std::memcpy(out.camera_projection.data(),matrices.data()+0x34,64);
            bool finite=true;double view_basis=0,projection=0;
            for(unsigned i=0;i<12;++i){const float value=out.camera_view[i];finite&=std::isfinite(value)&&std::fabs(value)<=1e9;
                if(i%4!=3)view_basis+=std::fabs(double(value));}
            for(float value:out.camera_projection){finite&=std::isfinite(value)&&std::fabs(value)<=1e6;projection+=std::fabs(double(value));}
            out.camera_valid=finite&&view_basis>0&&projection>0;
            if(!out.camera_valid){out.camera_view={};out.camera_projection={};}
        }
    }
    read_live_knife(&service_,metadata,out.knife);
    read_live_hud_renderer(&service_,metadata,out.hud_cached);
    return true;
}
