// Build-specific controls adapter. Runtime guards precede every hook install.
#include "lib.hpp"
#include "nn/fs.hpp"
#include "re4/input.hpp"
#include "re4/profile.hpp"
#include "re4/settings.hpp"
#include "re4/shared.hpp"
#include "re4/turn.hpp"
#include "re4/hook_context.hpp"
#include "re4/response.hpp"
#include "re4/sensor.hpp"
#include "re4/aim_input.hpp"
#include "re4/camera.hpp"
#include "re4/view.hpp"
#include "re4/modern_move.hpp"
#include "re4/unified.hpp"
#include "re4/reticle_draw.hpp"
#include "re4/gyro_frame.hpp"
#include "re6/journal.hpp"
#include <atomic>

namespace {
struct NpadState {
    std::uint64_t sample, buttons;
    std::int32_t lx, ly, rx, ry;
    std::uint32_t attributes, reserved;
};
static_assert(sizeof(NpadState) == 40);
extern "C" void get_full(NpadState*, const std::uint32_t&)
    asm("_ZN2nn3hid12GetNpadStateEPNS0_16NpadFullKeyStateERKj");
extern "C" void get_handheld(NpadState*, const std::uint32_t&)
    asm("_ZN2nn3hid12GetNpadStateEPNS0_17NpadHandheldStateERKj");
extern "C" void get_dual(NpadState*, const std::uint32_t&)
    asm("_ZN2nn3hid12GetNpadStateEPNS0_16NpadJoyDualStateERKj");
extern "C" Result sensor_handles(std::uint32_t*, int*, int, std::uint32_t, std::uint32_t)
    asm("_ZN2nn3hid6detail22GetSixAxisSensorHandleEPNS0_19SixAxisSensorHandleEPiijNS_4util10BitFlagSetILi32ENS0_12NpadStyleTagEEE");
extern "C" Result sensor_start(const std::uint32_t&)
    asm("_ZN2nn3hid6detail18StartSixAxisSensorERKNS0_19SixAxisSensorHandleE");
extern "C" Result sensor_stop(const std::uint32_t&)
    asm("_ZN2nn3hid6detail17StopSixAxisSensorERKNS0_19SixAxisSensorHandleE");
extern "C" Result sensor_read(int*, re4::SensorState*, int, const std::uint32_t&)
    asm("_ZN2nn3hid6detail22GetSixAxisSensorStatesEPiPNS0_18SixAxisSensorStateEiRKNS0_19SixAxisSensorHandleE");
extern "C" Result nn_mount_rom(const char*,void*,std::size_t)
    asm("_ZN2nn2fs8MountRomEPKcPvm");
extern "C" int get_operation_mode() asm("_ZN2nn2oe16GetOperationModeEv");
struct SensorBackend {
    static auto handles(std::uint32_t* h, int* n, int capacity, std::uint32_t id, std::uint32_t style) {
        return sensor_handles(h, n, capacity, id, style);
    }
    static auto start(const std::uint32_t& handle) { return sensor_start(handle); }
    static auto stop(const std::uint32_t& handle) { return sensor_stop(handle); }
    static auto read(int* n, re4::SensorState* states, int capacity, const std::uint32_t& handle) {
        return sensor_read(n, states, capacity, handle);
    }
};

__attribute__((used, section(".data.re4control")))
volatile re4::SharedControl control = re4::initial_shared_control();
__attribute__((used)) re6::ProducerRing<1024> telemetry_ring;
struct TelemetryInput {NpadState pad{};std::uint64_t tick=0;std::uint32_t id=0,style=0;};
static_assert(sizeof(TelemetryInput)==56);
std::atomic<std::uint64_t> telemetry_input_sequence{0},telemetry_input_words[7]{};
std::atomic_flag telemetry_input_writer=ATOMIC_FLAG_INIT;
std::uint32_t telemetry_input_id=UINT32_MAX,telemetry_input_style=0;
std::uint64_t telemetry_last_frame=0;
void telemetry_write_input(const NpadState& pad,std::uint32_t id,std::uint32_t style){
    if((id!=0&&id!=0x20)||telemetry_input_writer.test_and_set(std::memory_order_acquire))return;
    if(!(pad.attributes&1)&&(id!=telemetry_input_id||style!=telemetry_input_style)){
        telemetry_input_writer.clear(std::memory_order_release);return;
    }
    telemetry_input_id=id;telemetry_input_style=style;
    const TelemetryInput value{pad,svcGetSystemTick(),id,style};std::uint64_t words[7];std::memcpy(words,&value,sizeof(value));
    const auto before=telemetry_input_sequence.fetch_add(1,std::memory_order_acq_rel);
    for(unsigned i=0;i<7;++i)telemetry_input_words[i].store(words[i],std::memory_order_relaxed);
    telemetry_input_sequence.store(before+2,std::memory_order_release);
    telemetry_input_writer.clear(std::memory_order_release);
}
bool telemetry_read_input(std::uint64_t tick,re6::Record& out){
    const auto before=telemetry_input_sequence.load(std::memory_order_acquire);if(!before||(before&1))return false;
    std::uint64_t words[7];for(unsigned i=0;i<7;++i)words[i]=telemetry_input_words[i].load(std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_acquire);
    if(before!=telemetry_input_sequence.load(std::memory_order_acquire))return false;
    TelemetryInput value{};std::memcpy(&value,words,sizeof(value));
    if(!(value.pad.attributes&1)||!value.tick||tick<value.tick||tick-value.tick>re6::kTickHz/10)return false;
    out.input_tick=value.tick;out.sample_id=value.pad.sample;out.buttons=value.pad.buttons;
    out.controller_id=value.id;out.controller_style=value.style;
    out.sticks[0]=value.pad.lx;out.sticks[1]=value.pad.ly;out.sticks[2]=value.pad.rx;out.sticks[3]=value.pad.ry;
    out.validity|=re6::InputValid;return true;
}
re4::Settings settings=[] {re4::Settings s;s.analog_enabled=false;s.remap_enabled=false;return s;}();
float raw_left_x = 0;
bool dpad_turn = false;
bool dpad_move = false;
void* movement_actor = nullptr;
std::uint32_t last_sequence = 0;
bool file_read = false;
std::atomic<std::uint64_t> graphics_snapshot{0x3F80000000000000ull};
std::atomic<unsigned> filter_latched{255},aa_latched{255};
std::atomic<unsigned> sampler_calls{0},sampler_overrides{0},scene_submits{0},scene_draws{0},cull_updates{0},resolution_overrides{0};
std::atomic<std::uint64_t> last_cull_pair{0};
std::atomic_flag rom_settings_attempted=ATOMIC_FLAG_INIT;
static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
bool file_mounted = false;
unsigned file_attempts = 0;
std::uint64_t next_file_retry = 0;
std::uintptr_t main_base = 0;
std::uint32_t sensor_pad = 0, sensor_style = 0, sensor_attributes = 0;
re4::SensorReader<SensorBackend> sensor;
re4::GyroFilter gyro;
re4::GyroFrame gyro_frame;
re4::Settings gyro_tuning_cache;
bool gyro_tuning_ready=false;
re4::AimDelta gyro_pending;
std::uint64_t last_sensor_tick = 0;
std::uint64_t next_sensor_retry = 0, last_fresh_tick = 0, last_sensor_number = 0;
int (*aiming_predicate)(void*) = nullptr;
void* gyro_input_actor = nullptr;
struct CameraContext {
    void* camera;
    void* actor;
    re4::CameraOffsets before;
    bool neutral,reset;
    float x,y;
    bool view_allowed=false,basis_valid=false;
    re4::ViewMatrix3x4 basis{};
};
CameraContext* camera_context = nullptr;
re4::FreeCamera free_orbit;
bool ordinary_camera(void* camera,void* actor){
    if(camera!=reinterpret_cast<void*>(main_base+0x11319C0) || !actor
        || actor!=*reinterpret_cast<void**>(main_base+re4::profile::player_slot))return false;
    const auto* bytes=static_cast<const std::uint8_t*>(actor);
    if(bytes[0x114]!=0 || bytes[0x115]>4 || *reinterpret_cast<const std::uint8_t*>(main_base+0x1131718+0x54)!=10)return false;
    const auto* state=*reinterpret_cast<const std::uint8_t**>(main_base+0xB1AE08);
    return state && !(state[0x5092]&4) && aiming_predicate && !aiming_predicate(actor);
}
re4::AimRoute aim_route(){
    const auto* type=*reinterpret_cast<const std::uint8_t**>(main_base+0xB1AE10);
    if(*reinterpret_cast<const int*>(main_base+0xAFF39C)!=2 || !type || type[11]!=2)return {false,63};
    const auto* state=*reinterpret_cast<const std::uint8_t**>(main_base+0xB1AE08);
    if(!state)return {false,63};
    if(state[0x5096]&0x80){
        const float dt=*reinterpret_cast<const float*>(state+0xA4);
        return {true,std::isfinite(dt)?63*std::fmin(4.f,std::fmax(0.f,dt)):0};
    }
    const bool left=state[0x5099]&0x80;
    return {!left,left?63.f:127.f};
}

bool firearm_aim(void* actor) {
    auto* bytes = static_cast<std::uint8_t*>(actor);
    if (!actor || actor != *reinterpret_cast<void**>(main_base + re4::profile::player_slot)
        || bytes[0x114] != 0 || (bytes[0x115] != 6 && bytes[0x115] != 15)
        || !(*reinterpret_cast<std::uint64_t*>(main_base + re4::profile::input_flags) & 0x10)) return false;
    return aiming_predicate && aiming_predicate(actor) != 0;
}
// Settings and completed frame identities cross the native submit/draw boundary.
std::atomic<unsigned> view_settings_snapshot{0};
std::atomic<unsigned> fps_override_count{0},reticle_draw_count{0};
std::atomic<std::uint64_t> view_frame{1},view_camera_frame{0},first_person_frame{0},view_camera_tick{0};
std::atomic<std::uintptr_t> view_camera_actor{0};
std::atomic<bool> view_camera_aim{false};
void* main_view_actor=nullptr; // Scoped only across the synchronous native submission graph.
struct AimPoint {
    std::atomic<std::uint64_t> sequence{0},frame{0};
    std::atomic<std::uintptr_t> actor{0},weapon{0};
    std::atomic<std::uint32_t> x{0},y{0},z{0},weapon_id{0};
    std::atomic<std::uint32_t> matrices[28]{};
} aim_point,pending_aim_point,displayed_aim_point;
void (*native_ortho)(void*,float,float,float,float,float,float)=nullptr;
void (*native_projection)(const void*,unsigned)=nullptr;
void (*native_pose)(const void*,unsigned)=nullptr;
void (*native_matrix)(unsigned)=nullptr;
void (*native_rect)(const void*)=nullptr;
void (*native_gx_blend)(unsigned,unsigned,unsigned,unsigned)=nullptr;
void (*native_gx_format)(unsigned,unsigned,unsigned,unsigned,unsigned)=nullptr;
void (*native_gx_begin)(unsigned,unsigned,unsigned)=nullptr;
void (*native_gx_position)(float,float,float)=nullptr;
void (*native_gx_color)(unsigned,unsigned,unsigned,unsigned)=nullptr;
void (*native_gx_end)(unsigned)=nullptr;
re4::UnifiedLook unified_look;
float unified_look_gain=-1,unified_look_deadzone=-1,unified_aim_gain_x=-1,unified_aim_gain_y=-1;
re4::StickCurve unified_look_curve=re4::StickCurve::Linear;
struct UnifiedUpdate {
    void* actor=nullptr;
    re4::UnifiedDelta delta{};
    bool walking=false,consumed=false,handoff=false;
};
UnifiedUpdate* unified_update=nullptr;
struct UnifiedAimSession {void* actor=nullptr;void* weapon=nullptr;std::uintptr_t handler=0;unsigned item=0;bool ready=false;};
UnifiedAimSession unified_aim_session;
bool unified_new_aim=false;
void* unified_root_actor=nullptr;
float unified_root_yaw=0,unified_pitch=0;
std::uintptr_t unified_pitch_actor=0;
unsigned unified_pitch_route=0;
std::uint64_t unified_command_frame=0,unified_command_tick=0;
bool unified_camera_calculation=false;
void publish_view_settings(){
    const unsigned word=unsigned(settings.first_person)|(unsigned(settings.reticle)<<1)|(unsigned(settings.reticle_size)<<3)|(unsigned(settings.unified_controls)<<5);
    const auto before=view_settings_snapshot.exchange(word,std::memory_order_acq_rel);
    if((before^word)&32){
        unified_look.reset();unified_aim_session={};unified_pitch_actor=0;unified_pitch_route=0;unified_command_frame=0;
    }
    if(!(word&1) || ((before^word)&1))first_person_frame.store(0,std::memory_order_release);
    if(((before^word)&6) || !(word&6))aim_point.frame.store(0,std::memory_order_release);
}
bool view_scene(void* actor){
    if(!actor || actor!=*reinterpret_cast<void**>(main_base+re4::profile::player_slot))return false;
    const auto* b=static_cast<const std::uint8_t*>(actor);
    const auto* controller=reinterpret_cast<const std::uint8_t*>(main_base+0x1131718);
    const auto* state=*reinterpret_cast<const std::uint8_t**>(main_base+0xB1AE08);
    return b[0x114]==0 && controller[0x54]==10 && (controller[0x48]&1) && state
        && !(state[0x5092]&4) && !(state[0x509E]&0x80) && !(state[0x1AB]&0x40);
}
bool view_actor_state(void* actor){
    if(!view_scene(actor))return false;
    const auto* b=static_cast<const std::uint8_t*>(actor);
    if(b[0x115]<=4)return true;
    if(b[0x115]==11)return b[0x116]==1||b[0x116]==2; // Native stance/strike, including recovery.
    if(b[0x115]==6)return true; // Generic weapon action also owns normal reload camera.
    return (b[0x115]==6||b[0x115]==15)
        && (*reinterpret_cast<const std::uint64_t*>(main_base+re4::profile::input_flags)&0x10);
}
bool view_camera_allowed(void* camera,void* actor){
    if(camera!=reinterpret_cast<void*>(main_base+0x11319C0)||!view_actor_state(actor)||!aiming_predicate)return false;
    return true; // Native26C is checked after profile selection, including non-aim reload.
}
void* player_weapon(void* actor){
    if(!actor)return nullptr;
    auto* context=*reinterpret_cast<std::uint8_t**>(static_cast<std::uint8_t*>(actor)+0x938);
    return context?*reinterpret_cast<void**>(context+0x38):nullptr;
}
struct GyroAimSession {
    void* actor=nullptr;
    void* weapon=nullptr;
    std::uintptr_t handler=0;
    unsigned item=0;
    bool ready=false,drain=true;
} gyro_session;
bool gyro_aim_ready(void* actor,bool allow_arm=false){
    const auto* state=*reinterpret_cast<const std::uint8_t**>(main_base+0xB1AE08);
    if(!settings.gyro_enabled || !firearm_aim(actor) || !state
        || (state[0x5092]&4) || (state[0x509E]&0x80) || (state[0x1AB]&0x40) || state[0x86FF]){
        gyro_session={};return false;
    }
    const auto* b=static_cast<const std::uint8_t*>(actor);
    const auto handler=b[0x115]==6?*reinterpret_cast<const std::uintptr_t*>(main_base+0x1265A28):0;
    auto* weapon=player_weapon(actor);
    if(gyro_session.actor!=actor || gyro_session.weapon!=weapon || gyro_session.item!=state[0x5034]
        || gyro_session.handler!=handler)gyro_session={actor,weapon,handler,state[0x5034],false};
    const bool before=gyro_session.ready;
    // These native handlers finish raising by selecting their steady aim loop
    // (1/1). Initial 1/4 is still raising, but the same state after a shot is
    // recovery: retain an already armed session through it and through fire.
    bool native_raise=false;
    for(const auto offset : {0x31CC60u,0x31B9F0u,0x3212E0u,0x3270F0u,0x332CC0u,0x333DF0u,0x3351A0u,0x342F90u})
        native_raise|=handler==main_base+offset;
    if(native_raise){
        if(b[0x116]!=1 && b[0x116]!=2)gyro_session.ready=false;
        else if(allow_arm && b[0x116]==1 && b[0x117]==1)gyro_session.ready=true;
    }else if(allow_arm)gyro_session.ready=true; // Other tables keep their own semantics.
    if(!before && gyro_session.ready)gyro_session.drain=true;
    return gyro_session.ready;
}
bool view_completed(void* actor){
    const auto now=svcGetSystemTick(),stamp=view_camera_tick.load(std::memory_order_acquire);
    return view_camera_frame.load(std::memory_order_acquire)==view_frame.load(std::memory_order_acquire)
        && view_camera_actor.load(std::memory_order_acquire)==reinterpret_cast<std::uintptr_t>(actor)
        && now>=stamp && now-stamp<=1'920'000 && view_actor_state(actor);
}
void clear_view_frame(){
    view_camera_frame.store(0,std::memory_order_release);
    first_person_frame.store(0,std::memory_order_release);
}
std::uint32_t view_float_bits(float value){std::uint32_t bits;std::memcpy(&bits,&value,4);return bits;}
float view_float_value(std::uint32_t bits){float value;std::memcpy(&value,&bits,4);return value;}
void publish_aim_point(AimPoint& target,void* actor,void* weapon,re4::ViewVec3 value,std::uint64_t frame,
        const re4::ViewMatrix3x4* view=nullptr,const re4::ViewMatrix4x4* projection=nullptr){
    target.sequence.fetch_add(1,std::memory_order_acq_rel);
    std::atomic_thread_fence(std::memory_order_release);
    target.frame.store(frame,std::memory_order_relaxed);
    target.actor.store(reinterpret_cast<std::uintptr_t>(actor),std::memory_order_relaxed);
    target.weapon.store(reinterpret_cast<std::uintptr_t>(weapon),std::memory_order_relaxed);
    target.x.store(view_float_bits(value.x),std::memory_order_relaxed);
    target.y.store(view_float_bits(value.y),std::memory_order_relaxed);
    target.z.store(view_float_bits(value.z),std::memory_order_relaxed);
    const auto* state=*reinterpret_cast<const std::uint8_t**>(main_base+0xB1AE08);
    target.weapon_id.store(state?state[0x5034]:0,std::memory_order_relaxed);
    if(view&&projection){
        for(unsigned i=0;i<12;++i)target.matrices[i].store(view_float_bits(view->m[i/4][i%4]),std::memory_order_relaxed);
        for(unsigned i=0;i<16;++i)target.matrices[12+i].store(view_float_bits(projection->m[i/4][i%4]),std::memory_order_relaxed);
    }
    target.sequence.fetch_add(1,std::memory_order_release);
}
bool read_aim_point(const AimPoint& source,void* actor,std::uint64_t required_frame,re4::ViewVec3& value,
        re4::ViewMatrix3x4* view=nullptr,re4::ViewMatrix4x4* projection=nullptr){
    const auto before=source.sequence.load(std::memory_order_acquire);
    if(before&1)return false;
    const auto frame=source.frame.load(std::memory_order_relaxed);
    const auto who=source.actor.load(std::memory_order_relaxed),weapon=source.weapon.load(std::memory_order_relaxed);
    const auto weapon_id=source.weapon_id.load(std::memory_order_relaxed);
    value={view_float_value(source.x.load(std::memory_order_relaxed)),
        view_float_value(source.y.load(std::memory_order_relaxed)),view_float_value(source.z.load(std::memory_order_relaxed))};
    if(view&&projection){
        for(unsigned i=0;i<12;++i)view->m[i/4][i%4]=view_float_value(source.matrices[i].load(std::memory_order_relaxed));
        for(unsigned i=0;i<16;++i)projection->m[i/4][i%4]=view_float_value(source.matrices[12+i].load(std::memory_order_relaxed));
    }
    const auto* state=*reinterpret_cast<const std::uint8_t**>(main_base+0xB1AE08);
    // Keep payload reads before the version recheck on ARM as well as x86.
    std::atomic_thread_fence(std::memory_order_acquire);
    return before==source.sequence.load(std::memory_order_relaxed)&&state
        && frame==required_frame&&frame!=0
        && who==reinterpret_cast<std::uintptr_t>(actor)&&weapon_id==state[0x5034]
        && weapon==reinterpret_cast<std::uintptr_t>(player_weapon(actor))&&re4::view_finite(value);
}
bool unified_scene(void* actor){
    if(!settings.unified_controls||!view_scene(actor)||(sensor_attributes&1)==0
        ||*reinterpret_cast<const std::uint8_t*>(main_base+0x11C7D55)
        ||*reinterpret_cast<const int*>(main_base+0xAFF39C)!=2)return false;
    const auto* state=*reinterpret_cast<const std::uint8_t**>(main_base+0xB1AE08);
    const float factor=*reinterpret_cast<const float*>(state+0xA4);
    return !state[0x86FF]&&std::isfinite(factor)&&factor>0&&factor<=2;
}
bool modern_move_reload_active(void* actor);
bool unified_view_ready(void* actor){
    if(!unified_scene(actor)||unified_pitch_actor!=reinterpret_cast<std::uintptr_t>(actor)
        ||!std::isfinite(unified_pitch)||std::fabs(unified_pitch)>1.570797f)return false;
    const auto* b=static_cast<const std::uint8_t*>(actor);
    const auto held=*reinterpret_cast<const std::uint64_t*>(main_base+re4::profile::input_flags);
    return b[0x115]<=4||(b[0x115]==6&&((held&0x10)||modern_move_reload_active(actor)))
        ||(b[0x115]==11&&(held&re4::kKnifeHeldMask));
}
bool unified_aim_allowed(void* actor,bool allow_arm=false){
    unified_new_aim=false;
    if(!unified_scene(actor)){unified_aim_session={};return false;}
    const auto* b=static_cast<const std::uint8_t*>(actor);
    const auto held=*reinterpret_cast<const std::uint64_t*>(main_base+re4::profile::input_flags);
    if(b[0x115]==6&&firearm_aim(actor)){
        const auto* state=*reinterpret_cast<const std::uint8_t**>(main_base+0xB1AE08);
        auto* weapon=player_weapon(actor);const auto handler=*reinterpret_cast<const std::uintptr_t*>(main_base+0x1265A28);
        if(unified_aim_session.actor!=actor||unified_aim_session.weapon!=weapon
            ||unified_aim_session.handler!=handler||unified_aim_session.item!=state[0x5034]){
            unified_aim_session={actor,weapon,handler,state[0x5034],false};unified_look.reset();
        }
        const bool before=unified_aim_session.ready;
        if(b[0x116]!=1&&b[0x116]!=2)unified_aim_session.ready=false;
        else if(allow_arm&&b[0x116]==1&&b[0x117]==1)unified_aim_session.ready=true;
        unified_new_aim=!before&&unified_aim_session.ready;
        return unified_aim_session.ready;
    }
    unified_aim_session={};
    return b[0x115]==11&&(held&re4::kKnifeHeldMask)
        &&(b[0x116]==1||b[0x116]==2)&&b[0x117]==1;
}
void unified_player_update(void* actor,void (*original)(void*)){
    publish_view_settings();
    // A live tuning edit drains old velocity. Gain0 is an immediate stop,
    // including an already accelerated stick; orientation itself is retained.
    if(settings.camera_gain!=unified_look_gain||settings.right_deadzone!=unified_look_deadzone
        ||(settings.re6_response&&(settings.right_aim_gain_x!=unified_aim_gain_x||settings.right_aim_gain_y!=unified_aim_gain_y))
        ||settings.stick_curve!=unified_look_curve)unified_look.reset();
    unified_look_gain=settings.camera_gain;unified_look_deadzone=settings.right_deadzone;unified_look_curve=settings.stick_curve;
    unified_aim_gain_x=settings.right_aim_gain_x;unified_aim_gain_y=settings.right_aim_gain_y;
    const bool scene=unified_scene(actor);
    const auto* b=static_cast<const std::uint8_t*>(actor);
    const bool walking=scene&&b[0x115]<=4;
    const bool aiming=unified_aim_allowed(actor,true);
    const bool handoff=aiming&&(unified_new_aim||unified_pitch_route!=2)&&unified_pitch_actor==reinterpret_cast<std::uintptr_t>(actor);
    const unsigned route=walking?1:aiming?2:0;
    const bool measured_aim=settings.re6_response&&aiming;
    const float gain_x=measured_aim?settings.right_aim_gain_x:settings.camera_gain;
    const float gain_y=measured_aim?settings.right_aim_gain_y:settings.camera_gain;
    float x=re4::shape_axis(control.raw_rx,settings.right_deadzone,gain_x,settings.stick_curve);
    float y=re4::shape_axis(control.raw_ry,settings.right_deadzone,gain_y,settings.stick_curve);
    const auto* type=*reinterpret_cast<const int**>(main_base+0xB1AE10);
    if(type&&*type<0)y=-y;
    UnifiedUpdate context{actor,unified_look.update(reinterpret_cast<std::uintptr_t>(actor),route,svcGetSystemTick(),route!=0,x,y,settings.re6_response),walking,false,handoff};
    auto* prior=unified_update;unified_update=&context;
    auto* directions=reinterpret_cast<std::uint64_t*>(main_base+re4::profile::input_flags);
    const auto old_direction=*directions&15u;
    void* prior_movement=movement_actor;
    if(route){
        unified_command_frame=view_frame.load(std::memory_order_relaxed);unified_command_tick=svcGetSystemTick();
        const auto who=reinterpret_cast<std::uintptr_t>(actor);
        if(who!=unified_pitch_actor||route!=unified_pitch_route){
            // Native aim-entry clears/consumes offsets; seed from its current
            // manual target, never replay an old free-orbit state.
            if(who!=unified_pitch_actor){
                unified_pitch=0;
                if(aiming){const float value=*reinterpret_cast<const float*>(main_base+0x1265A0C);if(std::isfinite(value)&&std::fabs(value)<=1)unified_pitch=value*1.57079632679f;}
            }
            unified_pitch_actor=who;unified_pitch_route=route;
        }
        if(walking){
            const auto left=re4::modern_move_disk(re4::shape_axis(control.raw_lx,settings.left_deadzone,1,settings.stick_curve),re4::shape_axis(control.raw_ly,settings.left_deadzone,1,settings.stick_curve));
            if(!dpad_move&&!dpad_turn)*directions=re4::unified_direction_flags(*directions,left.x,left.z);
            movement_actor=actor;
        }
    }else{unified_command_frame=0;if(!settings.unified_controls){unified_pitch_actor=0;unified_pitch_route=0;}}
    original(actor);
    if(walking)*directions=(*directions&~std::uint64_t(15))|old_direction;
    movement_actor=prior_movement;unified_update=prior;
}
bool unified_walk_root(void* actor,float& rotation){
    if(!unified_update||unified_update->actor!=actor||!unified_update->walking||!unified_scene(actor)||dpad_move||dpad_turn)return false;
    const auto* b=static_cast<const std::uint8_t*>(actor);
    if(b[0x115]<1||b[0x115]>3)return false;
    const auto left=re4::modern_move_disk(re4::shape_axis(control.raw_lx,settings.left_deadzone,1,settings.stick_curve),re4::shape_axis(control.raw_ly,settings.left_deadzone,1,settings.stick_curve));
    if(std::hypot(left.x,left.z)<=0)return false;
    const auto* basis=reinterpret_cast<const float*>(b+0x18);
    const float native_yaw=std::atan2(basis[2],basis[10])+(b[0x115]==2?3.14159265359f:0);
    const float desired=*reinterpret_cast<const float*>(b+0xB4)+std::atan2(-left.x,left.z);
    rotation=std::remainder(desired-native_yaw,6.28318530718f);
    return std::isfinite(rotation);
}
bool draw_native_reticle(re4::ViewHudPoint point,unsigned style,unsigned size){
    if(!native_ortho||!native_projection||!native_pose||!native_matrix||!native_rect
        ||!native_gx_blend||!native_gx_format||!native_gx_begin||!native_gx_position||!native_gx_color||!native_gx_end)return false;
    const re4::ReticleDrawBackend backend{
        nullptr,
        +[](void*){
            re4::ViewMatrix4x4 ortho{};
            const re4::ViewMatrix3x4 identity{{{1,0,0,0},{0,1,0,0},{0,0,1,0}}};
            native_ortho(&ortho,0,448,0,512,0,-100);native_projection(&ortho,1);native_pose(&identity,0);native_matrix(0);
            // A degenerate native rect establishes the complete29D140 HUD state
            // without visible fragments. Its untextured vertex-color pipeline
            // passes RGBA unchanged and has no alpha-discard shader operation.
            alignas(8) const std::uint8_t packet[0x18]{};native_rect(packet);
        },
        +[](void*,unsigned a,unsigned b,unsigned c,unsigned d){native_gx_blend(a,b,c,d);},
        +[](void*,unsigned a,unsigned b,unsigned c,unsigned d,unsigned e){native_gx_format(a,b,c,d,e);},
        +[](void*,unsigned a,unsigned b,unsigned c){native_gx_begin(a,b,c);},
        +[](void*,float x,float y,float z){native_gx_position(x,y,z);},
        +[](void*,unsigned r,unsigned g,unsigned b,unsigned a){native_gx_color(r,g,b,a);},
        +[](void*,unsigned token){native_gx_end(token);}
    };
    return re4::draw_reticle(backend,point,style,size);
}
void poll_sensors() {
    gyro_pending = {};
    const auto& previous=gyro_tuning_cache;
    if(!gyro_tuning_ready||settings.gyro_enabled!=previous.gyro_enabled
        ||settings.gyro_speed!=previous.gyro_speed||settings.gyro_gain_x!=previous.gyro_gain_x
        ||settings.gyro_gain_y!=previous.gyro_gain_y||settings.gyro_deadzone_dps!=previous.gyro_deadzone_dps
        ||settings.gyro_bias_x!=previous.gyro_bias_x||settings.gyro_bias_y!=previous.gyro_bias_y
        ||settings.invert_x!=previous.invert_x||settings.invert_y!=previous.invert_y){
        gyro.reset();gyro_frame.reset();gyro_tuning_cache=settings;gyro_tuning_ready=true;
    }
    const auto now = svcGetSystemTick();
    if (sensor.select(sensor_pad, sensor_style, sensor_attributes, settings.gyro_enabled)) {
        gyro.reset();gyro_frame.reset();gyro_session={};last_sensor_tick=0;
        next_sensor_retry=now+19'200'000;last_fresh_tick=now;last_sensor_number=0;
    }
    void* actor = *reinterpret_cast<void**>(main_base + re4::profile::player_slot);
    const bool aiming=firearm_aim(actor);
    if (settings.gyro_enabled && (sensor_attributes&1) && now>=next_sensor_retry
        && (!sensor.running() || (aiming && now-last_fresh_tick>9'600'000))) {
        if (sensor.restart()) {
            gyro.reset();gyro_frame.reset();gyro_session={};last_sensor_tick=0;last_sensor_number=0;last_fresh_tick=now;
            control.sensor_restarts=control.sensor_restarts+1;
        }
        next_sensor_retry=now+19'200'000;
    }
    control.sensor_result = sensor.result;
    control.sensor_running = sensor.running();
    control.gyro_active=0;control.pad_style=sensor_style;
    if (!sensor.running()){gyro_session={};gyro_frame.reset();return;}
    // Tegra's system counter runs at 19.2 MHz. Ignore the first batch and any
    // batch following a >100 ms gap (HOME, overlay pause, controller switch).
    const bool continuous = last_sensor_tick && now >= last_sensor_tick && now - last_sensor_tick <= 1'920'000;
    last_sensor_tick = now;
    if(!continuous)gyro_session={};
    const bool ready=gyro_aim_ready(actor,true);
    // On the readiness edge the FIFO can still contain raising-animation
    // samples. Drain this entire batch before anchoring fresh measurements.
    const bool active = continuous && ready && !gyro_session.drain;
    gyro_frame.begin_frame(now,active);
    control.gyro_active=active;
    if(!active)gyro.reset();
    re4::SensorState samples[16]{};
    const int count = sensor.read(samples, 16);
    if(ready && count>0)gyro_session.drain=false;
    control.sensor_result = sensor.result;
    control.sensor_running = sensor.running();
    constexpr float tau = 6.28318530718f;
    for (int i = count - 1; i >= 0; --i) {
        const auto& s = samples[i];
        const bool connected = s.attributes & 1;
        const float yaw = -s.angular_velocity[1] * tau - settings.gyro_bias_x;
        const float pitch = -s.angular_velocity[0] * tau - settings.gyro_bias_y;
        if(s.number>last_sensor_number){
            last_sensor_number=s.number;last_fresh_tick=now;
            control.sensor_fresh_samples=control.sensor_fresh_samples+1;
            control.gyro_rate_yaw=yaw;control.gyro_rate_pitch=pitch;
        }
        const auto d = gyro.update({s.number, s.interval_ns, yaw, pitch}, active && connected,
            settings.gyro_speed * settings.gyro_gain_x * (settings.invert_x ? -1 : 1),
            settings.gyro_speed * settings.gyro_gain_y * (settings.invert_y ? -1 : 1),settings.gyro_deadzone_dps);
        if(!connected||gyro.discontinuity())gyro_frame.reset();
        if(gyro.emitted_ns())gyro_frame.push(d,gyro.emitted_ns());
    }
    gyro_pending=gyro_frame.finish_frame();
    control.sensor_samples = control.sensor_samples + count;
}

void publish_graphics_settings(){
    graphics_snapshot.store(re4::pack_graphics({settings.texture_filtering,settings.shadow_mode,
        settings.anti_aliasing,settings.draw_distance_scale},file_read),std::memory_order_release);
}
re4::GraphicsOptions render_graphics(std::uint64_t word){
    return re4::graphics_ready(word)?re4::unpack_graphics(word):re4::GraphicsOptions{};
}
unsigned latch_graphics(std::atomic<unsigned>& latch,unsigned requested,bool ready){
    auto mode=latch.load(std::memory_order_acquire);
    if(mode==255 && ready){
        unsigned pending=255;latch.compare_exchange_strong(pending,requested,std::memory_order_acq_rel);
        mode=latch.load(std::memory_order_acquire);
    }
    return mode==255?0:mode;
}
void publish_graphics_diagnostics(){
    control.fps_overrides=fps_override_count.load(std::memory_order_relaxed);
    control.reticle_draws=reticle_draw_count.load(std::memory_order_relaxed);
    control.graphics_sampler_calls=sampler_calls.load(std::memory_order_relaxed);
    control.graphics_sampler_overrides=sampler_overrides.load(std::memory_order_relaxed);
    control.graphics_filter_latched=filter_latched.load(std::memory_order_acquire);
    control.graphics_aa_latched=aa_latched.load(std::memory_order_acquire);
    control.graphics_scene_submits=scene_submits.load(std::memory_order_relaxed);
    control.graphics_scene_draws=scene_draws.load(std::memory_order_relaxed);
    control.graphics_cull_updates=cull_updates.load(std::memory_order_relaxed);
    control.graphics_resolution_requests=resolution_overrides.load(std::memory_order_relaxed);
    control.graphics_ready=re4::graphics_ready(graphics_snapshot.load(std::memory_order_acquire));
    const auto pair=last_cull_pair.load(std::memory_order_relaxed);
    const auto native=std::uint32_t(pair>>32),effective=std::uint32_t(pair);
    float value;std::memcpy(&value,&native,4);control.graphics_native_far=value;
    std::memcpy(&value,&effective,4);control.graphics_effective_far=value;
    const auto* renderer=reinterpret_cast<const volatile std::int32_t*>(main_base+0x1180D40);
    control.graphics_display_index=renderer[0x30/4];control.graphics_back_index=renderer[0x28/4];
}
void publish_settings() {
    publish_view_settings();
    std::uint8_t bytes[re4::kSettingsBytes];
    if (!re4::encode_settings(settings, bytes, sizeof(bytes))) return;
    control.sequence = control.sequence | 1;
    std::atomic_thread_fence(std::memory_order_seq_cst);
    for (unsigned i = 0; i < sizeof(bytes); ++i) control.settings[i] = bytes[i];
    std::atomic_thread_fence(std::memory_order_seq_cst);
    control.sequence = (control.sequence + 1) & ~1u;
    last_sequence = control.sequence;
    publish_graphics_settings();
    re4::acknowledge_settings(control, settings, last_sequence);
}
bool read_settings(const char* path) {
    nn::fs::FileHandle handle{};
    control.settings_read_result = nn::fs::OpenFile(&handle, path, 1);
    if (R_FAILED(control.settings_read_result)) return false;
    long size = 0;
    std::uint8_t bytes[re4::kSettingsBytes];
    re4::Settings candidate;
    control.settings_read_result = nn::fs::GetFileSize(&size, handle);
    bool valid = R_SUCCEEDED(control.settings_read_result) && (size == sizeof(bytes) || size == re4::kLegacySettingsBytes);
    if(valid){
        control.settings_read_result = nn::fs::ReadFile(handle, 0, bytes, size);
        valid = R_SUCCEEDED(control.settings_read_result) && re4::decode_settings(bytes, size, candidate);
    }
    nn::fs::CloseFile(handle);
    if(valid)settings=candidate;
    else if(!control.settings_read_result)control.settings_read_result=0xFFFFFFFFu;
    return valid;
}
void read_initial_settings() {
    if (file_read) return;
    const auto now=svcGetSystemTick();
    if(now<next_file_retry)return;
    next_file_retry=now+19'200'000; // At most once per second, eight attempts total.
    control.settings_attempts=++file_attempts;
    if(!file_mounted){
        control.settings_mount_result=nn::fs::MountSdCardForDebug("r4ctl");
        file_mounted=R_SUCCEEDED(control.settings_mount_result);
    }
    if(file_mounted){
        if(read_settings("r4ctl:/config/re4-controls/settings.bin"))control.settings_source=1;
        else if(read_settings("r4ctl:/config/re4-controls/settings.bin.previous"))control.settings_source=2;
        if(control.settings_source==1 || control.settings_source==2){
            file_read=true;publish_settings();return;
        }
    }
    if(file_attempts>=8){file_read=true;control.settings_source=3;publish_graphics_settings();}
}
void poll_settings() {
    const auto before = control.sequence;
    if ((before & 1) || before == last_sequence) return;
    std::atomic_thread_fence(std::memory_order_acquire);
    std::uint8_t bytes[re4::kSettingsBytes];
    for (unsigned i = 0; i < sizeof(bytes); ++i) bytes[i] = control.settings[i];
    std::atomic_thread_fence(std::memory_order_seq_cst);
    if (control.sequence != before || !re4::decode_settings(bytes, sizeof(bytes), settings)) return;
    last_sequence = before;
    publish_view_settings();
    file_read = true; // A live update always wins over a delayed startup read.
    control.settings_source = 4;
    publish_graphics_settings();
    re4::acknowledge_settings(control, settings, before);
}
void capture(NpadState& pad, std::uint32_t id, std::uint32_t style) {
    telemetry_write_input(pad,id,style); // Exact original SDK state, before remapping.
    // Transform the original SDK state once, including menu and QTE input.
    // Overlay/HOME controls belong to other processes and remain physical.
    pad.buttons=re4::remap_buttons(pad.buttons,settings.button_map,settings.remap_enabled);
    control.pad_samples = control.pad_samples + 1;
    control.last_pad_id = id;
    if (id != 0 && id != 0x20) return;
    const bool connected = pad.attributes & 1;
    const bool changed=connected?(sensor_pad!=id||sensor_style!=style||sensor_attributes!=(pad.attributes&21u))
        :(id==sensor_pad&&style==sensor_style&&sensor_attributes!=0);
    if(changed){unified_look.reset();unified_aim_session={};gyro_session={};gyro_pending={};gyro.reset();gyro_frame.reset();}
    if (connected) {
        sensor_pad = id; sensor_style = style; sensor_attributes = pad.attributes & 21u;
    } else if (id == sensor_pad && style == sensor_style) sensor_attributes = 0;
    raw_left_x = connected ? pad.lx / 32767.f : 0;
    if (std::fabs(raw_left_x) > control.maximum_raw_lx) control.maximum_raw_lx = std::fabs(raw_left_x);
    dpad_turn = connected && (pad.buttons & ((1ull << 12) | (1ull << 14)));
    dpad_move = connected && (pad.buttons & ((1ull << 13) | (1ull << 15)));
    control.raw_lx = raw_left_x;
    control.raw_ly = connected ? pad.ly / 32767.f : 0;
    control.raw_rx = connected ? pad.rx / 32767.f : 0;
    control.raw_ry = connected ? pad.ry / 32767.f : 0;
}

HOOK_DEFINE_TRAMPOLINE(RomMount) {
    static Result Callback(const char* name,void* cache,std::size_t size){
        const Result result=Orig(name,cache,size);
        if(R_SUCCEEDED(result) && !file_read && !rom_settings_attempted.test_and_set(std::memory_order_acq_rel)){
            next_file_retry=0;read_initial_settings(); // FS is ready before graphics allocations.
        }
        return result;
    }
};
HOOK_DEFINE_TRAMPOLINE(SamplerCreate) {
    static void Callback(void* sampler,void* device,const std::uint8_t* descriptor){
        sampler_calls.fetch_add(1,std::memory_order_relaxed);
        const auto word=graphics_snapshot.load(std::memory_order_acquire);
        const auto options=render_graphics(word);
        const auto mode=static_cast<re4::TextureFilter>(latch_graphics(filter_latched,unsigned(options.filter),re4::graphics_ready(word)));
        alignas(16) std::uint8_t copy[20];
        const bool changed=re4::filter_sampler_descriptor(descriptor,copy,sizeof(copy),mode);
        if(changed)sampler_overrides.fetch_add(1,std::memory_order_relaxed);
        Orig(sampler,device,changed?copy:descriptor);
    }
};
HOOK_DEFINE_TRAMPOLINE(ResolutionMap) {
    static void Callback(int width,int height,int* display,int* back){
        Orig(width,height,display,back);
        if(!display || !back)return;
        const auto word=graphics_snapshot.load(std::memory_order_acquire);
        const auto options=render_graphics(word);
        const auto mode=static_cast<re4::AntiAliasing>(latch_graphics(aa_latched,unsigned(options.aa),re4::graphics_ready(word)));
        const int desired=mode==re4::AntiAliasing::HandheldSsaa && get_operation_mode()==0
            ?re4::resolution_back_index(mode,width,height,*display,*back):*back;
        if(desired!=*back){*back=desired;resolution_overrides.fetch_add(1,std::memory_order_relaxed);}
    }
};
HOOK_DEFINE_TRAMPOLINE(FrustumUpdate) {
    static void Callback(void* frustum){
        if(frustum!=reinterpret_cast<void*>(main_base+0x1272FA0)){Orig(frustum);return;}
        const auto options=render_graphics(graphics_snapshot.load(std::memory_order_acquire));
        auto* far=reinterpret_cast<float*>(static_cast<std::uint8_t*>(frustum)+0x18);
        const float native=*far;
        const float effective=re4::culling_distance(native,*reinterpret_cast<const float*>(main_base+0x124ED40),
            *reinterpret_cast<const float*>(main_base+0x124ED44),options.draw_distance);
        const bool changed=std::isfinite(native) && effective!=native;
        if(changed)*far=effective;
        Orig(frustum); // Native plane regeneration/cache sees the chosen distance.
        if(changed)*far=native;
        cull_updates.fetch_add(1,std::memory_order_relaxed);
        std::uint32_t a,b;std::memcpy(&a,&native,4);std::memcpy(&b,&effective,4);
        last_cull_pair.store(std::uint64_t(a)<<32|b,std::memory_order_relaxed);
    }
};
template<class F> void shadow_frame(F original){
    const auto options=render_graphics(graphics_snapshot.load(std::memory_order_acquire));
    auto* state=*reinterpret_cast<std::uint8_t**>(main_base+0xB1AE08);
    if(options.shadows==re4::ShadowMode::Native || !state){original();return;}
    auto* flags=reinterpret_cast<std::uint32_t*>(state+0x7C);
    const auto saved=*flags;*flags=re4::shadow_flags(options.shadows,saved);
    original();
    if(state==*reinterpret_cast<std::uint8_t**>(main_base+0xB1AE08))*flags=re4::restore_shadow_flags(*flags,saved);
}
HOOK_DEFINE_TRAMPOLINE(ModelSubmit) {
    static void Callback(void* model){
        if(model&&model==main_view_actor&&(view_settings_snapshot.load(std::memory_order_acquire)&1)
            && first_person_frame.load(std::memory_order_acquire)==view_frame.load(std::memory_order_acquire)
            && view_completed(model))return;
        Orig(model);
    }
};
HOOK_DEFINE_TRAMPOLINE(SceneSubmit) {
    static void Callback(){
        scene_submits.fetch_add(1,std::memory_order_relaxed);
        auto* actor=*reinterpret_cast<void**>(main_base+re4::profile::player_slot);
        auto* previous=main_view_actor;
        main_view_actor=(view_settings_snapshot.load(std::memory_order_acquire)&1)
            && first_person_frame.load(std::memory_order_acquire)==view_frame.load(std::memory_order_acquire)
            && view_completed(actor)?actor:nullptr;
        shadow_frame([]{Orig();});
        main_view_actor=previous;
        pending_aim_point.frame.store(0,std::memory_order_release);
        re4::ViewVec3 point;
        const auto frame=view_frame.load(std::memory_order_acquire);
        if((view_settings_snapshot.load(std::memory_order_acquire)&38)&&view_completed(actor)
            &&read_aim_point(aim_point,actor,frame,point)){
            publish_aim_point(pending_aim_point,actor,player_weapon(actor),point,frame);
        }
    }
};
HOOK_DEFINE_TRAMPOLINE(SceneDraw) {
    static void Callback(){
        scene_draws.fetch_add(1,std::memory_order_relaxed);
        displayed_aim_point.frame.store(0,std::memory_order_release);
        auto* actor=*reinterpret_cast<void**>(main_base+re4::profile::player_slot);
        const auto frame=view_frame.load(std::memory_order_acquire);
        const auto* state=*reinterpret_cast<const std::uint8_t**>(main_base+0xB1AE08);
        re4::ViewVec3 point;
        // Queued scene belongs to the previous simulation. Capture its main
        // camera before native group23 temporarily swaps in a special camera.
        if(frame>1&&(view_settings_snapshot.load(std::memory_order_acquire)&38)&&view_actor_state(actor)&&state
            &&read_aim_point(pending_aim_point,actor,frame-1,point)){
            re4::ViewMatrix3x4 view;re4::ViewMatrix4x4 projection;
            std::memcpy(&view,state+0xD8,sizeof(view));std::memcpy(&projection,state+0x10C,sizeof(projection));
            publish_aim_point(displayed_aim_point,actor,player_weapon(actor),point,frame,&view,&projection);
        }
        pending_aim_point.frame.store(0,std::memory_order_release);
        shadow_frame([]{Orig();});
    }
};
HOOK_DEFINE_TRAMPOLINE(FullPad) {
    static void Callback(NpadState* out, const std::uint32_t& id) { Orig(out, id); capture(*out, id, 1); }
};
HOOK_DEFINE_TRAMPOLINE(HandheldPad) {
    static void Callback(NpadState* out, const std::uint32_t& id) { Orig(out, id); capture(*out, id, 2); }
};
HOOK_DEFINE_TRAMPOLINE(DualPad) {
    static void Callback(NpadState* out, const std::uint32_t& id) { Orig(out, id); capture(*out, id, 4); }
};
HOOK_DEFINE_TRAMPOLINE(PadNormalize) {
    static int Callback(void* state) {
        const int result = Orig(state);
        view_frame.fetch_add(1,std::memory_order_acq_rel);
        poll_settings(); read_initial_settings();publish_view_settings();
        poll_sensors();
        publish_graphics_diagnostics();
        control.frames = control.frames + 1;
        return result;
    }
};
HOOK_DEFINE_TRAMPOLINE(MovementTurn) {
    static void Callback(void* actor) {
        auto* bytes = static_cast<std::uint8_t*>(actor);
        const bool ours = actor && actor == *reinterpret_cast<void**>(main_base + re4::profile::player_slot);
        const bool normal = ours && bytes[0x114] == 0 && bytes[0x115] <= 4;
        if(normal&&unified_update&&unified_update->actor==actor&&unified_update->walking
            &&!unified_update->consumed&&unified_scene(actor)){
            auto* yaw=reinterpret_cast<float*>(bytes+0xB4);
            *yaw=re4::gyro_yaw(*yaw,unified_update->delta.yaw);
            unified_pitch+=re4::unified_stick_pitch_delta(unified_pitch,unified_update->delta.pitch);
            unified_update->consumed=true;
        }
        auto* flags = reinterpret_cast<volatile std::uint64_t*>(main_base + re4::profile::input_flags);
        const std::uint64_t old_direction = normal ? *flags & 15u : 0;
        const bool remap = normal && settings.analog_enabled && !(unified_update&&unified_update->actor==actor&&unified_update->walking);
        const float before = normal ? *reinterpret_cast<float*>(bytes + 0xB4) : 0;
        if (remap) {
            *flags = (*flags & ~std::uint64_t(12)) | (re4::turn_flags(0, raw_left_x,
                dpad_turn, true, settings.left_deadzone) & 12u);
            // Preserve digital pad direction when the user uses the D-pad.
            if (dpad_turn) *flags = (*flags & ~std::uint64_t(12)) | (old_direction & 12u);
            if (!dpad_move) {
                const float axis = re4::shape_axis(control.raw_ly, settings.left_deadzone, 1);
                *flags = (*flags & ~std::uint64_t(3)) | (axis > 0 ? 1u : axis < 0 ? 2u : 0u);
            }
        }
        const bool manual = remap && (*flags & 12u);
        void* previous_actor = movement_actor;
        movement_actor = remap ? actor : (unified_update&&unified_update->walking?actor:nullptr);
        Orig(actor);
        movement_actor = previous_actor;
        if (remap) *flags = (*flags & ~std::uint64_t(15)) | old_direction;
        if (normal) control.direction_calls = control.direction_calls + 1;
        if (!manual || bytes[0x114] != 0 || bytes[0x115] > 4) return;
        const float after = *reinterpret_cast<float*>(bytes + 0xB4);
        const float factor = re4::turn_factor(raw_left_x, dpad_turn, settings.left_deadzone, settings.turn_gain, settings.stick_curve);
        const float corrected = re4::corrected_turn_yaw(before, after, factor);
        if (!std::isfinite(before) || !std::isfinite(after) || !std::isfinite(corrected)) return;
        control.speed_calls = control.speed_calls + 1;
        control.last_turn_factor = factor;
        control.original_yaw_delta = re4::yaw_delta(before, after);
        control.corrected_yaw_delta = re4::yaw_delta(before, corrected);
        // Original walk/run handlers also write actor yaw AFTER root-motion
        // update. Normal game rendering rebuilds its matrices later in frame.
        *reinterpret_cast<float*>(bytes + 0xB4) = corrected;
    }
};
// The motion frame increment drives both the gait and next frame's root
// displacement. Collision/ground handling still receives the game's motion.
HOOK_DEFINE_TRAMPOLINE(MotionAdvance) {
    static unsigned Callback(void* motion) {
        auto* actor = static_cast<std::uint8_t*>(movement_actor);
        const bool walking = actor && actor[0x114] == 0 && actor[0x115] >= 1 && actor[0x115] <= 3;
        const bool ours = walking && (motion == actor + 0x228 || motion == *reinterpret_cast<void**>(actor + 0x320));
        if (!ours || dpad_move) return Orig(motion);
        auto* speed = reinterpret_cast<float*>(static_cast<std::uint8_t*>(motion) + 0xDC);
        const float original = *speed;
        const float factor = settings.unified_controls
            ?std::hypot(re4::modern_move_disk(re4::shape_axis(control.raw_lx,settings.left_deadzone,1,settings.stick_curve),re4::shape_axis(control.raw_ly,settings.left_deadzone,1,settings.stick_curve)).x,
                re4::modern_move_disk(re4::shape_axis(control.raw_lx,settings.left_deadzone,1,settings.stick_curve),re4::shape_axis(control.raw_ly,settings.left_deadzone,1,settings.stick_curve)).z)
            :std::fabs(re4::shape_axis(control.raw_ly, settings.left_deadzone, 1, settings.stick_curve));
        if (!std::isfinite(original) || std::fabs(original) > 16) return Orig(motion);
        *speed = original * factor;
        control.movement_calls = control.movement_calls + 1;
        control.movement_factor = factor;
        const auto result = Orig(motion);
        *speed = original;
        return result;
    }
};
HOOK_DEFINE_TRAMPOLINE(CameraBasis) {
    static void Callback(void* camera,void* output){
        Orig(camera,output);
        auto* context=camera_context;
        if(context&&context->camera==camera&&context->view_allowed&&output){
            std::memcpy(&context->basis,output,sizeof(context->basis));
            re4::ViewVec3 checked;
            context->basis_valid=re4::view_inverse_point(context->basis,{},checked);
        }
    }
};
HOOK_DEFINE_TRAMPOLINE(CameraApply) {
    static void Callback(void* camera,void* output){
        const auto* context=camera_context;
        const auto* bytes=static_cast<const std::uint8_t*>(camera);
        const bool fps=settings.first_person && context && context->view_allowed;
        if(!settings.unified_controls && context && context->camera==camera && ordinary_camera(camera,context->actor) && (settings.analog_enabled||fps)
            && !context->reset && !bytes[0x222] && bytes[0x26C]==2){
            if(settings.free_camera||fps){
                auto* offsets=reinterpret_cast<float*>(static_cast<std::uint8_t*>(camera)+0x270);
                const auto value=free_orbit.update(reinterpret_cast<std::uintptr_t>(camera),svcGetSystemTick(),
                    context->before,context->x,context->y);
                offsets[0]=value.pitch;offsets[1]=value.yaw;
            }else{
                free_orbit.reset();
                if(context->neutral&&settings.camera_return_speed!=1){
                    auto* offsets=reinterpret_cast<float*>(static_cast<std::uint8_t*>(camera)+0x270);
                    const auto value=re4::camera_return(context->before,{offsets[0],offsets[1]},true,settings.camera_return_speed);
                    offsets[0]=value.pitch;offsets[1]=value.yaw;
                }
            }
        }else if(context&&context->camera==camera)free_orbit.reset();
        auto* profile_pitch=reinterpret_cast<float*>(static_cast<std::uint8_t*>(camera)+0x270);
        const float native_profile_pitch=*profile_pitch;
        const bool stable_profile=context&&context->camera==camera&&context->view_allowed&&!context->reset&&!bytes[0x222]
            &&unified_view_ready(context->actor)&&std::isfinite(native_profile_pitch);
        if(stable_profile)*profile_pitch=std::clamp(re4::unified_pitch_weight(unified_pitch),-1.f,1.f);
        Orig(camera,output); // Native profiles first;18990 still collides the chosen eye afterward.
        if(stable_profile)*profile_pitch=native_profile_pitch; // Do not replace native weapon/camera bookkeeping.

        if(!context||context->camera!=camera||!context->view_allowed||!output||context->reset||bytes[0x222]
            ||!context->basis_valid||!view_camera_allowed(camera,context->actor))return;
        const auto* actor=static_cast<const std::uint8_t*>(context->actor);
        const bool native_aim=aiming_predicate(context->actor)!=0;
        if(bytes[0x26C]!=(native_aim?0:2))return;
        re4::ViewQfpsOutput native;std::memcpy(&native,output,sizeof(native));
        // Also validate unmodified native output before publishing render eligibility.
        if(!re4::view_finite(native.eye0)||!re4::view_finite(native.eye1)||!re4::view_finite(native.target)
            ||!std::isfinite(native.roll)||!std::isfinite(native.fov))return;
        if(fps){
            re4::ViewMatrix3x4 body;std::memcpy(&body,actor+0x18,sizeof(body));
            re4::ViewVec3 eye;re4::ViewQfpsOutput result;
            if(!re4::view_inverse_point(body,{},eye)||!re4::view_transform_point(body,{0,1700,0},eye)
                ||!re4::first_person_view(native,context->basis,eye,result))return;
            std::memcpy(output,&result,sizeof(result));
            fps_override_count.fetch_add(1,std::memory_order_relaxed);
            first_person_frame.store(view_frame.load(std::memory_order_relaxed),std::memory_order_release);
        }else if(stable_profile&&settings.re6_response
            &&!(*reinterpret_cast<const std::uint8_t**>(main_base+0xB1AE08))[0x503C]){
            re4::ViewMatrix3x4 body;std::memcpy(&body,actor+0x18,sizeof(body));
            re4::ViewVec3 root,checked;re4::ViewQfpsOutput result;
            if(!re4::view_inverse_point(body,{},checked)||!re4::view_transform_point(body,{},root)
                ||!re4::re6_shoulder_view(native,context->basis,root,
                    *reinterpret_cast<const float*>(actor+0xB4),unified_pitch,settings.third_person_distance,result))return;
            // Keep native eye1 and let the next native18990 wall trace decide
            // the final eye. Never fix composition after collision.
            std::memcpy(output,&result,sizeof(result));
        }else if(settings.third_person_distance!=1){
            re4::ViewQfpsOutput result;
            if(!re4::third_person_view(native,settings.third_person_distance,result))return;
            // 18990 traces eye1->eye0 and selects clipped candidates nearest
            // eye1. Keep that anchor; native collision/smoothing runs next.
            std::memcpy(output,&result,sizeof(result));
        }
        view_camera_aim.store(native_aim,std::memory_order_relaxed);
        view_camera_actor.store(reinterpret_cast<std::uintptr_t>(actor),std::memory_order_relaxed);
        view_camera_tick.store(svcGetSystemTick(),std::memory_order_relaxed);
        view_camera_frame.store(view_frame.load(std::memory_order_relaxed),std::memory_order_release);
    }
};
HOOK_DEFINE_TRAMPOLINE(CameraInput) {
    static void Callback(void* camera) {
        publish_view_settings();
        clear_view_frame();
        auto* actor=*reinterpret_cast<std::uint8_t**>(main_base+re4::profile::player_slot);
        const bool normal=actor&&actor[0x114]==0&&actor[0x115]<=4;
        const bool framing=settings.third_person_distance>1&&settings.third_person_distance<=2;
        const bool wanted=settings.unified_controls||settings.first_person||framing||settings.reticle!=re4::ReticleStyle::Off;
        const bool view_allowed=wanted&&view_camera_allowed(camera,actor);
        const bool unified=settings.unified_controls&&unified_scene(actor);
        const bool shape=!unified&&normal&&(settings.analog_enabled||(settings.first_person&&view_allowed));
        const bool ordinary=normal&&settings.analog_enabled&&ordinary_camera(camera,actor);
        if(!shape&&!view_allowed){free_orbit.reset();Orig(camera);return;}
        const bool alternate=*reinterpret_cast<int*>(main_base+0x11C7D68)==1;
        auto* input=reinterpret_cast<std::int8_t*>(main_base+0x124EC38+(alternate?0xC0:8));
        const auto x=input[0],y=input[1];
        if(unified){
            input[0]=input[1]=0;free_orbit.reset();
            if(normal){auto* offsets=reinterpret_cast<float*>(static_cast<std::uint8_t*>(camera)+0x270);offsets[0]=offsets[1]=0;}
        }
        if(shape){
            control.camera_calls=control.camera_calls+1;
            input[0]=re4::camera_axis(control.raw_rx,settings.right_deadzone,settings.camera_gain,settings.stick_curve);
            input[1]=re4::camera_axis(control.raw_ry,settings.right_deadzone,settings.camera_gain,settings.stick_curve);
        }
        CameraContext context{camera,actor,{},input[0]==0&&input[1]==0,false,
            re4::shape_axis(control.raw_rx,settings.right_deadzone,settings.camera_gain,settings.stick_curve),
            re4::shape_axis(control.raw_ry,settings.right_deadzone,settings.camera_gain,settings.stick_curve)};
        auto* previous=camera_context;
        if(ordinary||view_allowed){
            const auto* bytes=static_cast<const std::uint8_t*>(camera);
            const auto* offsets=reinterpret_cast<const float*>(bytes+0x270);
            context.before={offsets[0],offsets[1]};context.reset=bytes[0x222]!=0;context.view_allowed=view_allowed;
            const auto* type=*reinterpret_cast<const int**>(main_base+0xB1AE10);
            if(type&&*type<0)context.y=-context.y;
            camera_context=&context;
        }else{camera_context=nullptr;free_orbit.reset();}
        Orig(camera);
        camera_context=previous;
        if(shape||unified){input[0]=x;input[1]=y;}
        if(view_allowed&&!view_camera_allowed(camera,actor))clear_view_frame();
    }
};
HOOK_DEFINE_TRAMPOLINE(CameraCalculate) {
    static void Callback(){
        const bool previous=unified_camera_calculation;unified_camera_calculation=true;
        Orig();unified_camera_calculation=previous;
        const auto frame=view_frame.load(std::memory_order_acquire);
        if(frame==telemetry_last_frame)return;
        telemetry_last_frame=frame;
        re6::Record record{};record.frame=frame;record.tick=svcGetSystemTick();
        telemetry_read_input(record.tick,record);
        auto* actor=*reinterpret_cast<const std::uint8_t**>(main_base+re4::profile::player_slot);
        const auto* state=*reinterpret_cast<const std::uint8_t**>(main_base+0xB1AE08);
        if(view_scene(const_cast<std::uint8_t*>(actor))){
            record.actor=reinterpret_cast<std::uintptr_t>(actor);record.camera=main_base+0x11319C0;
            std::memcpy(record.player,actor+0x18,sizeof(record.player));
            std::memcpy(record.view,state+0xD8,sizeof(record.view));
            std::memcpy(record.projection,state+0x10C,sizeof(record.projection));
            record.validity|=re6::PlayerValid|re6::CameraValid|re6::ProjectionValid;
        }
        telemetry_ring.capture(record);
    }
};
HOOK_DEFINE_TRAMPOLINE(FinalLookAt) {
    static void Callback(void* output,const re4::ViewVec3* eye,const re4::ViewVec3* up,const re4::ViewVec3* target){
        auto* state=*reinterpret_cast<std::uint8_t**>(main_base+0xB1AE08);
        auto* actor=*reinterpret_cast<std::uint8_t**>(main_base+re4::profile::player_slot);
        if(unified_camera_calculation&&state&&output==state+0xD8&&eye==reinterpret_cast<void*>(state+0x14C)
            &&up==reinterpret_cast<void*>(state+0x16C)&&target==reinterpret_cast<void*>(state+0x158)
            &&unified_view_ready(actor)&&view_completed(actor)){
            const float heading=*reinterpret_cast<const float*>(actor+0xB4),pitch=unified_pitch;
            const double length=std::hypot(double(target->x)-eye->x,double(target->y)-eye->y,double(target->z)-eye->z);
            const double sh=std::sin(double(heading)),ch=std::cos(double(heading)),sp=std::sin(double(pitch)),cp=std::cos(double(pitch));
            re4::ViewVec3 endpoint,stable_up;float distance;
            // Camera direction belongs to the look command. Native animated
            // muzzle/collision points only determine the separately projected HUD.
            if(std::isfinite(heading)&&length>1e-4
                &&re4::view_store_point(eye->x+sh*cp*length,eye->y+sp*length,eye->z+ch*cp*length,endpoint)
                &&re4::view_store_point(-sh*sp,cp,-ch*sp,stable_up)
                &&re4::unified_target(*eye,endpoint,stable_up,endpoint,distance)){
                std::memcpy(state+0x158,&endpoint,sizeof(endpoint));std::memcpy(state+0x16C,&stable_up,sizeof(stable_up));
                std::memcpy(state+0x19C,&distance,sizeof(distance));
            }
        }
        Orig(output,eye,up,target); // Native FDB0 regenerates frustum after this returns.
    }
};
HOOK_DEFINE_TRAMPOLINE(WeaponLaser) {
    static void Callback(void* weapon,unsigned draw,unsigned reuse,unsigned style){
        Orig(weapon,draw,reuse,style); // Keep native laser, collision and effects unchanged.
        if(reuse&1)return;
        auto* actor=*reinterpret_cast<std::uint8_t**>(main_base+re4::profile::player_slot);
        if(!(view_settings_snapshot.load(std::memory_order_acquire)&38)||!view_scene(actor)
            ||!actor||(actor[0x115]!=6&&actor[0x115]!=15)||!firearm_aim(actor)||weapon!=player_weapon(actor))return;
        // Native skips model+40C copy when the clipped ray is shorter than500.
        // The shared endpoint is already rebuilt/clipped on every reuse==0 call.
        re4::ViewVec3 endpoint;std::memcpy(&endpoint,reinterpret_cast<const void*>(main_base+0x1265320),sizeof(endpoint));
        if(re4::view_finite(endpoint))publish_aim_point(aim_point,actor,weapon,endpoint,view_frame.load(std::memory_order_relaxed));
    }
};
HOOK_DEFINE_TRAMPOLINE(HudDraw) {
    static void Callback(void* list){
        Orig(list);
        const auto options=view_settings_snapshot.load(std::memory_order_acquire);
        const auto style=(options>>1)&3u,size=(options>>3)&3u;
        if(!style||style>2||size>2||!native_ortho||!native_projection||!native_pose||!native_matrix||!native_rect)return;
        auto* actor=*reinterpret_cast<std::uint8_t**>(main_base+re4::profile::player_slot);
        if(!actor||(actor[0x115]!=6&&actor[0x115]!=15)||!view_completed(actor)
            ||!view_camera_aim.load(std::memory_order_acquire)
            ||!(*reinterpret_cast<const std::uint64_t*>(main_base+re4::profile::input_flags)&0x10))return;
        const auto* state=*reinterpret_cast<const std::uint8_t**>(main_base+0xB1AE08);
        if(!state||(*reinterpret_cast<const std::uint32_t*>(state+0x7C)&(1u<<13)))return;
        re4::ViewVec3 endpoint;re4::ViewHudPoint point;
        re4::ViewMatrix3x4 view;re4::ViewMatrix4x4 projection;
        if(!read_aim_point(displayed_aim_point,actor,view_frame.load(std::memory_order_acquire),endpoint,&view,&projection)
            ||!re4::project_view_point(view,projection,endpoint,point))return;
        const bool drawn=draw_native_reticle(point,style,size);
        if(drawn)reticle_draw_count.fetch_add(1,std::memory_order_relaxed);
    }
};
#include "aim_raise.inc"
#include "modern_move.inc"
HOOK_DEFINE_TRAMPOLINE(RootDisplacement) {
    static void Callback(void* actor,void* motion,unsigned flags,re4::ViewVec3* translation,re4::ViewVec3* rotation){
        Orig(actor,motion,flags,translation,rotation);
        if(!actor||actor!=unified_root_actor||!translation||!unified_scene(actor))return;
        const auto* b=static_cast<const std::uint8_t*>(actor);
        if(motion!=b+0x228&&motion!=*reinterpret_cast<void* const*>(b+0x320))return;
        re4::ViewMatrix3x4 basis;std::memcpy(&basis,b+0x18,sizeof(basis));
        re4::ViewVec3 redirected;
        if(re4::unified_redirect_root(basis,*translation,unified_root_yaw,redirected))*translation=redirected;
    }
};


HOOK_DEFINE_TRAMPOLINE(AimSensitivity) {
    static int Callback(void* actor, int mode) {
        auto* bytes = static_cast<std::uint8_t*>(actor);
        const bool ours = actor && actor == *reinterpret_cast<void**>(main_base + re4::profile::player_slot);
        const auto flags=*reinterpret_cast<std::uint64_t*>(main_base+re4::profile::input_flags);
        const bool held=ours&&(bytes[0x115]==11?(flags&re4::kKnifeHeldMask):(flags&0x10));
        const bool modern=ours && modern_move_aim_active(actor);
        const bool unified=ours&&settings.unified_controls&&held&&(bytes[0x115]==6||bytes[0x115]==11)&&unified_scene(actor);
        const bool active = ours && (settings.analog_enabled || settings.gyro_enabled || modern || unified) && bytes[0x114] == 0
            && (bytes[0x115] == 6 || bytes[0x115] == 11 || bytes[0x115] == 15) && held;
        if (!active){
            if(ours){
                gyro_session={};gyro_pending={};gyro.reset();gyro_frame.reset();
                if(settings.unified_controls){
                    unified_aim_session={};unified_look.reset();
                    if(unified_update&&unified_update->actor==actor)unified_update->consumed=true;
                }
            }
            return Orig(actor, mode);
        }
        auto* pitch = reinterpret_cast<float*>(main_base + 0x1265A08);
        auto* yaw = reinterpret_cast<float*>(bytes + 0xB4);
        void* previous_input_actor = gyro_input_actor;
        gyro_input_actor = nullptr;
        if(unified&&unified_update&&unified_update->actor==actor&&!unified_update->consumed&&unified_aim_allowed(actor)){
            const auto d=unified_update->delta;unified_update->consumed=true;
            *yaw=re4::gyro_yaw(*yaw,d.yaw);
            const float applied_pitch=re4::unified_stick_pitch_delta(unified_pitch,d.pitch);
            unified_pitch+=applied_pitch;
            const float target=std::clamp(re4::unified_pitch_weight(unified_pitch),-1.f,1.f);
            pitch[0]=pitch[1]=target;
            if(unified_update->handoff||d.yaw!=0||applied_pitch!=0)gyro_input_actor=actor;
        }
        // Apply once before native stick/weapon processing, retaining both
        // inputs and letting the caller update aim matrices normally.
        if (settings.gyro_enabled && gyro_aim_ready(actor)) {
            const auto d = gyro_pending;
            gyro_pending = {};
            const float old_yaw = *yaw, old_pitch = pitch[1];
            *yaw = re4::gyro_yaw(old_yaw, d.yaw);
            if(unified){
                const auto who=reinterpret_cast<std::uintptr_t>(actor);
                if(unified_pitch_actor!=who){
                    unified_pitch=std::isfinite(old_pitch)&&std::fabs(old_pitch)<=1?old_pitch*1.57079632679f:0;
                    unified_pitch_actor=who;unified_pitch_route=2;
                }
                if(std::isfinite(d.pitch))unified_pitch=std::clamp(unified_pitch+d.pitch,-1.57079632679f,1.57079632679f);
                pitch[0]=pitch[1]=std::clamp(re4::unified_pitch_weight(unified_pitch),-1.f,1.f);
            }else{
                pitch[0]=re4::gyro_pitch(pitch[0],d.pitch);pitch[1]=re4::gyro_pitch(old_pitch,d.pitch);
            }
            if (*yaw != old_yaw || pitch[1] != old_pitch) gyro_input_actor = actor;
            control.gyro_yaw = d.yaw; control.gyro_pitch = d.pitch;
        }else {gyro_pending={};gyro.reset();gyro_frame.reset();}
        auto* axes=reinterpret_cast<std::int32_t*>(main_base+0x124E9C8);
        const std::int32_t old_x=axes[0],old_y=axes[1];
        auto* aim_directions=reinterpret_cast<std::uint32_t*>(main_base+0x124E9C8+0x14);
        const auto old_aim_directions=*aim_directions;
        if(unified){axes[0]=axes[1]=0;*aim_directions&=~15u;}
        else if(settings.analog_enabled || modern){
            auto route=aim_route();
            if(modern)route.right=true;
            const auto shaped=re4::shape_aim_input(control.raw_lx,control.raw_ly,control.raw_rx,control.raw_ry,settings,route);
            axes[0]=shaped.x;axes[1]=shaped.y;
            control.aim_source=route.right;control.applied_aim_x=shaped.x;control.applied_aim_y=shaped.y;
            control.aim_full_scale=route.full_scale;
        }
        // Native aim consumes these axes for reticle, sway baseline, animation
        // and cached aim matrices. Shape once before any of those consumers.
        const int result=Orig(actor,mode);
        axes[0]=old_x;axes[1]=old_y;
        if(unified){
            *aim_directions=(*aim_directions&~15u)|(old_aim_directions&15u);
        }
        gyro_input_actor=previous_input_actor;
        control.aim_calls=control.aim_calls+1;
        return result;
    }
};
HOOK_DEFINE_TRAMPOLINE(AimSway) {
    static void Callback(void* actor, unsigned flags, float* pitch, float* offset) {
        // Native sway clamps to the previous manual-input baseline unless bit0
        // is set. Gyro is manual input too; retain native sway everywhere else.
        if (actor && actor == gyro_input_actor) flags |= 1u;
        Orig(actor, flags, pitch, offset);
    }
};
}

