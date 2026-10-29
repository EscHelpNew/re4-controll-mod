// Exercise the actual game adapter callbacks against a bounded fake game image.
// Only external engine/SDK calls are substituted; no extracted game is needed.
#include <cstdint>
namespace {
struct NpadState;
void test_FullPad(NpadState*, const std::uint32_t&);
void test_HandheldPad(NpadState*, const std::uint32_t&);
void test_DualPad(NpadState*, const std::uint32_t&);
int test_PadNormalize(void*);
void test_MovementTurn(void*);
unsigned test_MotionAdvance(void*);
void test_CameraInput(void*);
int test_AimSensitivity(void*, int);
void test_AimSway(void*, unsigned, float*, float*);
}
#include "../source/game/module.cpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
namespace {
std::vector<re4::SensorState> sensor_batch;
bool consume_aim_axes=false;
int seen_aim_x=0,seen_aim_y=0;
float aim_full_scale=63,cached_aim_yaw=0,cached_aim_pitch=0,native_pitch_sign=1;
void test_FullPad(NpadState* state, const std::uint32_t&) { *state={}; state->attributes=game_test::pad_attributes; }
void test_HandheldPad(NpadState* state, const std::uint32_t& id) { test_FullPad(state,id); }
void test_DualPad(NpadState* state, const std::uint32_t& id) { test_FullPad(state,id); }
int test_PadNormalize(void*) { return 7; }
unsigned test_MotionAdvance(void* motion) {
    game_test::motion_speed_seen=*reinterpret_cast<float*>(static_cast<std::uint8_t*>(motion)+0xDC);
    return 3;
}
void test_MovementTurn(void* actor) {
    auto* b=static_cast<std::uint8_t*>(actor);
    auto* flags=reinterpret_cast<std::uint64_t*>(main_base+re4::profile::input_flags);
    game_test::directions_seen=*flags&15;
    *flags=(*flags&~64u)|128u; // The native routine consumes other input bits.
    *reinterpret_cast<float*>(b+0xB4)+=b[0x115]==5 ? 3.14159265f : 0.04f;
    MotionAdvance::Callback(b+0x228);
}
void test_CameraInput(void*) {
    const bool alternate=*reinterpret_cast<int*>(main_base+0x11C7D68)==1;
    auto* in=reinterpret_cast<std::int8_t*>(main_base+0x124EC38+(alternate?0xC0:8));
    game_test::camera_x=in[0]; game_test::camera_y=in[1];
}
int test_AimSensitivity(void* actor,int) {
    auto* b=static_cast<std::uint8_t*>(actor);
    auto* axes=reinterpret_cast<std::int32_t*>(main_base+0x124E9C8);
    seen_aim_x=axes[0];seen_aim_y=axes[1];
    const auto raw_dpad=*reinterpret_cast<std::uint32_t*>(main_base+0x124E9C8+0x14);
    const float x=(consume_aim_axes?float(axes[0])/aim_full_scale:1)+((raw_dpad&1)?1:0);
    const float y=(consume_aim_axes?float(axes[1])/aim_full_scale:1)*native_pitch_sign;
    *reinterpret_cast<float*>(b+0xB4)+=0.04f*x;
    *reinterpret_cast<float*>(b+0x524)+=0.02f*x;
    auto* pitch=reinterpret_cast<float*>(main_base+0x1265A08);
    pitch[0]+=game_test::aim_step*y; pitch[1]+=game_test::aim_step*y;
    if (game_test::apply_sway) {
        AimSway::Callback(actor,0,pitch,reinterpret_cast<float*>(b+0x524));
        pitch[1]=pitch[0];
    }
    cached_aim_yaw=*reinterpret_cast<float*>(b+0xB4);cached_aim_pitch=pitch[0];
    return 13;
}
void test_AimSway(void*,unsigned flags,float* pitch,float*) {
    if(flags&1) game_test::sway_baseline=*pitch;
    else *pitch=std::fmax(game_test::sway_baseline-0.02f,std::fmin(game_test::sway_baseline+0.02f,*pitch));
}
extern "C" void get_full(NpadState*,const std::uint32_t&) {}
extern "C" void get_handheld(NpadState*,const std::uint32_t&) {}
extern "C" void get_dual(NpadState*,const std::uint32_t&) {}
extern "C" Result sensor_handles(std::uint32_t* h,int* count,int cap,std::uint32_t id,std::uint32_t style) {
    assert(cap==2 && (id==0||id==32) && (style==1||style==2||style==4));
    h[0]=10;h[1]=20;*count=style==4?2:1;return 0;
}
extern "C" Result sensor_start(const std::uint32_t&) { ++game_test::sensor_starts;return game_test::start_result; }
extern "C" Result sensor_stop(const std::uint32_t&) { ++game_test::sensor_stops;return 0; }
extern "C" Result sensor_read(int* count,re4::SensorState* out,int capacity,const std::uint32_t&) {
    ++game_test::sensor_reads;assert(sensor_batch.size()<=std::size_t(capacity));
    *count=int(sensor_batch.size());std::copy(sensor_batch.begin(),sensor_batch.end(),out);return 0;
}
int test_is_aiming(void*) { return game_test::native_aim; }
bool near(float a,float b) {return std::fabs(a-b)<0.0001f;}
struct Fixture {
    std::vector<std::uint8_t> image=std::vector<std::uint8_t>(0x1267000);
    alignas(16) std::uint8_t actor[0xA00]{};
    Fixture() {
        sensor.stop();sensor={};gyro.reset();gyro_pending={};last_sensor_tick=0;
        consume_aim_axes=true;seen_aim_x=seen_aim_y=0;aim_full_scale=63;cached_aim_yaw=cached_aim_pitch=0;
        native_pitch_sign=1;
        sensor_pad=0;sensor_style=0;sensor_attributes=0;
        game_test::installed=game_test::initialized=game_test::mounts=game_test::fs_opens=0;
        game_test::sensor_starts=game_test::sensor_stops=game_test::sensor_reads=0;
        game_test::start_result=0;game_test::tick=1;game_test::native_aim=1;
        game_test::apply_sway=false;game_test::aim_step=0.08f;game_test::sway_baseline=0;
        game_test::program=re4::profile::title_id;
        settings={};settings.invert_x=settings.invert_y=false;
        settings.left_deadzone=settings.right_deadzone=0.15f;
        file_read=false;last_sequence=0;raw_left_x=0;dpad_move=dpad_turn=false;movement_actor=nullptr;
        std::memset(const_cast<re4::SharedControl*>(&control),0,sizeof(control));
        std::memcpy(image.data()+re4::profile::build_id_offset,re4::profile::build_id,32);
        for(auto s:re4::profile::sites)std::memcpy(image.data()+s.offset,&s.instruction,4);
        main_base=reinterpret_cast<std::uintptr_t>(image.data());
        game_test::module.m_Total={main_base,image.size()};
        *reinterpret_cast<void**>(image.data()+re4::profile::player_slot)=actor;
        *reinterpret_cast<float*>(actor+0x304)=1.25f;
    }
    std::uint64_t& flags(){return *reinterpret_cast<std::uint64_t*>(image.data()+re4::profile::input_flags);}
    float& yaw(){return *reinterpret_cast<float*>(actor+0xB4);}
    float* pitch(){return reinterpret_cast<float*>(image.data()+0x1265A08);}
    void aim(){actor[0x115]=6;actor[0x116]=1;flags()=0x10;aiming_predicate=test_is_aiming;}
    void sample(unsigned n) {
        re4::SensorState s{};s.number=n;s.interval_ns=5'000'000;s.attributes=1;
        s.angular_velocity[0]=s.angular_velocity[1]=-1/6.28318530718f;
        sensor_batch={s};
    }
};
}
int main(){
    {
        Fixture f;game_test::program=0;exl_main(nullptr,nullptr);
        assert(control.status==2 && game_test::installed==0 && game_test::initialized==0);
        game_test::program=re4::profile::title_id;f.image[0x19B20]^=1;exl_main(nullptr,nullptr);
        assert(control.status==2 && game_test::installed==0);
        f.image[0x19B20]^=1;exl_main(nullptr,nullptr);
        assert(control.status==1 && game_test::installed==9 && game_test::initialized==1);
        assert(PadNormalize::Callback(nullptr)==7);PadNormalize::Callback(nullptr);
        assert(control.frames==2 && game_test::mounts==1); // No filesystem polling per frame.
    }
    {
        Fixture f;f.actor[0x115]=1;f.flags()=0x45;
        raw_left_x=control.raw_ly=0.575f;settings.turn_gain=0;
        MovementTurn::Callback(f.actor);
        assert(near(f.yaw(),0) && game_test::directions_seen==5);
        assert(f.flags()==0x85); // Only temporarily remapped direction bits restored.
        assert(near(game_test::motion_speed_seen,1.25f*0.5f));
        assert(*reinterpret_cast<float*>(f.actor+0x304)==1.25f);
        dpad_turn=dpad_move=true;MovementTurn::Callback(f.actor);
        assert(near(f.yaw(),0.04f) && game_test::motion_speed_seen==1.25f);
        f.actor[0x115]=5;MovementTurn::Callback(f.actor);
        assert(f.yaw()>3.14f); // Quick-turn untouched.
    }
    {
        Fixture f;control.raw_rx=0.575f;control.raw_ry=-1;
        for(int mode=0;mode<=1;++mode){
            *reinterpret_cast<int*>(f.image.data()+0x11C7D68)=mode;
            auto* in=reinterpret_cast<std::int8_t*>(f.image.data()+0x124EC38+(mode?0xC0:8));in[0]=12;in[1]=-9;
            CameraInput::Callback(nullptr);
            assert(game_test::camera_x>=31 && game_test::camera_x<=32 && game_test::camera_y==-63);
            assert(in[0]==12 && in[1]==-9);
            f.actor[0x114]=1;CameraInput::Callback(nullptr);
            assert(game_test::camera_x==12 && game_test::camera_y==-9);f.actor[0x114]=0;
        }
    }
    {
        Fixture f;f.actor[0x115]=1;f.flags()=5;
        settings.stick_curve=re4::StickCurve::Exponential;raw_left_x=control.raw_ly=0.575f;
        MovementTurn::Callback(f.actor);
        assert(near(f.yaw(),0.01f) && near(game_test::motion_speed_seen,1.25f*0.25f));
        control.raw_rx=0.575f;CameraInput::Callback(nullptr);assert(game_test::camera_x==16);
        f.aim();control.raw_lx=control.raw_ly=0.575f;f.yaw()=0;
        AimSensitivity::Callback(f.actor,0);assert(near(f.yaw(),0.04f*16/63) && near(f.pitch()[0],0.08f*16/63));
    }
    {
        Fixture f;f.aim();consume_aim_axes=true;aim_full_scale=127;
        std::vector<std::uint8_t> global(0x5200);std::uint8_t type[16]{};type[11]=2;
        *reinterpret_cast<void**>(f.image.data()+0xB1AE08)=global.data();
        *reinterpret_cast<void**>(f.image.data()+0xB1AE10)=type;
        *reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;
        auto* axes=reinterpret_cast<std::int32_t*>(f.image.data()+0x124E9C8);axes[0]=31;axes[1]=-63;
        control.raw_lx=control.raw_ly=1;control.raw_rx=0.25f;control.raw_ry=-0.5f;
        settings.left_deadzone=settings.right_deadzone=0;
        settings.right_aim_gain_x=0.5f;settings.right_aim_gain_y=0.25f;
        settings.gyro_enabled=true;gyro_pending.yaw=0.01f;gyro_pending.pitch=0.02f;
        AimSensitivity::Callback(f.actor,0);
        assert(seen_aim_x==16 && seen_aim_y==-16);
        assert(near(f.yaw(),0.01f+0.04f*16/127) && near(f.pitch()[0],0.02f-0.08f*16/127));
        assert(near(cached_aim_yaw,f.yaw()) && near(cached_aim_pitch,f.pitch()[0]));
        assert(axes[0]==31 && axes[1]==-63); // Scoped engine axes restored.
        global[0x5099]=0x80;assert(!aim_route().right && aim_route().full_scale==63);
        global[0x5096]=0x80;*reinterpret_cast<float*>(global.data()+0xA4)=0.5f;
        assert(aim_route().right && aim_route().full_scale==31.5f);
        type[11]=1;assert(!aim_route().right);type[11]=2;
        *reinterpret_cast<void**>(f.image.data()+0xB1AE08)=nullptr;assert(!aim_route().right);
        *reinterpret_cast<void**>(f.image.data()+0xB1AE10)=nullptr;assert(!aim_route().right);
    }
    {
        Fixture f;f.aim();settings.aim_gain_x=settings.aim_gain_y=0;
        auto* dpad=reinterpret_cast<std::uint32_t*>(f.image.data()+0x124E9C8+0x14);*dpad=1;
        AimSensitivity::Callback(f.actor,0);assert(near(f.yaw(),0.04f) && *dpad==1);
        *dpad=0;settings.aim_gain_y=0.5f;settings.left_deadzone=0;control.raw_ly=1;
        native_pitch_sign=-1;settings.invert_y=true; // Gyro inversion cannot replace native stick inversion.
        AimSensitivity::Callback(f.actor,0);assert(near(f.pitch()[0],-0.08f*32/63));
    }
    {
        Fixture f;NpadState pad{};pad.attributes=1|16|32;
        capture(pad,0,4);assert(sensor_attributes==17);
        pad.attributes=1|4|8;capture(pad,0,4);assert(sensor_attributes==5);
    }
    {
        Fixture f;f.aim();settings.gyro_enabled=true;
        game_test::apply_sway=true;game_test::aim_step=0;
        for(unsigned i=0;i<40;++i){gyro_pending.pitch=0.01f;AimSensitivity::Callback(f.actor,0);}
        assert(near(f.pitch()[0],0.4f)); // Gyro must update native baseline, beyond its small sway window.
        assert(near(game_test::sway_baseline,0.4f));
    }
    {
        Fixture f;f.aim();settings.gyro_enabled=true;
        NpadState pad{};const std::uint32_t id=32;HandheldPad::Callback(&pad,id);
        game_test::start_result=5;poll_sensors();assert(!sensor.running());
        const auto attempts=game_test::sensor_starts;game_test::start_result=0;
        for(unsigned i=0;i<20;++i){game_test::tick+=480'000;poll_sensors();}
        assert(game_test::sensor_starts==attempts); // No retry storm in first500ms.
        game_test::tick+=19'200'000;f.sample(1);poll_sensors();
        assert(sensor.running() && game_test::sensor_starts==attempts+1 && gyro_pending.yaw==0);
    }
    {
        Fixture f;f.aim();settings.gyro_enabled=true;
        NpadState pad{};const std::uint32_t id=32;HandheldPad::Callback(&pad,id);
        f.sample(1);poll_sensors();assert(sensor.running() && gyro_pending.yaw==0);
        game_test::tick+=640'000;f.sample(2);
        const auto s=sensor_batch[0];sensor_batch={s,s,s};
        sensor_batch[0].number=4;sensor_batch[1].number=3;
        poll_sensors();assert(near(gyro_pending.yaw,(1-0.5f*0.01745329252f)*0.01f));
        const float filtered=gyro_pending.yaw;
        control.raw_lx=control.raw_ly=0.575f;settings.aim_gain_x=settings.aim_gain_y=0.3f;
        assert(AimSensitivity::Callback(f.actor,0)==13);
        assert(near(f.yaw(),filtered+0.04f*9/63));
        assert(near(f.pitch()[0],filtered+0.08f*9/63)); // Additive gyro and stick in same frame.
        const float before=f.yaw();AimSensitivity::Callback(f.actor,0);
        assert(near(f.yaw()-before,0.04f*9/63)); // Sensor delta applied once.
        game_test::tick+=2'000'000;f.sample(3);poll_sensors();assert(gyro_pending.yaw==0); // Pause backlog.
        game_test::tick+=640'000;game_test::native_aim=0;f.sample(4);poll_sensors();assert(gyro_pending.yaw==0);
        game_test::native_aim=1;f.flags()=0;f.sample(5);poll_sensors();assert(gyro_pending.yaw==0);
        game_test::pad_attributes=0;HandheldPad::Callback(&pad,id);poll_sensors();
        assert(!sensor.running() && game_test::sensor_stops==1);
    }
    std::puts("actual game callbacks: identity, flags, motion, camera, additive aim/gyro and pause PASS");
}
