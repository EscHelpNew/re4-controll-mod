#pragma once
#include <cstddef>
#include <cstdint>
using u8=std::uint8_t;
using u32=std::uint32_t;
using u64=std::uint64_t;
using s32=std::int32_t;
using Result=u32;
constexpr bool R_FAILED(Result result){return result!=0;}
constexpr u32 Module_Libnx=345;
constexpr u32 LibnxError_NotFound=9;
constexpr Result MAKERESULT(u32 module,u32 description){return (module&0x1ff)|((description&0x1fff)<<9);}
enum HidNpadIdType {HidNpadIdType_No1=0,HidNpadIdType_No2=1,HidNpadIdType_Handheld=0x20};
enum HidNpadStyleTag {
    HidNpadStyleTag_NpadFullKey=1u<<0,
    HidNpadStyleTag_NpadHandheld=1u<<1,
    HidNpadStyleTag_NpadJoyDual=1u<<2,
    HidNpadStyleTag_NpadJoyLeft=1u<<3
};
enum HidNpadAttribute {
    HidNpadAttribute_IsConnected=1u<<0,
    HidNpadAttribute_IsWired=1u<<1,
    HidNpadAttribute_IsLeftConnected=1u<<2,
    HidNpadAttribute_IsLeftWired=1u<<3,
    HidNpadAttribute_IsRightConnected=1u<<4
};
struct HidAnalogStickState {s32 x,y;};
struct PadState {
    u8 id_mask=0,active_id_mask=0;
    bool read_handheld=false,active_handheld=false;
    u32 style_set=0,attributes=0;
    u64 buttons_cur=0,buttons_old=0;
    HidAnalogStickState sticks[2]{};
    u32 gc_triggers[2]{};
};
constexpr bool padIsHandheld(const PadState* pad){return pad->active_handheld;}
constexpr u32 padGetStyleSet(const PadState* pad){return pad->style_set;}
constexpr u32 padGetAttributes(const PadState* pad){return pad->attributes;}
constexpr bool padIsConnected(const PadState* pad){return pad->attributes&HidNpadAttribute_IsConnected;}
union HidSixAxisSensorHandle {
    u32 type_value;
    struct {u32 npad_style_index:8,player_number:8,device_idx:8,pad:8;};
};
struct HidVector {float x,y,z;};
struct HidDirectionState {float direction[3][3];};
struct HidSixAxisSensorState {
    u64 delta_time,sampling_number;
    HidVector acceleration,angular_velocity,angle;
    HidDirectionState direction;
    u32 attributes,reserved;
};
Result hidGetSixAxisSensorHandles(HidSixAxisSensorHandle*,s32,HidNpadIdType,HidNpadStyleTag);
Result hidStartSixAxisSensor(HidSixAxisSensorHandle);
Result hidStopSixAxisSensor(HidSixAxisSensorHandle);
std::size_t hidGetSixAxisSensorStates(HidSixAxisSensorHandle,HidSixAxisSensorState*,std::size_t);
