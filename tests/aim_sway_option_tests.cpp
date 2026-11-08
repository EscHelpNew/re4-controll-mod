#define main game_module_baseline_main
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#include "game_module_tests.cpp"
#pragma GCC diagnostic pop
#undef main
int main(){
    for(bool enabled:{false,true}){
        Fixture f;f.view_mode();f.aim();settings.disable_aim_sway=enabled;
        *reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;
        float pitch=.4f,offset=.7f;game_test::sway_baseline=0;
        AimSway::Callback(f.actor,0,&pitch,&offset);
        assert(near(pitch,enabled?.4f:.02f)&&offset==.7f);
        if(enabled)assert(near(game_test::sway_baseline,.4f));
        // The gyro manual-input route remains active independently of the option.
        gyro_input_actor=f.actor;pitch=.5f;AimSway::Callback(f.actor,0,&pitch,&offset);
        assert(near(pitch,.5f)&&near(game_test::sway_baseline,.5f));gyro_input_actor=nullptr;
    }
    for(unsigned gate=0;gate<6;++gate){
        Fixture f;f.view_mode();f.aim();settings.disable_aim_sway=true;
        *reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;
        if(gate==0)f.input_state[0x86FF]=1;
        if(gate==1)f.actor[0x115]=1;
        if(gate==2)f.actor[0x114]=1;
        if(gate==3)game_test::native_aim=0;
        if(gate==4)f.flags()=0;
        if(gate==5)f.input_state[0x5092]=4;
        float pitch=.4f,offset=.7f;game_test::sway_baseline=0;
        AimSway::Callback(f.actor,0,&pitch,&offset);assert(near(pitch,.02f));
    }
    std::puts("aiming sway option: native manual baseline, defaultOff, gyro and unsafe-state exclusions PASS");
}
