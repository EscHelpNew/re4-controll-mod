// Included inside module.cpp's anonymous namespace immediately before
// AimSensitivity. Include re4/modern_move.hpp at the translation unit's top.
// Native addresses and model topology independently verified for this Build ID.
struct ModernMoveNative {
    re4::ModernMoveEngine move{};
    void (*evaluate)(void*,void*)=nullptr;
    void (*rotation)(void*,const void*)=nullptr;
    void (*position)(void*,const void*)=nullptr;
    void (*scale)(void*,const void*)=nullptr;
    void (*copy_matrix)(const void*,void*)=nullptr;
    int (*healthy)()=nullptr;
};
ModernMoveNative modern_native;
re4::ModernMoveCycle modern_cycle;
re4::ModernMovePose modern_pose;
re4::ModernMovePoseStorage<1> modern_body_pose;
re4::ModernMoveIdentity modern_body_owner{};
float modern_body_yaw=0;
bool modern_body_applied=false;
re4::ModernMoveIdentity modern_bound_owner{};
std::uintptr_t modern_bound_asset=0,modern_bound_clip=0;
void* modern_update_actor=nullptr;
std::uint64_t modern_epoch=0;
bool modern_root_done=false,modern_stepped=false,modern_pose_done=false;
float modern_lower_yaw=0;
std::array<re4::ModernMoveBone,9> modern_bones{};
struct ModernAimSession {
    std::uintptr_t actor=0,handler=0,weapon=0;
    unsigned item=0;
    bool ready=false;
} modern_aim_session;

bool modern_move_region(std::uintptr_t p,std::size_t size,bool write,re4::ModernMoveMemory& memory){
    if(!p||size>std::numeric_limits<std::uintptr_t>::max()-p)return false;
    MemoryInfo info{};std::uint32_t page=0;
    if(R_FAILED(svcQueryMemory(&info,&page,p))||!(info.perm&1)||(write&&!(info.perm&2)))return false;
    memory=re4::ModernMoveMemory::span(reinterpret_cast<const void*>(info.addr),info.size);
    return memory.contains(p,size);
}
template<class T> bool modern_move_read(std::uintptr_t p,T& value){
    re4::ModernMoveMemory m;return modern_move_region(p,sizeof(T),false,m)&&m.value(p,value);
}
std::uintptr_t modern_move_slot(std::size_t offset){return *reinterpret_cast<const std::uintptr_t*>(main_base+offset);}

bool modern_move_collect(void* actor,re4::ModernMoveIdentity& identity,std::array<re4::ModernMoveBone,9>& bones,re4::ModernMoveBone* root=nullptr){
    const auto address=reinterpret_cast<std::uintptr_t>(actor);
    re4::ModernMoveMemory am;
    if(actor!=reinterpret_cast<void*>(modern_move_slot(re4::profile::player_slot))
       ||!modern_move_region(address,0x950,true,am))return false;
    const auto head=*reinterpret_cast<const std::uintptr_t*>(address+0x108);
    const auto table=*reinterpret_cast<const std::uintptr_t*>(address+0x548);
    const auto model=*reinterpret_cast<const std::uintptr_t*>(address+0x428);
    const auto count=*reinterpret_cast<const std::uint8_t*>(address+0x11A);
    if(!head||!table||!model||count<26||count>=255)return false;
    identity={address,head,model,table};
    std::array<std::uintptr_t,26> pointers{},parents{};
    std::array<std::int16_t,26> parent_ids{};
    auto bone=head;
    for(unsigned i=0;i<26;++i){
        re4::ModernMoveMemory m;if(!modern_move_region(bone,0x1DC,true,m))return false;
        pointers[i]=bone;parents[i]=*reinterpret_cast<const std::uintptr_t*>(bone+0x78);
        if(i==0&&root)*root={reinterpret_cast<float*>(bone+0xA4),reinterpret_cast<float*>(bone+0x48),reinterpret_cast<std::uint32_t*>(bone+0x1D8),reinterpret_cast<std::uint16_t*>(bone+0x1B4)};
        if(i>=17)bones[i-17]={reinterpret_cast<float*>(bone+0xA4),reinterpret_cast<float*>(bone+0x48),reinterpret_cast<std::uint32_t*>(bone+0x1D8),reinterpret_cast<std::uint16_t*>(bone+0x1B4)};
        bone=*reinterpret_cast<const std::uintptr_t*>(bone+0x108);
    }
    for(unsigned i=0;i<26;++i){
        parent_ids[i]=-2;
        if(!i&&parents[i]==address)parent_ids[i]=-1;
        for(unsigned j=0;j<i;++j)if(parents[i]==pointers[j])parent_ids[i]=std::int16_t(j);
    }
    return re4::modern_move_leon_topology(parent_ids,count);
}

