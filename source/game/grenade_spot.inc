// Extra collision queries run only after native player update, with private
// vectors and restored native scratch. HUD receives copied scalar snapshots.
void* (*native_grenade_bone)(void*,unsigned)=nullptr;
unsigned (*native_grenade_segment)(void*,const re4::ViewVec3*,const re4::ViewVec3*,re4::ViewVec3*,re4::ViewVec3*,unsigned,unsigned)=nullptr;
void (*native_grenade_sphere)(void*,re4::ViewVec3*,const re4::ViewVec3*,re4::ViewVec3*,unsigned,unsigned,float)=nullptr;
int (*native_grenade_water)(const re4::ViewVec3*,float*)=nullptr;
AimPoint grenade_spot_point,pending_grenade_spot,displayed_grenade_spot;
struct GrenadeSpotCache {
    std::uintptr_t actor=0,weapon=0,pool=0;
    unsigned item=0,count=0,room=0;
    std::uint64_t tick=0;
    bool valid=false;
    re4::GrenadeLanding landing{};
} grenade_spot_cache;
std::atomic_flag grenade_spot_query_busy=ATOMIC_FLAG_INIT;
void invalidate_grenade_spot_frames(){
    grenade_spot_point.frame.store(0,std::memory_order_release);
    pending_grenade_spot.frame.store(0,std::memory_order_release);
    displayed_grenade_spot.frame.store(0,std::memory_order_release);
}
void grenade_spot_reset(){grenade_spot_cache={};invalidate_grenade_spot_frames();}
bool grenade_spot_allowed(void* actor){
    if(!(view_settings_snapshot.load(std::memory_order_acquire)&512)||!enemy_health_scene_allowed(actor)||!firearm_aim(actor))return false;
    const auto* bytes=static_cast<const std::uint8_t*>(actor);
    const auto* state=*reinterpret_cast<const std::uint8_t**>(main_base+0xB1AE08);
    const unsigned item=state[0x5034];
    return bytes[0x115]==6&&bytes[0x116]==1&&(item==0x13||item==0x16||item==0x17)
        &&*reinterpret_cast<const std::uintptr_t*>(main_base+0x1265A28)==main_base+0x31B9F0;
}
unsigned grenade_spot_room(const std::uint8_t* state){std::uint16_t room=0;if(state)std::memcpy(&room,state+0x5020,2);return room;}
std::uint64_t grenade_spot_pack_normal(re4::ViewVec3 normal){
    std::uint64_t bits=0;const float values[]={normal.x,normal.y,normal.z};
    for(unsigned i=0;i<3;++i)bits|=std::uint64_t(std::uint16_t(int(std::lround(std::clamp(values[i],-1.f,1.f)*32767))))<<(16*i);
    return bits;
}
re4::ViewVec3 grenade_spot_unpack_normal(std::uint64_t bits){
    float value[3];for(unsigned i=0;i<3;++i){const unsigned v=(bits>>(16*i))&65535;value[i]=float(v<32768?int(v):int(v)-65536)/32767;}
    return {value[0],value[1],value[2]};
}
struct GrenadeSpotScratch {
    // Refuse meshes above8192triangles below, so the native visited-bitset
    // clear touches at most1025bytes. Keep neighbouring camera globals out.
    std::array<std::uint8_t,0x419> saved;
    std::array<std::uint8_t,0x28> water;
    GrenadeSpotScratch(){
        std::memcpy(saved.data(),reinterpret_cast<const void*>(main_base+0x1131250),saved.size());
        std::memcpy(water.data(),reinterpret_cast<const void*>(main_base+0x113CC24),water.size());
    }
    ~GrenadeSpotScratch(){
        std::memcpy(reinterpret_cast<void*>(main_base+0x1131250),saved.data(),saved.size());
        std::memcpy(reinterpret_cast<void*>(main_base+0x113CC24),water.data(),water.size());
        grenade_spot_query_busy.clear(std::memory_order_release);
    }
};
struct GrenadeExplosionContext {void* world;re4::ViewVec3 up;};
bool grenade_spot_sweep(void* context,re4::ViewVec3 previous,re4::ViewVec3 velocity,re4::ViewVec3& point,re4::GrenadeExplosionContact& contact){
    auto* world=static_cast<GrenadeExplosionContext*>(context)->world;
    if(!re4::grenade_world_point(previous)||!re4::grenade_world_point(point)||!re4::view_finite(velocity))return false;
    re4::ViewVec3 ray_point=point;
    native_grenade_segment(world,&previous,&point,&ray_point,nullptr,0,0x4000);
    if(!re4::grenade_world_point(ray_point))return false;
    float height=0;
    if(native_grenade_water(&point,&height)){
        if(!std::isfinite(height))return false;
        if(point.y<=height&&ray_point.y<height){
            EnemyHealthReader reader;std::uint8_t enabled=0;
            if(!reader.field(reinterpret_cast<std::uintptr_t>(world),0x256,enabled)||!enabled)return false;
            point.y=height+20;contact.water=true;return re4::grenade_world_point(point);
        }
    }
    contact.normal={};native_grenade_sphere(world,&contact.normal,&previous,&point,0x2001,0x4000,25);
    if(!re4::grenade_world_point(point)||!re4::view_finite(contact.normal))return false;
    if(contact.normal.x==0&&contact.normal.y==0&&contact.normal.z==0)return true;
    const double speed=std::hypot(double(velocity.x),double(velocity.y),double(velocity.z));
    re4::ViewVec3 end=point;
    if(speed>0&&!re4::view_store_point(point.x+3000*double(velocity.x)/speed,point.y+3000*double(velocity.y)/speed,point.z+3000*double(velocity.z)/speed,end))return false;
    re4::ViewVec3 material_point=end;
    const auto flags=native_grenade_segment(world,&point,&end,&material_point,nullptr,0,0);
    const unsigned index=(flags&(1u<<24))?((flags>>23)&1)|((flags>>14)&2)|((flags>>5)&4):0;
    EnemyHealthReader reader;std::uint8_t enabled=0;
    const auto manager=reinterpret_cast<std::uintptr_t>(world);
    if(!reader.field(manager,0x254+index,enabled))return false;
    if(enabled&&!reader.field(manager,0x34+0x44*index,contact.material_flags))return false;
    return true;
}
bool grenade_spot_relocate(void* context,re4::ViewVec3 origin,re4::ViewVec3& burst){
    const auto& env=*static_cast<GrenadeExplosionContext*>(context);
    const re4::ViewVec3 from{origin.x+env.up.x,origin.y+env.up.y,origin.z+env.up.z};
    if(!re4::grenade_world_point(from))return false;
    const re4::ViewVec3 offsets[]={{400,0,0},{-400,0,0},{0,0,400},{0,0,-400}};
    for(const auto offset:offsets){
        re4::ViewVec3 end{from.x+offset.x,from.y,from.z+offset.z},point=end,normal{};
        const auto hit=native_grenade_segment(env.world,&from,&end,&point,&normal,0,0);
        if(!(hit&(1u<<24)))continue;
        if(!re4::grenade_world_point(point)||!re4::view_finite(normal))return false;
        const double length=std::hypot(double(normal.x),double(normal.z));
        if(length<=1e-6)continue;
        if(!re4::view_store_point(point.x+400*double(normal.x)/length,point.y,point.z+400*double(normal.z)/length,burst))return false;
    }
    return true;
}
bool grenade_spot_project_floor(void* world,const re4::GrenadeExplosion& burst,re4::GrenadeLanding& landing){
    re4::ViewVec3 from=burst.position,to=from,point,normal{};from.y+=2;to.y-=50000;
    if(!re4::grenade_world_point(from)||!re4::grenade_world_point(to))return false;
    const auto hit=native_grenade_segment(world,&from,&to,&point,&normal,0,0x4000);
    re4::ViewVec3 unit;bool ground=(hit&(1u<<24))&&re4::grenade_world_point(point)&&re4::grenade_unit_normal(normal,unit)&&unit.y>.7f&&point.y<=from.y;
    float water=0;
    if(native_grenade_water(&burst.position,&water)){
        if(!std::isfinite(water))return false;
        if(water<=from.y&&(!ground||water>point.y)){point={burst.position.x,water,burst.position.z};unit={0,1,0};ground=true;}
    }
    if(!ground)return false;
    landing={point,unit,burst.time,burst.steps};return true;
}
void grenade_spot_update(void* actor){
    grenade_spot_point.frame.store(0,std::memory_order_release);
    if(!grenade_spot_allowed(actor)||!native_grenade_bone||!native_grenade_segment||!native_grenade_sphere||!native_grenade_water){grenade_spot_reset();return;}
    EnemyHealthReader reader;
    const auto who=reinterpret_cast<std::uintptr_t>(actor),weapon=reinterpret_cast<std::uintptr_t>(player_weapon(actor));
    const auto* state=*reinterpret_cast<const std::uint8_t**>(main_base+0xB1AE08);
    float dt=0;std::uintptr_t pool=0;unsigned count=0,stride=0;std::uint8_t probe=0;
    if(!reader.field(reinterpret_cast<std::uintptr_t>(state),0xA4,dt)||!std::isfinite(dt)||dt<=0||dt>2
        ||!reader.field(main_base+0xAE24C0,8,pool)||!reader.field(main_base+0xAE24C0,0x10,count)
        ||!reader.field(main_base+0xAE24C0,0x14,stride)||!pool||!count||count>4096||stride!=0xF0
        ||pool>std::numeric_limits<std::uintptr_t>::max()-std::size_t(count)*stride
        ||!reader.field(pool,std::size_t(count)*stride-1,probe)||!reader.field(who,0x114,probe)){
        grenade_spot_reset();return;
    }
    const auto now=svcGetSystemTick();const unsigned item=state[0x5034],room=grenade_spot_room(state);
    // These three native rooms add dynamic carry before every projectile step.
    // Without its future platform state an honest static prediction is unavailable.
    if(room==0x21B||room==0x30F||room==0x334){grenade_spot_reset();return;}
    const bool same=grenade_spot_cache.actor==who&&grenade_spot_cache.weapon==weapon&&grenade_spot_cache.pool==pool
        &&grenade_spot_cache.count==count&&grenade_spot_cache.item==item&&grenade_spot_cache.room==room&&now>=grenade_spot_cache.tick;
    const bool refresh=!same||!grenade_spot_cache.tick||now-grenade_spot_cache.tick>=1'920'000;
    if(grenade_spot_query_busy.test_and_set(std::memory_order_acquire))return;
    {
        GrenadeSpotScratch scratch;
        auto* world=reinterpret_cast<void*>(main_base+0xAE24C0);
        for(unsigned i=0;i<count;++i){
            unsigned flags=0;std::uint16_t triangles=0;const auto mesh=pool+std::size_t(i)*stride;
            if(!reader.field(mesh,8,flags))return;
            if((flags&0x201)==1&&(!reader.field(mesh,0x3A,triangles)||triangles>8192))return;
        }
        if(refresh){
            grenade_spot_cache={who,weapon,pool,item,count,room,now,false,{}};
            const auto b0=reinterpret_cast<std::uintptr_t>(native_grenade_bone(actor,0));
            const auto b10=reinterpret_cast<std::uintptr_t>(native_grenade_bone(actor,10));
            re4::ViewVec3 root,hand,previous;
            re4::ViewMatrix3x4 basis;
            float aim=*reinterpret_cast<const float*>(main_base+0x1265A08);
            if(!reader.field(b0,0x80,root)||!reader.field(b10,0x80,hand)||!reader.field(b0,0x98,previous)
                ||!reader.field(who,0x18,basis)||!re4::view_finite(root)||!re4::view_finite(hand)||!re4::view_finite(previous)
                ||!std::isfinite(aim)||aim< -1||aim>1)return;
            re4::ViewVec3 origin=hand,normal{};
            const auto hit=native_grenade_segment(world,&root,&hand,&origin,&normal,0,0);
            if(hit&(1u<<24)){
                if(!re4::view_finite(normal)||!re4::view_store_point(origin.x+500*double(normal.x),origin.y+500*double(normal.y),origin.z+500*double(normal.z),origin))return;
            }
            re4::ViewVec3 velocity{0,30.000001907348633f,283.5f};
            const auto rotate_x=[](re4::ViewVec3& v,float angle){const auto y=v.y,z=v.z;v.y=y*std::cos(angle)-z*std::sin(angle);v.z=y*std::sin(angle)+z*std::cos(angle);};
            if(aim>.1f||aim< -.2f){const float scale=aim>.1f?1+aim:1+.4f*aim;velocity.y*=scale;velocity.z*=scale;}
            else rotate_x(velocity,-.2617993877991494f);
            // The native RNG adds lateral speed in[-15,15). Predict its center
            // without consuming or changing that game's random sequence.
            rotate_x(velocity,-.7853981633974483f*aim);
            const auto local=velocity;const float v[]={local.x,local.y,local.z};float world_v[3]{};
            for(unsigned i=0;i<3;++i)for(unsigned j=0;j<4;++j){
                if(!std::isfinite(basis.m[i][j]))return;
                if(j<3)world_v[i]+=basis.m[i][j]*v[j];
            }
            velocity={world_v[0]+root.x-previous.x,world_v[1]+root.y-previous.y,world_v[2]+root.z-previous.z};
            re4::ViewVec3 rotation;float heading_offset=0;
            if(!reader.field(who,0xB0,rotation)||!reader.field(who,0x524,heading_offset)||!re4::view_finite(rotation)||!std::isfinite(heading_offset))return;
            rotation.y+=heading_offset;
            const double sx=std::sin(double(rotation.x)),cx=std::cos(double(rotation.x)),sy=std::sin(double(rotation.y)),cy=std::cos(double(rotation.y)),sz=std::sin(double(rotation.z)),cz=std::cos(double(rotation.z));
            GrenadeExplosionContext environment{world,{float(300*(sy*sx*cz-cx*sz)),float(300*(sy*sx*sz+cx*cz)),float(300*cy*sx)}};
            re4::GrenadeExplosion burst;
            grenade_spot_cache.valid=re4::predict_grenade_explosion({origin,velocity,dt,item==0x13?0u:item==0x16?1u:2u},
                {&environment,grenade_spot_sweep,grenade_spot_relocate},burst,96)
                &&burst.outcome==re4::GrenadeTermination::Burst&&grenade_spot_project_floor(world,burst,grenade_spot_cache.landing);
        }
        if(!grenade_spot_cache.valid)return;
        // A center visibility query prevents a late-HUD marker through a wall.
        re4::ViewMatrix3x4 view;std::memcpy(&view,state+0xD8,sizeof(view));
        re4::ViewVec3 eye,end=grenade_spot_cache.landing.position,point,normal;
        const auto n=grenade_spot_cache.landing.normal;
        end={end.x+n.x*2,end.y+n.y*2,end.z+n.z*2};
        if(!re4::view_inverse_point(view,{},eye))return;
        const auto hit=native_grenade_segment(world,&eye,&end,&point,&normal,0,0x4000);
        if((hit&(1u<<24))&&(!re4::view_finite(point)||std::hypot(double(point.x)-end.x,double(point.y)-end.y,double(point.z)-end.z)>30))return;
    }
    if(grenade_spot_allowed(actor))publish_aim_point(grenade_spot_point,actor,reinterpret_cast<void*>(weapon),
        grenade_spot_cache.landing.position,view_frame.load(std::memory_order_acquire),nullptr,nullptr,room,
        grenade_spot_pack_normal(grenade_spot_cache.landing.normal));
}
void grenade_spot_submit(void* actor,std::uint64_t frame){
    pending_grenade_spot.frame.store(0,std::memory_order_release);
    re4::ViewVec3 point;std::uint64_t normal=0;std::uint32_t room=0;
    const auto* state=*reinterpret_cast<const std::uint8_t**>(main_base+0xB1AE08);
    if(grenade_spot_allowed(actor)&&read_aim_point(grenade_spot_point,actor,frame,point,nullptr,nullptr,&room,&normal)
        &&room==grenade_spot_room(state))
        publish_aim_point(pending_grenade_spot,actor,player_weapon(actor),point,frame,nullptr,nullptr,room,normal);
}
re4::GrenadeAimZoom grenade_aim_zoom;
void grenade_spot_zoom_reset(){grenade_aim_zoom.reset();}
float grenade_spot_zoom(void* actor,float native_fov){
    if(!actor||!settings.re6_response||settings.first_person||!unified_scene(actor)){
        grenade_spot_zoom_reset();return native_fov;
    }
    const auto* bytes=static_cast<const std::uint8_t*>(actor);
    // Only ordinary third-person locomotion/aim may carry the short return.
    // Scoped/special camera routes and invalid scene ownership reset elsewhere.
    if(bytes[0x115]>4&&bytes[0x115]!=6){
        grenade_spot_zoom_reset();return native_fov;
    }
    const auto* state=*reinterpret_cast<const std::uint8_t**>(main_base+0xB1AE08);
    const unsigned item=state[0x5034];
    const bool held=firearm_aim(actor)&&bytes[0x115]==6&&bytes[0x116]==1
        &&(item==0x13||item==0x16||item==0x17)
        &&*reinterpret_cast<const std::uintptr_t*>(main_base+0x1265A28)==main_base+0x31B9F0;
    return grenade_aim_zoom.update(reinterpret_cast<std::uintptr_t>(actor),svcGetSystemTick(),held,native_fov);
}
void grenade_spot_draw_scene(void* actor,std::uint64_t frame,const std::uint8_t* state){
    displayed_grenade_spot.frame.store(0,std::memory_order_release);
    re4::ViewVec3 point;std::uint64_t normal=0;std::uint32_t room=0;
    if(state&&frame>1&&grenade_spot_allowed(actor)&&read_aim_point(pending_grenade_spot,actor,frame-1,point,nullptr,nullptr,&room,&normal)
        &&room==grenade_spot_room(state)){
        re4::ViewMatrix3x4 view;re4::ViewMatrix4x4 projection;
        std::memcpy(&view,state+0xD8,sizeof(view));std::memcpy(&projection,state+0x10C,sizeof(projection));
        publish_aim_point(displayed_grenade_spot,actor,player_weapon(actor),point,frame,&view,&projection,room,normal);
    }
    pending_grenade_spot.frame.store(0,std::memory_order_release);
}
void draw_grenade_spot_hud(){
    auto* actor=*reinterpret_cast<void**>(main_base+re4::profile::player_slot);
    const auto* state=*reinterpret_cast<const std::uint8_t**>(main_base+0xB1AE08);
    if(!native_hud_ready()||!grenade_spot_allowed(actor)||!state||(*reinterpret_cast<const unsigned*>(state+0x7C)&(1u<<13)))return;
    re4::ViewVec3 point;re4::ViewMatrix3x4 view;re4::ViewMatrix4x4 projection;std::uint64_t normal=0;std::uint32_t room=0;
    if(read_aim_point(displayed_grenade_spot,actor,view_frame.load(std::memory_order_acquire),point,&view,&projection,&room,&normal)
        &&room==grenade_spot_room(state))
        re4::draw_grenade_landing_spot(native_hud_backend(),view,projection,{point,grenade_spot_unpack_normal(normal),0,0});
}
