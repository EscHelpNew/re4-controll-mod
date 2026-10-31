// Exercise the actual game adapter callbacks against a bounded fake game image.
// Only external engine/SDK calls are substituted; no extracted game is needed.
#include <cstdint>
#include <cstddef>
namespace re4 {struct ViewVec3;}
namespace {
struct NpadState;
void test_FullPad(NpadState*, const std::uint32_t&);
void test_HandheldPad(NpadState*, const std::uint32_t&);
void test_DualPad(NpadState*, const std::uint32_t&);
int test_PadNormalize(void*);
void test_MovementTurn(void*);
unsigned test_MotionAdvance(void*);
void test_CameraInput(void*);
void test_CameraCalculate();
void test_FinalLookAt(void*,const re4::ViewVec3*,const re4::ViewVec3*,const re4::ViewVec3*);
void test_CameraApply(void*,void*);
void test_CameraBasis(void*,void*);
void test_ModelSubmit(void*);
void test_WeaponLaser(void*,unsigned,unsigned,unsigned);
void test_HudDraw(void*);
void test_ModernPlayerUpdate(void*);
unsigned test_ModernRoot(void*,void*);
void test_ModernPose(void*,int);
void test_RootDisplacement(void*,void*,unsigned,re4::ViewVec3*,re4::ViewVec3*);
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
#include "re4/reticle_preview.hpp"
#undef assert
#define assert(c) do { if(!(c)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#c);std::exit(1);} } while(0)
namespace {
bool modern_probe=false,unified_probe=false;
unsigned unified_flags_seen=0,update_state_change=0;
unsigned modern_original_updates=0,modern_original_roots=0,modern_original_poses=0;
bool native_root_probe=false;
re4::ViewVec3 native_primary{0,3,10},native_secondary{0,5,6},root_result{};
void test_RootDisplacement(void* actor,void* motion,unsigned flags,re4::ViewVec3* out,re4::ViewVec3* rotation){
 assert(flags==0);*out=motion==static_cast<std::uint8_t*>(actor)+0x228?native_primary:native_secondary;*rotation={1,2,3};
}
void test_ModernPlayerUpdate(void* actor){
    ++modern_original_updates;
    if(update_state_change)static_cast<std::uint8_t*>(actor)[0x114]=1;
    if(unified_probe){unified_flags_seen=unsigned(*reinterpret_cast<std::uint64_t*>(main_base+re4::profile::input_flags)&15);MovementTurn::Callback(actor);}
    else if(settings.unified_controls&&static_cast<std::uint8_t*>(actor)[0x115]<=4)MovementTurn::Callback(actor);
    if(modern_probe)AimSensitivity::Callback(actor,0);
    assert(ModernRoot::Callback(actor,nullptr)==17);
}
unsigned test_ModernRoot(void* actor,void*){
 ++modern_original_roots;
 if(native_root_probe){
  auto* bytes=static_cast<std::uint8_t*>(actor);re4::ViewVec3 a,b,r;
  RootDisplacement::Callback(actor,bytes+0x228,0,&a,&r);assert(r.x==1&&r.y==2&&r.z==3);
  RootDisplacement::Callback(actor,*reinterpret_cast<void**>(bytes+0x320),0,&b,&r);
  root_result={.25f*a.x+.75f*b.x,.25f*a.y+.75f*b.y,.25f*a.z+.75f*b.z};
 }
 ModernPose::Callback(actor,0);return 17;
}
void test_ModernPose(void*,int){++modern_original_poses;}
std::vector<re4::SensorState> sensor_batch;
std::uint8_t sampler_seen[20]{};
std::uint32_t shadow_flags_seen=0;
unsigned rom_result=0;
bool native_view_test=false,submit_models=false,short_laser_ray=false,simulate_weapon_sway=false;
unsigned camera_native_calls=0,basis_native_calls=0,laser_native_calls=0,hud_native_calls=0,ortho_native_calls=0;
unsigned laser_draw_seen=0,laser_reuse_seen=0;
std::vector<void*> submitted_models;
std::vector<std::vector<std::uint8_t>> hud_rects;
unsigned gx_vertices=0,gx_last_alpha=0;
void* test_weapon=nullptr;void* test_other_model=nullptr;
re4::ViewMatrix3x4 native_basis{{{1,0,0,0},{0,1,0,0},{0,0,1,0}}};
re4::ViewQfpsOutput native_profile{{-500,1765,-1190},{-180,1700,-110},{0,1340,1480},0.2f,50},camera_collision_input{};
re4::ViewVec3 laser_endpoint{0,0,-1000};
void test_CameraBasis(void*,void* out){++basis_native_calls;std::memcpy(out,&native_basis,sizeof(native_basis));}
void test_ModelSubmit(void* model){submitted_models.push_back(model);}
void test_WeaponLaser(void* weapon,unsigned draw,unsigned reuse,unsigned){
    ++laser_native_calls;laser_draw_seen=draw;laser_reuse_seen=reuse;
    if(!(reuse&1)){
        std::memcpy(reinterpret_cast<void*>(main_base+0x1265320),&laser_endpoint,sizeof(laser_endpoint));
        if(!short_laser_ray)std::memcpy(static_cast<std::uint8_t*>(weapon)+0x40C,&laser_endpoint,sizeof(laser_endpoint));
    }
}
void test_HudDraw(void*){++hud_native_calls;}
bool calculate_probe=false;unsigned look_calls=0;re4::ViewVec3 look_seen{};
void test_FinalLookAt(void* out,const re4::ViewVec3* eye,const re4::ViewVec3* up,const re4::ViewVec3* target){
 ++look_calls;look_seen=*target;
 re4::ViewVec3 back{eye->x-target->x,eye->y-target->y,eye->z-target->z};
 auto normalize=[](re4::ViewVec3& v){const float l=std::hypot(v.x,v.y,v.z);v.x/=l;v.y/=l;v.z/=l;};normalize(back);
 re4::ViewVec3 right{up->y*back.z-up->z*back.y,up->z*back.x-up->x*back.z,up->x*back.y-up->y*back.x};normalize(right);
 re4::ViewVec3 actual_up{back.y*right.z-back.z*right.y,back.z*right.x-back.x*right.z,back.x*right.y-back.y*right.x};
 re4::ViewMatrix3x4 view{};const re4::ViewVec3 rows[]={right,actual_up,back};
 for(unsigned i=0;i<3;++i){view.m[i][0]=rows[i].x;view.m[i][1]=rows[i].y;view.m[i][2]=rows[i].z;view.m[i][3]=-(rows[i].x*eye->x+rows[i].y*eye->y+rows[i].z*eye->z);}
 std::memcpy(out,&view,sizeof(view));
}
void test_CameraCalculate(){
 if(!calculate_probe)return;
 CameraInput::Callback(reinterpret_cast<void*>(main_base+0x11319C0));
 auto* s=*reinterpret_cast<std::uint8_t**>(main_base+0xB1AE08);
 FinalLookAt::Callback(s+0xD8,reinterpret_cast<re4::ViewVec3*>(s+0x14C),reinterpret_cast<re4::ViewVec3*>(s+0x16C),reinterpret_cast<re4::ViewVec3*>(s+0x158));
 // FDB0's native frustum construction occurs only after this view is built.
 assert(look_calls>0);
}
void test_native_ortho(void* out,float top,float bottom,float left,float right,float near_plane,float far_plane){
    ++ortho_native_calls;assert(top==0&&bottom==448&&left==0&&right==512&&near_plane==0&&far_plane==-100);
    std::memset(out,0,sizeof(re4::ViewMatrix4x4));
}
void test_native_projection(const void*,unsigned mode){assert(mode==1);}
void test_native_pose(const void*,unsigned index){assert(index==0);}
void test_native_matrix(unsigned index){assert(index==0);}
void test_native_rect(const void* packet){const auto* p=static_cast<const std::uint8_t*>(packet);re4::ViewReticleRect r;std::memcpy(&r,p+0xC,sizeof(r));if(r.width&&r.height)hud_rects.emplace_back(p,p+0x18);}
std::vector<re4::ViewVec3> gx_quad;unsigned gx_blend_last=0,gx_format_last=0;
void test_gx_blend(unsigned a,unsigned b,unsigned c,unsigned d){assert(a==1&&d==0);assert((b==4&&c==5)||(b==1&&c==0));gx_blend_last=b;}
void test_gx_format(unsigned a,unsigned b,unsigned c,unsigned d,unsigned e){assert(a==0&&b==9&&c==1&&e==0);gx_format_last=d;}
void test_gx_begin(unsigned a,unsigned b,unsigned n){assert(a==0x98&&b==0&&n==4);gx_quad.clear();}
void test_gx_position(float x,float y,float z){++gx_vertices;gx_quad.push_back({x,y,z});}
void test_gx_color(unsigned r,unsigned g,unsigned b,unsigned a){assert(r==255&&g==255&&b==255&&a==128);gx_last_alpha=a;}
void test_gx_end(unsigned token){
 assert(token==0x34&&gx_quad.size()==4);const auto a=gx_quad[0],b=gx_quad[3];
 re4::ViewReticleRect rect{std::uint16_t(std::lround(a.x)),std::uint16_t(std::lround(a.y)),std::uint16_t(std::lround(b.x-a.x)),std::uint16_t(std::lround(b.y-a.y))};
 std::vector<std::uint8_t> packet(0x18);packet[8]=packet[9]=packet[10]=255;packet[11]=128;std::memcpy(packet.data()+0xC,&rect,sizeof(rect));hud_rects.push_back(packet);
}

unsigned test_RomMount(const char*,void*,std::size_t){return rom_result;}
extern "C" Result nn_mount_rom(const char*,void*,std::size_t){return rom_result;}
extern "C" int get_operation_mode(){return game_test::operation_mode;}
void test_SamplerCreate(void*,void*,const std::uint8_t* descriptor){std::memcpy(sampler_seen,descriptor,20);}
void test_ResolutionMap(int width,int height,int* display,int* back){*display=width==1280&&height==720?2:0;*back=*display==2?3:1;}
void test_FrustumUpdate(void* frustum){auto* value=static_cast<std::uint8_t*>(frustum);std::memcpy(value+0x24,value+0x18,4);}
void test_SceneSubmit(){
    if(submit_models){
        ModelSubmit::Callback(*reinterpret_cast<void**>(main_base+re4::profile::player_slot));
        ModelSubmit::Callback(test_weapon);ModelSubmit::Callback(test_other_model);
    }
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
    *reinterpret_cast<float*>(b+0xB4)+=b[0x115]==5 ? 3.14159265f : (settings.unified_controls?0.f:0.04f);
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
        if(simulate_weapon_sway&&game_test::native_aim)offsets[0]=*reinterpret_cast<const float*>(main_base+0x1265A08);
        if(game_test::camera_gate_change==1)bytes[0x26C]=0;
        if(game_test::camera_gate_change==2)bytes[0x222]=1;
        if(native_view_test){
            re4::ViewMatrix3x4 basis{};CameraBasis::Callback(camera,&basis);
            re4::ViewQfpsOutput out{};CameraApply::Callback(camera,&out);
            camera_collision_input=out; // Native18990 collision is the next consumer.
        }else CameraApply::Callback(camera,nullptr);
    }
}
void test_CameraApply(void* camera,void* output){
    ++camera_native_calls;
    if(output)std::memcpy(output,&native_profile,sizeof(native_profile));
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
    if(simulate_weapon_sway){pitch[0]+=.03f;pitch[1]+=.03f;}
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
    std::vector<std::uint8_t> input_state=std::vector<std::uint8_t>(0x8700);
    alignas(4) std::uint8_t input_type[16]{};
    alignas(16) std::uint8_t weapon_context[0x60]{},weapon[0x500]{},other_model[0x500]{};
    Fixture() {
        sensor.stop();sensor={};gyro.reset();gyro_session={};gyro_pending={};last_sensor_tick=0;sensor_batch.clear();
        free_orbit.reset();camera_context=nullptr;
        unified_look.reset();unified_update=nullptr;unified_pitch=0;unified_pitch_actor=0;unified_pitch_route=0;
        unified_aim_session={};unified_new_aim=false;unified_command_frame=0;unified_command_tick=0;
        unified_root_actor=nullptr;unified_camera_calculation=false;
        native_view_test=submit_models=short_laser_ray=simulate_weapon_sway=false;laser_endpoint={0,0,-1000};camera_native_calls=basis_native_calls=laser_native_calls=hud_native_calls=ortho_native_calls=0;
        submitted_models.clear();hud_rects.clear();test_weapon=test_other_model=nullptr;
        native_basis={{{1,0,0,0},{0,1,0,0},{0,0,1,0}}};
        native_profile={{-500,1765,-1190},{-180,1700,-110},{0,1340,1480},0.2f,50};
        view_settings_snapshot=0;view_frame=1;view_camera_frame=0;first_person_frame=0;
        view_camera_actor=0;view_camera_tick=0;view_camera_aim=false;aim_point.sequence=0;aim_point.frame=0;
        pending_aim_point.sequence=0;pending_aim_point.frame=0;displayed_aim_point.sequence=0;displayed_aim_point.frame=0;
        fps_override_count=0;reticle_draw_count=0;
        main_view_actor=nullptr;
        modern_probe=false;modern_original_updates=modern_original_roots=modern_original_poses=0;
        game_test::memory_maps.clear();modern_update_actor=nullptr;modern_cycle.reset();modern_pose.discard();
        modern_native={};modern_bound_owner={};modern_bound_asset=modern_bound_clip=0;
        modern_epoch=0;modern_root_done=modern_stepped=modern_pose_done=false;
        native_ortho=test_native_ortho;native_projection=test_native_projection;
        native_pose=test_native_pose;native_matrix=test_native_matrix;native_rect=test_native_rect;
        native_gx_blend=test_gx_blend;native_gx_format=test_gx_format;native_gx_begin=test_gx_begin;
        native_gx_position=test_gx_position;native_gx_color=test_gx_color;native_gx_end=test_gx_end;
        gx_vertices=gx_last_alpha=0;gx_blend_last=gx_format_last=0;gx_quad.clear();
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
    void aim(){game_test::native_aim=1;actor[0x115]=6;actor[0x116]=1;actor[0x117]=1;
        *reinterpret_cast<std::uintptr_t*>(image.data()+0x1265A28)=main_base+0x31CC60;
        if(!*reinterpret_cast<void**>(image.data()+0xB1AE08))*reinterpret_cast<void**>(image.data()+0xB1AE08)=input_state.data();
        flags()=0x10;aiming_predicate=test_is_aiming;}
    std::uint8_t* camera(){return image.data()+0x11319C0;}
    float* look(){return reinterpret_cast<float*>(camera()+0x270);}
    void camera_mode(){
        actor[0x115]=1;game_test::native_aim=0;aiming_predicate=test_is_aiming;
        image[0x1131718+0x54]=10;camera()[0x26C]=2;
        *reinterpret_cast<void**>(image.data()+0xB1AE08)=input_state.data();
        *reinterpret_cast<void**>(image.data()+0xB1AE10)=input_type;
    }
    void view_mode(){
        camera_mode();image[0x1131718+0x48]=1;native_view_test=true;
        re4::ViewMatrix3x4 body{{{1,0,0,100},{0,1,0,200},{0,0,1,300}}};
        std::memcpy(actor+0x18,&body,sizeof(body));
        *reinterpret_cast<void**>(actor+0x938)=weapon_context;
        *reinterpret_cast<void**>(weapon_context+0x38)=weapon;
        test_weapon=weapon;test_other_model=other_model;
        const re4::ViewMatrix3x4 view{{{1,0,0,0},{0,1,0,0},{0,0,1,0}}};
        const re4::ViewMatrix4x4 projection{{{0.75f,0,0,0},{0,1,0,0},{0,0,-0.001001f,-100.1001f},{0,0,-1,0}}};
        std::memcpy(input_state.data()+0xD8,&view,sizeof(view));
        std::memcpy(input_state.data()+0x10C,&projection,sizeof(projection));
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
        Fixture f;f.view_mode();f.aim();settings.unified_controls=true;settings.re6_response=true;settings.analog_enabled=false;
        settings.camera_gain=0;settings.right_aim_gain_x=1;settings.right_aim_gain_y=0;
        sensor_attributes=1;*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;*reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        control.raw_rx=1;control.raw_ry=1;modern_probe=true;
        ModernPlayerUpdate::Callback(f.actor);
        for(unsigned n=0;n<40;++n){game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);}
        assert(f.yaw()<-1.5f&&near(unified_pitch,0)); // RE6 aim gain is independent of overview gain.
        const float before=f.yaw();settings.right_aim_gain_x=0;settings.right_aim_gain_y=1;
        game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);assert(near(f.yaw(),before));
        for(unsigned n=0;n<10;++n){game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);}
        assert(near(f.yaw(),before)&&unified_pitch>.1f); // Gain0 drains its axis without freezing the other.
        modern_probe=false;game_test::native_aim=0;f.actor[0x115]=1;f.flags()=1;
        const float walking_before=f.yaw();
        for(unsigned n=0;n<10;++n){game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);}
        assert(near(f.yaw(),walking_before)); // Overview still follows its own zero gain.
        settings.camera_gain=1;game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);
        for(unsigned n=0;n<10;++n){game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);}
        assert(f.yaw()<walking_before-1.f);
    }
    {
        Fixture f;f.view_mode();
        re6::BuildId id{};std::memcpy(id.data(),re4::profile::build_id,32);
        assert(telemetry_ring.initialize(100,id,re6::kRe4TitleId));
        telemetry_last_frame=0;
        settings.remap_enabled=true;settings.button_map[0]=1;
        NpadState pad{};pad.sample=53;pad.buttons=1;pad.lx=-32768;pad.ly=321;pad.rx=17321;pad.ry=-12345;pad.attributes=1;
        game_test::tick=192000;capture(pad,0,4);assert(pad.buttons!=1);
        game_test::tick+=640000;CameraCalculate::Callback();
        re6::Record r{};std::uint64_t words[32]{};
        for(unsigned i=0;i<32;++i)words[i]=telemetry_ring.storage.slots[0].words[i].load();
        std::memcpy(&r,words,sizeof(r));
        assert(r.validity==15&&r.buttons==1&&r.sample_id==53&&r.input_tick==192000);
        assert(r.tick==832000&&r.frame==view_frame&&r.sticks[0]==-32768&&r.sticks[2]==17321);
        assert(r.player[3]==100&&r.player[7]==200&&r.player[11]==300&&r.view[0]==1&&r.projection[0]==.75f);
        CameraCalculate::Callback();assert(telemetry_ring.storage.header.sequence.load()==4); // No duplicate simulation frame.
        view_frame.fetch_add(1);game_test::tick+=1920001;CameraCalculate::Callback();
        for(unsigned i=0;i<32;++i)words[i]=telemetry_ring.storage.slots[1].words[i].load();
        std::memcpy(&r,words,sizeof(r));assert(!(r.validity&re6::InputValid));
        NpadState other=pad;other.rx=-99;other.attributes=1;capture(other,1,1);
        re6::Record input{};assert(!telemetry_read_input(game_test::tick,input)); // Secondary pad cannot replace primary snapshot.
        pad.attributes=0;capture(pad,0,4);assert(!telemetry_read_input(game_test::tick,input));
    }
    for(bool aim:{false,true}){
        Fixture f;f.view_mode();if(aim)f.aim();settings.unified_controls=true;settings.analog_enabled=false;
        sensor_attributes=1;*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;*reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        modern_probe=aim;control.raw_rx=1;control.raw_ry=1;ModernPlayerUpdate::Callback(f.actor);
        game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);const float yaw=f.yaw(),pitch=unified_pitch;
        settings.camera_gain=0;game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);
        assert(near(f.yaw(),yaw)&&near(unified_pitch,pitch));
        settings.camera_gain=1;game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);assert(near(f.yaw(),yaw));
        game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);assert(near(f.yaw()-yaw,-.038524245f));
        modern_probe=false;
    }

    for(bool fps:{false,true}){
        Fixture f;f.view_mode();f.aim();settings.unified_controls=true;settings.analog_enabled=false;settings.first_person=fps;
        sensor_attributes=1;*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;*reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        f.camera()[0x26C]=0;modern_probe=true;simulate_weapon_sway=true;
        ModernPlayerUpdate::Callback(f.actor);assert(near(unified_pitch,0)&&f.pitch()[1]>.02f);
        game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);assert(near(unified_pitch,0));
        const re4::ViewVec3 eye{},up{0,1,0},target{0,0,1000};std::memcpy(f.input_state.data()+0x14C,&eye,sizeof(eye));std::memcpy(f.input_state.data()+0x158,&target,sizeof(target));std::memcpy(f.input_state.data()+0x16C,&up,sizeof(up));
        calculate_probe=true;CameraCalculate::Callback();calculate_probe=false;
        assert(near(look_seen.x,0)&&near(look_seen.y,0));assert(near(game_test::rendered_pitch,0));assert(f.look()[0]>.02f);
        modern_probe=false;
    }
    for(bool aim:{false,true}){
        Fixture f;f.view_mode();if(aim)f.aim();settings.unified_controls=true;settings.analog_enabled=false;
        sensor_attributes=1;*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;*reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        modern_probe=aim;ModernPlayerUpdate::Callback(f.actor);unified_pitch=.85f;
        f.pitch()[0]=f.pitch()[1]=.541126806f;control.raw_ry=1;
        game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);assert(near(unified_pitch,.872664626f));
        for(unsigned i=0;i<5;++i){game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);assert(near(unified_pitch,.872664626f));}
        modern_probe=false;
    }
    {
        Fixture f;f.view_mode();f.aim();settings.unified_controls=true;settings.analog_enabled=false;settings.gyro_enabled=true;
        sensor_attributes=1;*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;*reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        modern_probe=true;ModernPlayerUpdate::Callback(f.actor);unified_pitch=1.2f;f.pitch()[0]=f.pitch()[1]=.763943727f;
        assert(gyro_aim_ready(f.actor,true));gyro_pending={0,.1f};control.raw_ry=1;
        game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);assert(near(unified_pitch,1.3f));
        control.raw_ry=0;game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);assert(near(unified_pitch,1.3f));
        control.raw_ry=-1;game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);assert(unified_pitch>1.2f&&unified_pitch<1.3f);
        // Gyro can reach the native pole; stable analytic up keeps the view finite.
        gyro_pending={0,.5f};game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);assert(near(unified_pitch,1.570796327f));
        const re4::ViewVec3 eye{},up{0,1,0},target{0,0,1000};std::memcpy(f.input_state.data()+0x14C,&eye,sizeof(eye));std::memcpy(f.input_state.data()+0x158,&target,sizeof(target));std::memcpy(f.input_state.data()+0x16C,&up,sizeof(up));
        f.camera()[0x26C]=0;calculate_probe=true;CameraCalculate::Callback();calculate_probe=false;assert(look_seen.y>990);
        const auto* view=reinterpret_cast<const re4::ViewMatrix3x4*>(f.input_state.data()+0xD8);for(const auto& row:view->m)for(float value:row)assert(std::isfinite(value));
        modern_probe=false;
    }

    {
        Fixture f;f.view_mode();settings.unified_controls=true;settings.analog_enabled=false;
        sensor_attributes=1;*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;
        *reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        control.raw_lx=1;control.raw_ly=0;control.raw_rx=1;control.raw_ry=1;f.flags()=15;unified_probe=true;
        ModernPlayerUpdate::Callback(f.actor);assert(unified_flags_seen==1);assert((f.flags()&15)==15);
        game_test::tick+=960000;const float before=f.yaw();ModernPlayerUpdate::Callback(f.actor);
        assert(near(f.yaw()-before,-.038524245f));assert(near(game_test::motion_speed_seen,1.25f));
        unified_probe=false;
    }

    {
        Fixture f;f.view_mode();f.aim();settings.unified_controls=true;settings.analog_enabled=false;
        sensor_attributes=1;*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;*reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        control.raw_rx=1;control.raw_ry=1;control.raw_lx=-1;control.raw_ly=-1;modern_probe=true;
        auto* axes=reinterpret_cast<int*>(f.image.data()+0x124E9C8);axes[0]=63;axes[1]=-63;
        ModernPlayerUpdate::Callback(f.actor);assert(seen_aim_x==0&&seen_aim_y==0);
        game_test::tick+=960000;const auto before=f.yaw();ModernPlayerUpdate::Callback(f.actor);
        assert(near(f.yaw()-before,-.038524245f));assert(near(f.pitch()[1],.018393972f));
        assert(axes[0]==63&&axes[1]==-63);assert(near(f.pitch()[0],f.pitch()[1]));
        const auto yaw=f.yaw();AimSensitivity::Callback(f.actor,0);assert(near(f.yaw(),yaw));
        const float expected_yaw[]={-.080367656f,-.095760823f};unsigned interval=0;
        for(unsigned phase:{2u,1u}){
            f.actor[0x116]=phase;f.actor[0x117]=phase==1?4:1;
            game_test::tick+=960000;const float value=f.yaw();ModernPlayerUpdate::Callback(f.actor);
            assert(near(f.yaw()-value,expected_yaw[interval++]));
        }
        settings.unified_controls=false;publish_view_settings();settings.unified_controls=true;publish_view_settings();
        game_test::tick+=960000;const float toggled=f.yaw();ModernPlayerUpdate::Callback(f.actor);assert(near(f.yaw(),toggled));
        NpadState switched{};switched.attributes=1;switched.rx=switched.ry=32767;
        capture(switched,0,4);game_test::tick+=960000;const float stopped=f.yaw();ModernPlayerUpdate::Callback(f.actor);assert(near(f.yaw(),stopped));
        f.actor[0x117]=1;game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);
        f.flags()=0;AimSensitivity::Callback(f.actor,0);f.flags()=0x10;f.actor[0x117]=4;
        game_test::tick+=960000;const float released=f.yaw();ModernPlayerUpdate::Callback(f.actor);assert(near(f.yaw(),released));
        modern_probe=false;
    }
    {
        Fixture f;f.view_mode();settings.unified_controls=true;settings.analog_enabled=false;
        sensor_attributes=1;*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;*reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        const re4::ViewMatrix3x4 basis{{{2,0,0,0},{0,3,0,0},{0,0,4,0}}};std::memcpy(f.actor+0x18,&basis,sizeof(basis));
        void* secondary=f.other_model;*reinterpret_cast<void**>(f.actor+0x320)=secondary;
        control.raw_lx=1;control.raw_ly=0;native_root_probe=true;
        ModernPlayerUpdate::Callback(f.actor);
        re4::ViewVec3 world;assert(re4::view_transform_point(basis,root_result,world));
        assert(near(world.x,-28)&&near(world.y,13.5f)&&std::fabs(world.z)<.0001f);
        assert(control.modern_move_steps==0);native_root_probe=false;
    }
    {
        Fixture f;f.view_mode();f.aim();settings.unified_controls=true;settings.reticle=re4::ReticleStyle::Crosshair;
        sensor_attributes=1;*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;*reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        f.camera()[0x26C]=0;modern_probe=true;ModernPlayerUpdate::Callback(f.actor);modern_probe=false;
        laser_endpoint={500,300,1000};
        const re4::ViewVec3 eye{10,20,30},up{0,1,0},target{0,0,100};
        std::memcpy(f.input_state.data()+0x14C,&eye,sizeof(eye));std::memcpy(f.input_state.data()+0x158,&target,sizeof(target));std::memcpy(f.input_state.data()+0x16C,&up,sizeof(up));
        WeaponLaser::Callback(f.weapon,1,0,0);calculate_probe=true;CameraCalculate::Callback();calculate_probe=false;
        assert(near(look_seen.x,eye.x)&&near(look_seen.y,eye.y)&&look_seen.z>eye.z);
        re4::ViewMatrix3x4 view;re4::ViewMatrix4x4 projection;std::memcpy(&view,f.input_state.data()+0xD8,sizeof(view));std::memcpy(&projection,f.input_state.data()+0x10C,sizeof(projection));
        re4::ViewHudPoint point;assert(re4::project_view_point(view,projection,laser_endpoint,point));assert(std::fabs(point.x-256)>5);
        SceneSubmit::Callback();view_frame.fetch_add(1);SceneDraw::Callback();
        CameraInput::Callback(f.camera());HudDraw::Callback(nullptr);assert(hud_rects.size()==4);
        const auto drawn=std::size_t(hud_rects.size());
        // Animated/near-wall endpoints must move the truthful marker, never the view.
        for(auto ray:{re4::ViewVec3{15,21,40},re4::ViewVec3{-300,200,800},re4::ViewVec3{10,20,50000}}){
            laser_endpoint=ray;WeaponLaser::Callback(f.weapon,1,0,0);calculate_probe=true;CameraCalculate::Callback();calculate_probe=false;
            const auto* current=reinterpret_cast<const re4::ViewMatrix3x4*>(f.input_state.data()+0xD8);
            for(unsigned i=0;i<3;++i)for(unsigned j=0;j<4;++j)assert(near(current->m[i][j],view.m[i][j]));
        }
        view_frame.fetch_add(1);CameraInput::Callback(f.camera());HudDraw::Callback(nullptr);assert(hud_rects.size()==drawn);
    }
    {
        Fixture f;f.view_mode();settings.unified_controls=true;settings.analog_enabled=false;
        sensor_attributes=1;*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;*reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        control.raw_ry=1;ModernPlayerUpdate::Callback(f.actor);game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);
        control.raw_ry=0;f.aim();modern_probe=true;
        f.actor[0x116]=0;game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);
        f.actor[0x116]=1;f.actor[0x117]=4;game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);
        f.actor[0x117]=1;game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);
        assert(near(f.pitch()[1],.018393972f));modern_probe=false;
        f.actor[0x115]=1;game_test::native_aim=0;dpad_move=true;f.flags()=1;unified_probe=true;
        ModernPlayerUpdate::Callback(f.actor);assert(unified_flags_seen==1);unified_probe=false;
    }
    for(bool fps:{false,true}){
        Fixture f;f.view_mode();settings.unified_controls=true;settings.first_person=fps;settings.analog_enabled=false;
        sensor_attributes=1;*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;*reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        control.raw_rx=1;control.raw_ry=1;ModernPlayerUpdate::Callback(f.actor);game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);
        const re4::ViewVec3 eye{},up{0,1,0},target{0,0,1000};std::memcpy(f.input_state.data()+0x14C,&eye,sizeof(eye));std::memcpy(f.input_state.data()+0x158,&target,sizeof(target));std::memcpy(f.input_state.data()+0x16C,&up,sizeof(up));
        calculate_probe=true;CameraCalculate::Callback();calculate_probe=false;
        assert(near(look_seen.x/1000,std::sin(-.038524245f)*std::cos(.028893184f)));
        assert(near(look_seen.y/1000,std::sin(.028893184f)));assert(f.look()[0]==0&&f.look()[1]==0);
    }
    {
        Fixture f;f.view_mode();f.aim();settings.unified_controls=true;settings.analog_enabled=false;settings.gyro_enabled=true;
        sensor_attributes=1;*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;*reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        game_test::apply_sway=true;assert(gyro_aim_ready(f.actor,true));gyro_pending={.02f,.03f};AimSensitivity::Callback(f.actor,0);
        assert(near(f.yaw(),.02f)&&near(f.pitch()[1],.03f*.636619772368f));assert(near(game_test::sway_baseline,f.pitch()[1]));
    }
    {
        Fixture f;f.view_mode();settings.unified_controls=true;settings.analog_enabled=false;
        sensor_attributes=1;*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;*reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        control.raw_rx=1;control.raw_ry=1;unified_probe=true;ModernPlayerUpdate::Callback(f.actor);
        game_test::tick+=960000;const float before=f.yaw();update_state_change=1;ModernPlayerUpdate::Callback(f.actor);
        assert(near(f.yaw(),before)&&unified_pitch==0);update_state_change=0;unified_probe=false;
    }
    {
        Fixture f;f.aim();settings.gyro_enabled=true;game_test::aim_step=0;
        NpadState pad{};pad.attributes=1;capture(pad,0,1);
        f.actor[0x116]=0;f.sample(1);poll_sensors();
        f.actor[0x116]=1;sensor_batch.clear();game_test::tick+=96'000;poll_sensors();
        // An empty readiness-edge read does not flush the hardware FIFO.
        f.sample(2);const auto s=sensor_batch[0];sensor_batch={s,s,s};
        sensor_batch[0].number=4;sensor_batch[1].number=3;
        game_test::tick+=96'000;poll_sensors();AimSensitivity::Callback(f.actor,0);
        assert(f.pitch()[1]==0 && f.yaw()==0);
        game_test::tick+=96'000;poll_sensors();AimSensitivity::Callback(f.actor,0);
        assert(f.pitch()[1]==0 && f.yaw()==0); // Repeated FIFO must not replay the drained samples.
        for(unsigned n=5;n<=8;++n){game_test::tick+=96'000;f.sample(n);poll_sensors();AimSensitivity::Callback(f.actor,0);}
        assert(f.pitch()[1]>0);
    }
    {
        Fixture f;f.aim();settings.gyro_enabled=true;game_test::aim_step=0;
        assert(gyro_aim_ready(f.actor,true));gyro_pending={0.2f,0.2f};
        f.flags()=0;AimSensitivity::Callback(f.actor,0); // Release after the poll.
        f.flags()=0x10;AimSensitivity::Callback(f.actor,0);
        assert(f.pitch()[1]==0 && f.yaw()==0); // Old session cannot be revived by re-holding.
    }
    {
        Fixture f;f.aim();settings.gyro_enabled=true;game_test::aim_step=0;
        NpadState pad{};pad.attributes=1;capture(pad,0,1);
        // Broad native camera-aim predicate is already true while the weapon
        // is rising. Neither this motion nor its queued FIFO may move aim.
        for(auto phase : {0,1}){
            f.actor[0x116]=std::uint8_t(phase);f.actor[0x117]=phase?4:1;
            for(unsigned n=1;n<=8;++n){
                game_test::tick+=96'000;f.sample(unsigned(phase)*8+n);poll_sensors();
                AimSensitivity::Callback(f.actor,0);
                assert(f.pitch()[0]==0 && f.pitch()[1]==0 && f.yaw()==0);
            }
        }
        f.actor[0x117]=1;
        // Native state can become ready after polling, within actor update.
        // Consumption must not arm early and replay a pre-transition sample.
        gyro_pending={0.2f,0.2f};AimSensitivity::Callback(f.actor,0);
        assert(f.pitch()[1]==0 && f.yaw()==0);
        game_test::tick+=96'000;f.sample(17);poll_sensors();
        AimSensitivity::Callback(f.actor,0);assert(f.pitch()[1]==0); // Drain readiness-edge batch.
        for(unsigned n=18;n<=22;++n){game_test::tick+=96'000;f.sample(n);poll_sensors();AimSensitivity::Callback(f.actor,0);}
        assert(f.pitch()[1]>0);
        const float ready_pitch=f.pitch()[1];
        f.actor[0x116]=2;f.actor[0x117]=1;
        for(unsigned n=23;n<=25;++n){game_test::tick+=96'000;f.sample(n);poll_sensors();AimSensitivity::Callback(f.actor,0);}
        assert(f.pitch()[1]>ready_pitch); // Fire must not restart the entry gate.
        f.flags()=0;game_test::tick+=96'000;f.sample(26);poll_sensors();
        f.flags()=0x10;f.actor[0x116]=1;f.actor[0x117]=4;
        const float before=f.pitch()[1];
        for(unsigned n=27;n<=31;++n){game_test::tick+=96'000;f.sample(n);poll_sensors();AimSensitivity::Callback(f.actor,0);}
        assert(f.pitch()[1]==before); // A new raise cannot reuse the previous latch.
    }
    for(const auto handler : {0x31CC60u,0x31B9F0u,0x3212E0u,0x3270F0u,0x332CC0u,0x333DF0u,0x3351A0u,0x342F90u}){
        Fixture f;f.aim();settings.gyro_enabled=true;game_test::aim_step=0;
        *reinterpret_cast<std::uintptr_t*>(f.image.data()+0x1265A28)=main_base+handler;
        f.actor[0x117]=4;assert(!gyro_aim_ready(f.actor,true));
        f.actor[0x117]=0;assert(!gyro_aim_ready(f.actor,true));
        f.actor[0x117]=1;assert(gyro_aim_ready(f.actor,true));
        for(unsigned sub : {2,3,4,5}){f.actor[0x117]=std::uint8_t(sub);assert(gyro_aim_ready(f.actor));}
        f.actor[0x116]=2;assert(gyro_aim_ready(f.actor));
        f.actor[0x116]=4;gyro_pending={0.1f,0.1f};AimSensitivity::Callback(f.actor,0);
        assert(f.yaw()==0 && f.pitch()[1]==0); // Reload began after sample collection.
        f.actor[0x116]=1;f.actor[0x117]=4;assert(!gyro_aim_ready(f.actor,true));
        f.actor[0x117]=1;assert(gyro_aim_ready(f.actor,true));
        f.input_state[0x5034]++;gyro_pending={0.1f,0.1f};AimSensitivity::Callback(f.actor,0);
        assert(f.yaw()==0 && f.pitch()[1]==0); // Late weapon switch cannot consume the old delta.
        assert(gyro_aim_ready(f.actor,true));
        f.input_state[0x86FF]=1;assert(!gyro_aim_ready(f.actor));
        f.input_state[0x86FF]=0;f.actor[0x117]=4;assert(!gyro_aim_ready(f.actor,true));
        f.actor[0x117]=1;assert(gyro_aim_ready(f.actor,true));
        settings.gyro_enabled=false;assert(!gyro_aim_ready(f.actor));
        settings.gyro_enabled=true;f.actor[0x117]=4;assert(!gyro_aim_ready(f.actor,true));
    }
    for(unsigned style=0;style<3;++style)for(unsigned size=0;size<3;++size){
        Fixture f;f.view_mode();f.aim();f.camera()[0x26C]=0;
        settings.reticle=static_cast<re4::ReticleStyle>(style);settings.reticle_size=static_cast<re4::ReticleSize>(size);publish_view_settings();
        CameraInput::Callback(f.camera());WeaponLaser::Callback(f.weapon,1,0,0);SceneSubmit::Callback();
        view_frame.fetch_add(1);SceneDraw::Callback();CameraInput::Callback(f.camera());HudDraw::Callback(nullptr);
        const auto preview=re4::reticle_marks(settings.reticle,settings.reticle_size);
        assert(hud_rects.size()==preview.count);
        for(unsigned i=0;i<preview.count;++i)assert(!std::memcmp(hud_rects[i].data()+0xC,&preview.rects[i],sizeof(re4::ViewReticleRect)));
    }
    for(bool knife:{false,true}){
        Fixture f;f.view_mode();f.aim();f.actor[0x117]=1;
        if(knife){f.actor[0x115]=11;f.flags()=0x800ull;}
        *reinterpret_cast<std::uint16_t*>(f.actor+0x352)=0x300;
        *reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        *reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;
        *reinterpret_cast<std::uintptr_t*>(f.image.data()+0x1265A28)=main_base+0x31CC60;
        sensor_attributes=1;
        game_test::memory_maps={{reinterpret_cast<std::uintptr_t>(f.actor),sizeof(f.actor),3},
            {reinterpret_cast<std::uintptr_t>(f.input_state.data()),f.input_state.size(),3}};
        settings.analog_enabled=settings.gyro_enabled=false;
        settings.move_while_aiming=!knife;settings.move_while_knife=knife;
        control.raw_lx=1;control.raw_ly=-1;control.raw_rx=0.5f;control.raw_ry=0.25f;
        auto expected=re4::shape_aim_input(1,-1,0.5f,0.25f,settings,{true,63});
        modern_probe=true;ModernPlayerUpdate::Callback(f.actor);
        assert(seen_aim_x==expected.x && seen_aim_y==expected.y && control.aim_source==1);
        assert(modern_original_updates==1 && modern_original_roots==1 && modern_original_poses==1 && modern_update_actor==nullptr);
        assert(control.modern_move_steps==0 && control.modern_pose_steps==0); // Missing assets keep original frame safe.
        control.raw_lx=0;control.raw_ly=0;control.raw_rx=-0.25f;
        expected=re4::shape_aim_input(0,0,-0.25f,0.25f,settings,{true,63});
        ModernPlayerUpdate::Callback(f.actor);
        assert(seen_aim_x==expected.x && seen_aim_y==expected.y); // Neutral left still owns right aiming.
        settings.move_while_aiming=settings.move_while_knife=false;
        auto* axes=reinterpret_cast<std::int32_t*>(f.image.data()+0x124E9C8);axes[0]=51;axes[1]=-31;
        ModernPlayerUpdate::Callback(f.actor);
        assert(seen_aim_x==51 && seen_aim_y==-31 && axes[0]==51 && axes[1]==-31);
        assert(control.modern_move_status==0 && modern_original_updates==3 && modern_original_roots==3 && modern_original_poses==3);
    }
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
    {
        Fixture f;f.view_mode();settings.third_person_distance=1.35f;
        // A framing setting alone must create the verified view context, even
        // with analog controls, first person and the reticle disabled.
        settings.analog_enabled=false;settings.free_camera=false;
        CameraInput::Callback(f.camera());
        assert(camera_native_calls==1&&basis_native_calls==1);
        assert(near(camera_collision_input.eye0.x,-612)&&near(camera_collision_input.eye0.y,1787.75f)&&near(camera_collision_input.eye0.z,-1568));
        assert(near(camera_collision_input.target.x,-112)&&near(camera_collision_input.target.y,1362.75f)&&near(camera_collision_input.target.z,1102));
        assert(std::memcmp(&camera_collision_input.eye1,&native_profile.eye1,sizeof(re4::ViewVec3))==0);
        assert(camera_collision_input.fov==native_profile.fov&&camera_collision_input.roll==native_profile.roll);
        assert(first_person_frame.load()==0&&fps_override_count.load()==0);
        submit_models=true;SceneSubmit::Callback();
        assert(submitted_models.size()==3&&submitted_models[0]==f.actor); // Framing never hides the player.
        settings.third_person_distance=2;CameraInput::Callback(f.camera());
        assert(near(camera_collision_input.eye0.x,-820)&&near(camera_collision_input.target.z,400));
        settings.third_person_distance=1;CameraInput::Callback(f.camera());
        assert(std::memcmp(&camera_collision_input,&native_profile,sizeof(native_profile))==0);
        settings.third_person_distance=1.35f;settings.first_person=true;CameraInput::Callback(f.camera());
        assert(near(camera_collision_input.eye0.x,100)&&near(camera_collision_input.eye0.y,1900)&&near(camera_collision_input.eye0.z,300));
        assert(near(camera_collision_input.target.x-camera_collision_input.eye0.x,500));
    }
    for(unsigned routine:{6u,11u,15u})for(unsigned phase:{1u,2u}){
        Fixture f;f.view_mode();f.aim();settings.third_person_distance=1.35f;
        f.actor[0x115]=routine;f.actor[0x116]=phase;f.actor[0x117]=1;f.camera()[0x26C]=0;
        f.flags()=routine==11?0x800ull:0x10;
        CameraInput::Callback(f.camera());
        assert(near(camera_collision_input.eye0.z,-1568)&&near(camera_collision_input.target.z,1102));
        assert(std::memcmp(&camera_collision_input.eye1,&native_profile.eye1,sizeof(re4::ViewVec3))==0);
        assert(first_person_frame.load()==0);
    }
    for(unsigned gate=0;gate<17;++gate){
        Fixture f;f.view_mode();settings.third_person_distance=1.35f;
        if(gate==0)f.camera()[0x222]=1;
        if(gate==1)f.image[0x1131718+0x54]=16;
        if(gate==2)f.image[0x1131718+0x54]=17;
        if(gate==3)f.input_state[0x5092]=4;
        if(gate==4)f.input_state[0x509E]=0x80;
        if(gate==5)f.input_state[0x1AB]=0x40;
        if(gate==6)f.actor[0x114]=1;
        if(gate==7)f.actor[0x115]=5;
        if(gate==8)f.image[0x1131718+0x48]=0;
        if(gate==9)native_basis.m[2][2]=0;
        if(gate==10)game_test::camera_gate_change=2;
        if(gate==11)game_test::camera_gate_change=1;
        if(gate==12)native_basis.m[0][0]=std::numeric_limits<float>::quiet_NaN();
        if(gate==13)settings.third_person_distance=std::numeric_limits<float>::quiet_NaN();
        if(gate==14)settings.third_person_distance=2.01f;
        if(gate==15)settings.third_person_distance=0.99f;
        if(gate==16)aiming_predicate=nullptr;
        CameraInput::Callback(f.camera());
        assert(std::memcmp(&camera_collision_input,&native_profile,sizeof(native_profile))==0);
        assert(first_person_frame.load()==0&&camera_context==nullptr);
    }
    {
        Fixture f;f.view_mode();settings.third_person_distance=2;
        native_basis={{{0,0,2,30},{0,3,0,20},{-4,0,0,10}}};
        CameraInput::Callback(f.camera());
        // Adjustment is in native profile space before this exact frame basis
        // and collision consume it; no actor-yaw approximation is involved.
        assert(near(camera_collision_input.eye0.x,-820)&&near(camera_collision_input.eye0.z,-2270));
        assert(std::memcmp(&camera_collision_input.eye1,&native_profile.eye1,sizeof(re4::ViewVec3))==0);
    }
    for(unsigned invalid=0;invalid<8;++invalid){
        Fixture f;f.view_mode();settings.third_person_distance=1.35f;
        const float nan=std::numeric_limits<float>::quiet_NaN();
        if(invalid==0)native_profile.eye0.x=nan;
        if(invalid==1)native_profile.eye1.y=nan;
        if(invalid==2)native_profile.target.z=nan;
        if(invalid==3)native_profile.roll=nan;
        if(invalid==4)native_profile.fov=nan;
        if(invalid==5)native_profile.eye0=native_profile.eye1;
        if(invalid==6)native_profile.target=native_profile.eye0;
        if(invalid==7){native_profile.eye0.x=std::numeric_limits<float>::max();native_profile.eye1.x=-native_profile.eye0.x;}
        CameraInput::Callback(f.camera());
        assert(std::memcmp(&camera_collision_input,&native_profile,sizeof(native_profile))==0);
    }
    for(unsigned gate=0;gate<5;++gate){
        Fixture f;f.view_mode();settings.third_person_distance=1.35f;
        CameraContext context{};context.camera=f.camera();context.actor=f.actor;
        context.view_allowed=context.basis_valid=true;context.basis=native_basis;
        auto* camera=f.camera();
        if(gate==0)context.actor=f.other_model; // A captured actor must still be the player.
        if(gate==1)camera=f.camera()+0x400;
        if(gate==2)context.basis_valid=false;
        if(gate==3)context.view_allowed=false;
        camera_context=gate==4?nullptr:&context;
        re4::ViewQfpsOutput result{};CameraApply::Callback(camera,&result);camera_context=nullptr;
        assert(std::memcmp(&result,&native_profile,sizeof(native_profile))==0);
    }
    {
        Fixture f;f.view_mode();settings.third_person_distance=1;
        // Native bypass must retain payload bits, including signed zero/NaN.
        native_profile.eye0.x=-0.f;
        const std::uint32_t nan_bits=0x7FC12345;
        std::memcpy(&native_profile.target.z,&nan_bits,sizeof(nan_bits));
        CameraInput::Callback(f.camera());
        assert(std::memcmp(&camera_collision_input,&native_profile,sizeof(native_profile))==0);
    }
    {
        Fixture f;f.view_mode();CameraInput::Callback(f.camera());
        assert(camera_native_calls==1&&basis_native_calls==1);
        assert(std::memcmp(&camera_collision_input,&native_profile,sizeof(native_profile))==0);
        assert(first_person_frame.load()==0); // Disabled means exact original output.
        settings.first_person=true;settings.analog_enabled=false;settings.free_camera=false;
        CameraInput::Callback(f.camera());
        assert(near(camera_collision_input.eye0.x,100)&&near(camera_collision_input.eye0.y,1900)&&near(camera_collision_input.eye0.z,300));
        assert(std::memcmp(&camera_collision_input.eye0,&camera_collision_input.eye1,sizeof(re4::ViewVec3))==0);
        assert(near(camera_collision_input.target.x-camera_collision_input.eye0.x,500));
        assert(near(camera_collision_input.target.y-camera_collision_input.eye0.y,-425));
        assert(camera_collision_input.fov==native_profile.fov&&camera_collision_input.roll==native_profile.roll);
        assert(first_person_frame.load()==view_frame.load());
        publish_graphics_diagnostics();assert(control.fps_overrides==1);
        control.raw_rx=1;game_test::tick+=320'000;CameraInput::Callback(f.camera());
        for(unsigned n=0;n<30;++n){game_test::tick+=320'000;CameraInput::Callback(f.camera());}
        assert(f.look()[1]<-1); // FPS enables continuous orbit independently of analog/free toggles.
        f.look()[0]=f.look()[1]=0;control.raw_rx=0; // Native14380 consumed yaw before entering weapon mode.
        game_test::tick+=320'000;CameraInput::Callback(f.camera());assert(f.look()[0]==0&&f.look()[1]==0);
        // FPS cannot restore its cached pre-aim orbit after the native handoff.
        submit_models=true;SceneSubmit::Callback();
        assert(submitted_models.size()==2&&submitted_models[0]==f.weapon&&submitted_models[1]==f.other_model);
        ModelSubmit::Callback(f.actor);assert(submitted_models.back()==f.actor); // Independent shadow/outside submit scope remains native.
        submitted_models.clear();settings.first_person=false;publish_view_settings();SceneSubmit::Callback();
        assert(submitted_models.size()==3&&submitted_models[0]==f.actor);
    }
    for(unsigned gate=0;gate<10;++gate){
        Fixture f;f.view_mode();settings.first_person=true;
        if(gate==0)f.camera()[0x222]=1;
        if(gate==1)f.image[0x1131718+0x54]=16;
        if(gate==2)f.input_state[0x5092]=4;
        if(gate==3)f.input_state[0x509E]=0x80;
        if(gate==4)f.actor[0x114]=1;
        if(gate==5)f.actor[0x115]=5;
        if(gate==6)f.image[0x1131718+0x48]=0;
        if(gate==7)native_basis.m[2][2]=0;
        if(gate==8)game_test::camera_gate_change=2;
        if(gate==9)std::memset(f.actor+0x18,0,sizeof(re4::ViewMatrix3x4));
        CameraInput::Callback(f.camera());
        assert(std::memcmp(&camera_collision_input,&native_profile,sizeof(native_profile))==0);
        assert(first_person_frame.load()==0&&camera_context==nullptr);
    }
    {
        Fixture f;f.view_mode();settings.first_person=true;settings.reticle=re4::ReticleStyle::Dot;
        f.aim();f.camera()[0x26C]=0;
        auto* axes=reinterpret_cast<std::int8_t*>(f.image.data()+0x124EC38+8);axes[0]=7;axes[1]=-9;
        control.raw_rx=1;control.raw_ry=-1;gyro_pending={0.2f,0.3f};
        CameraInput::Callback(f.camera());
        assert(game_test::camera_x==7&&game_test::camera_y==-9&&axes[0]==7&&axes[1]==-9);
        assert(near(gyro_pending.yaw,0.2f)&&near(gyro_pending.pitch,0.3f));
        assert(first_person_frame.load()==view_frame.load());
        WeaponLaser::Callback(f.weapon,1,0,0);HudDraw::Callback(nullptr);
        assert(laser_native_calls==1&&laser_draw_seen==1&&laser_reuse_seen==0);
        assert(hud_native_calls==1&&ortho_native_calls==0&&hud_rects.empty()); // Current simulation has not been displayed yet.
        SceneSubmit::Callback();
        view_frame.fetch_add(1);SceneDraw::Callback(); // Display previous submitted scene.
        CameraInput::Callback(f.camera());
        // Current simulation camera differs: late HUD must use the displayed camera captured above.
        *reinterpret_cast<float*>(f.input_state.data()+0xD8+0xC)=9000;
        HudDraw::Callback(nullptr);assert(ortho_native_calls==1&&!hud_rects.empty());
        assert(gx_vertices>0&&gx_last_alpha==128&&gx_blend_last==1&&gx_format_last==3);
        publish_graphics_diagnostics();assert(control.reticle_draws==1);
        const auto size=hud_rects.size();
        f.input_state[0x5034]=1;HudDraw::Callback(nullptr);assert(hud_rects.size()==size); // Same model, changed weapon ID.
        f.input_state[0x5034]=0;
        view_frame.fetch_add(1);HudDraw::Callback(nullptr);assert(hud_rects.size()==size); // Stale display stamp.
        CameraInput::Callback(f.camera());WeaponLaser::Callback(f.weapon,1,1,0);SceneSubmit::Callback();
        view_frame.fetch_add(1);SceneDraw::Callback();CameraInput::Callback(f.camera());HudDraw::Callback(nullptr);
        assert(hud_rects.size()==size); // Native reuse does not freshen old endpoint.
        WeaponLaser::Callback(f.other_model,1,0,0);SceneSubmit::Callback();
        view_frame.fetch_add(1);SceneDraw::Callback();CameraInput::Callback(f.camera());HudDraw::Callback(nullptr);
        assert(hud_rects.size()==size);
        *reinterpret_cast<float*>(f.input_state.data()+0xD8+0xC)=0;
        settings.reticle=re4::ReticleStyle::Crosshair;settings.reticle_size=re4::ReticleSize::Large;publish_view_settings();
        WeaponLaser::Callback(f.weapon,1,0,0);SceneSubmit::Callback();
        view_frame.fetch_add(1);SceneDraw::Callback();CameraInput::Callback(f.camera());HudDraw::Callback(nullptr);
        assert(hud_rects.size()==size+4);
        const auto cross_size=hud_rects.size();f.image[0x1131718+0x54]=17;HudDraw::Callback(nullptr);
        assert(hud_rects.size()==cross_size);
        f.image[0x1131718+0x54]=10;f.input_state[0x7D]=0x20;HudDraw::Callback(nullptr);
        assert(hud_rects.size()==cross_size); // Native HUDdisable bit13 respected.
        f.input_state[0x7D]=0;game_test::tick+=2'000'000;HudDraw::Callback(nullptr);assert(hud_rects.size()==cross_size);
    }
    {
        Fixture f;f.view_mode();f.aim();settings.first_person=true;
        for(unsigned routine:{6u,11u,15u})for(unsigned phase:{1u,2u}){
            f.actor[0x115]=routine;f.actor[0x116]=phase;f.actor[0x117]=1;f.camera()[0x26C]=0;
            f.flags()=routine==11?(0x800ull):0x10;
            CameraInput::Callback(f.camera());assert(first_person_frame.load()==view_frame.load());
        }
        f.actor[0x115]=11;f.actor[0x116]=1;f.flags()=0x800ull;settings.gyro_enabled=true;
        settings.left_deadzone=0;control.raw_lx=0.5f;control.raw_ly=0.5f;
        gyro_pending={0.3f,0.4f};f.yaw()=0;
        AimSensitivity::Callback(f.actor,0);
        assert(near(f.yaw(),0.04f*32/63)&&near(f.pitch()[0],0.08f*32/63)); // Knife gets shaped stick, zero gyro injection.
        assert(!firearm_aim(f.actor));
        NpadState pad{};pad.attributes=1;capture(pad,0,1);f.sample(1);poll_sensors();
        game_test::tick+=320'000;f.sample(2);poll_sensors();
        assert(gyro_pending.yaw==0&&gyro_pending.pitch==0&&control.gyro_active==0);
    }
    {
        Fixture f;f.view_mode();f.aim();settings.reticle=re4::ReticleStyle::Dot;f.camera()[0x26C]=0;
        CameraInput::Callback(f.camera());
        const re4::ViewVec3 stale{10000,0,-1000};std::memcpy(f.weapon+0x40C,&stale,sizeof(stale));
        short_laser_ray=true;laser_endpoint={0,0,-200}; // Native skips model+40C copy below500units.
        WeaponLaser::Callback(f.weapon,1,0,0);SceneSubmit::Callback();
        view_frame.fetch_add(1);SceneDraw::Callback();CameraInput::Callback(f.camera());HudDraw::Callback(nullptr);
        assert(hud_rects.size()==1); // Fresh global clipped endpoint wins over stale model cache.
    }
    {
        Fixture f;f.view_mode();settings.analog_enabled=false;settings.free_camera=true;
        settings.camera_return_speed=0;settings.reticle=re4::ReticleStyle::Dot;
        f.look()[0]=0.5f;f.look()[1]=0.7f;CameraInput::Callback(f.camera());
        assert(near(f.look()[0],0.45f)&&near(f.look()[1],0.63f)); // Reticle alone cannot enable camera controls.
        assert(std::memcmp(&camera_collision_input,&native_profile,sizeof(native_profile))==0);
        assert(fps_override_count.load()==0);
    }
    {
        Fixture f;f.view_mode();settings.first_person=true;
        native_basis={{{0,0,2,30},{0,3,0,20},{-4,0,0,10}}};
        CameraInput::Callback(f.camera());
        assert(basis_native_calls==1&&near(camera_collision_input.eye0.x,-72.5f));
        assert(near(camera_collision_input.eye0.y,1880.f/3)&&near(camera_collision_input.eye0.z,35));
        assert(camera_collision_input.fov==50&&camera_collision_input.roll==0.2f);
    }
    {
        Fixture f;f.view_mode();settings.first_person=true;f.aim();
        f.actor[0x115]=11;f.actor[0x116]=2;f.flags()=0x800ull;f.camera()[0x26C]=0;
        for(unsigned recovery=0;recovery<5;++recovery){f.actor[0x117]=recovery;CameraInput::Callback(f.camera());assert(first_person_frame.load()==view_frame.load());}
        f.actor[0x115]=6;f.actor[0x116]=4;f.flags()=0;game_test::native_aim=0;f.camera()[0x26C]=2;
        CameraInput::Callback(f.camera());assert(first_person_frame.load()==view_frame.load()); // Native generic reload remains FPS.
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
        for(unsigned n=1;n<=5;++n){game_test::tick+=320'000;f.sample(n);PadNormalize::Callback(nullptr);}
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
        assert(control.status==1 && game_test::installed==26 && game_test::initialized==1);
        assert(PadNormalize::Callback(nullptr)==7);PadNormalize::Callback(nullptr);
        assert(control.frames==2 && game_test::mounts==1); // No filesystem polling per frame.
    }
    {
        Fixture f;f.actor[0x115]=1;f.flags()=0x45;
        raw_left_x=0.575f;control.raw_ly=0.575f;settings.turn_gain=0;
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
        settings.stick_curve=re4::StickCurve::Exponential;raw_left_x=0.575f;control.raw_ly=0.575f;
        MovementTurn::Callback(f.actor);
        assert(near(f.yaw(),0.01f) && near(game_test::motion_speed_seen,1.25f*0.25f));
        control.raw_rx=0.575f;CameraInput::Callback(nullptr);assert(game_test::camera_x==16);
        f.aim();control.raw_lx=0.575f;control.raw_ly=0.575f;f.yaw()=0;
        AimSensitivity::Callback(f.actor,0);assert(near(f.yaw(),0.04f*16/63) && near(f.pitch()[0],0.08f*16/63));
    }
    {
        Fixture f;f.aim();consume_aim_axes=true;aim_full_scale=127;
        std::vector<std::uint8_t> global(0x8700);std::uint8_t type[16]{};type[11]=2;
        *reinterpret_cast<void**>(f.image.data()+0xB1AE08)=global.data();
        *reinterpret_cast<void**>(f.image.data()+0xB1AE10)=type;
        *reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;
        auto* axes=reinterpret_cast<std::int32_t*>(f.image.data()+0x124E9C8);axes[0]=31;axes[1]=-63;
        control.raw_lx=1;control.raw_ly=1;control.raw_rx=0.25f;control.raw_ry=-0.5f;
        settings.left_deadzone=settings.right_deadzone=0;
        settings.right_aim_gain_x=0.5f;settings.right_aim_gain_y=0.25f;
        settings.gyro_enabled=true;assert(gyro_aim_ready(f.actor,true));
        gyro_pending.yaw=0.01f;gyro_pending.pitch=0.02f;
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
        assert(gyro_aim_ready(f.actor,true));
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
        control.raw_lx=0.575f;control.raw_ly=0.575f;settings.aim_gain_x=settings.aim_gain_y=0.3f;
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
    std::puts("actual game callbacks: identity, flags, motion, camera, additive aim/gyro, native raise, FIFO and pause PASS");
}