extern "C" void exl_main(void*, void*) {
    u64 program_id = 0;
    if (R_FAILED(svcGetInfo(&program_id, InfoType_ProgramId, CUR_PROCESS_HANDLE, 0))
        || program_id != re4::profile::title_id) { control.status = 2; return; }
    const auto& main = exl::util::GetMainModuleInfo();
    const auto* memory = reinterpret_cast<const std::uint8_t*>(main.m_Total.m_Start);
    if (!re4::profile::matches(memory, main.m_Total.m_Size) || !modern_move_matches(memory,main.m_Total.m_Size)) {
        control.status = 2; // Unsupported executable: no game/SDK hooks installed.
        return;
    }
    if (main.m_Total.m_Size < 0x1273300) { control.status = 2; return; }
    main_base = reinterpret_cast<std::uintptr_t>(memory);
    re6::BuildId telemetry_build{};std::memcpy(telemetry_build.data(),re4::profile::build_id,32);
    telemetry_ring.initialize(svcGetSystemTick(),telemetry_build,re4::profile::title_id);
    aiming_predicate = reinterpret_cast<int (*)(void*)>(main_base + 0x317630);
    native_ortho=reinterpret_cast<decltype(native_ortho)>(main_base+0x27A0D0);
    native_projection=reinterpret_cast<decltype(native_projection)>(main_base+0x2515E0);
    native_pose=reinterpret_cast<decltype(native_pose)>(main_base+0x2507D0);
    native_matrix=reinterpret_cast<decltype(native_matrix)>(main_base+0x2511A0);
    native_rect=reinterpret_cast<decltype(native_rect)>(main_base+0x29D460);
    native_gx_blend=reinterpret_cast<decltype(native_gx_blend)>(main_base+0x250EA0);
    native_gx_format=reinterpret_cast<decltype(native_gx_format)>(main_base+0x251E90);
    native_gx_begin=reinterpret_cast<decltype(native_gx_begin)>(main_base+0x24FB30);
    native_gx_position=reinterpret_cast<decltype(native_gx_position)>(main_base+0x250BC0);
    native_gx_color=reinterpret_cast<decltype(native_gx_color)>(main_base+0x24FD80);
    native_gx_end=reinterpret_cast<decltype(native_gx_end)>(main_base+0x250060);
    publish_settings();
    exl::hook::Initialize();
    modern_move_install();
    RootDisplacement::InstallAtOffset(0x2B8370);
    CameraCalculate::InstallAtOffset(0xFDB0);
    FinalLookAt::InstallAtOffset(0x279F70);
    FullPad::InstallAtFuncPtr(get_full);
    HandheldPad::InstallAtFuncPtr(get_handheld);
    DualPad::InstallAtFuncPtr(get_dual);
    PadNormalize::InstallAtOffset(0x25F680);
    MovementTurn::InstallAtOffset(re4::profile::movement_dispatch);
    MotionAdvance::InstallAtOffset(0x2B8F10);
    CameraInput::InstallAtOffset(0x19B20);
    CameraApply::InstallAtOffset(0x18520);
    CameraBasis::InstallAtOffset(0x17F40);
    ModelSubmit::InstallAtOffset(0x4EB950);
    WeaponLaser::InstallAtOffset(0x2E8750);
    HudDraw::InstallAtOffset(0x29CEF0);
    RomMount::InstallAtFuncPtr(nn_mount_rom);
    SamplerCreate::InstallAtOffset(0x64C3A0);
    ResolutionMap::InstallAtOffset(0x24FAE0);
    FrustumUpdate::InstallAtOffset(0x4F5C90);
    SceneSubmit::InstallAtOffset(0x4EB700);
    SceneDraw::InstallAtOffset(0x4ECFB0);
    AimRaise::InstallAtOffset(0x33F130);
    AimSensitivity::InstallAtOffset(0x33DF80);
    AimSway::InstallAtOffset(0x33EC50);
    control.status = 1;
}
extern "C" NORETURN void exl_exception_entry() { EXL_ABORT("RE4 controls exception"); }
