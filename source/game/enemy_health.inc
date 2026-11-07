// Read-only native target capture. No physics calls, enemy writes or render-time
// enemy pointers: only scalar snapshots cross the displayed-scene boundary.
class EnemyHealthReader {
    re4::ModernMoveMemory memory_{};
public:
    template<class T>bool field(std::uintptr_t base,std::size_t offset,T& value){
        if(!base||base>std::numeric_limits<std::uintptr_t>::max()-offset)return false;
        const auto address=base+offset;
        if(!memory_.contains(address,sizeof(T))){
            MemoryInfo info{};std::uint32_t page=0;
            if(R_FAILED(svcQueryMemory(&info,&page,address))||!(info.perm&1))return false;
            memory_=re4::ModernMoveMemory::span(reinterpret_cast<const void*>(info.addr),info.size);
        }
        return memory_.value(address,value);
    }
};
bool enemy_health_scene_allowed(void* actor){
    if(!actor||actor!=*reinterpret_cast<void**>(main_base+re4::profile::player_slot))return false;
    const auto* bytes=static_cast<const std::uint8_t*>(actor);
    if(bytes[0x114]||!(bytes[0x115]<=4||bytes[0x115]==6||bytes[0x115]==11||bytes[0x115]==15))return false;
    const auto* controller=reinterpret_cast<const std::uint8_t*>(main_base+0x1131718);
    const auto* state=*reinterpret_cast<const std::uint8_t**>(main_base+0xB1AE08);
    return state&&(controller[0x48]&1)&&(controller[0x54]==10||scoped_camera_owner(actor))
        &&*reinterpret_cast<const int*>(main_base+0xAFF39C)==2&&!state[0x86FF]
        &&!(state[0x5092]&4)&&!(state[0x509E]&0x80)&&!(state[0x1AB]&0x40);
}
bool enemy_health_aim_allowed(void* actor){
    if(!enemy_health_scene_allowed(actor)||!firearm_aim(actor))return false;
    const auto* bytes=static_cast<const std::uint8_t*>(actor);
    return bytes[0x115]!=6||bytes[0x116]==1||bytes[0x116]==2;
}
bool enemy_health_snapshot(void* weapon,re4::ViewVec3& anchor,std::uint32_t& health,std::uint64_t& target_key){
    EnemyHealthReader reader;
    std::uintptr_t target=0,pool=0,part=0;unsigned count=0,stride=0,flags=0;
    const auto manager=main_base+0xAE6EF8;
    if(!reader.field(reinterpret_cast<std::uintptr_t>(weapon),0x418,target)
        ||!reader.field(manager,8,pool)||!reader.field(manager,0x10,count)||!reader.field(manager,0x14,stride)
        ||!re4::enemy_health_pool_member(pool,count,stride,target))return false;
    std::uint8_t type=0,parts=0;std::int16_t current=0,maximum=0;re4::ViewVec3 root;
    if(!reader.field(target,8,flags)||!reader.field(target,0x118,type)
        ||!reader.field(target,0x3C2,current)||!reader.field(target,0x3C4,maximum)
        ||!re4::enemy_health_target(flags,type,current,maximum)
        ||!reader.field(target,0xA4,root)||!re4::view_finite(root)
        ||!reader.field(target,0x11A,parts)||!parts||!reader.field(target,0x108,part))return false;
    const auto head=part;
    std::array<std::uintptr_t,255> visited{};
    float top=root.y;bool visible_part=false;
    for(unsigned i=0;i<parts;++i){
        if(!part)return false;
        for(unsigned j=0;j<i;++j)if(part==visited[j])return false;
        visited[i]=part;re4::ViewVec3 point;unsigned bone_flags=0;
        if(!reader.field(part,0x1D8,bone_flags))return false;
        if(!(bone_flags&2)){
            if(!reader.field(part,0x80,point)||!re4::view_finite(point))return false;
            top=std::max(top,point.y);visible_part=true;
        }
        if(i+1<parts&&!reader.field(part,0x108,part))return false;
    }
    if(!visible_part||!re4::view_store_point(root.x,double(top)+100,root.z,anchor))return false;
    health=std::uint32_t(std::uint16_t(current))|(std::uint32_t(std::uint16_t(maximum))<<16);
    // Opaque identity distinguishes reused slots with a different native model.
    // Only this scalar, never an enemy/bone pointer, reaches the render thread.
    auto key=std::uint64_t(target)^(std::uint64_t(head)+0x9E3779B97F4A7C15ull);
    key=(key^(key>>30))*0xBF58476D1CE4E5B9ull;
    key=(key^(key>>27))*0x94D049BB133111EBull;
    target_key=(key^(key>>31))|1;
    return true;
}
void capture_enemy_health(void* actor,void* weapon){
    enemy_health_point.frame.store(0,std::memory_order_release);
    if(!(view_settings_snapshot.load(std::memory_order_acquire)&64)||!enemy_health_aim_allowed(actor)
        ||weapon!=player_weapon(actor))return;
    re4::ViewVec3 anchor;std::uint32_t health=0;std::uint64_t key=0;
    if(enemy_health_snapshot(weapon,anchor,health,key))publish_aim_point(enemy_health_point,actor,weapon,anchor,
        view_frame.load(std::memory_order_acquire),nullptr,nullptr,health,key);
}
void enemy_health_submit(void* actor,std::uint64_t frame){
    pending_enemy_health.frame.store(0,std::memory_order_release);
    re4::ViewVec3 anchor;std::uint32_t health=0;std::uint64_t key=0;
    if((view_settings_snapshot.load(std::memory_order_acquire)&64)&&enemy_health_aim_allowed(actor)
        &&read_aim_point(enemy_health_point,actor,frame,anchor,nullptr,nullptr,&health,&key))
        publish_aim_point(pending_enemy_health,actor,player_weapon(actor),anchor,frame,nullptr,nullptr,health,key);
}
void enemy_health_draw_scene(void* actor,std::uint64_t frame,const std::uint8_t* state){
    displayed_enemy_health.frame.store(0,std::memory_order_release);
    enemy_health_camera.frame.store(0,std::memory_order_release);
    re4::ViewVec3 anchor;std::uint32_t health=0;std::uint64_t key=0;
    if(state&&(view_settings_snapshot.load(std::memory_order_acquire)&64)&&enemy_health_scene_allowed(actor)){
        re4::ViewMatrix3x4 view;re4::ViewMatrix4x4 projection;
        std::memcpy(&view,state+0xD8,sizeof(view));std::memcpy(&projection,state+0x10C,sizeof(projection));
        publish_aim_point(enemy_health_camera,actor,nullptr,{},frame,&view,&projection);
        if(frame>1&&enemy_health_aim_allowed(actor)
            &&read_aim_point(pending_enemy_health,actor,frame-1,anchor,nullptr,nullptr,&health,&key))
            publish_aim_point(displayed_enemy_health,actor,player_weapon(actor),anchor,frame,&view,&projection,health,key);
    }
    pending_enemy_health.frame.store(0,std::memory_order_release);
}
void draw_enemy_health_hud(){
    const auto reset=[](){
        enemy_health_fade.reset();enemy_health_fade_tick=enemy_health_fade_frame=0;
        enemy_health_fade_actor=0;enemy_health_fade_character=0;
    };
    const auto generation=enemy_health_fade_generation.load(std::memory_order_acquire);
    if(generation!=enemy_health_fade_seen){reset();enemy_health_fade_seen=generation;}
    const unsigned settings_word=view_settings_snapshot.load(std::memory_order_acquire);
    if(!(settings_word&64)||!native_hud_ready()){reset();return;}
    auto* actor=*reinterpret_cast<void**>(main_base+re4::profile::player_slot);
    const auto* state=*reinterpret_cast<const std::uint8_t**>(main_base+0xB1AE08);
    if(!enemy_health_scene_allowed(actor)||!state||(*reinterpret_cast<const std::uint32_t*>(state+0x7C)&(1u<<13))){reset();return;}
    const auto frame=view_frame.load(std::memory_order_acquire),now=svcGetSystemTick();
    const auto owner=reinterpret_cast<std::uintptr_t>(actor);const unsigned character=state[0x503C];
    if((enemy_health_fade_actor&&(enemy_health_fade_actor!=owner||enemy_health_fade_character!=character))
        ||(enemy_health_fade_tick&&(now<enemy_health_fade_tick
            ||(frame!=enemy_health_fade_frame&&now-enemy_health_fade_tick>1'920'000))))reset();
    re4::ViewVec3 anchor;re4::ViewHudPoint point;std::uint32_t health=0;std::uint64_t key=0;
    re4::ViewMatrix3x4 view;re4::ViewMatrix4x4 projection;
    if(!read_aim_point(enemy_health_camera,actor,frame,anchor,&view,&projection,nullptr,nullptr,false)){reset();return;}
    enemy_health_fade_actor=owner;enemy_health_fade_character=character;
    enemy_health_fade_tick=now;enemy_health_fade_frame=frame;
    re4::EnemyHealthFadeTarget visible{};
    const bool selected=read_aim_point(displayed_enemy_health,actor,frame,anchor,nullptr,nullptr,&health,&key)
        &&key&&re4::enemy_health_values(int(health&0xFFFF),int(health>>16))
        &&re4::project_view_point(view,projection,anchor,point)&&re4::enemy_health_in_view(point);
    if(selected)visible={key,anchor,int(health&0xFFFF),int(health>>16)};
    enemy_health_fade.update(now,selected?&visible:nullptr);
    const auto backend=native_hud_backend();
    for(const auto& entry:enemy_health_fade.entries()){
        if(!entry.active||!re4::project_view_point(view,projection,entry.target.anchor,point))continue;
        if(re4::draw_enemy_health(backend,point,entry.target.current,entry.target.maximum,entry.opacity)
            &&(settings_word&128))
            re4::draw_enemy_health_number(backend,point,entry.target.current,re4::kEnemyHealthPowerUnit,settings_word&256,entry.opacity);
    }
}
