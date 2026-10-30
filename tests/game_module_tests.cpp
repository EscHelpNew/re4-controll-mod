// Exercise the actual game adapter callbacks against a bounded fake game image.
// Only external engine/SDK calls are substituted; no extracted game is needed.
#include <cstdint>
#include <cstddef>
namespace {
struct NpadState;
void test_FullPad(NpadState*, const std::uint32_t&);
void test_HandheldPad(NpadState*, const std::uint32_t&);
void test_DualPad(NpadState*, const std::uint32_t&);
int test_PadNormalize(void*);
void test_MovementTurn(void*);
unsigned test_MotionAdvance(void*);
void test_CameraInput(void*);
void test_CameraApply(void*,void*);
int test_AimSensitivity(void*, int);
void test_AimSway(void*, unsigned, float*, float*);
unsigned test_RomMount(const char*,void*,std::size_t);
void test_SamplerCreate(void*,void*,const std::uint8_t*);
void test_ResolutionMap(int,int,int*,int*);
void test_FrustumUpdate(void*);
void test_SceneSubmit();
void test_SceneDraw();
}
#include "../source/game/module.cpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
namespace {
std::vector<re4::SensorState> sensor_batch;
std::uint8_t sampler_seen[20]{};
std::uint32_t shadow_flags_seen=0;
unsigned rom_result=0;
unsigned test_RomMount(const char*,void*,std::size_t){return rom_result;}
extern "C" Result nn_mount_rom(const char*,void*,std::size_t){return rom_result;}
extern "C" int get_operation_mode(){return game_test::operation_mode;}
void test_SamplerCreate(void*,void*,const std::uint8_t* descriptor){std::memcpy(sampler_seen,descriptor,20);}
void test_ResolutionMap(int width,int height,int* display,int* back){*display=width==1280&&height==720?2:0;*back=*display==2?3:1;}
void test_FrustumUpdate(void* frustum){auto* value=static_cast<std::uint8_t*>(frustum);std::memcpy(value+0x24,value+0x18,4);}
void test_SceneSubmit(){
    auto* state=*reinterpret_cast<std::uint8_t**>(main_base+0xB1AE08);
    if(state){auto* flags=reinterpret_cast<std::uint32_t*>(state+0x7C);shadow_flags_seen=*flags;*flags|=0x400;}
}
void test_SceneDraw(){test_SceneSubmit();}
bool consume_aim_axes=false;
int seen_aim_x=0,seen_aim_y=0;
float aim_full_scale=63,cached_aim_yaw=0,cached_aim_pitch=0,native_pitch_sign=1;
void test_FullPad(NpadState* state, const std::uint32_t&) { *state={}; state->attributes=game_test::pad_attributes;state->buttons=game_test::pad_buttons; }
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
void test_CameraInput(void* camera) {
    const bool alternate=*reinterpret_cast<int*>(main_base+0x11C7D68)==1;
    auto* in=reinterpret_cast<std::int8_t*>(main_base+0x124EC38+(alternate?0xC0:8));
    game_test::camera_x=in[0]; game_test::camera_y=in[1];
    if(camera){
        auto* bytes=static_cast<std::uint8_t*>(camera);
        auto* offsets=reinterpret_cast<float*>(bytes+0x270);
        if(in[0]||in[1]){
            const auto* type=*reinterpret_cast<const int**>(main_base+0xB1AE10);
            offsets[0]=std::clamp(float(in[1])/59*(type&&*type<0?-1:1),-1.f,1.f);
            offsets[1]=float(in[0])/59*-1.04719758f;
        }else{offsets[0]*=0.9f;offsets[1]*=0.9f;}
        if(game_test::camera_gate_change==1)bytes[0x26C]=0;
        if(game_test::camera_gate_change==2)bytes[0x222]=1;
        CameraApply::Callback(camera,nullptr);
    }
}
void test_CameraApply(void* camera,void*){
    const auto* offsets=reinterpret_cast<const float*>(static_cast<const std::uint8_t*>(camera)+0x270);
    game_test::rendered_pitch=offsets[0];game_test::rendered_camera_yaw=offsets[1];
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
    std::vector<std::uint8_t> image=std::vector<std::uint8_t>(0x1281000);
    alignas(16) std::uint8_t actor[0xA00]{};
    std::vector<std::uint8_t> input_state=std::vector<std::uint8_t>(0x5200);
    alignas(4) std::uint8_t input_type[16]{};
    Fixture() {
        sensor.stop();sensor={};gyro.reset();gyro_pending={};last_sensor_tick=0;
        free_orbit.reset();camera_context=nullptr;
        graphics_snapshot.store(0x3F80000000000000ull);filter_latched=255;aa_latched=255;
        sampler_calls=0;sampler_overrides=0;scene_submits=0;scene_draws=0;cull_updates=0;resolution_overrides=0;
        last_cull_pair=0;rom_settings_attempted.clear();rom_result=0;
        consume_aim_axes=true;seen_aim_x=seen_aim_y=0;aim_full_scale=63;cached_aim_yaw=cached_aim_pitch=0;
        native_pitch_sign=1;
        sensor_pad=0;sensor_style=0;sensor_attributes=0;
        game_test::installed=game_test::initialized=game_test::mounts=game_test::fs_opens=0;
        game_test::mount_result=1;game_test::mount_failures=game_test::fs_reads=game_test::fs_closes=0;
        game_test::settings_primary.clear();game_test::settings_backup.clear();
        game_test::sensor_starts=game_test::sensor_stops=game_test::sensor_reads=0;
        game_test::start_result=0;game_test::tick=1;game_test::native_aim=1;
        game_test::apply_sway=false;game_test::aim_step=0.08f;game_test::sway_baseline=0;
        game_test::pad_buttons=0;
        game_test::camera_gate_change=0;
        game_test::operation_mode=0;
        game_test::program=re4::profile::title_id;
        settings={};settings.gyro_speed=1;settings.invert_x=settings.invert_y=false;
        settings.left_deadzone=settings.right_deadzone=0.15f;
        file_read=false;file_mounted=false;file_attempts=0;next_file_retry=0;
        last_sequence=0;raw_left_x=0;dpad_move=dpad_turn=false;movement_actor=nullptr;
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
    std::uint8_t* camera(){return image.data()+0x11319C0;}
    float* look(){return reinterpret_cast<float*>(camera()+0x270);}
    void camera_mode(){
        actor[0x115]=1;game_test::native_aim=0;aiming_predicate=test_is_aiming;
        image[0x1131718+0x54]=10;camera()[0x26C]=2;
        *reinterpret_cast<void**>(image.data()+0xB1AE08)=input_state.data();
        *reinterpret_cast<void**>(image.data()+0xB1AE10)=input_type;
    }
    void sample(unsigned n) {
        re4::SensorState s{};s.number=n;s.interval_ns=5'000'000;s.attributes=1;
        s.angular_velocity[0]=s.angular_velocity[1]=-1/6.28318530718f;
        sensor_batch={s};
    }
};
}
int main(){
    {
        Fixture f;rom_result=7;assert(RomMount::Callback("rom",nullptr,0)==7 && game_test::mounts==0);
        std::uint8_t descriptor[20]{};descriptor[5]=1;descriptor[6]=0x29;
        settings.texture_filtering=re4::TextureFilter::Aniso16;publish_graphics_settings();
        SamplerCreate::Callback(nullptr,nullptr,descriptor);
        assert(filter_latched.load()==255 && sampler_seen[5]==1 && sampler_seen[6]==0x29);
        file_read=true;publish_graphics_settings();SamplerCreate::Callback(nullptr,nullptr,descriptor);
        assert(filter_latched.load()==unsigned(re4::TextureFilter::Aniso16) && sampler_seen[5]==16);
        int display,back;ResolutionMap::Callback(1280,720,&display,&back);assert(display==2 && back==3);
        settings.anti_aliasing=re4::AntiAliasing::HandheldSsaa;publish_graphics_settings();
        ResolutionMap::Callback(1280,720,&display,&back);assert(back==3); // Native AA latch also requires restart.
    }
    {
        Fixture f;re4::Settings saved;saved.texture_filtering=re4::TextureFilter::Aniso8;
        game_test::settings_primary.resize(re4::kSettingsBytes);
        assert(re4::encode_settings(saved,game_test::settings_primary.data(),re4::kSettingsBytes));
        game_test::mount_result=0;rom_result=0;
        assert(RomMount::Callback("rom",nullptr,0)==0 && file_read);
        std::uint8_t descriptor[20]{};descriptor[5]=1;descriptor[6]=0x29;
        SamplerCreate::Callback(nullptr,nullptr,descriptor);
        assert(sampler_seen[5]==8 && sampler_seen[6]==0x6A && descriptor[5]==1 && descriptor[6]==0x29);
        settings.texture_filtering=re4::TextureFilter::Aniso16;publish_graphics_settings();
        SamplerCreate::Callback(nullptr,nullptr,descriptor);assert(sampler_seen[5]==8); // Process-latched, no cache variants.
        descriptor[6]=0xAA;SamplerCreate::Callback(nullptr,nullptr,descriptor);
        assert(sampler_seen[6]==0xAA && sampler_seen[5]==1); // Comparison sampler untouched.
    }
    {
        Fixture f;file_read=true;settings.anti_aliasing=re4::AntiAliasing::HandheldSsaa;publish_graphics_settings();
        int display=-1,back=-1;ResolutionMap::Callback(1280,720,&display,&back);assert(display==2 && back==1);
        game_test::operation_mode=1;ResolutionMap::Callback(1280,720,&display,&back);assert(display==2 && back==3);
        game_test::operation_mode=0;
        settings.anti_aliasing=re4::AntiAliasing::Native;publish_graphics_settings();
        ResolutionMap::Callback(1280,720,&display,&back);assert(display==2 && back==1); // Restart required.
        ResolutionMap::Callback(1920,1080,&display,&back);assert(display==0 && back==1);
    }
    {
        Fixture f;f.camera_mode();file_read=true;settings.shadow_mode=re4::ShadowMode::Off;
        settings.draw_distance_scale=1.5f;publish_graphics_settings();
        auto* flags=reinterpret_cast<std::uint32_t*>(f.input_state.data()+0x7C);*flags=0x80;
        SceneSubmit::Callback();assert((shadow_flags_seen&re4::kShadowDisableMask)==re4::kShadowDisableMask && *flags==0x480);
        SceneDraw::Callback();assert(*flags==0x480);
        auto* frustum=f.image.data()+0x1272FA0;
        *reinterpret_cast<float*>(frustum+0x18)=1000;
        *reinterpret_cast<float*>(f.image.data()+0x124ED40)=100;
        *reinterpret_cast<float*>(f.image.data()+0x124ED44)=1'000'000;
        FrustumUpdate::Callback(frustum);
        assert(*reinterpret_cast<float*>(frustum+0x18)==1000 && *reinterpret_cast<float*>(frustum+0x24)==1500);
        FrustumUpdate::Callback(frustum);assert(*reinterpret_cast<float*>(frustum+0x24)==1500); // No compounded scaling.
        settings.draw_distance_scale=1;publish_graphics_settings();FrustumUpdate::Callback(frustum);
        assert(*reinterpret_cast<float*>(frustum+0x24)==1000);
        settings.draw_distance_scale=2;publish_graphics_settings();
        *reinterpret_cast<float*>(frustum+0x18)=1;FrustumUpdate::Callback(frustum);
        assert(*reinterpret_cast<float*>(frustum+0x18)==1 && *reinterpret_cast<float*>(frustum+0x24)==1);
        alignas(16) std::uint8_t other[0x40]{};*reinterpret_cast<float*>(other+0x18)=1000;
        FrustumUpdate::Callback(other);assert(*reinterpret_cast<float*>(other+0x24)==1000);
        settings.shadow_mode=re4::ShadowMode::Native;publish_graphics_settings();*flags=0x02000000;
        SceneDraw::Callback();assert(shadow_flags_seen==0x02000000 && *flags==0x02000400);
    }
    for(unsigned style:{1u,2u,4u}){
        Fixture f;f.actor[0x114]=9;f.actor[0x115]=99; // Menus/QTE remain remapped too.
        settings.button_map[0]=1;settings.button_map[1]=0;settings.button_map[2]=re4::kUnboundButton;
        settings.button_map[4]=8;game_test::pad_buttons=1ull|4ull|16ull|(1ull<<40);
        NpadState pad{};const std::uint32_t id=style==2?32:0;
        if(style==1)FullPad::Callback(&pad,id);
        else if(style==2)HandheldPad::Callback(&pad,id);
        else DualPad::Callback(&pad,id);
        assert(pad.buttons==(2ull|256ull|(1ull<<40)) && pad.attributes==game_test::pad_attributes);
        settings.remap_enabled=false;FullPad::Callback(&pad,id);assert(pad.buttons==game_test::pad_buttons);
    }
    {
        Fixture f;f.camera_mode();f.look()[0]=0.8f;f.look()[1]=-1;
        settings.camera_return_speed=0.5f;CameraInput::Callback(f.camera());
        assert(near(game_test::rendered_pitch,0.8f*std::sqrt(0.9f)));
        assert(near(game_test::rendered_camera_yaw,-std::sqrt(0.9f)));
        assert(game_test::rendered_camera_yaw==f.look()[1]); // Correction precedes native consumers.
        settings.camera_return_speed=0;const auto held=f.look()[1];CameraInput::Callback(f.camera());
        assert(f.look()[1]==held);
        settings.camera_return_speed=1;CameraInput::Callback(f.camera());assert(near(f.look()[1],held*0.9f));
        settings.camera_return_speed=0;control.raw_rx=1;CameraInput::Callback(f.camera());
        assert(near(f.look()[1],-63.f/59*1.04719758f) && f.look()[0]==0); // Joint neutral, not per-axis decay.
    }
    {
        Fixture f;f.camera_mode();settings.free_camera=true;settings.right_deadzone=0;control.raw_rx=1;
        CameraInput::Callback(f.camera());
        for(unsigned n=0;n<60;++n){game_test::tick+=320'000;CameraInput::Callback(f.camera());}
        assert(near(game_test::rendered_camera_yaw,-2.0943951f) && f.yaw()==0);
        control.raw_rx=0;const auto held=f.look()[1];game_test::tick+=320'000;CameraInput::Callback(f.camera());
        assert(f.look()[1]==held); // Free orbit remains held after release.
        *reinterpret_cast<int*>(f.input_type)=-1;control.raw_ry=1;
        game_test::tick+=320'000;CameraInput::Callback(f.camera());assert(f.look()[0]<0);
        f.look()[0]=f.look()[1]=0;control.raw_ry=0; // Native aim entry consumes and clears camera state.
        game_test::tick+=320'000;CameraInput::Callback(f.camera());assert(f.look()[1]==0);
        control.raw_rx=1;game_test::tick+=320'000;CameraInput::Callback(f.camera());assert(f.look()[1]<0);
        f.input_state[0x5092]=4;const auto before=f.look()[1];game_test::tick+=320'000;
        CameraInput::Callback(f.camera());assert(near(f.look()[1],-63.f/59*1.04719758f) && f.look()[1]!=before);
        f.input_state[0x5092]=0;control.raw_rx=0;game_test::tick+=320'000;CameraInput::Callback(f.camera());
        const auto resumed=f.look()[1];game_test::tick+=320'000;CameraInput::Callback(f.camera());
        assert(f.look()[1]==resumed); // Pause resets continuity instead of replaying old orbit.
    }
    for(unsigned gate=0;gate<11;++gate){
        Fixture f;f.camera_mode();settings.free_camera=true;settings.camera_return_speed=0;
        f.look()[0]=0.5f;f.look()[1]=0.7f;
        if(gate==0)f.camera()[0x222]=1;
        if(gate==1)f.camera()[0x26C]=0;
        if(gate==2)f.image[0x1131718+0x54]=9;
        if(gate==3)f.actor[0x114]=1;
        if(gate==4)f.actor[0x115]=6;
        if(gate==5)game_test::native_aim=1;
        if(gate==6)settings.analog_enabled=false;
        if(gate==7)*reinterpret_cast<void**>(f.image.data()+0xB1AE08)=nullptr;
        if(gate==8)f.input_state[0x5092]=4;
        if(gate==9)game_test::camera_gate_change=1;
        if(gate==10)game_test::camera_gate_change=2;
        CameraInput::Callback(f.camera());
        assert(near(game_test::rendered_pitch,0.45f) && near(game_test::rendered_camera_yaw,0.63f));
        assert(camera_context==nullptr);
    }
    {
        Fixture f;f.camera_mode();settings.free_camera=true;settings.right_deadzone=0;
        settings.camera_gain=0.5f;settings.stick_curve=re4::StickCurve::Exponential;
        *reinterpret_cast<int*>(f.image.data()+0x11C7D68)=1;control.raw_rx=0.5f;
        CameraInput::Callback(f.camera());
        for(unsigned n=0;n<60;++n){game_test::tick+=320'000;CameraInput::Callback(f.camera());}
        assert(near(f.look()[1],-2.0943951f*0.125f)); // Curve and sensitivity applied once in the alternate input route.
        assert(f.image[0x124EC38+0xC0]==0 && f.image[0x124EC38+0xC1]==0);
    }
    {
        Fixture f;re4::Settings legacy;legacy.gyro_enabled=true;legacy.gyro_speed=1.7f;
        std::uint8_t current[re4::kSettingsBytes];assert(re4::encode_settings(legacy,current,sizeof(current)));
        game_test::settings_primary.assign(current,current+re4::kLegacySettingsBytes);
        auto& old=game_test::settings_primary;old[4]=6;
        std::uint32_t sum=2166136261u;for(unsigned i=0;i<60;++i){sum^=old[i];sum*=16777619u;}
        for(unsigned i=0;i<4;++i)old[60+i]=std::uint8_t(sum>>(i*8));
        game_test::mount_result=0;PadNormalize::Callback(nullptr);
        assert(settings.gyro_enabled && settings.gyro_speed==1.7f);
        assert(settings.button_map==re4::identity_button_map() && !settings.free_camera && settings.camera_return_speed==1);
    }
    for(bool enabled:{false,true}){
        Fixture f;re4::Settings saved;saved.gyro_enabled=enabled;saved.gyro_speed=2.5f;
        saved.gyro_deadzone_dps=0.7f;saved.turn_gain=0.37f;
        game_test::settings_primary.resize(re4::kSettingsBytes);
        assert(re4::encode_settings(saved,game_test::settings_primary.data(),re4::kSettingsBytes));
        game_test::mount_result=0;game_test::mount_failures=1;
        f.aim();NpadState pad{};pad.attributes=1;capture(pad,0,1);
        assert(PadNormalize::Callback(nullptr)==7 && !settings.gyro_enabled);
        for(unsigned n=0;n<60;++n)PadNormalize::Callback(nullptr);
        assert(game_test::mounts==1 && game_test::fs_opens==0);
        game_test::tick+=19'200'000;PadNormalize::Callback(nullptr);
        assert(settings.gyro_enabled==enabled && near(settings.gyro_speed,2.5f));
        assert(near(settings.turn_gain,0.37f) && near(settings.gyro_deadzone_dps,0.7f));
        assert(sensor.running()==enabled && game_test::mounts==2 && game_test::fs_closes==1);
        const auto opens=game_test::fs_opens;
        game_test::tick+=19'200'000;PadNormalize::Callback(nullptr);
        assert(game_test::mounts==2 && game_test::fs_opens==opens);
    }
    {
        Fixture f;re4::Settings saved;saved.gyro_enabled=true;
        game_test::settings_primary.assign(re4::kSettingsBytes,0); // Corrupt primary, valid recovery.
        game_test::settings_backup.resize(re4::kSettingsBytes);
        assert(re4::encode_settings(saved,game_test::settings_backup.data(),re4::kSettingsBytes));
        game_test::mount_result=0;f.aim();NpadState pad{};pad.attributes=1;capture(pad,0,1);
        PadNormalize::Callback(nullptr);
        assert(settings.gyro_enabled && sensor.running() && control.settings_source==2);
        assert(game_test::fs_closes==2 && control.settings_read_result==0);
        for(unsigned n=1;n<=3;++n){game_test::tick+=320'000;f.sample(n);PadNormalize::Callback(nullptr);}
        AimSensitivity::Callback(f.actor,0);
        assert(std::fabs(f.yaw())>0 && std::fabs(f.pitch()[0])>0); // No overlay or toggle required.
    }
    {
        Fixture f;
        for(unsigned n=0;n<20;++n){PadNormalize::Callback(nullptr);game_test::tick+=19'200'000;}
        assert(file_read && game_test::mounts==8 && control.settings_attempts==8);
        assert(control.settings_source==3 && !settings.gyro_enabled && !sensor.running());
        assert(game_test::fs_opens==0); // Exhausted mount attempts never poll SD every frame.
    }
    {
        Fixture f;game_test::mount_result=0;PadNormalize::Callback(nullptr);
        assert(game_test::mounts==1 && game_test::fs_opens==2 && !file_read);
        re4::Settings saved;saved.gyro_enabled=true;
        game_test::settings_primary.resize(re4::kSettingsBytes);
        assert(re4::encode_settings(saved,game_test::settings_primary.data(),re4::kSettingsBytes));
        game_test::tick+=19'200'000;PadNormalize::Callback(nullptr);
        assert(settings.gyro_enabled && control.settings_source==1 && file_read);
        assert(game_test::mounts==1 && game_test::fs_opens==3); // Retry reads without a duplicate mount.
    }
    {
        Fixture f;publish_settings();PadNormalize::Callback(nullptr); // Initial mount fails.
        re4::Settings live;live.gyro_enabled=true;live.gyro_speed=3.25f;
        std::uint8_t bytes[re4::kSettingsBytes];assert(re4::encode_settings(live,bytes,sizeof(bytes)));
        for(unsigned i=0;i<sizeof(bytes);++i)control.settings[i]=bytes[i];
        control.sequence=last_sequence+2;
        game_test::mount_result=0;game_test::tick+=19'200'000;PadNormalize::Callback(nullptr);
        assert(settings.gyro_enabled && settings.gyro_speed==3.25f && control.settings_source==4);
        assert(file_read && game_test::mounts==1 && game_test::fs_opens==0); // Live change wins over retry.
    }
    {
        Fixture f;game_test::program=0;exl_main(nullptr,nullptr);
        assert(control.status==2 && game_test::installed==0 && game_test::initialized==0);
        game_test::program=re4::profile::title_id;f.image[0x19B20]^=1;exl_main(nullptr,nullptr);
        assert(control.status==2 && game_test::installed==0);
        f.image[0x19B20]^=1;exl_main(nullptr,nullptr);
        assert(control.status==1 && game_test::installed==16 && game_test::initialized==1);
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
