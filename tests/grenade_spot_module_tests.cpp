#define main game_module_baseline_main
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#include "game_module_tests.cpp"
#pragma GCC diagnostic pop
#undef main

namespace {
std::array<std::uint8_t,0xB0> spot_bones[2];
unsigned spot_rays=0,spot_spheres=0;
bool spot_floor=true;
bool spot_origin_hit=false,spot_hidden=false;
bool spot_water=false,spot_wall=false;
void* test_spot_bone(void*,unsigned index){return index==0?spot_bones[0].data():index==10?spot_bones[1].data():nullptr;}
unsigned test_spot_segment(void*,const re4::ViewVec3* from,const re4::ViewVec3* to,re4::ViewVec3* point,re4::ViewVec3* normal,unsigned flags,unsigned exclude){
    ++spot_rays;assert(flags==0&&(exclude==0||exclude==0x4000));
    if(point)*point=*to;
    if(normal)*normal={};
    *reinterpret_cast<unsigned*>(main_base+0x1131250)=0xDEADBEEF;
    *reinterpret_cast<std::uintptr_t*>(main_base+0x1131260)=0xBAD;
    std::memset(reinterpret_cast<void*>(main_base+0x1131268),0xF1,1025);
    if(exclude&&normal&&from->y-to->y>10000&&spot_floor){if(point)*point={from->x,0,from->z};*normal={0,1,0};return 1u<<24;}
    if(!exclude&&normal&&spot_origin_hit&&from->x==to->x&&from->y==to->y&&from->z==to->z){*normal={1,0,0};return 1u<<24;}
    if(exclude&&normal&&spot_hidden){if(point)*point=*from;*normal={0,0,1};return 1u<<24;}
    return 0;
}
void test_spot_sphere(void*,re4::ViewVec3* normal,const re4::ViewVec3*,re4::ViewVec3* center,unsigned flags,unsigned exclude,float radius){
    ++spot_spheres;assert(flags==0x2001&&exclude==0x4000&&radius==25);
    *normal={};if(spot_floor&&center->y<=25){center->y=25;*normal={0,1,0};}
    if(spot_wall&&center->x+center->z < -1975){
        const float push=(-1975-center->x-center->z)*.5f;
        center->x+=push;center->z+=push;*normal={.70710678f,0,.70710678f};
    }
}
int test_spot_water(const re4::ViewVec3*,float* height){
    std::memset(reinterpret_cast<void*>(main_base+0x113CC24),0xD7,0x28);
    if(spot_water){*height=200;return 1;}return 0;
}
struct SpotFixture {
    Fixture f;
    std::array<std::uint8_t,0xF0> mesh{};
    SpotFixture(){
        grenade_spot_reset();grenade_spot_zoom_reset();spot_rays=spot_spheres=0;spot_floor=true;spot_origin_hit=spot_hidden=spot_water=spot_wall=false;
        // Sentinel only the collision scratch itself. The former0x2018 fill
        // also poisoned main-camera reset/profile/follow fields and prevented
        // any actual camera pipeline test from exercising the override.
        std::memset(f.image.data()+0x1131250,0x55,0x419);
        f.view_mode();f.aim();settings.grenade_landing_spot=true;f.input_state[0x5034]=0x13;
        *reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;
        *reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        *reinterpret_cast<std::uintptr_t*>(f.image.data()+0x1265A28)=main_base+0x31B9F0;
        const re4::ViewMatrix3x4 basis{{{-1,0,0,0},{0,1,0,0},{0,0,-1,0}}};
        std::memcpy(f.actor+0x18,&basis,sizeof(basis));
        for(auto& bone:spot_bones){bone.fill(0);const re4::ViewVec3 p{0,500,-1000};std::memcpy(bone.data()+0x80,&p,sizeof(p));std::memcpy(bone.data()+0x98,&p,sizeof(p));}
        const auto pool=reinterpret_cast<std::uintptr_t>(mesh.data());
        std::memcpy(f.image.data()+0xAE24C8,&pool,8);
        *reinterpret_cast<unsigned*>(f.image.data()+0xAE24D0)=1;
        *reinterpret_cast<unsigned*>(f.image.data()+0xAE24D4)=0xF0;
        const re4::ViewMatrix3x4 view{{{1,0,0,0},{0,1,0,-1000},{0,0,1,0}}};
        std::memcpy(f.input_state.data()+0xD8,&view,sizeof(view));
        const re4::ViewMatrix4x4 projection{{{1,0,0,0},{0,1,0,0},{0,0,-.001001001f,-100.1001f},{0,0,-1,0}}};
        std::memcpy(f.input_state.data()+0x10C,&projection,sizeof(projection));
        native_grenade_bone=test_spot_bone;native_grenade_segment=test_spot_segment;native_grenade_sphere=test_spot_sphere;
        native_grenade_water=test_spot_water;
        game_test::memory_maps={{reinterpret_cast<std::uintptr_t>(f.image.data()),f.image.size(),3},
            {reinterpret_cast<std::uintptr_t>(f.actor),sizeof(f.actor),3},
            {reinterpret_cast<std::uintptr_t>(f.weapon),sizeof(f.weapon),3},
            {reinterpret_cast<std::uintptr_t>(f.weapon_context),sizeof(f.weapon_context),3},
            {reinterpret_cast<std::uintptr_t>(f.input_state.data()),f.input_state.size(),3},
            {pool,mesh.size(),1},{reinterpret_cast<std::uintptr_t>(spot_bones),sizeof(spot_bones),1}};
        health_color_probe=true;publish_view_settings();
    }
    void submit(){SceneSubmit::Callback();view_frame.fetch_add(1);SceneDraw::Callback();}
    void zoom_profile(float weight){
        const re4::ViewQfpsOutput neutral{{-400,2300,-2760},{-260,1630,-130},{0,1210,1410},0,45};
        const re4::ViewQfpsOutput endpoint=weight>=0
            ?re4::ViewQfpsOutput{{-400,600,-2600},{-265,1280,-350},{0,2770,1380},0,45}
            :re4::ViewQfpsOutput{{-400,2800,-1300},{-200,1550,220},{0,500,1500},0,45};
        const auto mix=[weight](re4::ViewVec3 a,re4::ViewVec3 b){const float t=std::fabs(weight);return re4::ViewVec3{
            a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t};};
        native_profile={mix(neutral.eye0,endpoint.eye0),mix(neutral.eye1,endpoint.eye1),mix(neutral.target,endpoint.target),0,45};
        const auto resource=reinterpret_cast<std::uintptr_t>(f.image.data()+0x1000);
        std::memcpy(reinterpret_cast<void*>(resource+0x2C),&neutral,sizeof(neutral));
        *reinterpret_cast<std::uintptr_t*>(f.camera()+0x1B0)=resource;
        f.camera()[0x26C]=0;settings.unified_controls=true;settings.re6_response=true;sensor_attributes=1;
        publish_view_settings();
        unified_pitch_actor=reinterpret_cast<std::uintptr_t>(f.actor);unified_pitch=weight*1.570796327f;
        f.yaw()=3.141592653589793f;f.pitch()[0]=f.pitch()[1]=weight;
        grenade_spot_cache.tick=0;grenade_spot_update(f.actor);
    }
    void zoom_camera(unsigned ticks=0){
        game_test::tick+=ticks;
        native_follow_probe=world_follow_probe=calculate_probe=camera_projection_probe=true;
        CameraCalculate::Callback();calculate_probe=false;
    }
    void final_view(float command,bool response=true){
        settings.unified_controls=true;settings.re6_response=response;sensor_attributes=1;
        unified_pitch_actor=reinterpret_cast<std::uintptr_t>(f.actor);unified_pitch=command;
        f.yaw()=3.141592653589793f;
        view_camera_actor=unified_pitch_actor;view_camera_frame=view_frame.load();view_camera_tick=game_test::tick;
        view_camera_native_pitch=false;
        const re4::ViewVec3 eye{0,1000,0},target{0,1000,-1000},up{0,1,0};
        std::memcpy(f.input_state.data()+0x14C,&eye,sizeof(eye));
        std::memcpy(f.input_state.data()+0x158,&target,sizeof(target));
        std::memcpy(f.input_state.data()+0x16C,&up,sizeof(up));
        unified_camera_calculation=true;
        FinalLookAt::Callback(f.input_state.data()+0xD8,
            reinterpret_cast<re4::ViewVec3*>(f.input_state.data()+0x14C),
            reinterpret_cast<re4::ViewVec3*>(f.input_state.data()+0x16C),
            reinterpret_cast<re4::ViewVec3*>(f.input_state.data()+0x158));
        unified_camera_calculation=false;
    }
};
}
int main(){
    {
        SpotFixture h;ModernPlayerUpdate::Callback(h.f.actor);h.final_view(1.2f);
        // A cached burst/visibility change must never turn the owned camera.
        assert(near(look_seen.y,1932.0391f)&&near(look_seen.z,-362.35776f));
    }
    // Native grenade items all select profile2, not the ordinary firearm
    // profile0. Exercise its real up/neutral/down records through18520->18990,
    // normal world follow, current FOV projection, and final stable look-at.
    for(unsigned item:{0x13u,0x16u,0x17u})for(float weight:{-1.f,0.f,1.f})for(float clipping:{1.f,.4f}){
        SpotFixture h;h.f.input_state[0x5034]=std::uint8_t(item);h.zoom_profile(weight);
        assert(grenade_spot_cache.valid);const auto landing=grenade_spot_cache.landing;
        const auto rays=spot_rays,spheres=spot_spheres;native_collision_fraction=clipping;
        h.zoom_camera();assert(camera_collision_input.fov==45);
        const auto eye=camera_collision_input.eye0;const auto initial_target=look_seen;
        float previous=45;
        for(unsigned i=0;i<6;++i){h.zoom_camera(960000);assert(camera_collision_input.fov>=previous&&camera_collision_input.fov<=68);previous=camera_collision_input.fov;}
        assert(camera_collision_input.fov>67.9f&&near(native_profile.fov,45));
        assert(!std::memcmp(&eye,&camera_collision_input.eye0,sizeof(eye)));
        assert(near(look_seen.x,initial_target.x)&&near(look_seen.y,initial_target.y)&&near(look_seen.z,initial_target.z));
        assert(unified_pitch==weight*1.570796327f&&h.f.pitch()[0]==weight&&h.f.pitch()[1]==weight);
        assert(rays==spot_rays&&spheres==spot_spheres&&!std::memcmp(&landing,&grenade_spot_cache.landing,sizeof(landing)));
        const auto& projection=*reinterpret_cast<const re4::ViewMatrix4x4*>(h.f.input_state.data()+0x10C);
        const auto& view=*reinterpret_cast<const re4::ViewMatrix3x4*>(h.f.input_state.data()+0xD8);
        assert(std::fabs(projection.m[1][1]-1/std::tan(camera_collision_input.fov*.00872664626f))<.0001f);
        assert(re4::draw_grenade_landing_spot(native_hud_backend(),view,projection,landing));
        // Native order: queue scene, then next frame draws its camera snapshot
        // before current simulation changes prediction/camera for the next scene.
        SceneSubmit::Callback();view_frame.fetch_add(1);SceneDraw::Callback();
        const auto before=gx_colors.size();HudDraw::Callback(nullptr);assert(gx_colors.size()>before);
        const auto displayed=displayed_grenade_spot.frame.load();
        spot_hidden=true;grenade_spot_update(h.f.actor);h.zoom_camera(192000);
        assert(displayed_grenade_spot.frame.load()==displayed);
        assert(near(look_seen.x,initial_target.x)&&near(look_seen.y,initial_target.y)&&near(look_seen.z,initial_target.z));
        assert(camera_collision_input.fov>67.9f); // Hidden/no-cache markers do not twitch the lens or angle.
    }
    for(unsigned phase:{2u,3u,4u}){
        SpotFixture h;h.zoom_profile(0);h.zoom_camera();h.zoom_camera(9600000);
        assert(camera_collision_input.fov>67.99f);
        h.f.actor[0x116]=std::uint8_t(phase);h.f.flags()=0;game_test::native_aim=0;h.f.camera()[0x26C]=2;
        h.zoom_camera(960000);assert(camera_collision_input.fov>57&&camera_collision_input.fov<59);
        h.zoom_camera(2880000);assert(camera_collision_input.fov>45&&camera_collision_input.fov<45.5f);
    }
    {
        SpotFixture h;h.zoom_profile(0);native_profile.fov=80;h.zoom_camera();h.zoom_camera(5760000);
        assert(camera_collision_input.fov==80);
    }
    for(unsigned gate=0;gate<5;++gate){
        SpotFixture h;h.zoom_profile(0);h.zoom_camera();h.zoom_camera(9600000);
        assert(camera_collision_input.fov>67.99f);
        if(gate==0)settings.first_person=true;
        if(gate==1)settings.re6_response=false;
        if(gate==2)h.f.input_state[0x86FF]=1;
        if(gate==3)h.f.image[0x1131718+0x54]=16;
        if(gate==4)h.f.input_state[0x503C]=1;
        h.zoom_camera(192000);assert(camera_collision_input.fov==45);
        settings.first_person=false;settings.re6_response=true;h.f.input_state[0x86FF]=0;
        h.f.image[0x1131718+0x54]=10;h.f.input_state[0x503C]=0;
        h.zoom_camera(192000);assert(camera_collision_input.fov==45); // Fresh eligibility has no inherited zoom.
    }
    {
        SpotFixture h;h.zoom_profile(0);h.zoom_camera();h.zoom_camera(9600000);
        re4::ViewQfpsOutput other{};
        CameraApply::Callback(h.f.image.data()+0x2000,&other);
        h.zoom_camera();assert(camera_collision_input.fov>67.99f); // Secondary cameras cannot reset main lens.
    }
    {
        SpotFixture h;std::array<std::uint8_t,0x2018> scratch;
        std::memcpy(scratch.data(),h.f.image.data()+0x1131250,scratch.size());
        ModernPlayerUpdate::Callback(h.f.actor);
        assert(spot_spheres>0&&spot_spheres<=96&&spot_rays>spot_spheres);
        assert(!std::memcmp(scratch.data(),h.f.image.data()+0x1131250,scratch.size()));
        assert(grenade_spot_point.frame.load()==view_frame.load());
        const auto sweeps=spot_spheres;ModernPlayerUpdate::Callback(h.f.actor);assert(spot_spheres==sweeps);
        h.submit();assert(displayed_grenade_spot.frame.load()==view_frame.load());
        const auto before=gx_colors.size();HudDraw::Callback(nullptr);assert(gx_colors.size()>before);
        assert(gx_blend_last==1&&gx_format_last==3);
        h.f.actor[0x116]=2;h.submit();const auto thrown=gx_colors.size();HudDraw::Callback(nullptr);assert(gx_colors.size()==thrown);
    }
    for(unsigned gate=0;gate<10;++gate){
        SpotFixture h;
        if(gate==0){settings.grenade_landing_spot=false;publish_view_settings();}
        if(gate==1)h.f.input_state[0x86FF]=1;
        if(gate==2)h.f.input_state[0x5034]=1;
        if(gate==3)h.f.actor[0x116]=2;
        if(gate==4)game_test::native_aim=0;
        if(gate==5)*reinterpret_cast<float*>(h.f.input_state.data()+0xA4)=0;
        if(gate==6)*reinterpret_cast<unsigned*>(h.f.image.data()+0xAE24D4)=0;
        if(gate==7)*reinterpret_cast<unsigned*>(h.f.image.data()+0xAE24D0)=4097;
        if(gate==8)native_grenade_sphere=nullptr;
        if(gate==9)game_test::memory_maps.clear();
        ModernPlayerUpdate::Callback(h.f.actor);assert(!grenade_spot_point.frame.load()&&!spot_rays&&!spot_spheres);
    }
    {
        SpotFixture h;spot_floor=false;ModernPlayerUpdate::Callback(h.f.actor);
        assert(!grenade_spot_point.frame.load()&&spot_spheres<=96);
        h.submit();assert(!displayed_grenade_spot.frame.load());
    }
    {
        SpotFixture h;spot_origin_hit=true;ModernPlayerUpdate::Callback(h.f.actor);
        re4::ViewVec3 point;assert(read_aim_point(grenade_spot_point,h.f.actor,view_frame.load(),point));
        assert(near(point.x,500)); // Native origin correction, not actor position.
        const auto before=spot_spheres;game_test::tick+=1'919'999;ModernPlayerUpdate::Callback(h.f.actor);assert(spot_spheres==before);
        ++game_test::tick;ModernPlayerUpdate::Callback(h.f.actor);assert(spot_spheres>before);
    }
    {
        SpotFixture h;ModernPlayerUpdate::Callback(h.f.actor);const auto before=spot_spheres;
        h.f.input_state[0x5020]=1;ModernPlayerUpdate::Callback(h.f.actor);assert(spot_spheres>before&&grenade_spot_cache.room==1);
        SceneSubmit::Callback();h.f.input_state[0x5021]=1;view_frame.fetch_add(1);SceneDraw::Callback();
        assert(!displayed_grenade_spot.frame.load()); // Reject a staged old room even if pointers are reused.
        ModernPlayerUpdate::Callback(h.f.actor);h.submit();assert(displayed_grenade_spot.frame.load());
        h.f.input_state[0x5020]=2;const auto colors=gx_colors.size();HudDraw::Callback(nullptr);assert(gx_colors.size()==colors);
    }
    {
        SpotFixture h;spot_hidden=true;
        std::array<std::uint8_t,0x2018> scratch;std::memcpy(scratch.data(),h.f.image.data()+0x1131250,scratch.size());
        ModernPlayerUpdate::Callback(h.f.actor);assert(!grenade_spot_point.frame.load()&&spot_spheres>0);
        assert(!std::memcmp(scratch.data(),h.f.image.data()+0x1131250,scratch.size()));
    }
    {
        SpotFixture h;*reinterpret_cast<unsigned*>(h.mesh.data()+8)=1;
        *reinterpret_cast<std::uint16_t*>(h.mesh.data()+0x3A)=8193;
        std::array<std::uint8_t,0x2018> scratch;std::memcpy(scratch.data(),h.f.image.data()+0x1131250,scratch.size());
        ModernPlayerUpdate::Callback(h.f.actor);assert(!grenade_spot_point.frame.load()&&!spot_rays&&!spot_spheres);
        assert(!std::memcmp(scratch.data(),h.f.image.data()+0x1131250,scratch.size()));
    }
    for(unsigned item:{0x13u,0x16u,0x17u}){
        SpotFixture h;h.f.input_state[0x5034]=std::uint8_t(item);ModernPlayerUpdate::Callback(h.f.actor);
        assert(grenade_spot_point.frame.load()&&spot_spheres>0);
        settings.grenade_landing_spot=false;publish_view_settings();assert(!grenade_spot_point.frame.load());
    }
    for(unsigned item:{0x13u,0x16u,0x17u}){
        SpotFixture h;h.f.input_state[0x5034]=std::uint8_t(item);spot_water=true;
        h.f.image[0xAE24C0+0x256]=1; // Native water contact requires active material record2.
        std::array<std::uint8_t,0x28> scratch;std::memcpy(scratch.data(),h.f.image.data()+0x113CC24,scratch.size());
        ModernPlayerUpdate::Callback(h.f.actor);
        assert(bool(grenade_spot_point.frame.load())==(item==0x13)); // Other types extinguish/remove, not a predicted burst.
        assert(!std::memcmp(scratch.data(),h.f.image.data()+0x113CC24,scratch.size()));
    }
    {
        SpotFixture h;ModernPlayerUpdate::Callback(h.f.actor);const auto explosion=grenade_spot_cache.landing;
        // The hand grenade continues beyond its first bounce until its fuse.
        assert(explosion.time==45&&explosion.steps==46&&spot_spheres>20);
        h.f.input_state[0x5020]=0x1B;h.f.input_state[0x5021]=2;
        ModernPlayerUpdate::Callback(h.f.actor);assert(!grenade_spot_point.frame.load());
    }
    {
        SpotFixture h;spot_water=true;ModernPlayerUpdate::Callback(h.f.actor);
        assert(!grenade_spot_point.frame.load()); // Invalid water material fails closed.
    }
    std::puts("actual grenade marker callbacks: private bounded prediction, scratch restoration, staging and exclusions PASS");
}
