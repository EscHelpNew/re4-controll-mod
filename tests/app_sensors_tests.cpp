#include "app_sensors.hpp"
#include <cassert>
#include <cstdio>
#include <vector>

namespace {
struct Request {s32 wanted;HidNpadIdType id;HidNpadStyleTag tag;};
struct Backend {
    std::vector<Request> requests;
    std::vector<u32> starts,stops,reads;
    Result get_result=0,start_result[2]{};
    std::size_t read_limit=0;
} backend;
constexpr u32 connected=HidNpadAttribute_IsConnected;
constexpr u32 both=connected|HidNpadAttribute_IsLeftConnected|HidNpadAttribute_IsRightConnected;
constexpr Result left_failure=0x111,right_failure=0x222,get_failure=0x333;
PadState pad(u32 style=HidNpadStyleTag_NpadFullKey,u32 attributes=connected,bool handheld=false){
    PadState value{};value.style_set=style;value.attributes=attributes;value.active_handheld=handheld;return value;
}
#define CHECK(condition) do { if(!(condition)){std::fprintf(stderr,"%s:%d: %s\n",__func__,__LINE__,#condition);return false;} } while(false)

bool diagnostics_reuses_the_same_selected_controller(){
    backend={};re4::AppSensors sensors;const auto source=pad();HidSixAxisSensorState samples[16]{};
    CHECK(sensors.read(samples)==0&&backend.reads.empty());
    CHECK(sensors.select(source,HidNpadIdType_No1,re4::InstallerPage::Diagnostics));
    CHECK(sensors.count==1&&sensors.result==0&&backend.requests.size()==1&&backend.starts==std::vector<u32>{101});
    CHECK(backend.requests[0].wanted==1&&backend.requests[0].id==HidNpadIdType_No1&&backend.requests[0].tag==HidNpadStyleTag_NpadFullKey);
    CHECK(!sensors.select(source,HidNpadIdType_No1,re4::InstallerPage::GyroTest));
    CHECK(backend.requests.size()==1&&backend.starts.size()==1);
    CHECK(sensors.read(samples)==3&&samples[0].sampling_number==101&&backend.read_limit==16);
    sensors.stop();CHECK(backend.stops==std::vector<u32>{101});return true;
}
bool player_change_stops_the_old_selection(){
    backend={};re4::AppSensors sensors;const auto source=pad();
    CHECK(sensors.select(source,HidNpadIdType_No1,re4::InstallerPage::Diagnostics));
    CHECK(sensors.select(source,HidNpadIdType_No2,re4::InstallerPage::Diagnostics));
    CHECK(backend.requests.size()==2&&backend.requests.back().id==HidNpadIdType_No2);
    CHECK(sensors.selected_id==HidNpadIdType_No2&&backend.stops==std::vector<u32>{101});
    sensors.stop();CHECK(backend.stops==std::vector<u32>({101,101}));return true;
}
bool handheld_uses_its_own_id_and_style(){
    backend={};re4::AppSensors sensors;const auto source=pad(HidNpadStyleTag_NpadHandheld,connected,true);
    CHECK(sensors.select(source,HidNpadIdType_No2,re4::InstallerPage::GyroTest));
    CHECK(backend.requests.size()==1&&backend.requests[0].wanted==1);
    CHECK(backend.requests[0].id==HidNpadIdType_Handheld&&backend.requests[0].tag==HidNpadStyleTag_NpadHandheld);
    CHECK(sensors.selected_id==HidNpadIdType_Handheld);
    CHECK(!sensors.select(source,HidNpadIdType_No1,re4::InstallerPage::Diagnostics));
    sensors.stop();CHECK(backend.stops==std::vector<u32>{101});return true;
}
bool split_controllers_prefer_a_connected_running_right_sensor(){
    backend={};re4::AppSensors sensors;HidSixAxisSensorState samples[16]{};
    CHECK(sensors.select(pad(HidNpadStyleTag_NpadJoyDual,both),HidNpadIdType_No2,re4::InstallerPage::GyroTest));
    CHECK(backend.requests.size()==1&&backend.requests[0].wanted==2&&backend.requests[0].id==HidNpadIdType_No2);
    CHECK(backend.requests[0].tag==HidNpadStyleTag_NpadJoyDual&&sensors.count==2);
    CHECK(sensors.read(samples)==3&&backend.reads.back()==102);
    CHECK(sensors.select(pad(HidNpadStyleTag_NpadJoyDual,connected|HidNpadAttribute_IsLeftConnected|HidNpadAttribute_IsLeftWired),HidNpadIdType_No2,re4::InstallerPage::GyroTest));
    CHECK(sensors.read(samples)==3&&backend.reads.back()==101);
    CHECK(backend.stops==std::vector<u32>({101,102}));
    sensors.stop();CHECK(backend.stops==std::vector<u32>({101,102,101,102}));return true;
}
bool partial_start_failure_keeps_the_other_sensor_usable(){
    for(unsigned failed=0;failed<2;++failed){
        backend={};backend.start_result[failed]=failed?right_failure:left_failure;
        re4::AppSensors sensors;HidSixAxisSensorState samples[16]{};
        CHECK(sensors.select(pad(HidNpadStyleTag_NpadJoyDual,both),HidNpadIdType_No1,re4::InstallerPage::Diagnostics));
        CHECK(sensors.count==1&&sensors.result==(failed?right_failure:left_failure));
        CHECK(sensors.read(samples)==3&&backend.reads.back()==(failed?101u:102u));
        sensors.stop();CHECK(backend.stops==std::vector<u32>{failed?101u:102u});
    }
    return true;
}
bool unavailable_controllers_do_not_start_sensors(){
    backend={};re4::AppSensors sensors;HidSixAxisSensorState samples[16]{};
    CHECK(sensors.select(pad(HidNpadStyleTag_NpadFullKey,0),HidNpadIdType_No1,re4::InstallerPage::Diagnostics));
    CHECK(!sensors.count&&backend.requests.empty()&&backend.starts.empty());
    CHECK(sensors.select(pad(HidNpadStyleTag_NpadJoyLeft),HidNpadIdType_No1,re4::InstallerPage::Diagnostics));
    CHECK(sensors.result==MAKERESULT(Module_Libnx,LibnxError_NotFound)&&backend.requests.empty());
    CHECK(sensors.read(samples)==0&&backend.reads.empty());
    sensors.stop();CHECK(backend.stops.empty());return true;
}
bool acquisition_and_start_errors_do_not_create_retry_storms(){
    backend={};backend.get_result=get_failure;re4::AppSensors sensors;const auto source=pad();HidSixAxisSensorState samples[16]{};
    CHECK(sensors.select(source,HidNpadIdType_No1,re4::InstallerPage::Diagnostics));
    CHECK(sensors.result==get_failure&&!sensors.count&&backend.starts.empty());
    CHECK(!sensors.select(source,HidNpadIdType_No1,re4::InstallerPage::Diagnostics)&&backend.requests.size()==1);
    CHECK(sensors.read(samples)==0&&backend.reads.empty());sensors.stop();CHECK(backend.stops.empty());
    backend={};backend.start_result[0]=left_failure;backend.start_result[1]=right_failure;
    const auto split=pad(HidNpadStyleTag_NpadJoyDual,both);
    CHECK(sensors.select(split,HidNpadIdType_No1,re4::InstallerPage::Diagnostics));
    CHECK(!sensors.count&&sensors.result==right_failure);
    CHECK(!sensors.select(split,HidNpadIdType_No1,re4::InstallerPage::Diagnostics)&&backend.starts.size()==2);
    CHECK(sensors.read(samples)==0&&backend.reads.empty());sensors.stop();CHECK(backend.stops.empty());return true;
}
bool disconnect_stops_running_handles(){
    backend={};re4::AppSensors sensors;
    CHECK(sensors.select(pad(),HidNpadIdType_No1,re4::InstallerPage::Diagnostics));
    CHECK(sensors.select(pad(HidNpadStyleTag_NpadFullKey,0),HidNpadIdType_No1,re4::InstallerPage::Diagnostics));
    CHECK(!sensors.count&&backend.requests.size()==1&&backend.stops==std::vector<u32>{101});
    sensors.stop();CHECK(backend.stops.size()==1);return true;
}
bool ordinary_pages_never_acquire_start_or_read_sensors(){
    constexpr re4::InstallerPage pages[]={re4::InstallerPage::Home,re4::InstallerPage::Controls,
        re4::InstallerPage::Updates,re4::InstallerPage::Mapping,re4::InstallerPage::MappingTargets,
        re4::InstallerPage::Graphics,re4::InstallerPage::ControlGroup,re4::InstallerPage::Game,
        re4::InstallerPage::CharacterAppearance,re4::InstallerPage::LocalUpdate};
    for(const auto page:pages){
        backend={};re4::AppSensors sensors;HidSixAxisSensorState samples[16]{};
        sensors.select(pad(HidNpadStyleTag_NpadJoyDual,both),HidNpadIdType_No1,page);
        const auto count=sensors.read(samples);
        CHECK(backend.requests.empty()&&backend.starts.empty()&&backend.reads.empty()&&count==0&&sensors.count==0);
        sensors.stop();CHECK(backend.stops.empty());
    }
    return true;
}
bool leaving_a_diagnostic_page_stops_each_handle_once(){
    backend={};re4::AppSensors sensors;HidSixAxisSensorState samples[16]{};const auto split=pad(HidNpadStyleTag_NpadJoyDual,both);
    CHECK(sensors.select(split,HidNpadIdType_No2,re4::InstallerPage::Diagnostics));
    CHECK(sensors.select(split,HidNpadIdType_No2,re4::InstallerPage::LocalUpdate));
    CHECK(backend.stops==std::vector<u32>({101,102})&&sensors.count==0&&sensors.result==0);
    CHECK(!sensors.select(split,HidNpadIdType_No2,re4::InstallerPage::Home));
    CHECK(sensors.read(samples)==0&&backend.reads.empty());sensors.stop();CHECK(backend.stops.size()==2);
    CHECK(sensors.select(split,HidNpadIdType_No2,re4::InstallerPage::GyroTest));
    CHECK(backend.requests.size()==2&&backend.starts.size()==4&&sensors.count==2);
    sensors.stop();CHECK(backend.stops==std::vector<u32>({101,102,101,102}));return true;
}
bool explicit_stop_allows_the_identical_controller_to_restart(){
    backend={};re4::AppSensors sensors;const auto source=pad();
    CHECK(sensors.select(source,HidNpadIdType_No2,re4::InstallerPage::Diagnostics));
    sensors.stop();sensors.stop();CHECK(backend.stops==std::vector<u32>{101});
    CHECK(sensors.select(source,HidNpadIdType_No2,re4::InstallerPage::Diagnostics));
    CHECK(sensors.count==1&&backend.requests.size()==2&&backend.starts.size()==2);
    sensors.stop();return true;
}
bool a_failed_selection_can_retry_after_leaving_diagnostics(){
    backend={};backend.start_result[0]=left_failure;re4::AppSensors sensors;const auto source=pad();
    CHECK(sensors.select(source,HidNpadIdType_No1,re4::InstallerPage::GyroTest));
    CHECK(sensors.result==left_failure&&!sensors.count);
    CHECK(sensors.select(source,HidNpadIdType_No1,re4::InstallerPage::Home));
    CHECK(sensors.result==0&&backend.stops.empty());
    backend.start_result[0]=0;
    CHECK(sensors.select(source,HidNpadIdType_No1,re4::InstallerPage::Diagnostics));
    CHECK(sensors.count==1&&backend.starts.size()==2);sensors.stop();return true;
}
}
Result hidGetSixAxisSensorHandles(HidSixAxisSensorHandle* handles,s32 wanted,HidNpadIdType id,HidNpadStyleTag tag){
    assert(wanted==1||wanted==2);backend.requests.push_back({wanted,id,tag});
    if(backend.get_result)return backend.get_result;
    for(s32 i=0;i<wanted;++i)handles[i].type_value=101+u32(i);
    return 0;
}
Result hidStartSixAxisSensor(HidSixAxisSensorHandle handle){
    assert(handle.type_value==101||handle.type_value==102);backend.starts.push_back(handle.type_value);
    return backend.start_result[handle.type_value-101];
}
Result hidStopSixAxisSensor(HidSixAxisSensorHandle handle){backend.stops.push_back(handle.type_value);return 0;}
std::size_t hidGetSixAxisSensorStates(HidSixAxisSensorHandle handle,HidSixAxisSensorState* samples,std::size_t count){
    assert(samples&&count==16);backend.read_limit=count;backend.reads.push_back(handle.type_value);
    samples[0].sampling_number=handle.type_value;return 3;
}
int main(){
    struct Case {const char* name;bool (*run)();};
    const Case cases[]={
        {"Diagnostics/GyroTest preserve controller reuse and read16",diagnostics_reuses_the_same_selected_controller},
        {"P1/P2 selection",player_change_stops_the_old_selection},
        {"Handheld ID/style",handheld_uses_its_own_id_and_style},
        {"JoyDual right preference and left fallback",split_controllers_prefer_a_connected_running_right_sensor},
        {"Partial start failure",partial_start_failure_keeps_the_other_sensor_usable},
        {"Disconnected/unsupported controller",unavailable_controllers_do_not_start_sensors},
        {"Get/start failures without retry storm",acquisition_and_start_errors_do_not_create_retry_storms},
        {"Disconnect cleanup",disconnect_stops_running_handles},
        {"Ordinary pages perform no SixAxis operations",ordinary_pages_never_acquire_start_or_read_sensors},
        {"Leaving stops once and reentering restarts",leaving_a_diagnostic_page_stops_each_handle_once},
        {"Explicit stop invalidates selection cache",explicit_stop_allows_the_identical_controller_to_restart},
        {"Failure clears on exit and retries on reentry",a_failed_selection_can_retry_after_leaving_diagnostics}
    };
    unsigned failures=0;
    for(const auto& item:cases){const bool ok=item.run();std::printf("%s: %s\n",ok?"PASS":"FAIL",item.name);if(!ok)++failures;}
    std::printf("app sensors SDK adapter: %u/%u cases passed\n",unsigned(sizeof(cases)/sizeof(cases[0]))-failures,unsigned(sizeof(cases)/sizeof(cases[0])));
    return failures?1:0;
}