bool modern_move_frame(void* actor,re4::ModernMoveFrame& frame){
    frame.aim_enabled=settings.move_while_aiming;frame.knife_enabled=settings.move_while_knife;
    frame.reload_enabled=settings.move_while_reloading;
    if(!frame.aim_enabled&&!frame.knife_enabled&&!frame.reload_enabled){modern_aim_session={};return false;}
    const auto a=reinterpret_cast<std::uintptr_t>(actor);re4::ModernMoveMemory am;
    if(!actor||actor!=reinterpret_cast<void*>(modern_move_slot(re4::profile::player_slot))
       ||!modern_move_region(a,0x950,false,am)){modern_aim_session={};return false;}
    const auto state=modern_move_slot(0xB1AE08);re4::ModernMoveMemory sm;
    if(!modern_move_region(state,0x8700,false,sm)){modern_aim_session={};return false;}
    const auto* bytes=reinterpret_cast<const std::uint8_t*>(a);
    frame.active_player=true;frame.character=*reinterpret_cast<const std::uint8_t*>(state+0x503C);
    frame.mode=bytes[0x114];frame.routine=bytes[0x115];frame.phase=bytes[0x116];frame.subphase=bytes[0x117];
    frame.held=*reinterpret_cast<const std::uint64_t*>(main_base+re4::profile::input_flags);
    frame.native_aim=aiming_predicate&&aiming_predicate(actor)!=0;
    const auto handler=modern_move_slot(0x1265A28);
    frame.generic_firearm=handler==main_base+0x31CC60;
    frame.weapon=re4::modern_weapon(handler-main_base);
    frame.reload_type1=*reinterpret_cast<const int*>(main_base+0x11C7D68)==1;
    frame.frame_factor=*reinterpret_cast<const float*>(state+0xA4);
    frame.frozen=*reinterpret_cast<const std::uint8_t*>(state+0x86FF)!=0;
    frame.scripted=(*reinterpret_cast<const std::uint8_t*>(state+0x5092)&4)!=0
        ||*reinterpret_cast<const std::uint8_t*>(main_base+0x1131718+0x54)!=10;
    frame.input_live=(sensor_attributes&1)&&!*reinterpret_cast<const std::uint8_t*>(main_base+0x11C7D55)
        &&*reinterpret_cast<const int*>(main_base+0xAFF39C)==2;
    std::uintptr_t context=0,weapon=0;
    if(modern_move_read(a+0x938,context)&&context)modern_move_read(context+0x38,weapon);
    const auto item=*reinterpret_cast<const std::uint8_t*>(state+0x5034);
    if(modern_aim_session.actor!=a||modern_aim_session.handler!=handler
       ||modern_aim_session.weapon!=weapon||modern_aim_session.item!=item)
        modern_aim_session={a,handler,weapon,item,false};
    if(!re4::modern_move_context_allowed(frame)||frame.routine!=6||!(frame.held&0x10)
       ||!frame.native_aim||frame.weapon==re4::ModernWeapon::Unsupported||(frame.phase!=1&&frame.phase!=2))modern_aim_session.ready=false;
    else if((frame.phase==1&&frame.subphase==1)||frame.phase==2)modern_aim_session.ready=true;
    frame.aim_ready=modern_aim_session.ready;
    const auto collision=*reinterpret_cast<const std::uint16_t*>(a+0x352);
    if((collision&0x300)!=0x300){modern_aim_session={};return false;}
    return re4::modern_move_allowed(frame);
}
// Routing context, deliberately independent of left-stick magnitude and asset
// readiness. Neutral left input must continue to aim with the right stick.
bool modern_move_aim_active(void* actor){
    re4::ModernMoveFrame frame;return modern_move_frame(actor,frame)&&!re4::modern_reload_allowed(frame);
}
bool modern_move_reload_active(void* actor){
    re4::ModernMoveFrame frame;return modern_move_frame(actor,frame)&&re4::modern_reload_allowed(frame);
}

