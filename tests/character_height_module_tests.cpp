#define main game_module_baseline_main
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#include "game_module_tests.cpp"
#pragma GCC diagnostic pop
#undef main

namespace {
constexpr re4::ViewMatrix3x4 height_identity{{{1,0,0,0},{0,1,0,0},{0,0,1,0}}};
struct HeightFixture {
    Fixture f;
    alignas(32) std::uint8_t body[0x1A8]{},bin[0x60]{},bones[0x80]{};
    HeightFixture(){
        character_appearance_reset();f.view_mode();character_renderer_ready.store(true);
        settings.unified_controls=true;settings.re6_response=true;sensor_attributes=1;
        settings.character_appearance=re4::CharacterAppearance::RamonSalazar;
        character_store(f.image.data(),0xAFF39C,2);character_store(f.input_state.data(),0xA4,1.f);
        f.actor[0x11A]=119;character_store(f.actor,0x180,static_cast<void*>(body));
        character_store(f.actor,0x108,static_cast<void*>(bones));character_store(body,0x18,static_cast<void*>(bin));bin[0x29]=119;
        for(const auto& range:std::vector<MemoryInfo>{{main_base,f.image.size(),1},
            {reinterpret_cast<std::uintptr_t>(f.actor),sizeof(f.actor),1},
            {reinterpret_cast<std::uintptr_t>(f.input_state.data()),f.input_state.size(),1},
            {reinterpret_cast<std::uintptr_t>(body),sizeof(body),1},
            {reinterpret_cast<std::uintptr_t>(bin),sizeof(bin),1}})game_test::memory_maps.push_back(range);
        unified_pitch_actor=reinterpret_cast<std::uintptr_t>(f.actor);unified_pitch=.3f;
        auto& bank=character_banks[0];assert(character_identity(f.actor,bank.identity));
        bank.appearance=re4::CharacterAppearance::RamonSalazar;bank.prepared=true;bank.map.count=26;
        for(unsigned i=0;i<26;++i)bank.map.bones[i].source=std::uint8_t(i);
        bank.map.source_bind[4]={0,1650,0};bank.map.bones[4].translation={0,548.309f,0};
        bank.token.store(1);bank.drawn.store(1);bank.phase.store(CharacterPhase::Free);
        control.appearance_applied=unsigned(bank.appearance);control.appearance_status=unsigned(re4::CharacterAppearanceStatus::Active);
        publish_view_settings();
        // Publishing an Off->On control setting drains the previous pitch
        // owner, as native update would. Establish this fixture's live owner
        // afterward so shoulder/follow callbacks actually take their route.
        unified_pitch_actor=reinterpret_cast<std::uintptr_t>(f.actor);
    }
    re4::ViewQfpsOutput apply(){
        CameraContext context{};context.camera=f.camera();context.actor=f.actor;
        context.view_allowed=context.basis_valid=true;context.basis=height_identity;
        auto* previous=camera_context;camera_context=&context;
        re4::ViewQfpsOutput out;CameraApply::Callback(f.camera(),&out);camera_context=previous;return out;
    }
    void active(){character_banks[0].phase.store(CharacterPhase::Active);}
};
void same_points(const re4::ViewQfpsOutput& a,const re4::ViewQfpsOutput& b,float difference){
    const re4::ViewVec3 aa[]{a.eye0,a.eye1,a.target},bb[]{b.eye0,b.eye1,b.target};
    for(unsigned i=0;i<3;++i){assert(near(aa[i].x,bb[i].x)&&near(aa[i].z,bb[i].z));
        if(std::fabs(bb[i].y-aa[i].y-difference)>=.0003f)
            std::fprintf(stderr,"camera point %u: height change %.6f, expected %.6f\n",i,double(bb[i].y-aa[i].y),double(difference));
        assert(std::fabs(bb[i].y-aa[i].y-difference)<.0003f);}
    assert(a.roll==b.roll&&a.fov==b.fov);
}
void height_moves_complete_camera_once(){
    for(unsigned route=0;route<3;++route){
        HeightFixture x;
        if(route==1){x.f.aim();x.f.camera()[0x26C]=0;
            const auto resource=main_base+0x1000;character_store(x.f.camera(),0x1B0,resource);
            const re4::ViewQfpsOutput neutral{{-470,1715,-615},{-260,1630,-125},{-200,1345,1525},0,50};
            std::memcpy(reinterpret_cast<void*>(resource+0x2C),&neutral,sizeof(neutral));}
        if(route==2)settings.first_person=true;
        const auto native=x.apply();x.active();
        const auto actor_before=std::vector<std::uint8_t>(x.f.actor,x.f.actor+sizeof(x.f.actor));
        const float command=unified_pitch,native_pitch=x.f.pitch()[0],target_pitch=x.f.pitch()[1];
        const auto shifted=x.apply();same_points(native,shifted,-548.309f); // RED: height currently ignored.
        same_points(shifted,x.apply(),0); // No accumulated offset on repeated callbacks.
        assert(!character_render_lock.test_and_set(std::memory_order_acquire));
        same_points(shifted,x.apply(),0); // A confirmed immutable height survives render-lock contention.
        character_render_lock.clear(std::memory_order_release);
        assert(!std::memcmp(actor_before.data(),x.f.actor,actor_before.size()));
        assert(unified_pitch==command&&x.f.pitch()[0]==native_pitch&&x.f.pitch()[1]==target_pitch);
        // The same pre-collision shift reaches the original wall trace once.
        NativeFollowOutput clipped{};const auto calls=camera_collision_calls;
        auto input=shifted;CameraCollision::Callback(nullptr,nullptr,&input,&clipped);
        assert(camera_collision_calls==calls+1);assert(near(clipped.eye.y,shifted.eye0.y));
        character_appearance_request_retirement(character_banks[0]);same_points(native,x.apply(),0);
    }
}
void unconfirmed_or_replaced_owner_does_not_move_camera(){
    for(unsigned guard=0;guard<7;++guard){
        HeightFixture x;const auto native=x.apply();x.active();
        if(guard==0)character_banks[0].phase.store(CharacterPhase::Probe);
        if(guard==1)character_banks[0].drawn.store(0);
        if(guard==2)character_banks[0].prepared=false;
        if(guard==3)character_renderer_ready.store(false);
        if(guard==4)character_store(x.f.actor,0x108,static_cast<void*>(x.f.other_model));
        if(guard==5)character_banks[0].map.bones[4].translation.y=std::numeric_limits<float>::quiet_NaN();
        if(guard==6)character_banks[0].appearance=re4::CharacterAppearance::Leon;
        same_points(native,x.apply(),0);
    }
    {
        HeightFixture x;const auto native=x.apply();x.active();same_points(native,x.apply(),-548.309f);
        character_store(x.f.actor,0x108,static_cast<void*>(x.f.other_model));
        same_points(native,x.apply(),0); // Identity change cannot retain the primed old height.
        character_appearance_reset();same_points(native,x.apply(),0);
    }
}
void native_controls_height_only_context(){
    HeightFixture x;settings.unified_controls=false;settings.analog_enabled=false;
    settings.first_person=false;settings.third_person_distance=1;settings.reticle=re4::ReticleStyle::Off;
    // Retained unified state is deliberately nonzero; height alone must not
    // acquire native pitch or stick ownership when unified controls are off.
    unified_pitch=.9f;x.f.look()[0]=.2f;x.f.look()[1]=.4f;
    auto* input=reinterpret_cast<std::int8_t*>(main_base+0x124EC38+8);input[0]=5;input[1]=-3;
    CameraInput::Callback(x.f.camera());const auto native=camera_collision_input;
    const float pitch=x.f.look()[0],yaw=x.f.look()[1],rendered_pitch=game_test::rendered_pitch;
    const auto calls=camera_native_calls;x.active();
    CameraInput::Callback(x.f.camera());same_points(native,camera_collision_input,-548.309f);
    assert(camera_native_calls==calls+1&&x.f.look()[0]==pitch&&x.f.look()[1]==yaw);
    assert(game_test::rendered_pitch==rendered_pitch&&input[0]==5&&input[1]==-3);
    assert(unified_pitch==.9f);
}
void actual_camera_projection_preserves_angles_and_wall_arm(){
    for(float clipping:{1.f,.4f}){
        HeightFixture x;unified_pitch=.15f;
        native_follow_probe=world_follow_probe=calculate_probe=camera_projection_probe=true;
        native_collision_fraction=clipping;character_store(x.f.image.data(),0x1131F50+0xFC,.8f);
        CameraCalculate::Callback();
        re4::ViewMatrix3x4 before;re4::ViewMatrix4x4 projection;
        re4::ViewVec3 eye,target;
        std::memcpy(&before,x.f.input_state.data()+0xD8,sizeof(before));
        std::memcpy(&projection,x.f.input_state.data()+0x10C,sizeof(projection));
        std::memcpy(&eye,x.f.input_state.data()+0x14C,sizeof(eye));
        std::memcpy(&target,x.f.input_state.data()+0x158,sizeof(target));
        const auto original_clipped=camera_collision_result;const auto calls=camera_collision_calls;
        x.active();CameraCalculate::Callback();
        re4::ViewMatrix3x4 after;re4::ViewVec3 shifted_eye,shifted_target;
        std::memcpy(&after,x.f.input_state.data()+0xD8,sizeof(after));
        std::memcpy(&shifted_eye,x.f.input_state.data()+0x14C,sizeof(shifted_eye));
        std::memcpy(&shifted_target,x.f.input_state.data()+0x158,sizeof(shifted_target));
        assert(camera_collision_calls==calls+1);
        if(std::fabs(shifted_eye.y-eye.y+548.309f)>=.0003f)
            std::fprintf(stderr,"full camera clipping %.2f eyeY %.6f -> %.6f, profile %.6f, clipped %.6f\n",double(clipping),double(eye.y),double(shifted_eye.y),double(camera_collision_input.eye0.y),double(camera_collision_result.eye.y));
        assert(std::fabs(shifted_eye.y-eye.y+548.309f)<.0003f);
        assert(std::fabs(shifted_target.y-target.y+548.309f)<.0003f);
        assert(near(camera_collision_result.eye.x,original_clipped.eye.x)&&near(camera_collision_result.eye.z,original_clipped.eye.z));
        for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)assert(near(before.m[r][c],after.m[r][c]));
        // Native FDB0 look-at/frustum pipeline projects the donor's lowered
        // head at the same location as Leon; there is no extra final-eye move.
        re4::ViewHudPoint leon,donor;
        assert(re4::project_view_point(before,projection,{100,1850,300},leon));
        assert(re4::project_view_point(after,projection,{100,1301.691f,300},donor));
        assert(std::fabs(leon.x-donor.x)<.0003f&&std::fabs(leon.y-donor.y)<.0003f);
        calculate_probe=native_follow_probe=world_follow_probe=camera_projection_probe=false;
    }
}
}
int main(){native_controls_height_only_context();height_moves_complete_camera_once();unconfirmed_or_replaced_owner_does_not_move_camera();actual_camera_projection_preserves_angles_and_wall_arm();
    std::puts("character height actual CameraApply: active owner, once-only rig translation, native controls/collision PASS");}
