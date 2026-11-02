// Scope15460 owns pitch118, zoom13C and FOVC8. Adapt controls before Orig
// constructs its camera vectors, retaining native sway, HUD and projection.
struct ScopedSession {
    void* actor=nullptr;void* weapon=nullptr;std::uintptr_t handler=0;
    unsigned item=0;bool ready=false,tuned=false;
    std::array<float,8> tuning{};
} scoped_session;
void reset_scoped_controls(){scoped_look.reset();scoped_session={};}
bool scoped_controls_enabled(){
    return settings.analog_enabled||settings.unified_controls||settings.move_while_aiming||settings.gyro_enabled;
}
class ScopedInputGuard {
    std::int32_t* axes_=reinterpret_cast<std::int32_t*>(main_base+0x124E9C8);
    std::int8_t* zoom_=reinterpret_cast<std::int8_t*>(main_base+0x124E9D1);
    std::uint32_t* directions_=reinterpret_cast<std::uint32_t*>(main_base+0x124E9DC);
    std::int32_t x_=axes_[0],y_=axes_[1];std::int8_t z_=*zoom_;
    std::uint32_t flags_=*directions_&0x6Fu;
public:
    ScopedInputGuard(){axes_[0]=axes_[1]=0;*zoom_=0;*directions_&=~0x6Fu;}
    ~ScopedInputGuard(){axes_[0]=x_;axes_[1]=y_;*zoom_=z_;*directions_=(*directions_&~0x6Fu)|flags_;}
    ScopedInputGuard(const ScopedInputGuard&)=delete;
    ScopedInputGuard& operator=(const ScopedInputGuard&)=delete;
};
struct ScopedZoomTarget {void* scope;float zoom;};
ScopedZoomTarget* scoped_zoom_target=nullptr;
HOOK_DEFINE_INLINE(ScopedZoom) {
    static void Callback(exl::hook::InlineFloatCtx* context){
        const auto* target=scoped_zoom_target;
        if(!target||re4::saved_x(context,19)!=reinterpret_cast<std::uintptr_t>(target->scope))return;
        // Native15490 has already retained the old zoom in s9. Commit at the
        // clamp boundary so native FOV, zoom label and feedback see the change.
        re4::set_saved_s0(context,target->zoom);
        *reinterpret_cast<float*>(static_cast<std::uint8_t*>(target->scope)+0x13C)=target->zoom;
    }
};
HOOK_DEFINE_TRAMPOLINE(ScopeUpdate) {
    static void Callback(void* scope){
        auto* actor=*reinterpret_cast<std::uint8_t**>(main_base+re4::profile::player_slot);
        auto* state=*reinterpret_cast<std::uint8_t**>(main_base+0xB1AE08);
        const auto* controller=reinterpret_cast<const std::uint8_t*>(main_base+0x1131718);
        const bool scene=scoped_controls_enabled()&&scoped_camera_owner(actor)&&scope==controller+0x530
            &&actor[0x114]==0&&(controller[0x48]&1)&&!state[0x503C]
            &&!(state[0x5092]&4)&&!(state[0x509E]&0x80)&&!(state[0x1AB]&0x40)&&!state[0x86FF]
            &&(sensor_attributes&1)&&!*reinterpret_cast<const std::uint8_t*>(main_base+0x11C7D55)
            &&*reinterpret_cast<const int*>(main_base+0xAFF39C)==2&&firearm_aim(actor);
        if(!scene){reset_scoped_controls();Orig(scope);return;}
        const auto factor=*reinterpret_cast<const float*>(state+0xA4);
        auto* b=static_cast<std::uint8_t*>(scope);
        auto& pitch=*reinterpret_cast<float*>(b+0x118);
        auto& zoom=*reinterpret_cast<float*>(b+0x13C);
        auto& yaw=*reinterpret_cast<float*>(actor+0xB4);
        const float lower=*reinterpret_cast<const float*>(b+0x124),upper=*reinterpret_cast<const float*>(b+0x130);
        if(!std::isfinite(factor)||factor<=0||factor>2||!std::isfinite(pitch)||!std::isfinite(yaw)
           ||!std::isfinite(zoom)||zoom<0||zoom>1||!std::isfinite(lower)||!std::isfinite(upper)
           ||lower>=upper||lower<-1.570797f||upper>1.570797f){reset_scoped_controls();Orig(scope);return;}
        const auto handler=*reinterpret_cast<const std::uintptr_t*>(main_base+0x1265A28);
        auto* weapon=player_weapon(actor);
        if(scoped_session.actor!=actor||scoped_session.weapon!=weapon||scoped_session.handler!=handler||scoped_session.item!=state[0x5034]){
            reset_scoped_controls();scoped_session.actor=actor;scoped_session.weapon=weapon;
            scoped_session.handler=handler;scoped_session.item=state[0x5034];
        }
        if(actor[0x116]!=1&&actor[0x116]!=2)scoped_session.ready=false;
        else if(actor[0x116]==1&&actor[0x117]==1)scoped_session.ready=true;
        const auto* type=*reinterpret_cast<const int**>(main_base+0xB1AE10);
        const bool inverted=type&&*type<0;
        const std::array<float,8> tuning={settings.right_aim_gain_x,settings.right_aim_gain_y,
            settings.right_deadzone,settings.left_deadzone,float(settings.stick_curve),
            float(settings.re6_response),float(inverted),float(settings.gyro_enabled)};
        if(!scoped_session.tuned||scoped_session.tuning!=tuning){scoped_look.reset();scoped_session.tuning=tuning;scoped_session.tuned=true;}
        const float x=re4::shape_axis(control.raw_rx,settings.right_deadzone,settings.right_aim_gain_x,settings.stick_curve);
        const float y=re4::shape_axis(control.raw_ry,settings.right_deadzone,settings.right_aim_gain_y,settings.stick_curve)*(inverted?-1.f:1.f);
        // The user retains native stationary scoped play: leftY controls zoom,
        // rightXY and gyro aim. No player translation is added in scope mode16.
        const float z=re4::shape_axis(control.raw_ly,settings.left_deadzone,1,settings.stick_curve);
        const auto d=scoped_look.update(reinterpret_cast<std::uintptr_t>(actor),svcGetSystemTick(),scoped_session.ready,x,y,z,zoom,settings.re6_response);
        const float scale=re4::scope_slowdown(zoom);
        yaw=re4::gyro_yaw(yaw,d.yaw);pitch=re4::scope_pitch(pitch,d.pitch,lower,upper);
        const float target_zoom=re4::scope_zoom(zoom,d.zoom);
        if(settings.gyro_enabled&&scoped_session.ready&&gyro_aim_ready(actor)){
            const auto motion=gyro_pending;gyro_pending={};
            yaw=re4::gyro_yaw(yaw,motion.yaw*scale);
            if(std::isfinite(motion.pitch))pitch=re4::scope_pitch(pitch,motion.pitch*scale,lower,upper);
            control.gyro_yaw=motion.yaw;control.gyro_pitch=motion.pitch;
        }else gyro_pending={};
        ScopedInputGuard input;
        ScopedZoomTarget target{scope,target_zoom};
        auto* previous=scoped_zoom_target;scoped_zoom_target=&target;
        Orig(scope);
        scoped_zoom_target=previous;
    }
};