bool modern_move_bind(void* actor,bool backwards,const re4::ModernMoveIdentity& owner){
    const auto a=reinterpret_cast<std::uintptr_t>(actor);
    std::uintptr_t table=0;if(!modern_move_read(a+0x548,table))return false;
    const bool healthy=modern_native.healthy&&modern_native.healthy()!=0;
    const std::size_t offset=backwards?(healthy?0x40:0x318):(healthy?0x10:0x308);
    std::uintptr_t asset=0,clip=0;
    if(!modern_move_read(table+offset,asset)||!asset||!modern_move_read(table+offset+8,clip))return false;
    if(modern_cycle.valid()&&owner==modern_bound_owner&&asset==modern_bound_asset&&clip==modern_bound_clip)return true;
    modern_cycle.reset();modern_bound_asset=modern_bound_clip=0;
    re4::ModernMoveMemory memory;if(!modern_move_region(asset,3,false,memory))return false;
    re4::ModernMoveMotion motion;
    if(!motion.parse(memory,asset,clip)||!motion.prime(modern_native.move.sample)||!modern_cycle.bind(motion))return false;
    modern_bound_owner=owner;modern_bound_asset=asset;modern_bound_clip=clip;return true;
}
bool modern_move_prepare(void* actor){
    re4::ModernMoveFrame frame;if(!modern_move_frame(actor,frame)){control.modern_move_status=1;return false;}
    const auto left=re4::modern_move_disk(
        re4::shape_axis(control.raw_lx,settings.left_deadzone,1,settings.stick_curve),
        re4::shape_axis(control.raw_ly,settings.left_deadzone,1,settings.stick_curve));
    if(!(std::hypot(left.x,left.z)>0)){control.modern_move_status=1;return false;}
    float camera_yaw;
    if(settings.unified_controls){
        // Unified view heading is owned by the player, including at gyro poles
        // where the view's horizontal projection vanishes or changes sign.
        if(!modern_move_read(reinterpret_cast<std::uintptr_t>(actor)+0xB4,camera_yaw)
            ||!std::isfinite(camera_yaw))return false;
    }else{
        const auto state=modern_move_slot(0xB1AE08);re4::ModernMoveVec3 eye,target;
        if(!modern_move_read(state+0x14C,eye)||!modern_move_read(state+0x158,target)
           ||!re4::modern_move_finite(eye)||!re4::modern_move_finite(target))return false;
        const auto dx=target.x-eye.x,dz=target.z-eye.z;
        if(!(std::hypot(dx,dz)>0.001f))return false;
        camera_yaw=std::atan2(dx,dz);
    }
    re4::ModernMoveVec3 direction;const re4::ModernMoveBasis unit{};
    if(!re4::modern_move_world_step({0,0,1},unit,left.x,left.z,camera_yaw,direction))return false;
    re4::ModernMoveIdentity owner;
    if(!modern_move_collect(actor,owner,modern_bones)){control.modern_move_status=5;return false;}
    bool backwards=false;
    modern_lower_yaw=re4::modern_move_lower_yaw(direction,*reinterpret_cast<const float*>(static_cast<const std::uint8_t*>(actor)+0xB4),backwards);
    if(!modern_move_bind(actor,backwards,owner)){control.modern_move_status=2;return false;}
    re4::ModernMoveBasis basis;
    const auto* matrix=reinterpret_cast<const float*>(static_cast<const std::uint8_t*>(actor)+0x18);
    for(unsigned row=0;row<3;++row)for(unsigned col=0;col<3;++col)basis.m[row][col]=matrix[4*row+col];
    re4::ModernMoveVec3 delta;
    if(!modern_cycle.step(modern_native.move,actor,modern_epoch,left.x,left.z,camera_yaw,basis,1,delta))return false;
    control.modern_move_steps=control.modern_move_steps+1;control.modern_move_distance=std::hypot(delta.x,delta.z);control.modern_move_status=3;return true;
}

