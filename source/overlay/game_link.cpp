// Atmosphere dmnt:cht ABI:
// https://github.com/Atmosphere-NX/Atmosphere/blob/master/docs/components/modules/dmnt.md
#include "game_link.hpp"
#include "re4/profile.hpp"
#include "re4/health_profile.hpp"
#include "re4/enemy_health_reference.hpp"
#include "re4/enemy_health_families.hpp"
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
unsigned observation_module_patch(const char (&text)[16]){
    if(std::memcmp(text,"0.28.",5))return 0;
    unsigned value=0;std::size_t i=5;
    if(text[i]<'1'||text[i]>'9')return 0;
    for(;i<sizeof(text)&&text[i];++i){
        if(text[i]<'0'||text[i]>'9'||value>99)return 0;
        value=value*10+unsigned(text[i]-'0');
    }
    return i<sizeof(text)&&value>=13&&value<=30?value:0;
}
bool observation_candidate(const re4::SharedControl& block){
    return !std::memcmp(block.marker,re4::kSharedMarker,16)&&block.abi==re4::kSharedAbi
        &&(block.capabilities&re4::kCapabilityTurning)&&block.status<=2&&observation_module_patch(block.module_version);
}
void copy_module_observation(re4::ModuleObservation& out,const re4::SharedControl& block){
    out.found=true;out.abi=block.abi;out.status=block.status;out.capabilities=block.capabilities;out.frames=block.frames;
    std::copy_n(block.module_version,16,out.version.data());
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
// Historical18 added requirements only: current19 class authentication excludes
// mutable relative slots. This explicit observation never changes module gates.
bool read_added_profile(Service* service,const Metadata& metadata,re4::LiveDiagnosticSnapshot& snapshot){
    auto& out=snapshot.profile_observation;const auto& module=snapshot.module_observation;
    if(!module.found)return true;
    if(!module.stable||(std::memcmp(module.version.data(),"0.28.18",8)&&std::memcmp(module.version.data(),"0.28.19",8)&&std::memcmp(module.version.data(),"0.28.20",8)&&std::memcmp(module.version.data(),"0.28.21",8)&&std::memcmp(module.version.data(),"0.28.22",8)&&std::memcmp(module.version.data(),"0.28.23",8)&&std::memcmp(module.version.data(),"0.28.24",8)&&std::memcmp(module.version.data(),"0.28.25",8)&&std::memcmp(module.version.data(),"0.28.26",8)&&std::memcmp(module.version.data(),"0.28.27",8)&&std::memcmp(module.version.data(),"0.28.28",8)&&std::memcmp(module.version.data(),"0.28.29",8)&&std::memcmp(module.version.data(),"0.28.30",8))){
        Metadata after{};
        const bool same=R_SUCCEEDED(serviceDispatchOut(service,65002,after))&&supported(after)&&after.pid==metadata.pid
            &&after.main.base==metadata.main.base&&after.main.size==metadata.main.size;
        if(!same)snapshot.module_observation={};
        return same;
    }
    out.attempted=true;
    std::array<u32,128> pages{};unsigned page_count=0;bool complete=true;
    const auto record=[&](bool pointer,u32 rva,u32 expected,bool in_bounds,bool readable,u64 actual){
        bool match=false;re4::ProfileMismatch value{};value.pointer=pointer;value.rva=rva;value.expected=expected;
        value.in_bounds=in_bounds;value.readable=readable;
        if(readable&&pointer){
            value.observed_null=actual==0;
            value.observed_in_main=actual>=metadata.main.base&&actual-metadata.main.base<metadata.main.size
                &&actual-metadata.main.base<=UINT32_MAX;
            if(value.observed_in_main)value.observed=u32(actual-metadata.main.base);
            match=expected<metadata.main.size&&actual==metadata.main.base+expected;
        }else if(readable){value.observed=u32(actual);match=value.observed==expected;}
        if(match)return;
        ++out.mismatch_count;
        if(out.record_count<out.mismatches.size())out.mismatches[out.record_count++]=value;
    };
    const auto guard_bounds=[&](u32 rva,unsigned size){u64 checked=0;return !(rva&(size-1))&&main_address(metadata,rva,size,checked);};
    const auto add_page=[&](u32 rva,u32 expected,bool pointer){
        if(!guard_bounds(rva,pointer?8:4)){complete=false;record(pointer,rva,expected,false,false,0);return;}
        const u32 page=rva&~u32(0xFFF);
        if(std::find(pages.begin(),pages.begin()+page_count,page)!=pages.begin()+page_count)return;
        if(page_count==pages.size()){out.budget_exceeded=true;complete=false;return;}
        pages[page_count++]=page;
    };
    for(const auto& guard:re4::health_profile::kAddedWords)add_page(guard.rva,guard.expected,false);
    for(const auto& guard:re4::health_profile::kAddedPointers)add_page(guard.rva,guard.expected_rva,true);
    std::sort(pages.begin(),pages.begin()+page_count);
    std::array<u8,0x1000> buffer{};
    for(unsigned page_index=0;page_index<page_count;++page_index){
        const u32 page=pages[page_index];const auto bytes=std::min<u64>(buffer.size(),metadata.main.size-page);
        u64 remote=0;const bool bounded=main_address(metadata,page,bytes,remote);
        ++out.pages;++out.read_calls;out.bytes+=u32(bytes);
        const bool readable=bounded&&R_SUCCEEDED(read_memory(service,remote,buffer.data(),std::size_t(bytes)));
        if(!readable){++out.unread_pages;complete=false;}
        for(const auto& guard:re4::health_profile::kAddedWords){
            if((guard.rva&~u32(0xFFF))!=page||!guard_bounds(guard.rva,4))continue;
            u32 actual=0;if(readable){std::memcpy(&actual,buffer.data()+guard.rva-page,4);++out.words_checked;}
            record(false,guard.rva,guard.expected,true,readable,actual);
        }
        for(const auto& guard:re4::health_profile::kAddedPointers){
            if((guard.rva&~u32(0xFFF))!=page||!guard_bounds(guard.rva,8))continue;
            u64 actual=0;if(readable){std::memcpy(&actual,buffer.data()+guard.rva-page,8);++out.pointers_checked;}
            record(true,guard.rva,guard.expected_rva,true,readable,actual);
        }
    }
    Metadata after{};
    out.stable=R_SUCCEEDED(serviceDispatchOut(service,65002,after))&&supported(after)&&after.pid==metadata.pid
        &&after.main.base==metadata.main.base&&after.main.size==metadata.main.size;
    out.complete=complete&&out.stable;
    if(!out.stable){out.record_count=out.mismatch_count=0;snapshot.module_observation={};}
    return out.stable;
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
    appearance={};money={};address_=process_=0;
    popup_diagnostic=0;popup_bounds_valid=false;popup_viewport={};popup_scissor={};
    if (!borrowed_ && serviceIsActive(&service_)) serviceClose(&service_);
    service_ = {};borrowed_ = false;
    if (info_ready_) pminfoExit();
    info_ready_ = false;
}
bool GameLink::connect(){return connect_observing(nullptr);}
bool GameLink::connect_observing(re4::ModuleObservation* observation) {
    observation_identity_={};if(observation)*observation={};
    appearance={};money={};
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
    if (R_FAILED(metadata_result) || !supported(metadata)||(observation&&metadata.pid!=pid)) {
        details += "\nunsupported profile";
        message = tr("Unsupported RE4 build","Эта сборка RE4 пока не поддерживается"); return false;
    }
    std::array<MemoryInfo, 32> mappings;
    std::array<std::uint8_t, 0x1000 + sizeof(re4::SharedControl)> buffer;
    bool page_readable=false,incomplete=false;
    const auto scan_page=[&](const MemoryInfo& mapping,u64 page){
        page_readable=false;
        if(observation&&(observation->scan_reads>=127||observation->scan_bytes>=512*1024-sizeof(re4::SharedControl))){observation->scan_limit=true;return false;}
        auto bytes = std::min<u64>(buffer.size(), mapping.size - page);
        if(observation){const auto desired=bytes;bytes=std::min<u64>(bytes,512*1024-sizeof(re4::SharedControl)-observation->scan_bytes);
            if(bytes<desired)incomplete=true;
            ++observation->scan_reads;observation->scan_bytes+=unsigned(bytes);}
        if (R_FAILED(read_memory(&service_, mapping.addr + page, buffer.data(), bytes))){if(observation)incomplete=true;return false;}
        page_readable=true;
        for (size_t j = 0; j + sizeof(re4::SharedControl) <= bytes; j += 8) {
            re4::SharedControl block{};
            if (std::memcmp(buffer.data() + j, re4::kSharedMarker, sizeof(re4::kSharedMarker))) continue;
            std::memcpy(&block, buffer.data() + j, sizeof(block));
            if(observation&&!observation->found&&observation_candidate(block)){
                copy_module_observation(*observation,block);
                observation_identity_={mapping.addr+page+j,metadata.pid,metadata.main.base,metadata.main.size};
            }
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
        return false;
    };
    // Diagnostics must reach every module's first writable page before spending
    // its bounded budget inside an earlier, unrelated large mapping.
    std::array<MemoryInfo,128> heads{};std::array<bool,128> fallback{};
    unsigned head_count=0;u64 offset=0;
    for (;offset<4096&&(!observation||!observation->scan_limit);) {
        u64 count=0;
        const Result rc=serviceDispatchInOut(&service_,65101,offset,count,
            .buffer_attrs={SfBufferAttr_HipcMapAlias|SfBufferAttr_Out},
            .buffers={{mappings.data(),sizeof(mappings)}});
        if(R_FAILED(rc)||count>mappings.size()){if(observation)incomplete=true;break;}
        if(!count)break;
        for(u64 i=0;i<count&&(!observation||!observation->scan_limit);++i){
            const auto& mapping=mappings[i];
            if((mapping.type&0xFF)!=MemType_CodeMutable||mapping.perm!=Perm_Rw||!mapping.size)continue;
            if(observation){
                if(mapping.addr>std::numeric_limits<u64>::max()-mapping.size){incomplete=true;continue;}
                if(head_count==heads.size()){observation->scan_limit=true;break;}
                const auto index=head_count++;heads[index]=mapping;
                if(scan_page(mapping,0))return true;
                fallback[index]=page_readable;
            }else{
                const auto limit=std::min<u64>(mapping.size,0x20000);
                for(u64 page=0;page<limit;page+=0x1000){
                    if(scan_page(mapping,page))return true;
                    if(!page_readable)break;
                }
            }
        }
        offset+=count;
    }
    if(observation&&offset>=4096)incomplete=true;
    if(observation)for(u64 page=0x1000;page<0x20000&&!observation->scan_limit;page+=0x1000){
        for(unsigned i=0;i<head_count&&!observation->scan_limit;++i){
            if(!fallback[i]||page>=std::min<u64>(heads[i].size,0x20000))continue;
            if(scan_page(heads[i],page))return true;
            if(!page_readable)fallback[i]=false;
        }
    }
    if(observation&&incomplete)observation->scan_limit=true;
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
bool GameLink::refresh_money(){
    money={};if(!address_)return false;
    Metadata metadata{};
    if(R_FAILED(serviceDispatchOut(&service_,65002,metadata))||!supported(metadata)||metadata.pid!=process_){
        address_=process_=0;return false;
    }
    u32 before=0,after=0;re4::SharedControl block{};
    const auto ack=address_+offsetof(re4::SharedControl,money_ack);
    if(R_FAILED(read_memory(&service_,ack,&before,4))
        ||R_FAILED(read_memory(&service_,address_,&block,sizeof(block)))
        ||R_FAILED(read_memory(&service_,ack,&after,4))||before!=after||block.money_ack!=after
        ||std::memcmp(block.marker,re4::kSharedMarker,16)||block.abi!=re4::kSharedAbi||block.status!=1
        ||block.money_result>unsigned(re4::MoneyResult::Capacity)
        ||block.money_availability>unsigned(re4::MoneyAvailability::Capacity))return false;
    money={block.money_request,block.money_ack,static_cast<re4::MoneyResult>(block.money_result),
        static_cast<re4::MoneyAvailability>(block.money_availability)};return true;
}
bool GameLink::request_money(){
    if(!refresh_money()||money.pending()||money.availability!=re4::MoneyAvailability::Ready)return false;
    const u32 request=money.request+1;
    if(R_FAILED(write_memory(&service_,address_+offsetof(re4::SharedControl,money_request),&request,sizeof(request))))return false;
    money.request=request;return true;
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
namespace {
// Only the release line that introduced this optional wire is authenticated.
bool health_capture_module_version(const char (&text)[16]){
    constexpr char prefix[]="0.28.";
    if(std::memcmp(text,prefix,5))return false;
    unsigned patch=0;std::size_t i=5;
    if(text[i]<'1'||text[i]>'9')return false;
    for(;i<sizeof(text)&&text[i];++i){
        if(text[i]<'0'||text[i]>'9'||patch>(UINT32_MAX-unsigned(text[i]-'0'))/10)return false;
        patch=patch*10+unsigned(text[i]-'0');
    }
    return i<sizeof(text)&&patch>=17;
}
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
    if(!address_||process_!=pid){address_=process_=0;connect_observing(&out.module_observation);} // Explicit bounded discovery only.
    Metadata metadata{};
    if(R_FAILED(serviceDispatchOut(&service_,65002,metadata))){out.module_observation={};out.status=S::ReadFailed;address_=process_=0;return false;}
    if(!supported(metadata)){out.module_observation={};out.status=S::UnsupportedBuild;address_=process_=0;return false;}
    out.supported=true;
    if(metadata.pid!=pid){out.module_observation={};out.status=S::ReadFailed;address_=process_=0;return false;}
    if(out.module_observation.found){
        if(observation_identity_[1]!=metadata.pid||observation_identity_[2]!=metadata.main.base||observation_identity_[3]!=metadata.main.size){
            out.module_observation={};out.status=S::ReadFailed;address_=process_=0;return false;
        }
        re4::SharedControl observed{};
        ++out.module_observation.scan_reads;out.module_observation.scan_bytes+=sizeof(observed);
        if(R_SUCCEEDED(read_memory(&service_,observation_identity_[0],&observed,sizeof(observed)))&&observation_candidate(observed)
            &&observed.status==out.module_observation.status&&observed.capabilities==out.module_observation.capabilities
            &&!std::memcmp(observed.module_version,out.module_observation.version.data(),16)){
            copy_module_observation(out.module_observation,observed);out.module_observation.stable=true;
        }
    }
    if(!address_||process_!=pid){
        if(!read_added_profile(&service_,metadata,out)){out.status=S::ReadFailed;address_=process_=0;return false;}
        out.status=out.module_observation.scan_limit?S::DiscoveryLimited:S::ModuleMissing;return false;
    }
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
    if(!out.module_observation.found&&observation_candidate(block)){copy_module_observation(out.module_observation,block);out.module_observation.stable=true;}
    if(!read_added_profile(&service_,metadata,out)){out.status=S::ReadFailed;out.connected=false;address_=process_=0;return false;}
    (void)read_health_report(out.health_reference); // Existing explicit SNAPSHOT/Diagnostics path only.
    // Optional diagnostic sidecar: old modules and torn observations remain
    // valid core snapshots. This path is never used by ordinary UI refreshes.
    if(address_<=std::numeric_limits<u64>::max()-re4::kEmitterDiagnosticsOffset-re4::kEmitterDiagnosticsBytes){
        const auto sidecar=address_+re4::kEmitterDiagnosticsOffset;
        const auto sequence=sidecar+offsetof(re4::EmitterDiagnosticsWire,sequence);
        u32 first=0,last=0;re4::EmitterDiagnosticsWire emitter{};
        if(R_SUCCEEDED(read_memory(&service_,sequence,&first,sizeof(first)))&&!(first&1)
            &&R_SUCCEEDED(read_memory(&service_,sidecar,&emitter,sizeof(emitter)))
            &&R_SUCCEEDED(read_memory(&service_,sequence,&last,sizeof(last)))
            &&first==last&&emitter.sequence==last&&re4::emitter_diagnostics_header_valid(emitter)){
            out.emitters=emitter;out.emitters_valid=true;
        }
    }
    if(health_capture_module_version(block.module_version)
        &&address_<=std::numeric_limits<u64>::max()-re4::kHealthCaptureOffset-re4::kHealthCaptureBytes){
        const auto sidecar=address_+re4::kHealthCaptureOffset;
        const auto sequence=sidecar+offsetof(re4::HealthCaptureWire,sequence);
        for(unsigned attempt=0;attempt<2;++attempt){
            u32 first=0,last=0;re4::HealthCaptureWire capture{};
            if(R_FAILED(read_memory(&service_,sequence,&first,sizeof(first))))break;
            if(first&1)continue;
            if(R_FAILED(read_memory(&service_,sidecar,&capture,sizeof(capture)))
                ||R_FAILED(read_memory(&service_,sequence,&last,sizeof(last))))break;
            if(first!=last||capture.sequence!=last)continue;
            if(!re4::health_capture_header_valid(capture)||capture.valid!=1)break;
            out.health_last_capture=capture;out.health_last_capture_valid=true;break;
        }
    }
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

namespace {
struct HealthReader {
    Service* service;unsigned count=0;bool failed=false;
    template<class T> bool field(u64 base,u64 offset,T& out,bool required=true){
        out={};constexpr auto maximum=std::numeric_limits<u64>::max();
        if(!base||offset>maximum-base||sizeof(T)>maximum-(base+offset)||count>=96){failed|=required;return false;}
        ++count;
        if(R_FAILED(read_memory(service,base+offset,&out,sizeof(out)))){failed|=required;return false;}return true;
    }
};
struct HealthNative {
    std::array<u64,11> pointers{};
    std::array<u32,21> values{};
    u32 mask=0;u8 globals=0;bool target=false;
};
u32 health_rva(const Metadata& metadata,u64 value,std::size_t bytes=1){
    if(value<metadata.main.base||value-metadata.main.base>UINT32_MAX)return UINT32_MAX;
    u64 checked=0;return main_address(metadata,value-metadata.main.base,bytes,checked)?u32(value-metadata.main.base):UINT32_MAX;
}
HealthNative read_health_native(HealthReader& reader,const Metadata& metadata){
    HealthNative n;auto& p=n.pointers;auto& v=n.values;v[5]=v[17]=v[18]=v[19]=UINT32_MAX;
    u64 location=0;
    const auto main_field=[&](u64 offset,auto& value,bool required=true){return main_address(metadata,offset,sizeof(value),location)&&reader.field(location,0,value,required);};
    bool roots=main_field(re4::profile::player_slot,p[0]);roots=main_field(0xB1AE08,p[1])&&roots;
    u8 control=0,owner=0;u64 held=0;
    bool context=roots&&p[0]&&p[1];
    if(p[0]){
        context=reader.field(p[0],0x938,p[2])&&context;
        context=reader.field(p[0],0x114,v[0])&&context;
    }
    if(p[2])context=reader.field(p[2],0x38,p[3])&&context;else context=false;
    if(p[3])context=reader.field(p[3],0x418,p[5])&&context;else context=false;
    context=main_field(0x1131718+0x48,control)&&context;context=main_field(0x1131718+0x54,owner)&&context;
    context=main_field(re4::profile::input_flags,held)&&context;v[1]=u32(control)|(u32(owner)<<8);v[2]=u32(held);
    if(context)n.mask|=re4::HealthContext;
    if(p[1]){
        u8 index=0,mode=0,rank=0;
        if(reader.field(p[1],0x5034,index,false))n.globals|=1;
        if(reader.field(p[1],0x84F0,mode,false))n.globals|=2;
        if(reader.field(p[1],0x500C,rank,false))n.globals|=4;
        v[14]=mode;v[15]=rank;v[16]=index;
        if(n.globals==7)n.mask|=re4::HealthGlobals;
    }
    struct Pool{u64 base=0;u32 count=0,stride=0;} pool;static_assert(sizeof(Pool)==16);
    if(main_field(0xAE6EF8+8,pool)){p[4]=pool.base;v[3]=pool.count;v[4]=pool.stride;n.mask|=re4::HealthPool;}
    if(!pool.base||pool.count<2||pool.count>256||pool.stride!=0x10C8||p[5]<=pool.base)return n;
    const auto delta=p[5]-pool.base;
    if(delta%pool.stride||delta/pool.stride>=pool.count)return n;
    v[5]=u32(delta/pool.stride);n.target=true;
    u16 identity=0;
    bool id=reader.field(p[5],0,p[6]);id=reader.field(p[5],8,v[6])&&id;
    id=reader.field(p[5],0x118,identity)&&id;v[7]=identity;
    v[17]=health_rva(metadata,p[6],0x30);
    if(v[17]!=UINT32_MAX){id=reader.field(p[6],0x28,p[7])&&id;v[18]=health_rva(metadata,p[7],4);}
    if(id)n.mask|=re4::HealthIdentity;
    if(reader.field(p[5],0x3C2,v[8]))n.mask|=re4::HealthValues;
    const auto* descriptor=re4::enemy_health_reference_class(v[17],v[18]);
    const bool common=descriptor&&descriptor->family==re4::EnemyHealthFamily::Common;
    const bool needs_coefficient=descriptor&&re4::enemy_health_family_needs_coefficient(descriptor->family);
    u8 byte=0;(void)reader.field(p[5],0x48D,byte,false);v[9]=byte;
    bool body=reader.field(p[5],0xB8C,byte,common);v[10]=byte;
    body=reader.field(p[5],0xB4C,byte,common)&&body;v[11]=byte;
    body=reader.field(p[5],0x944,p[9],common)&&body;body=reader.field(p[5],0x974,p[10],common)&&body;
    v[12]=p[9]!=0;v[13]=p[10]!=0;if(body)n.mask|=re4::HealthBody;
    const auto type=u8(identity);
    p[8]=metadata.main.base+0xAF055C;
    bool table=true;
    if(type>=0x23&&type<=0x4E)table=main_field(0xAF1440+u64(type-0x23)*8,p[8],needs_coefficient);
    v[19]=health_rva(metadata,p[8],0xC4);
    const auto base=metadata.main.base+0xAF055C;
    if(table&&p[8]>=base&&(p[8]-base)%0xC4==0&&(p[8]-base)/0xC4<12&&v[16]<=0x30
        &&reader.field(p[8],u64(v[16])*4,v[20],needs_coefficient))n.mask|=re4::HealthCoefficient;
    return n;
}
}
bool GameLink::read_health_report(re4::HealthReport& out){
    out={};using S=re4::HealthReportStatus;using G=re4::HealthReportGate;
    const auto reject=[&](G gate){out.rejected_mask|=re4::health_report_gate_bit(gate);if(out.first_failed_gate==G::None)out.first_failed_gate=gate;};
    if(!info_ready_||!serviceIsActive(&service_))return false;
    u64 pid=0,title=0;
    if(R_FAILED(pmdmntGetApplicationProcessId(&pid))||!pid||R_FAILED(pminfoGetProgramId(&title,pid))||title!=re4::profile::title_id){out.status=S::NoGame;return false;}
    if(!address_||process_!=pid)return false;
    Metadata before{},after{};
    if(R_FAILED(serviceDispatchOut(&service_,65002,before))){out.status=S::ReadFailed;return false;}
    if(!supported(before)){out.status=S::UnsupportedBuild;return false;}
    if(before.pid!=pid){out.status=S::Unstable;reject(G::Unstable);return false;}
    u64 checked=0;if(!main_address(before,0x124EC50,sizeof(u64),checked)){out.status=S::ReadFailed;return false;}
    HealthReader reader{&service_};
    u32 first=0,last=0,ack=0;re4::SharedControl shared{};re4::Settings settings;
    bool settings_read=reader.field(address_,offsetof(re4::SharedControl,sequence),first);
    settings_read=reader.field(address_,0,shared)&&settings_read;
    settings_read=reader.field(address_,offsetof(re4::SharedControl,sequence),last)&&settings_read;
    settings_read=reader.field(address_,offsetof(re4::SharedControl,applied_sequence),ack)&&settings_read;
    out.settings_sequence=shared.sequence;out.applied_sequence=shared.applied_sequence;out.settings_source=shared.settings_source;
    out.settings_valid=settings_read&&!(first&1)&&first==last&&last==shared.sequence&&ack==shared.applied_sequence
        &&!std::memcmp(shared.marker,re4::kSharedMarker,16)&&shared.abi==re4::kSharedAbi&&shared.status==1
        &&re4::decode_settings(shared.settings,sizeof(shared.settings),settings);
    if(out.settings_valid){out.read_mask|=re4::HealthSettings;out.bars_on=settings.enemy_health_bars;out.numbers_on=settings.enemy_health_numbers;out.settings_applied=ack==last;}
    if(!out.settings_valid)reject(G::SettingsRead);
    else if(!out.settings_applied)reject(G::SettingsPending);
    else {if(!out.bars_on)reject(G::BarsOff);if(!out.numbers_on)reject(G::NumbersOff);}
    const auto a=read_health_native(reader,before),b=read_health_native(reader,before);
    out.ipc_reads=reader.count+4;out.read_mask|=a.mask;
    const auto& v=a.values;out.routine_pack=v[0];out.controller_pack=v[1];out.input_held_low=v[2];
    out.pool_count=v[3];out.pool_stride=v[4];out.target_slot=v[5];out.flags=v[6];out.type=u8(v[7]);out.subtype=u8(v[7]>>8);
    out.current=std::int16_t(v[8]&0xFFFF);out.maximum=std::int16_t(v[8]>>16);out.part_flags=u8(v[9]);out.body_status=u8(v[10]);out.phase=u8(v[11]);
    out.link944=v[12]!=0;out.link974=v[13]!=0;out.mode=u8(v[14]);out.rank=u8(v[15]);out.weapon_index=u8(v[16]);
    out.vptr_rva=v[17];out.update_rva=v[18];out.table_rva=v[19];out.vptr_in_main=v[17]!=UINT32_MAX;out.update_in_main=v[18]!=UINT32_MAX;
    std::memcpy(&out.coefficient,&v[20],sizeof(out.coefficient));
    out.observed_consistent=a.pointers==b.pointers&&a.values==b.values&&a.mask==b.mask&&a.globals==b.globals&&a.target==b.target;
    if(R_FAILED(serviceDispatchOut(&service_,65002,after))||!supported(after)||after.pid!=before.pid
        ||after.main.base!=before.main.base||after.main.size!=before.main.size)out.observed_consistent=false;
    if(!(a.mask&re4::HealthContext))reject(G::Context);
    if(!(a.mask&re4::HealthPool)||out.pool_count<2||out.pool_count>256||out.pool_stride!=0x10C8)reject(G::Pool);
    if(!a.target)reject(G::Target);
    bool required_globals_failed=false;
    if(a.target){
        using F=re4::EnemyHealthFamily;
        const auto* descriptor=re4::enemy_health_reference_class(out.vptr_rva,out.update_rva);
        const auto family=descriptor?descriptor->family:F::Unknown;
        const bool common=family==F::Common,needs_coefficient=re4::enemy_health_family_needs_coefficient(family);
        const bool needs_rank=re4::enemy_health_family_needs_rank(family);
        const auto required_globals=1u|(needs_coefficient?2u:0u)|(needs_rank?4u:0u);
        required_globals_failed=(a.globals&required_globals)!=required_globals;
        if(!(a.mask&re4::HealthValues)||out.current<=0||out.maximum<=0)reject(G::Health);
        if(!(a.globals&1)||!re4::enemy_health_family_weapon_allowed(family,out.weapon_index))reject(G::Weapon);
        if((needs_rank&&(!(a.globals&4)||out.rank>10))
            ||(needs_coefficient&&(!(a.globals&2)||out.mode<2)))reject(G::ModeRank);
        if(family==F::LegacyBoss)reject(G::UnsupportedFamily); // Retained producer record covers resolved native owners.
        if(!(a.mask&re4::HealthIdentity)||!descriptor||(out.flags&0x201)!=1
            ||(out.flags&0x10000000)||out.type<0x10||out.type>0x4F)reject(G::Class);
        if(!re4::enemy_health_reference_subtype_allowed(out.subtype))reject(G::Subtype);
        if(common&&(!(a.mask&re4::HealthBody)||out.body_status>2))reject(G::BodyStatus);
        if(needs_coefficient&&(!(a.mask&re4::HealthCoefficient)||out.coefficient<0||out.coefficient>=9999))reject(G::Coefficient);
        const auto native_rejections=out.rejected_mask&~(re4::health_report_gate_bit(G::SettingsRead)|re4::health_report_gate_bit(G::SettingsPending)|re4::health_report_gate_bit(G::BarsOff)|re4::health_report_gate_bit(G::NumbersOff));
        if(!native_rejections)out.predicted_unit=re4::enemy_health_family_damage(family,out.weapon_index,out.coefficient,out.type,
            out.mode,out.rank,out.body_status,out.link944||out.link974,out.phase,out.subtype,unsigned(out.maximum));
        if(!native_rejections&&!out.predicted_unit)reject(G::Prediction);
    }
    if(!out.observed_consistent){out.status=S::Unstable;out.predicted_unit=0;reject(G::Unstable);return false;}
    if(reader.failed||required_globals_failed){out.status=S::ReadFailed;return false;}
    if(!a.target){out.status=S::NoTarget;return false;}
    out.status=S::Collected;return true;
}
