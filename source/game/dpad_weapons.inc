// Included after modern_move.inc inside module.cpp's anonymous namespace.
struct DpadWeaponsNative {
    void (*classify)(unsigned,void*)=nullptr;
    unsigned (*effective_id)(void*,const void*)=nullptr;
    unsigned (*weapon)(unsigned)=nullptr;
    unsigned (*variant)(unsigned)=nullptr;
    int (*select)(void*,void*)=nullptr;
    void (*prepare)(unsigned)=nullptr;
    void (*teardown)(void*)=nullptr;
    void (*load)(void*,unsigned,unsigned)=nullptr;
    void (*initialize)(void*)=nullptr;
    void (*reset_motion)(void*)=nullptr;
} dpad_weapons_native;
constexpr std::uint64_t dpad_weapons_mask=0xF000;
struct DpadWeaponsContext {
    std::uintptr_t actor=0,state=0,manager=0,selected=0,slots=0;
    std::int32_t count=0;
    std::uint16_t effective_id=0;
    std::uint8_t owner=0;
    std::array<std::uint8_t,14> selected_record{};
    bool ordinary=false;
};
struct DpadWeaponsRequest {
    DpadWeaponsContext context{};
    int direction=0;
    re4::WeaponCycleGroup group=re4::WeaponCycleGroup::Guns;
    unsigned age=0;
    bool before_safe=false;
};
std::atomic_flag dpad_weapons_queue_lock=ATOMIC_FLAG_INIT;
std::atomic<bool> dpad_weapons_busy{false};
std::atomic<bool> dpad_weapons_before_valid{false};
std::atomic<bool> dpad_weapons_connected{false};
std::atomic<std::uint64_t> dpad_weapons_physical{0};
DpadWeaponsRequest dpad_weapons_pending;

