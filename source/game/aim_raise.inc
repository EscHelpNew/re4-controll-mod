// Generic firearm raising initializes its secondary aim motion from these
// fields before controls become ready. Seed the retained look at that boundary
// so the original animation raises toward its eventual aim pitch. This boundary
// is verified only for the generic handler's ordinary native mode0 controller.
HOOK_DEFINE_TRAMPOLINE(AimRaise) {
    static void Callback(void* actor){
        Orig(actor);
        if(!actor||!unified_update||unified_update->actor!=actor||!unified_scene(actor)
            ||unified_pitch_actor!=reinterpret_cast<std::uintptr_t>(actor)
            ||!std::isfinite(unified_pitch)||std::fabs(unified_pitch)>1.570797f)return;
        const auto* b=static_cast<const std::uint8_t*>(actor);
        if(b[0x115]!=6||b[0x116]!=0||b[0x117]!=0
            ||!(*reinterpret_cast<const std::uint64_t*>(main_base+re4::profile::input_flags)&0x10)
            ||*reinterpret_cast<const std::uintptr_t*>(main_base+0x1265A28)!=main_base+0x31CC60
            ||*reinterpret_cast<const int*>(main_base+0x11C7D68)!=0)return;
        const auto* type=*reinterpret_cast<const std::uint8_t**>(main_base+0xB1AE10);
        auto* context=*reinterpret_cast<std::uint8_t* const*>(b+0x938);
        if(!type||(type[3]&0x20)||!context)return;
        // Context28 is radians; the native current/target pair is normalized.
        // Retain Orig's interpolation factor and all raise/readiness state.
        *reinterpret_cast<float*>(context+0x28)=unified_pitch;
        auto* pitch=reinterpret_cast<float*>(main_base+0x1265A08);
        pitch[0]=pitch[1]=std::clamp(re4::unified_pitch_weight(unified_pitch),-1.f,1.f);
    }
};