HOOK_DEFINE_TRAMPOLINE(AutomaticFire) {
    static void Callback(void* actor){
        // The first automatic round computes its ray before native root. Move
        // at this verified action boundary so the ray uses this frame's body
        // position; the later root still runs its original pose/physics once.
        if(actor&&actor==modern_update_actor&&!modern_root_done
           &&modern_move_slot(0x1265A28)==main_base+0x3270F0){
            const auto* b=static_cast<const std::uint8_t*>(actor);
            re4::ModernMoveFrame frame;
            if(b[0x116]==2&&b[0x117]==0&&!*reinterpret_cast<const std::uint32_t*>(b+0x520)
               &&modern_move_frame(actor,frame)){
                modern_root_done=true;modern_stepped=modern_move_prepare(actor);
            }
        }
        Orig(actor);
    }
};
HOOK_DEFINE_TRAMPOLINE(ModernPlayerUpdate) {
    static void Callback(void* actor){
        if(!actor||actor!=reinterpret_cast<void*>(modern_move_slot(re4::profile::player_slot))){Orig(actor);return;}
        dpad_weapons_before_update(actor);
        // Restoration runs before native code even when a mode was just turned
        // off, changed state, or received zero input.
        re4::ModernMoveIdentity owner;std::array<re4::ModernMoveBone,9> bones;
        if(modern_body_pose.pending()){
            re4::ModernMoveBone root;
            if(modern_move_collect(actor,owner,bones,&root))modern_body_pose.restore(owner,{root});
            else modern_body_pose.discard();
        }
        if(modern_pose.pending()){
            if(modern_move_collect(actor,owner,bones))modern_pose.restore(owner,bones);
            else modern_pose.discard();
        }
        const auto previous=modern_update_actor;modern_update_actor=actor;
        ++modern_epoch;modern_root_done=modern_stepped=modern_pose_done=false;
        control.modern_move_status=(settings.move_while_aiming||settings.move_while_knife||settings.move_while_reloading)?1:0;
        control.modern_move_distance=0;
        modern_body_applied=false;
        unified_player_update(actor,+[](void* value){Orig(value);});modern_update_actor=previous;
        grenade_spot_update(actor);
        dpad_weapons_update(actor);
        weapon_popup_validate_scene(actor);
        character_appearance_update(actor);
        if(!modern_body_applied){modern_body_yaw=0;modern_body_owner={};}
    }
};
HOOK_DEFINE_TRAMPOLINE(ModernRoot) {
    static std::uint32_t Callback(void* actor,void* flag){
        character_menu_rotate(actor);
        if(actor&&actor==modern_update_actor&&!modern_root_done){
            modern_root_done=true;
            unified_reload_root(actor);
            if(settings.move_while_aiming||settings.move_while_knife||settings.move_while_reloading)modern_stepped=modern_move_prepare(actor);
        }
        const auto previous_root=unified_root_actor;const auto previous_yaw=unified_root_yaw;
        float rotation=0;
        unified_root_actor=unified_walk_root(actor,rotation)?actor:nullptr;unified_root_yaw=rotation;
        const auto result=Orig(actor,flag);
        unified_root_actor=previous_root;unified_root_yaw=previous_yaw;
        return result;
    }
};
HOOK_DEFINE_TRAMPOLINE(ModernPose) {
    static void Callback(void* actor,int flag){
        const bool native_walk=actor&&actor==unified_root_actor&&settings.unified_controls;
        if(actor&&actor==modern_update_actor&&flag==0&&(modern_stepped||native_walk)&&!modern_pose_done){
            modern_pose_done=true;
            re4::ModernMoveIdentity owner;std::array<re4::ModernMoveBone,9> bones;re4::ModernMoveBone root;
            if((native_walk||modern_native.evaluate)&&modern_native.rotation&&modern_native.position&&modern_native.scale&&modern_native.copy_matrix
               &&modern_move_collect(actor,owner,bones,&root)&&(native_walk||owner==modern_bound_owner)&&modern_pose.begin(owner,bones)){
                re4::ModernMoveCursorScope cursors(bones);
                if(!cursors.valid()){modern_pose.rollback();Orig(actor,flag);return;}
                float lower_yaw=modern_lower_yaw;
                if(native_walk){
                    const auto left=re4::modern_move_disk(re4::shape_axis(control.raw_lx,settings.left_deadzone,1,settings.stick_curve),re4::shape_axis(control.raw_ly,settings.left_deadzone,1,settings.stick_curve));
                    lower_yaw=std::remainder(std::atan2(-left.x,left.z)-(static_cast<const std::uint8_t*>(actor)[0x115]==2?3.14159265359f:0),6.28318530718f);
                }else{
                    modern_cycle.motion().set<std::uint32_t>(0x58,modern_cycle.motion().get<std::uint32_t>(0x58)&~(1u<<27));
                    modern_native.evaluate(actor,modern_cycle.motion().data());
                }
                cursors.restore(); // Before matrix helpers, native world/IK and every rollback path.
                // Ordinary locomotion only: rotate the already blended pelvis
                // matrix, preserving its native translation and animation.
                // Actor yaw remains the view heading; weapon states never own
                // this overlay. The lower branch compensates for the body turn.
                const auto* bytes=static_cast<const std::uint8_t*>(actor);
                float dt=0;
                const bool body=native_walk&&bytes[0x115]>=1&&bytes[0x115]<=3&&!bytes[0x309]
                    &&modern_move_read(modern_move_slot(0xB1AE08)+0xA4,dt)
                    &&std::isfinite(dt)&&dt>0&&dt<=2&&modern_body_pose.begin(owner,{root});
                if(body){
                    if(!(owner==modern_body_owner))modern_body_yaw=0;
                    const float target=std::clamp(lower_yaw*.3f,-.436332313f,.436332313f);
                    const float amount=modern_body_yaw+(target-modern_body_yaw)*(-std::expm1(-dt/4.5f));
                    const float c=std::cos(amount),s=std::sin(amount);
                    for(unsigned col=0;col<3;++col){
                        const float x=root.matrix[col],z=root.matrix[8+col];
                        root.matrix[col]=c*x+s*z;root.matrix[8+col]=-s*x+c*z;
                    }
                    if(modern_body_pose.finish()){
                        modern_body_owner=owner;modern_body_yaw=amount;modern_body_applied=true;
                        lower_yaw-=amount;
                    }
                }
                bones[0].local[4]+=lower_yaw;
                bool finite=true;for(const auto& bone:bones)for(unsigned i=0;i<9;++i)finite=finite&&std::isfinite(bone.local[i]);
                if(finite){
                    for(const auto& bone:bones){
                        modern_native.rotation(bone.matrix,bone.local+3);modern_native.position(bone.matrix,bone.local);
                        modern_native.scale(bone.matrix,bone.local+6);
                        modern_native.copy_matrix(bone.matrix,reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(bone.local)-0xA4+0x18));
                    }
                    if(modern_pose.finish())control.modern_pose_steps=control.modern_pose_steps+1;
                    else{modern_body_pose.rollback();modern_body_applied=false;}
                }else{modern_pose.rollback();modern_body_pose.rollback();modern_body_applied=false;control.modern_move_status=5;}
            }
        }
        Orig(actor,flag); // Native world matrices and IK run once after gait.
    }
};

