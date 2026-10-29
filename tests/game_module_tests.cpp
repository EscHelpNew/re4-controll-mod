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
    *reinterpret_cast<float*>(b+0xB4)+=0.04f;
    *reinterpret_cast<float*>(b+0x524)+=0.02f;
    auto* pitch=reinterpret_cast<float*>(main_base+0x1265A08);
    pitch[0]+=game_test::aim_step; pitch[1]+=game_test::aim_step;
    if (game_test::apply_sway) {
        AimSway::Callback(actor,0,pitch,reinterpret_cast<float*>(b+0x524));
        pitch[1]=pitch[0];
    }
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
        sensor_pad=0;sensor_style=0;sensor_attributes=0;
        game_test::installed=game_test::initialized=game_test::mounts=game_test::fs_opens=0;
        game_test::sensor_starts=game_test::sensor_stops=game_test::sensor_reads=0;
        game_test::start_result=0;game_test::tick=1;game_test::native_aim=1;
        game_test::apply_sway=false;game_test::aim_step=0.08f;game_test::sway_baseline=0;
        game_test::program=re4::profile::title_id;
        settings={};settings.invert_x=settings.invert_y=false;
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
        raw_left_x=control.raw_ly=0.575f;settings.turn_gain=0;settings.movement_gain=0.3f;
        MovementTurn::Callback(f.actor);
        assert(near(f.yaw(),0) && game_test::directions_seen==5);
        assert(f.flags()==0x85); // Only temporarily remapped direction bits restored.
        assert(near(game_test::motion_speed_seen,1.25f*0.15f));
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
        Fixture f;f.aim();settings.gyro_enabled=true;
        game_test::apply_sway=true;game_test::aim_step=0;
        for(unsigned i=0;i<40;++i){gyro_pending.pitch=0.01f;AimSensitivity::Callback(f.actor,0);}
        assert(near(f.pitch()[0],0.4f)); // Gyro must update native baseline, beyond its small sway window.
        assert(near(game_test::sway_baseline,0.4f));
    }
    {
        Fixture f;f.aim();settings.gyro_enabled=true;
        NpadState pad{};const std::uint32_t id=32;HandheldPad::Callback(&pad,id);
        f.sample(1);poll_sensors();assert(sensor.running() && gyro_pending.yaw==0);
        game_test::tick+=640'000;f.sample(2);poll_sensors();assert(near(gyro_pending.yaw,0.005f));
        control.raw_lx=control.raw_ly=0.575f;settings.aim_gain_x=settings.aim_gain_y=0.3f;
        assert(AimSensitivity::Callback(f.actor,0)==13);
        assert(near(f.yaw(),0.005f+0.04f*0.15f));
        assert(near(f.pitch()[0],0.005f+0.08f*0.15f)); // Additive gyro and stick in same frame.
        const float before=f.yaw();AimSensitivity::Callback(f.actor,0);
        assert(near(f.yaw()-before,0.04f*0.15f)); // Sensor delta applied once.
        game_test::tick+=2'000'000;f.sample(3);poll_sensors();assert(gyro_pending.yaw==0); // Pause backlog.
        game_test::tick+=640'000;game_test::native_aim=0;f.sample(4);poll_sensors();assert(gyro_pending.yaw==0);
        game_test::native_aim=1;f.flags()=0;f.sample(5);poll_sensors();assert(gyro_pending.yaw==0);
        game_test::pad_attributes=0;HandheldPad::Callback(&pad,id);poll_sensors();
        assert(!sensor.running() && game_test::sensor_stops==1);
    }
    std::puts("actual game callbacks: identity, flags, motion, camera, additive aim/gyro and pause PASS");
}
