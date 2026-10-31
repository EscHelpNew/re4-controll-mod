// Exercise the native initializer boundary with the existing actual-module fixture.
#define main game_module_baseline_main
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#include "game_module_tests.cpp"
#pragma GCC diagnostic pop
#undef main
#include <limits>

namespace {
void raise_callback(void* actor){
    AimRaise::Callback(actor);
}
void prepare_raise(Fixture& f,float pitch){
    f.view_mode();settings.unified_controls=true;settings.analog_enabled=false;
    settings.gyro_enabled=false;sensor_attributes=1;
    *reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;
    *reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
    ModernPlayerUpdate::Callback(f.actor); // Establish the walking pitch owner.
    unified_pitch=pitch;f.aim();f.actor[0x116]=f.actor[0x117]=0;
    game_test::native_aim=0; // Broad native predicate is false at initial0/0.
    aim_raise_native_calls=aim_raise_native_change=0;
}
float& baseline(Fixture& f){return *reinterpret_cast<float*>(f.weapon_context+0x28);}
}
int main(){
    for(float pitch:{.4f,-.35f,1.570796327f,-1.570796327f}){
        Fixture f;prepare_raise(f,pitch);
        UnifiedUpdate update{f.actor};unified_update=&update;
        raise_callback(f.actor);unified_update=nullptr;
        assert(aim_raise_native_calls==1);
        assert(near(baseline(f),pitch));
        assert(near(f.pitch()[0],pitch*.636619746685f)&&near(f.pitch()[1],pitch*.636619746685f));
        assert(f.pitch()[2]==0); // Preserve the native initializer's factor.
        assert(!unified_aim_session.ready&&!update.consumed&&gyro_input_actor==nullptr);
        assert(near(unified_pitch,pitch)&&f.actor[0x116]==0&&f.actor[0x117]==0);
        const float before=f.pitch()[1];
        f.actor[0x116]=1;f.actor[0x117]=1;game_test::native_aim=1;modern_probe=true;
        game_test::tick+=480000;ModernPlayerUpdate::Callback(f.actor);modern_probe=false;
        assert(unified_aim_session.ready&&near(f.pitch()[1],before)); // No late ready-edge jump.
        assert(near(unified_pitch,pitch));
    }
    for(unsigned gate=0;gate<26;++gate){
        Fixture f;prepare_raise(f,.4f);
        UnifiedUpdate update{f.actor};unified_update=&update;
        alignas(16) std::uint8_t other[0xA00];std::memcpy(other,f.actor,sizeof(other));
        void* actor=f.actor;
        if(gate==0)settings.unified_controls=false;
        if(gate==1)f.input_state[0x5092]|=4;
        if(gate==2)*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=1;
        if(gate==3)sensor_attributes=0;
        if(gate==4)*reinterpret_cast<void**>(f.actor+0x938)=nullptr;
        if(gate==5)unified_pitch_actor=0;
        if(gate==6)unified_pitch=std::numeric_limits<float>::quiet_NaN();
        if(gate==7)unified_pitch=2;
        if(gate==8)f.actor[0x114]=1;
        if(gate==9)f.actor[0x115]=11;
        if(gate==10)f.actor[0x116]=1;
        if(gate==11)f.actor[0x117]=1;
        if(gate==12)f.flags()=0;
        if(gate==13)*reinterpret_cast<std::uintptr_t*>(f.image.data()+0x1265A28)=main_base+0x3212E0;
        if(gate==14)*reinterpret_cast<int*>(f.image.data()+0x11C7D68)=1;
        if(gate==15)*reinterpret_cast<void**>(f.image.data()+0xB1AE10)=nullptr;
        if(gate==16)f.input_type[3]|=0x20;
        if(gate==17)unified_update=nullptr;
        if(gate==18)update.actor=other;
        if(gate==19)actor=other;
        if(gate==20)aim_raise_native_change=1;
        if(gate==21)aim_raise_native_change=2;
        if(gate==22)aim_raise_native_change=3;
        if(gate==23)f.input_state[0x86FF]=1;
        if(gate==24)*reinterpret_cast<float*>(f.input_state.data()+0xA4)=0;
        if(gate==25)actor=nullptr;
        raise_callback(actor);unified_update=nullptr;
        assert(aim_raise_native_calls==1);
        assert(f.pitch()[0]==0&&f.pitch()[1]==0&&f.pitch()[2]==0);
        if(gate!=4)assert(baseline(f)==0);
        assert(!unified_aim_session.ready&&!update.consumed);
    }
    std::puts("native firearm raise: initial pitch continuity, readiness and scoped guards PASS");
}
