#include "game_link.hpp"
#include "re5/profile.hpp"
#include "re5/native_input.hpp"
#include "re5/runtime_probe.hpp"
#include "re4/version.hpp"
#include <array>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <limits>
#include <tuple>
namespace {
struct Re5Extents {u64 base,size;};
struct Re5Metadata {u64 pid,title;Re5Extents main,heap,alias,space;u8 build_id[32];};
static_assert(sizeof(Re5Metadata)==112);
void re5_build_text(const u8* build,char* out){
    constexpr char digits[]="0123456789ABCDEF";
    for(unsigned i=0;i<32;++i){out[i*2]=digits[build[i]>>4];out[i*2+1]=digits[build[i]&15];}
    out[64]=0;
}
using Re5Build=re5::profile::Build;
Re5Build re5_build(const u8* id){
    for(auto build:{Re5Build::Latest,Re5Build::Legacy})
        if(!std::memcmp(id,re5::profile::view(build).id->data(),32))return build;
    return Re5Build::None;
}
bool re5_zero_id(const u8* id){return std::all_of(id,id+32,[](u8 byte){return byte==0;});}
bool re5_same_process(u64 expected){
    u64 pid=0,title=0;
    return R_SUCCEEDED(pmdmntGetApplicationProcessId(&pid))&&pid==expected
        &&R_SUCCEEDED(pminfoGetProgramId(&title,pid))&&title==re5::profile::title_id;
}
Result re5_read(Service* s,u64 address,void* out,std::size_t size){
    const struct {u64 address,size;} range{address,size};
    return serviceDispatchIn(s,65102,range,.buffer_attrs={SfBufferAttr_HipcMapAlias|SfBufferAttr_Out},.buffers={{out,size}});
}
struct Re5NoteRecovery {Re5Build build=Re5Build::None;bool stable=true;char json[600]="null";};
// Probe only the two verified GNU header/descriptor ranges; never patched instruction words.
Re5NoteRecovery re5_recover(Service* service,Re5Extents main,u64 pid){
    Re5NoteRecovery recovery;char notes[2][240]{};bool rejected=false;unsigned known=0;
    const bool bounds=main.base>=0x1000&&!(main.base&0xfff)&&main.size==re5::profile::main_size
        &&main.size<=UINT64_MAX-main.base;
    unsigned index=0;
    for(auto build:{Re5Build::Latest,Re5Build::Legacy}){
        const auto profile=re5::profile::view(build);const auto offset=profile.id_offset;
        std::array<u8,32> note{},padded{};bool attempted=false,valid=false;Result result=0;
        if(!re5_same_process(pid)){recovery.stable=false;return recovery;}
        if(bounds&&offset>=16&&offset-16<=main.size&&note.size()<=main.size-(offset-16)){
            attempted=true;result=re5_read(service,main.base+offset-16,note.data(),note.size());
            constexpr u8 header[]={4,0,0,0,16,0,0,0,3,0,0,0,'G','N','U',0};
            valid=R_SUCCEEDED(result)&&!std::memcmp(note.data(),header,sizeof(header))
                &&std::any_of(note.begin()+16,note.end(),[](u8 byte){return byte!=0;});
            if(R_FAILED(result))rejected=true;
            if(valid){
                std::copy_n(note.begin()+16,16,padded.begin());
                if(re5_build(padded.data())!=build)rejected=true;
                else{recovery.build=build;++known;}
            }
        }
        if(!re5_same_process(pid)){recovery.stable=false;recovery.build=Re5Build::None;return recovery;}
        char code[16]="null",id[67]="null";
        if(attempted)std::snprintf(code,sizeof(code),"%u",unsigned(result));
        if(valid){id[0]='"';re5_build_text(padded.data(),id+1);id[65]='"';id[66]=0;}
        std::snprintf(notes[index++],sizeof(notes[0]),
            "{\"offset\":%llu,\"read_result\":%s,\"valid\":%s,\"build_id\":%s}",
            static_cast<unsigned long long>(offset),code,valid?"true":"false",id);
    }
    std::snprintf(recovery.json,sizeof(recovery.json),"[%s,%s]",notes[0],notes[1]);
    if(rejected||known!=1)recovery.build=Re5Build::None;
    return recovery;
}
bool re5_image_identity(Service* service,u64 address,Re5Build build,Re5Extents main,u64 pid){
    if(build==Re5Build::None||address>UINT64_MAX-sizeof(re5::SharedControl)-sizeof(re5::ImageIdentity))return false;
    const auto profile=re5::profile::view(build);re5::ImageIdentity first{},second{};
    auto read=[&](re5::ImageIdentity& identity){
        return re5_same_process(pid)
            &&R_SUCCEEDED(re5_read(service,address+sizeof(re5::SharedControl),&identity,sizeof(identity)))
            &&re5_same_process(pid);
    };
    if(!read(first)||!read(second)||std::memcmp(&first,&second,sizeof(first)))return false;
    return !std::memcmp(first.marker,re5::kImageIdentityMarker,16)&&first.abi==1&&first.status==1
        &&!std::memcmp(first.build_id,profile.id->data(),32)
        &&!std::memcmp(first.sdk_build_id,re5::profile::sdk_build_id.data(),32)
        &&first.main_base==main.base&&first.main_size==main.size&&first.guard_count==profile.count&&!first.reserved;
}
#include "input_probe.inc"
#include "camera_probe.inc"
Result re5_write(Service* s,u64 address,const void* in,std::size_t size){
    const struct {u64 address,size;} range{address,size};
    return serviceDispatchIn(s,65103,range,.buffer_attrs={SfBufferAttr_HipcMapAlias|SfBufferAttr_In},.buffers={{in,size}});
}
// Optional diagnostic discovery does not authorize a write. The full immutable
// native image proof is required before reading counters and again before request.
constexpr std::size_t re5_runtime_offset=sizeof(re5::SharedControl)+sizeof(re5::ImageIdentity);
constexpr std::size_t re5_runtime_header=offsetof(re5::RuntimeProbe,counters);
struct Re5RuntimeSnapshot {re5::RuntimeProbe value{};u64 address=0;u32 requested=0;};
bool re5_runtime_valid(const re5::RuntimeProbe& value){
    return !std::memcmp(value.marker,re5::kRuntimeProbeMarker,sizeof(value.marker))
        &&value.abi==re5::kRuntimeProbeAbi&&value.size==re5::kRuntimeProbeBytes
        &&value.active<=1&&!value.reserved;
}
bool re5_runtime_read(Service* service,u64 address,u64 pid,void* out,std::size_t size){
    return re5_same_process(pid)&&R_SUCCEEDED(re5_read(service,address,out,size))&&re5_same_process(pid);
}
Re5RuntimeSnapshot re5_runtime_capture(Service* service,u64 control,Re5Build build,Re5Extents main,u64 pid){
    Re5RuntimeSnapshot snapshot;
    if(!control||build==Re5Build::None||main.base<0x1000||(main.base&0xfff)
        ||main.size!=re5::profile::main_size||main.size>UINT64_MAX-main.base
        ||control>UINT64_MAX-re5_runtime_offset-sizeof(re5::RuntimeProbe))return snapshot;
    const auto address=control+re5_runtime_offset;re5::RuntimeProbe header{},value{};
    if(!re5_runtime_read(service,address,pid,&header,re5_runtime_header)||!re5_runtime_valid(header))return snapshot;
    if(!re5_image_identity(service,control,build,main,pid)
        ||!re5_runtime_read(service,address,pid,&value,sizeof(value))
        ||std::memcmp(&header,&value,re5_runtime_header))return snapshot;
    // Mutable counters are observations, not authorization. A changing capture
    // header invalidates the observation rather than labelling a reset as old data.
    re5::RuntimeProbe verified{};
    if(!re5_runtime_read(service,address,pid,&verified,re5_runtime_header)
        ||std::memcmp(&value,&verified,re5_runtime_header))return snapshot;
    snapshot.value=value;snapshot.address=address;
    if(!value.active&&value.request_sequence==value.applied_sequence){
        snapshot.requested=value.request_sequence+1;
        if(!snapshot.requested)snapshot.requested=1;
    }
    return snapshot;
}
void re5_runtime_json(const Re5RuntimeSnapshot& snapshot,char* out,std::size_t capacity){
    if(!out||capacity<5)return;
    std::memcpy(out,"null",5);if(!snapshot.address)return;
    char json[1800],requested[16]="null";std::size_t used=0;
    if(snapshot.requested)std::snprintf(requested,sizeof(requested),"%u",snapshot.requested);
    auto append=[&](const char* format,auto... args){
        if(used>=sizeof(json))return;
        const auto n=std::snprintf(json+used,sizeof(json)-used,format,args...);
        if(n<0||std::size_t(n)>=sizeof(json)-used){used=sizeof(json);return;}
        used+=std::size_t(n);
    };
    const auto& value=snapshot.value;
    append("{\"abi\":%u,\"request_sequence\":%u,\"applied_sequence\":%u,\"requested_sequence\":%s,\"active\":%s,\"counters\":{",
        value.abi,value.request_sequence,value.applied_sequence,requested,value.active?"true":"false");
    for(unsigned i=0;i<re5::kRuntimeCounterCount;++i)
        append("%s\"%s\":%llu",i?",":"",re5::kRuntimeCounterNames[i],static_cast<unsigned long long>(value.counters[i]));
    append("},\"input_thread\":\"%016llX\",\"camera_thread\":\"%016llX\",\"pinned_thread\":\"%016llX\"}",
        static_cast<unsigned long long>(value.input_thread),static_cast<unsigned long long>(value.camera_thread),
        static_cast<unsigned long long>(value.pinned_thread));
    if(used<sizeof(json)&&used<capacity)std::memcpy(out,json,used+1);
}
void re5_runtime_request(Service* service,Re5RuntimeSnapshot& snapshot,u64 control,Re5Build build,Re5Extents main,u64 pid){
    if(!snapshot.address||!snapshot.requested)return;
    re5::RuntimeProbe verified{};
    if(!re5_image_identity(service,control,build,main,pid)
        ||!re5_runtime_read(service,snapshot.address,pid,&verified,re5_runtime_header)
        ||std::memcmp(&snapshot.value,&verified,re5_runtime_header)||!re5_same_process(pid)){
        snapshot.address=0;snapshot.requested=0;return;
    }
    // This is the only writable field in the optional diagnostic ABI. All native
    // counters, capture state, identity, and settings bytes remain native-owned.
    if(R_FAILED(re5_write(service,snapshot.address+offsetof(re5::RuntimeProbe,request_sequence),
        &snapshot.requested,sizeof(snapshot.requested))))snapshot.requested=0;
    if(!re5_same_process(pid)){snapshot.address=0;snapshot.requested=0;}
}
u32 control_capabilities(const re4::Settings& a,const re4::Settings& b){
    u32 result=0;
    if(std::tie(a.gyro_enabled,a.gyro_speed,a.gyro_gain_x,a.gyro_gain_y,a.gyro_deadzone_dps,a.invert_x,a.invert_y,a.gyro_bias_x,a.gyro_bias_y,a.calibration_request)
        !=std::tie(b.gyro_enabled,b.gyro_speed,b.gyro_gain_x,b.gyro_gain_y,b.gyro_deadzone_dps,b.invert_x,b.invert_y,b.gyro_bias_x,b.gyro_bias_y,b.calibration_request))result|=re5::kCapabilityGyro;
    if(std::tie(a.analog_enabled,a.stick_curve,a.turn_gain,a.aim_gain_x,a.aim_gain_y,a.right_aim_gain_x,a.right_aim_gain_y,a.left_deadzone,a.right_deadzone,a.remap_enabled,a.button_map,a.unified_controls,a.re6_response)
        !=std::tie(b.analog_enabled,b.stick_curve,b.turn_gain,b.aim_gain_x,b.aim_gain_y,b.right_aim_gain_x,b.right_aim_gain_y,b.left_deadzone,b.right_deadzone,b.remap_enabled,b.button_map,b.unified_controls,b.re6_response))result|=re5::kCapabilitySticks;
    if(std::tie(a.move_while_aiming,a.move_while_knife,a.move_while_reloading,a.movement_gain)
        !=std::tie(b.move_while_aiming,b.move_while_knife,b.move_while_reloading,b.movement_gain))result|=re5::kCapabilityMovement;
    if(std::tie(a.camera_gain,a.free_camera,a.camera_return_speed,a.third_person_distance,a.first_person)
        !=std::tie(b.camera_gain,b.free_camera,b.camera_return_speed,b.third_person_distance,b.first_person))result|=re5::kCapabilityCamera;
    if(std::tie(a.reticle,a.reticle_size)!=std::tie(b.reticle,b.reticle_size))result|=re5::kCapabilityReticle;
    return result;
}
bool re5_ready(const re5::SharedControl& c){return !std::memcmp(c.marker,re5::kSharedMarker,16)
    &&c.abi==re5::kSharedAbi&&c.status==1&&c.capabilities&&!(c.capabilities&~re5::kKnownCapabilities)&&!c.reserved
    &&!(c.sequence&1)&&c.applied_sequence==c.sequence;}
}
void Re5GameLink::initialize(){
    close();
    initialized_=true;info_result_=pminfoInitialize();info_ready_=R_SUCCEEDED(info_result_);
    service_result_=smGetService(&service_,"dmnt:cht");
    if(R_FAILED(service_result_))message=tr("Debug service unavailable","Сервис отладки недоступен");
}
void Re5GameLink::initialize_shared(const Re5GameLink& owner){
    if(this==&owner)return;
    close();
    initialized_=true;info_result_=pminfoInitialize();info_ready_=R_SUCCEEDED(info_result_);
    service_=owner.service_;service_result_=owner.service_result_;borrowed_=true;
    if(!serviceIsActive(&service_))message=tr("Debug service unavailable","Сервис отладки недоступен");
}
void Re5GameLink::close(){
    if(!borrowed_&&serviceIsActive(&service_))serviceClose(&service_);
    service_={};borrowed_=false;
    if(info_ready_)pminfoExit();
    info_ready_=initialized_=false;process_=address_=0;expected_sequence_=0;
    connected_build_=Re5Build::None;connected_main_base_=connected_main_size_=connected_control_bytes_=0;recovered_identity_=false;
    observed_={};diagnostics={};
}
bool Re5GameLink::connect(){
    address_=process_=0;expected_sequence_=0;connected_build_=Re5Build::None;
    connected_main_base_=connected_main_size_=connected_control_bytes_=0;recovered_identity_=false;
    diagnostics={};observed_={};details.clear();u64 pid=0,title=0;
    auto record=[&](unsigned stage,Result result){observed_.results[stage]={result,true};return result;};
    auto error=[&](const char* status,const char* text,Result result){
        observed_.status=status;message=text;
        char buffer[128];std::snprintf(buffer,sizeof(buffer),"%s: Result 0x%08X",text,unsigned(result));details=buffer;
        return false;
    };
    if(!info_ready_)return error("info_unavailable","Process information unavailable",info_result_);
    if(R_FAILED(record(0,pmdmntGetApplicationProcessId(&pid))))
        return error("process_error","Could not query application process",observed_.results[0].value);
    observed_.process_id=pid;observed_.process_valid=pid!=0;
    if(!pid){observed_.status="no_game";message=tr("Start Resident Evil 5","Запустите Resident Evil 5");return false;}
    if(R_FAILED(record(1,pminfoGetProgramId(&title,pid))))
        return error("program_error","Could not query application program",observed_.results[1].value);
    observed_.program_id=title;observed_.program_valid=true;
    if(title!=re5::profile::title_id){observed_.status="no_game";
        message=tr("Start Resident Evil 5","Запустите Resident Evil 5");return false;}
    if(!serviceIsActive(&service_))return error("service_unavailable","Debug service unavailable",service_result_);
    if(R_FAILED(record(2,serviceDispatch(&service_,65003))))
        return error("attach_error","Could not attach to RE5",observed_.results[2].value);
    Re5Metadata metadata{};u64 current_pid=0,current_title=0;
    if(R_FAILED(record(3,serviceDispatchOut(&service_,65002,metadata))))
        return error("metadata_error","Could not read RE5 metadata",observed_.results[3].value);
    observed_.metadata_valid=true;observed_.metadata_process_id=metadata.pid;observed_.metadata_program_id=metadata.title;
    std::copy_n(metadata.build_id,32,observed_.build_id.begin());
    observed_.main_base=metadata.main.base;observed_.main_size=metadata.main.size;
    if(R_FAILED(record(4,pmdmntGetApplicationProcessId(&current_pid)))){
        observed_.process_valid=observed_.program_valid=false;
        return error("process_error","Could not verify application process",observed_.results[4].value);
    }
    observed_.process_id=current_pid;observed_.process_valid=current_pid!=0;observed_.program_valid=false;
    if(current_pid){
        if(R_FAILED(record(5,pminfoGetProgramId(&current_title,current_pid))))
            return error("program_error","Could not verify application program",observed_.results[5].value);
        observed_.program_id=current_title;observed_.program_valid=true;
    }
    if(current_pid!=pid||current_title!=re5::profile::title_id||metadata.pid!=pid||metadata.title!=title){
        observed_.status="identity_changed";message=tr("Game restarted. Reconnect","Игра перезапущена. Подключитесь заново");
        details="Application process or program does not match attached RE5 metadata";return false;
    }
    const bool recovered=re5_zero_id(metadata.build_id);
    auto build=re5_build(metadata.build_id);
    if(recovered){
        const auto notes=re5_recover(&service_,metadata.main,pid);
        std::snprintf(observed_.image_notes.data(),observed_.image_notes.size(),"%s",notes.json);
        build=notes.build;
        if(!notes.stable){observed_.status="identity_changed";message=tr("Game restarted. Reconnect","Игра перезапущена. Подключитесь заново");return false;}
        if(build==Re5Build::None){
            observed_.status="build_id_unavailable";message=tr("RE5 image identity unavailable","Не получен BuildID RE5");
            details="Zero metadata BuildID requires one exact known GNU note in the complete main extent";return false;
        }
    }
    if(build==Re5Build::None){
        observed_.status="unsupported_build";message=tr("Unsupported RE5 build","Эта версия RE5 не поддерживается");
        char actual[65],expected[65],buffer[208];re5_build_text(metadata.build_id,actual);
        re5_build_text(re5::profile::build_id.data(),expected);
        std::snprintf(buffer,sizeof(buffer),"RE5 BuildID %s\nexpected %s",actual,expected);details=buffer;return false;
    }
    if(!recovered){
        observed_.supported=true;observed_.effective_build_id=*re5::profile::view(build).id;
        observed_.identity_source="metadata";
    }
    std::array<MemoryInfo,32> maps{};
    std::array<std::uint8_t,0x1000+sizeof(re5::SharedControl)> buffer{};
    std::size_t budget=8*1024*1024;bool pending=false;
    for(u64 offset=0;offset<4096&&budget;){
        u64 count=0;
        if(R_FAILED(serviceDispatchInOut(&service_,65101,offset,count,
            .buffer_attrs={SfBufferAttr_HipcMapAlias|SfBufferAttr_Out},.buffers={{maps.data(),sizeof(maps)}}))
            ||!count||count>maps.size())break;
        for(u64 i=0;i<count&&budget;++i){
            const auto& m=maps[i];
            if((m.type&0xFF)!=MemType_CodeMutable||m.perm!=Perm_Rw
                ||m.size>std::numeric_limits<u64>::max()-m.addr)continue;
            const auto limit=std::min<u64>(m.size,2*1024*1024);
            for(u64 page=0;page<limit&&budget;page+=0x1000){
                const auto bytes=std::min<std::size_t>({buffer.size(),std::size_t(m.size-page),budget});
                budget-=bytes;
                if(bytes<sizeof(re5::SharedControl)||R_FAILED(re5_read(&service_,m.addr+page,buffer.data(),bytes)))break;
                for(std::size_t j=0;j+sizeof(re5::SharedControl)<=bytes;j+=8){
                    if(std::memcmp(buffer.data()+j,re5::kSharedMarker,16))continue;
                    re5::SharedControl c{};std::memcpy(&c,buffer.data()+j,sizeof(c));
                    if(c.abi==re5::kSharedAbi&&c.status==0&&!(c.capabilities&~re5::kKnownCapabilities))pending=true;
                    if(!re5_ready(c))continue;
                    re5::Settings applied;
                    re5::SharedControl verified{};
                    if(!re5::decode_settings(c.settings,sizeof(c.settings),applied)
                        ||R_FAILED(re5_read(&service_,m.addr+page+j,&verified,sizeof(verified)))
                        ||!re5_ready(verified)||verified.sequence!=c.sequence
                        ||std::memcmp(verified.settings,c.settings,sizeof(c.settings)))continue;
                    const u64 candidate=m.addr+page+j;
                    if(recovered){
                        if(candidate-m.addr>m.size
                            ||sizeof(re5::SharedControl)+sizeof(re5::ImageIdentity)>m.size-(candidate-m.addr))continue;
                        if(!re5_image_identity(&service_,candidate,build,metadata.main,pid)){
                            if(!re5_same_process(pid)){
                                observed_.status="identity_changed";observed_.supported=false;
                                message=tr("Game restarted. Reconnect","Игра перезапущена. Подключитесь заново");return false;
                            }
                            continue;
                        }
                    }
                    if(!re5_same_process(pid)){
                        observed_.status="identity_changed";observed_.supported=false;
                        message=tr("Game restarted. Reconnect","Игра перезапущена. Подключитесь заново");return false;
                    }
                    address_=candidate;process_=pid;diagnostics=verified;expected_sequence_=c.sequence;
                    connected_build_=build;connected_main_base_=metadata.main.base;connected_main_size_=metadata.main.size;
                    connected_control_bytes_=m.size-(candidate-m.addr);
                    recovered_identity_=recovered;observed_.supported=true;
                    observed_.effective_build_id=*re5::profile::view(build).id;
                    observed_.identity_source=recovered?"native_verified":"metadata";
                    char text[448];std::snprintf(text,sizeof(text),
                        "RE5 abi=%u sequence=%u applied=%u source=%u frames=%llu\nfilter=%u samplers=%u overrides=%u",
                        c.abi,c.sequence,c.applied_sequence,c.settings_source,(unsigned long long)c.frames,
                        c.graphics_filter_latched,c.sampler_calls,c.sampler_overrides);
                    observed_.status="connected";details=text;message=tr("Connected to RE5 module","Подключено к модулю RE5");return true;
                }
            }
        }
        offset+=count;
    }
    if(!re5_same_process(pid)){
        observed_.status="identity_changed";observed_.supported=false;message=tr("Game restarted. Reconnect","Игра перезапущена. Подключитесь заново");return false;
    }
    observed_.status=recovered?"image_identity_unverified":pending?"module_initializing":"module_not_found";
    message=pending?tr("Settings initializing. Try again","Загрузка настроек. Попробуйте ещё раз"):
        tr("Mod not found. Install and restart RE5","Мод не найден. Установите и перезапустите RE5");return false;
}
std::size_t Re5GameLink::diagnostic_snapshot(char* out,std::size_t capacity){
    if(!out||!capacity)return 0;
    out[0]=0;
    // The caller owns this GameLink and its services; connect only reads metadata and IPC.
    connect();
    char ids[4][19]{};
    auto id=[](char* text,bool valid,u64 value){
        if(valid)std::snprintf(text,19,"\"%016llX\"",static_cast<unsigned long long>(value));
        else std::memcpy(text,"null",5);
    };
    id(ids[0],observed_.process_valid,observed_.process_id);id(ids[1],observed_.program_valid,observed_.program_id);
    id(ids[2],observed_.metadata_valid,observed_.metadata_process_id);id(ids[3],observed_.metadata_valid,observed_.metadata_program_id);
    char actual[67]="null",effective[67]="null",expected[65];
    if(observed_.metadata_valid){actual[0]='"';re5_build_text(observed_.build_id.data(),actual+1);actual[65]='"';actual[66]=0;}
    if(observed_.supported){effective[0]='"';re5_build_text(observed_.effective_build_id.data(),effective+1);effective[65]='"';effective[66]=0;}
    re5_build_text(re5::profile::build_id.data(),expected);
    char main_extent[100]="null",notes[600]="null";
    if(observed_.metadata_valid)std::snprintf(main_extent,sizeof(main_extent),
        "{\"base\":\"%016llX\",\"size\":%llu}",
        static_cast<unsigned long long>(observed_.main_base),static_cast<unsigned long long>(observed_.main_size));
    if(observed_.image_notes[0])std::snprintf(notes,sizeof(notes),"%s",observed_.image_notes.data());
    char results[8][16]{};
    auto result=[](char* text,bool called,Result value){
        if(called)std::snprintf(text,16,"%u",unsigned(value));else std::memcpy(text,"null",5);
    };
    result(results[0],initialized_,info_result_);result(results[1],initialized_,service_result_);
    for(unsigned i=0;i<6;++i)result(results[i+2],observed_.results[i].called,observed_.results[i].value);
    char ipc[1024]="null";
    if(address_&&observed_.supported){const auto& c=diagnostics;
        re5::Settings settings;char players[2][384]{};
        if(!re5::decode_settings(c.settings,sizeof(c.settings),settings))return 0;
        for(unsigned i=0;i<2;++i){const auto& p=settings.players[i];
            auto flag=[](bool enabled){return enabled?"true":"false";};
            std::snprintf(players[i],sizeof(players[i]),
                "{\"analog\":%s,\"unified\":%s,\"gyro\":%s,\"move_aim\":%s,\"move_knife\":%s,"
                "\"move_reload\":%s,\"free_camera\":%s,\"first_person\":%s,\"third_person_distance\":%.6g,\"camera_gain\":%.6g,\"camera_return_speed\":%.6g}",
                flag(p.analog_enabled),flag(p.unified_controls),flag(p.gyro_enabled),flag(p.move_while_aiming),
                flag(p.move_while_knife),flag(p.move_while_reloading),flag(p.free_camera),flag(p.first_person),
                double(p.third_person_distance),double(p.camera_gain),double(p.camera_return_speed));
        }
        std::snprintf(ipc,sizeof(ipc),"{\"abi\":%u,\"status\":%u,\"capabilities\":%u,\"sequence\":%u,\"applied_sequence\":%u,"
            "\"settings_source\":%u,\"frames\":%llu,\"graphics_filter_latched\":%u,\"sampler_calls\":%u,\"sampler_overrides\":%u,\"players\":[%s,%s]}",
            c.abi,c.status,c.capabilities,c.sequence,c.applied_sequence,c.settings_source,static_cast<unsigned long long>(c.frames),
            c.graphics_filter_latched,c.sampler_calls,c.sampler_overrides,players[0],players[1]);
    }
    auto& routes=diagnostic_routes_;std::memcpy(routes.data(),"null",5);
    if(address_&&observed_.supported&&observed_.main_base>=0x1000&&!(observed_.main_base&0xfff)
        &&observed_.main_size==re5::profile::main_size&&observed_.main_size<=UINT64_MAX-observed_.main_base)
        re5_input_probe(&service_,observed_.main_base,process_,routes.data(),routes.size(),connected_build_,address_);
    auto runtime=address_&&observed_.supported
        ?re5_runtime_capture(&service_,address_,connected_build_,{connected_main_base_,connected_main_size_},process_)
        :Re5RuntimeSnapshot{};
    char runtime_json[1800]="null";re5_runtime_json(runtime,runtime_json,sizeof(runtime_json));
    auto camera=address_&&observed_.supported
        ?re5_camera_capture(&service_,address_,connected_control_bytes_,connected_build_,{connected_main_base_,connected_main_size_},process_,camera_probe_buffer_)
        :Re5CameraSnapshot{};
    // Any failed native/process proof also invalidates a pending runtime lease;
    // do not retry authorization later in the same diagnostic observation.
    if(!std::strcmp(camera.status,"identity_unverified")||!std::strcmp(camera.status,"identity_changed"))runtime={};
    re5_runtime_json(runtime,runtime_json,sizeof(runtime_json));
    if(!re5_camera_json(camera,camera_probe_json_.data(),camera_probe_json_.size())){camera={nullptr,"capacity"};std::memcpy(camera_probe_json_.data(),"null",5);}
    auto& json=diagnostic_json_;
    auto format_json=[&](){return std::snprintf(json.data(),json.size(),"{\"schema\":1,\"game\":\"RE5\",\"overlay_version\":\"%s\",\"status\":\"%s\","
        "\"process_id\":%s,\"program_id\":%s,\"metadata_process_id\":%s,\"metadata_program_id\":%s,"
        "\"build_id\":%s,\"effective_build_id\":%s,\"identity_source\":\"%s\",\"expected_build_id\":\"%s\",\"supported\":%s,"
        "\"main_extent\":%s,\"image_notes\":%s,"
        "\"result_codes\":{\"pminfo\":%s,\"service\":%s,\"process\":%s,\"program\":%s,\"attach\":%s,\"metadata\":%s,"
        "\"verify_process\":%s,\"verify_program\":%s},\"ipc\":%s,\"input_routes\":%s,\"runtime\":%s,\"camera_probe_status\":\"%s\",\"camera_probe\":%s}",
        re4::kVersion,observed_.status,ids[0],ids[1],ids[2],ids[3],actual,effective,observed_.identity_source,expected,observed_.supported?"true":"false",
        main_extent,notes,results[0],results[1],results[2],results[3],results[4],results[5],results[6],results[7],ipc,routes.data(),runtime_json,camera.status,camera_probe_json_.data());};
    int size=format_json();
    if(size<0||std::size_t(size)>=json.size()||std::size_t(size)>=capacity)return 0;
    // Serialize the retained observation and verify total capacity before a new
    // request can reset native counters. Tiny caller buffers consume no capture.
    re5_runtime_request(&service_,runtime,address_,connected_build_,{connected_main_base_,connected_main_size_},process_);
    re5_runtime_json(runtime,runtime_json,sizeof(runtime_json));size=format_json();
    if(size<0||std::size_t(size)>=json.size()||std::size_t(size)>=capacity)return 0;
    std::memcpy(out,json.data(),std::size_t(size)+1);return std::size_t(size);
}
bool Re5GameLink::apply(const re5::Settings& settings){
    if(!re5::valid_settings(settings)){message=tr("Invalid RE5 settings","Недопустимые настройки RE5");return false;}
    if(!address_&&!connect())return false;
    auto lost=[&](){
        address_=process_=0;connected_build_=Re5Build::None;connected_main_base_=connected_main_size_=connected_control_bytes_=0;
        recovered_identity_=false;observed_.supported=false;observed_.identity_source="unavailable";
        message=tr("Game restarted. Reconnect","Игра перезапущена. Подключитесь заново");return false;
    };
    Re5Metadata metadata{};
    if(!re5_same_process(process_)||R_FAILED(serviceDispatchOut(&service_,65002,metadata))
        ||metadata.pid!=process_||metadata.title!=re5::profile::title_id||!re5_same_process(process_))return lost();
    const bool zero=re5_zero_id(metadata.build_id);
    if(!zero&&re5_build(metadata.build_id)!=connected_build_)return lost();
    const bool need_proof=zero||recovered_identity_;
    if(need_proof){
        if(metadata.main.base!=connected_main_base_||metadata.main.size!=connected_main_size_)return lost();
        const auto notes=re5_recover(&service_,metadata.main,process_);
        if(!notes.stable||notes.build!=connected_build_)return lost();
    }
    re5::SharedControl c{};
    if(R_FAILED(re5_read(&service_,address_,&c,sizeof(c)))||!re5_ready(c)||c.sequence!=expected_sequence_){
        connected_build_=Re5Build::None;recovered_identity_=false;observed_.supported=false;
        address_=process_=0;message=tr("Connection lost","Связь потеряна");return false;}
    re5::SharedControl verified{};re5::Settings previous;
    if(!re5::decode_settings(c.settings,sizeof(c.settings),previous)
        ||R_FAILED(re5_read(&service_,address_,&verified,sizeof(verified)))||!re5_ready(verified)
        ||verified.sequence!=c.sequence||std::memcmp(verified.settings,c.settings,sizeof(c.settings))){
        message=tr("Settings busy or invalid. Reconnect","Настройки заняты или повреждены. Подключитесь заново");return false;}
    c=verified; // Use the most recently validated capability snapshot.
    u32 required=0;
    if((settings.graphics.filter!=previous.graphics.filter||settings.graphics.filter!=re4::TextureFilter::Native))required|=re5::kCapabilityFiltering;
    if((settings.graphics.aa!=previous.graphics.aa||settings.graphics.aa!=re5::AntiAliasing::Native))required|=re5::kCapabilityAntiAliasing;
    if((settings.graphics.shadows!=previous.graphics.shadows||settings.graphics.shadows!=re4::ShadowMode::Native))required|=re5::kCapabilityShadows;
    if((settings.graphics.draw_distance!=previous.graphics.draw_distance||settings.graphics.draw_distance!=1.0f))required|=re5::kCapabilityDistance;
    for(unsigned i=0;i<2;++i)required|=control_capabilities(previous.players[i],settings.players[i])
        |control_capabilities(re5::native_controls(),settings.players[i]);
    if((c.capabilities&required)!=required){
        message=tr("Update the RE5 module","Обновите модуль RE5");return false;}
    std::uint8_t bytes[re5::kSettingsBytes];
    if(!re5::encode_settings(settings,bytes,sizeof(bytes)))return false;
    const u32 odd=(c.sequence+1)|1u,even=odd+1;
    const u64 sequence=address_+offsetof(re5::SharedControl,sequence);
    if((need_proof&&!re5_image_identity(&service_,address_,connected_build_,metadata.main,process_))
        ||!re5_same_process(process_))return lost();
    const bool sent=R_SUCCEEDED(re5_write(&service_,sequence,&odd,sizeof(odd)))
        &&re5_same_process(process_)
        &&R_SUCCEEDED(re5_write(&service_,address_+offsetof(re5::SharedControl,settings),bytes,sizeof(bytes)))
        &&re5_same_process(process_)
        &&R_SUCCEEDED(re5_write(&service_,sequence,&even,sizeof(even)));
    if(sent)expected_sequence_=even;
    message=sent?tr("Settings sent to RE5","Настройки отправлены в RE5"):
        tr("Could not send settings","Не удалось отправить настройки");
    return sent;
}