template<class T> bool dpad_weapons_read(std::uintptr_t p,T& value){
    re4::ModernMoveMemory memory;
    if(!modern_move_region(p,sizeof(T),false,memory))return false;
    std::memcpy(&value,reinterpret_cast<const void*>(p),sizeof(T));return true;
}
bool dpad_weapons_context(void* actor,DpadWeaponsContext& out){
    if(!main_base||!settings.dpad_weapon_switching||!(sensor_attributes&1)
       ||!dpad_weapons_connected.load(std::memory_order_acquire))return false;
    std::uintptr_t player=0,state=0;
    if(!dpad_weapons_read(main_base+re4::profile::player_slot,player)||!player
       ||(actor&&player!=reinterpret_cast<std::uintptr_t>(actor))
       ||!dpad_weapons_read(main_base+0xB1AE08,state))return false;
    re4::ModernMoveMemory memory;
    if(!modern_move_region(player,0x950,true,memory)||!modern_move_region(state,0x8700,true,memory)
       ||!modern_move_region(main_base+0x1131718,0x58,false,memory))return false;
    const auto* a=reinterpret_cast<const std::uint8_t*>(player);
    const auto* s=reinterpret_cast<const std::uint8_t*>(state);
    const auto* controller=reinterpret_cast<const std::uint8_t*>(main_base+0x1131718);
    std::uint32_t flags=0;std::uint8_t suppressed=0;int gameplay=0;
    std::memcpy(&flags,s+0x5090,4);
    if(a[0x114]||s[0x503C]||controller[0x54]!=10||!(controller[0x48]&1)
       ||(flags&0x60004040u)||(s[0x5092]&4)||(s[0x509E]&0x80)||(s[0x1AB]&0x40)||s[0x86FF]
       ||!dpad_weapons_read(main_base+0x11C7D55,suppressed)||suppressed
       ||!dpad_weapons_read(main_base+0xAFF39C,gameplay)||gameplay!=2)return false;
    // Only normal locomotion and known weapon busy states can retain a request.
    // Damage/death/action/QTE routines must retain their native input behavior.
    const auto routine=a[0x115];
    if(routine>4&&routine!=6&&routine!=11&&routine!=15)return false;
    const auto manager=main_base+0x124D918;
    if(!modern_move_region(manager,0x44,true,memory)
       ||*reinterpret_cast<const std::uint8_t*>(manager+0x1B)==1)return false;
    std::uintptr_t selected=0,slots=0;std::int32_t count=0;
    std::memcpy(&selected,reinterpret_cast<const void*>(manager+0x10),sizeof(selected));
    if(!dpad_weapons_read(manager+0x20,slots)||!dpad_weapons_read(manager+0x30,count)||count<1||count>384
       ||!modern_move_region(slots,std::size_t(count)*14,true,memory))return false;
    if(selected&&(selected<slots||(selected-slots)%14||(selected-slots)/14>=std::size_t(count)))return false;
    out.actor=player;out.state=state;out.manager=manager;out.selected=selected;out.slots=slots;out.count=count;
    std::memcpy(&out.effective_id,reinterpret_cast<const void*>(manager+0x18),2);
    out.owner=*reinterpret_cast<const std::uint8_t*>(manager+0x1B);out.ordinary=routine<=3;
    if(selected)std::memcpy(out.selected_record.data(),reinterpret_cast<const void*>(selected),14);
    return true;
}
bool dpad_weapons_same(const DpadWeaponsContext& a,const DpadWeaponsContext& b){
    return a.actor==b.actor&&a.state==b.state&&a.manager==b.manager&&a.selected==b.selected
        &&a.slots==b.slots&&a.count==b.count&&a.owner==b.owner&&a.effective_id==b.effective_id
        &&a.selected_record==b.selected_record;
}
std::uint64_t dpad_weapons_filter(std::uint64_t buttons,bool connected){
    const auto physical=connected?(buttons&dpad_weapons_mask):0;
    const auto previous=dpad_weapons_physical.exchange(physical,std::memory_order_acq_rel);
    const bool was_connected=dpad_weapons_connected.exchange(connected,std::memory_order_acq_rel);
    DpadWeaponsContext context;
    const bool active=connected&&dpad_weapons_context(nullptr,context);
    if(!dpad_weapons_queue_lock.test_and_set(std::memory_order_acquire)){
        if(!active||!connected||(physical&(physical-1)))dpad_weapons_pending={};
        // Require all directions released between requests. In particular,
        // releasing half of a diagonal cannot turn it into a fresh key press.
        else if(was_connected&&!previous&&physical&&!(physical&(physical-1))
                &&!dpad_weapons_pending.direction&&!dpad_weapons_busy.load(std::memory_order_acquire)){
            dpad_weapons_pending.context=context;
            dpad_weapons_pending.group=(physical&0xA000)?re4::WeaponCycleGroup::Grenades:re4::WeaponCycleGroup::Guns;
            dpad_weapons_pending.direction=(physical&0x6000)?1:-1;
            dpad_weapons_pending.age=0;dpad_weapons_pending.before_safe=false;
            weapon_popup_trace(1);
        }
        dpad_weapons_queue_lock.clear(std::memory_order_release);
    }
    return active?buttons&~dpad_weapons_mask:buttons;
}
void dpad_weapons_before_update(void* actor){
    if(dpad_weapons_busy.load(std::memory_order_acquire))return;
    // Failure to obtain this frame's snapshot cannot reuse a prior safe frame.
    dpad_weapons_before_valid.store(false,std::memory_order_release);
    if(dpad_weapons_queue_lock.test_and_set(std::memory_order_acquire))return;
    if(dpad_weapons_pending.direction){
        DpadWeaponsContext context;
        if(!dpad_weapons_context(actor,context)||!dpad_weapons_same(context,dpad_weapons_pending.context)
           ||++dpad_weapons_pending.age>30)dpad_weapons_pending={};
        else{
            dpad_weapons_pending.before_safe=context.ordinary;
            dpad_weapons_before_valid.store(context.ordinary,std::memory_order_release);
        }
    }
    dpad_weapons_queue_lock.clear(std::memory_order_release);
}
struct DpadWeaponsInventory {
    std::uintptr_t slots=0;
    std::size_t count=0,current=re4::weapon_cycle_none;
    std::uint8_t owner=0;
    std::array<re4::WeaponCycleSlot,re4::weapon_cycle_capacity> entries{};
};
bool dpad_weapons_inventory(const DpadWeaponsContext& context,DpadWeaponsInventory& out){
    std::int32_t count=0;std::uintptr_t slots=0;
    if(!dpad_weapons_read(context.manager+0x30,count)||count<1||count>384
       ||!dpad_weapons_read(context.manager+0x20,slots))return false;
    re4::ModernMoveMemory memory;
    if(!modern_move_region(slots,std::size_t(count)*14,true,memory))return false;
    out.slots=slots;out.count=std::size_t(count);
    if(!dpad_weapons_read(context.manager+0x1B,out.owner)||out.owner==1)return false;
    if(context.selected){
        if(context.selected<slots||(context.selected-slots)%14||(context.selected-slots)/14>=out.count)return false;
        out.current=(context.selected-slots)/14;
    }
    for(std::size_t i=0;i<out.count;++i){
        const auto* p=reinterpret_cast<const std::uint8_t*>(slots+i*14);auto& e=out.entries[i];
        std::memcpy(&e.raw_id,p,2);std::memcpy(&e.quantity,p+2,2);e.flags=p[4];e.owner=p[5];
        if(e.quantity&&(e.flags&1)&&e.owner==out.owner&&!re4::weapon_cycle_grenade(e.raw_id)){
            std::array<std::uint8_t,6> type{};dpad_weapons_native.classify(e.raw_id,type.data());e.item_class=type[2];
        }
    }
    return true;
}
void dpad_weapons_restore_pose(void* actor){
    re4::ModernMoveIdentity owner;std::array<re4::ModernMoveBone,9> bones{};
    if(modern_body_pose.pending()){
        re4::ModernMoveBone root;
        if(modern_move_collect(actor,owner,bones,&root))modern_body_pose.restore(owner,{root});
        else modern_body_pose.discard();
    }
    if(modern_pose.pending()){
        if(modern_move_collect(actor,owner,bones))modern_pose.restore(owner,bones);
        else modern_pose.discard();
    }
}
void dpad_weapons_reset_sessions(){
    modern_pose.discard();modern_body_pose.discard();modern_cycle.reset();modern_aim_session={};
    modern_body_owner={};modern_body_yaw=0;modern_body_applied=false;
    modern_bound_owner={};modern_bound_asset=modern_bound_clip=0;modern_root_done=modern_stepped=modern_pose_done=false;
    // A weapon rebuild invalidates animation and aim owners, but it does not
    // start a new view session. Clearing pitch ownership makes the next player
    // update seed pitch=0 and visibly snaps the shoulder camera to its base.
    unified_look.reset();unified_aim_session={};unified_reload_session={};
    unified_command_frame=0;unified_update=nullptr;reset_scoped_controls();
    gyro_session={};gyro_pending={};gyro.reset();gyro_frame.reset();gyro_input_actor=nullptr;
    first_person_frame.store(0,std::memory_order_release);
    aim_point.frame.store(0,std::memory_order_release);pending_aim_point.frame.store(0,std::memory_order_release);
    displayed_aim_point.frame.store(0,std::memory_order_release);invalidate_grenade_spot_frames();
}
bool dpad_weapons_natives_ready(){
    const auto& n=dpad_weapons_native;
    return n.classify&&n.effective_id&&n.weapon&&n.variant&&n.select&&n.prepare&&n.teardown&&n.load&&n.initialize&&n.reset_motion;
}
void dpad_weapons_update(void* actor){
    if(dpad_weapons_busy.exchange(true,std::memory_order_acq_rel))return;
    struct Finish {~Finish(){dpad_weapons_busy.store(false,std::memory_order_release);}} finish;
    if(dpad_weapons_queue_lock.test_and_set(std::memory_order_acquire))return;
    DpadWeaponsContext context;DpadWeaponsRequest request;
    if(dpad_weapons_pending.direction){
        if(!dpad_weapons_context(actor,context)||!dpad_weapons_same(context,dpad_weapons_pending.context))dpad_weapons_pending={};
        else if(context.ordinary&&dpad_weapons_pending.before_safe
                &&dpad_weapons_before_valid.load(std::memory_order_acquire)){request=dpad_weapons_pending;dpad_weapons_pending={};}
    }
    dpad_weapons_before_valid.store(false,std::memory_order_release);
    dpad_weapons_queue_lock.clear(std::memory_order_release);
    if(!request.direction||!dpad_weapons_natives_ready())return;
    DpadWeaponsInventory inventory;
    if(!dpad_weapons_inventory(context,inventory))return;
    const auto next=re4::weapon_cycle_next(inventory.entries.data(),inventory.count,inventory.current,inventory.owner,request.group,request.direction);
    if(next==re4::weapon_cycle_none||next==inventory.current)return;
    const auto target=inventory.slots+14*next;
    std::array<std::uint8_t,14> record{};std::memcpy(record.data(),reinterpret_cast<const void*>(target),14);
    auto& n=dpad_weapons_native;
    // Keep a proven recovery target while the actor still owns its old weapon.
    // A consumed/stale record must first be cleared by the native owner.
    const bool restore_none=!context.selected&&context.effective_id==0x800;
    const bool restore_slot=inventory.current<inventory.count
        &&re4::weapon_cycle_candidate(inventory.entries[inventory.current],inventory.owner,
            re4::weapon_cycle_grenade(inventory.entries[inventory.current].raw_id)?re4::WeaponCycleGroup::Grenades:re4::WeaponCycleGroup::Guns)
        &&n.effective_id(reinterpret_cast<void*>(context.manager),reinterpret_cast<const void*>(context.selected))==context.effective_id
        &&n.weapon(context.effective_id)<255&&n.variant(context.effective_id)<255;
    if(!restore_none&&!restore_slot)return;
    const unsigned effective=n.effective_id(reinterpret_cast<void*>(context.manager),reinterpret_cast<const void*>(target));
    unsigned weapon=n.weapon(effective),variant=n.variant(effective);
    if(weapon>=255||variant>=255)return;
    // These pose owners must be drained before native33B3E0 replaces +548.
    dpad_weapons_restore_pose(actor);
    DpadWeaponsContext after;std::uintptr_t slots=0;std::int32_t count=0;std::uint8_t owner=0;
    if(!dpad_weapons_context(actor,after)||!after.ordinary||!dpad_weapons_same(context,after)
       ||!dpad_weapons_read(context.manager+0x20,slots)||slots!=inventory.slots
       ||!dpad_weapons_read(context.manager+0x30,count)||count!=static_cast<std::int32_t>(inventory.count)
       ||!dpad_weapons_read(context.manager+0x1B,owner)||owner!=inventory.owner
       ||std::memcmp(record.data(),reinterpret_cast<const void*>(target),14)
       ||n.effective_id(reinterpret_cast<void*>(context.manager),reinterpret_cast<const void*>(target))!=effective)return;
    if(!(n.select(reinterpret_cast<void*>(context.manager),reinterpret_cast<void*>(target))&1))return;
    std::uint16_t actual=0;std::memcpy(&actual,reinterpret_cast<const void*>(context.manager+0x18),2);
    if(actual!=effective){
        const unsigned actual_weapon=n.weapon(actual),actual_variant=n.variant(actual);
        if(actual_weapon<255&&actual_variant<255){weapon=actual_weapon;variant=actual_variant;}
        else{
            // Null here deliberately restores the previously verified native
            // unequipped state (manager+18==0x800); it never selects a target.
            n.select(reinterpret_cast<void*>(context.manager),reinterpret_cast<void*>(context.selected));
            dpad_weapons_reset_sessions();return;
        }
    }
    // The native setter owns packed parameter bits16..18 and can clear them.
    // Match menu apply: snapshot the selected record's value AFTER selection,
    // then retain it across resource loading just like the native menu does.
    const auto native_parameter=*reinterpret_cast<const std::uint8_t*>(target+8)&7;
    // EXPERIMENTAL: resource loading33B3B0 may yield. This post-native player
    // update context is established, but scheduler timing still needs console
    // verification. Once teardown starts, always complete the native sequence.
    n.prepare(2);n.teardown(actor);n.load(actor,weapon,variant);
    *reinterpret_cast<std::uint8_t*>(context.state+0x5036)=std::uint8_t(native_parameter);
    n.initialize(actor);n.reset_motion(actor);dpad_weapons_reset_sessions();
    weapon_popup_trace(2);
    // Loading may yield into another scene. Publish only a completed equip
    // still owned by this actor and exact inventory slot after native init.
    DpadWeaponsContext committed;
    if(dpad_weapons_context(actor,committed)&&committed.selected==target
        &&committed.slots==inventory.slots&&committed.effective_id==actual
        &&committed.owner==inventory.owner&&committed.selected_record[0]==record[0]
        &&committed.selected_record[1]==record[1])
        weapon_popup_commit(actor,unsigned(record[0])|(unsigned(record[1])<<8));
    else weapon_popup_trace(4);
}
