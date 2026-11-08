// Exercise real input adapters while reproducing only the independently traced
// native automatic weapon state transitions (3272C0,328090,328190,3285B0).
#define main game_module_baseline_main
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#include "game_module_tests.cpp"
#pragma GCC diagnostic pop
#undef main

namespace {
unsigned burst_frame=0;
void native_tmp_update(void* actor){
    auto* b=static_cast<std::uint8_t*>(actor);
    assert(b[0x115]==6&&(b[0x116]==1||b[0x116]==2));
    if(b[0x116]==1){
        assert(b[0x117]==0);
        b[0x117]=1; // 328124: native installs steady aim inside this update.
        AimSensitivity::Callback(actor,0); // 3272EC, before held-fire selection.
        b[0x116]=2;b[0x117]=0; // 327404..40C: held fire enters the shot.
    }else{
        AimSensitivity::Callback(actor,0); // 3274E4, before the shot117 dispatch.
        if(b[0x117]==0)b[0x117]=1; // 328590: shot initializer.
        else if((++burst_frame%3)==0)b[0x117]=0; // 328644..64C: held-fire next round.
    }
}
void prepare_tmp(Fixture& f,bool stick,bool motion){
    f.view_mode();f.aim();sensor_attributes=1;sensor_pad=0;sensor_style=1;
    settings.unified_controls=true;settings.analog_enabled=false;settings.gyro_enabled=motion;
    settings.gyro_deadzone_dps=0;settings.gyro_gain_x=settings.gyro_gain_y=1;
    *reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;
    *reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
    *reinterpret_cast<std::uintptr_t*>(f.image.data()+0x1265A28)=main_base+0x3270F0;
    f.input_state[0x5034]=11;f.actor[0x116]=1;f.actor[0x117]=0;
    control.raw_rx=stick?1.f:0.f;control.raw_ry=stick?1.f:0.f;
    burst_frame=0;
}
void burst(Fixture& f,unsigned& number,unsigned count){
    for(unsigned i=0;i<count;++i){
        // Neither sampling nor pre-update sees1/1: setup and held-fire
        // selection happen within native_tmp_update in one player update.
        assert(!(f.actor[0x116]==1&&f.actor[0x117]==1));
        game_test::tick+=640000;f.sample(++number);poll_sensors();
        unified_player_update(f.actor,native_tmp_update);
    }
}
}
int main(){
    for(bool motion:{false,true}){
        Fixture f;prepare_tmp(f,!motion,motion);unsigned number=0;
        burst(f,number,18);
        assert(motion?f.yaw()>.02f:f.yaw()<-.02f);
        assert(unified_pitch>.02f); // Both physical input axes reach native aim.
        assert(unified_aim_session.ready&&(!motion||gyro_session.ready));

        if(motion){
            // The real sensor path drops its session after a >100ms gap.
            // A firing TMP must recover with a fresh FIFO drain, without
            // replaying the motion accumulated while paused.
            game_test::tick+=2'000'000;f.sample(++number);poll_sensors();
            assert(gyro_session.ready&&!control.gyro_active);
            assert(gyro_pending.yaw==0&&gyro_pending.pitch==0);
            const float pause_before=f.yaw();burst(f,number,18);
            assert(f.yaw()>pause_before+.02f);
        }

        // A scene interruption clears both latches while native TMP remains
        // in its burst. Recovery cannot depend on seeing a new steady1/1.
        f.input_state[0x5092]|=4;
        assert(!unified_aim_allowed(f.actor,true));assert(!gyro_aim_ready(f.actor,true));
        f.input_state[0x5092]&=~4;
        f.actor[0x116]=2;f.actor[0x117]=1;
        const float before=f.yaw(),pitch_before=unified_pitch;
        burst(f,number,18);
        assert(motion?f.yaw()>before+.02f:f.yaw()<before-.02f);
        assert(unified_pitch>pitch_before+.02f);

        // A release/re-aim can again start shooting in the same update that
        // installs steady aim; the previous latch is deliberately gone.
        f.flags()=0;assert(!unified_aim_allowed(f.actor,true));assert(!gyro_aim_ready(f.actor,true));
        f.flags()=0x10;f.actor[0x116]=1;f.actor[0x117]=0;
        const float reaim_before=f.yaw();burst(f,number,18);
        assert(motion?f.yaw()>reaim_before+.02f:f.yaw()<reaim_before-.02f);
    }
    for(unsigned gate=0;gate<15;++gate){
        Fixture f;prepare_tmp(f,true,true);f.actor[0x116]=2;f.actor[0x117]=0;
        if(gate==0)f.actor[0x114]=1; // Damage/QTE/busy mode.
        if(gate==1)f.actor[0x115]=7; // Knockback routine.
        if(gate==2)f.flags()=0;
        if(gate==3)f.input_state[0x5092]|=4;
        if(gate==4)f.input_state[0x509E]|=0x80;
        if(gate==5)f.input_state[0x1AB]|=0x40;
        if(gate==6)f.input_state[0x86FF]=1;
        if(gate==7){f.actor[0x116]=0;f.actor[0x117]=1;} // Still raising.
        if(gate==8){f.actor[0x116]=1;f.actor[0x117]=4;} // Initial raise tail.
        if(gate==9){f.actor[0x116]=3;f.actor[0x117]=0;} // Lowering.
        if(gate==10){f.actor[0x116]=4;f.actor[0x117]=0;} // Reload.
        if(gate==11){f.actor[0x116]=6;f.actor[0x117]=0;}
        if(gate==12)*reinterpret_cast<std::uintptr_t*>(f.image.data()+0x1265A28)=main_base+0x31CC60;
        if(gate==13)*reinterpret_cast<std::uintptr_t*>(f.image.data()+0x1265A28)=main_base+0x332CC0;
        if(gate==14)f.actor[0x117]=4; // No verified ordinary TMP shot subhandler.
        assert(!unified_aim_allowed(f.actor,true));assert(!gyro_aim_ready(f.actor,true));
    }
    for(unsigned subphase:{0u,1u}){
        Fixture f;prepare_tmp(f,true,true);f.actor[0x116]=2;f.actor[0x117]=subphase;
        assert(!unified_aim_allowed(f.actor));assert(!gyro_aim_ready(f.actor)); // No callback-only arming.
        assert(unified_aim_allowed(f.actor,true));assert(gyro_aim_ready(f.actor,true));
        assert(gyro_session.drain); // Re-entry still discards queued sensor data.
    }
    std::puts("TMP native bursts: stick/gyro readiness, interruption recovery and safety gates PASS");
}