bool modern_move_matches(const std::uint8_t* memory,std::size_t size){
    constexpr re4::profile::Site guards[]={
        {0x311400,0xD10183FF},{0x2B7C60,0xD10683FF},{0x2B90F0,0xD10243FF},
        {0x2B78F0,0xD102C3FF},{0x2B8370,0xD10343FF},{0x2B8700,0xD10603FF},
        {0x81D0,0xA9BD57F6},{0x2A5430,0xFC1B0FEC},{0x2A5B90,0xB9400028},
        {0x2A5B00,0xBD400020},{0x279710,0xEB01001F},{0x315E10,0x90004088},
        {0x311678,0xD63F0100},{0x31178C,0x97F3E7D5},{0x3117A0,0x97F3D7F0},
        {0x328190,0xD10183FF},{0x328194,0xFD0013E8},{0x328198,0xF90017F7},{0x32819C,0xA90357F6}
    };
    if(!memory)return false;
    for(const auto& site:guards){std::uint32_t value=0;if(site.offset+4>size)return false;std::memcpy(&value,memory+site.offset,4);if(value!=site.instruction)return false;}
    return true;
}
void modern_move_install(){
    modern_native.move.sample=+[](const re4::ModernMoveSample* r,re4::ModernMoveVec3* out,std::uint16_t* cursor,unsigned a,unsigned b){
        using Fn=std::uint32_t (*)(const re4::ModernMoveSample*,re4::ModernMoveVec3*,std::uint16_t*,unsigned,unsigned);
        reinterpret_cast<Fn>(main_base+0x2B78F0)(r,out,cursor,a,b);
    };
    modern_native.move.advance=reinterpret_cast<decltype(modern_native.move.advance)>(main_base+0x2B8F10);
    modern_native.move.root=reinterpret_cast<decltype(modern_native.move.root)>(main_base+0x2B8370);
    modern_native.move.translate=reinterpret_cast<decltype(modern_native.move.translate)>(main_base+0x81D0);
    modern_native.evaluate=reinterpret_cast<decltype(modern_native.evaluate)>(main_base+0x2B8700);
    modern_native.rotation=reinterpret_cast<decltype(modern_native.rotation)>(main_base+0x2A5430);
    modern_native.position=reinterpret_cast<decltype(modern_native.position)>(main_base+0x2A5B90);
    modern_native.scale=reinterpret_cast<decltype(modern_native.scale)>(main_base+0x2A5B00);
    modern_native.copy_matrix=reinterpret_cast<decltype(modern_native.copy_matrix)>(main_base+0x279710);
    modern_native.healthy=reinterpret_cast<decltype(modern_native.healthy)>(main_base+0x315E10);
    ModernPlayerUpdate::InstallAtOffset(0x311400);ModernRoot::InstallAtOffset(0x2B7C60);ModernPose::InstallAtOffset(0x2B90F0);
    AutomaticFire::InstallAtOffset(0x328190);
}
