// Rotation owns only the native case body. Cosmetic rendering remains limited
// to the separately authenticated Leon identity in character_menu_identity.
bool character_menu_rotation_identity(void* actor,CharacterIdentity& out){
    if(character_menu_identity(actor,out))return true;
    const auto p=reinterpret_cast<std::uintptr_t>(actor),pool=main_base+0xAFF4A8;
    if(!main_base||!character_renderer_ready.load(std::memory_order_acquire)
        ||character_teardown.load(std::memory_order_acquire))return false;
    std::uintptr_t base=0,preview=0,vptr=0;unsigned count=0,stride=0,flags=0;
    if(!character_read(pool+8,base)||!base||!character_read(main_base+0x1272140,preview)||p!=preview||p!=base
        ||!character_read(pool+0x10,count)||!count||count>161||!character_read(pool+0x14,stride)||stride!=0x3C8
        ||!character_region(base,std::size_t(count)*stride)||!character_read(p,vptr)||vptr!=main_base+0xAFF538
        ||!character_read(p+8,flags)||(flags&0x201)!=1)return false;
    CharacterIdentity value;value.actor=p;value.state=modern_move_slot(0xB1AE08);
    value.context=character_menu_generation.load(std::memory_order_acquire);
    value.renderer=character_renderer_generation.load(std::memory_order_acquire);
    std::uint8_t native=255,costume=255,type=255,bones=0;
    if(!value.state||!character_region(value.state,0x50A0)
        ||!character_read(value.state+0x503C,native)||native!=2
        ||!character_read(value.state+0x503D,costume)||costume>3)return false;
    // Native dispatch4C7070 selects4C87D0 for actual Ada (type2). It initializes
    // this same case body from state70 BIN0. Factory494BF4 selects pl0b (81),
    // pl0c (100, costume1) or pl16 (100, costume3);2B4810 copies BIN29 to11A.
    const unsigned expected_bones=costume==1||costume==3?100:81;
    std::uintptr_t alternate=0,remap=0,effect=0;
    if(!character_read(p+0x118,type)||type||!character_read(p+0x11A,bones)||bones!=expected_bones
        ||!character_read(p+0x180,value.chain)||!value.chain||!character_read(value.chain+0x18,value.bin)||!value.bin
        ||!character_read(value.bin+0x29,bones)||bones!=expected_bones||!character_read(p+0x108,value.bones)||!value.bones
        ||!character_read(value.state+0x70,value.asset)||!value.asset
        ||!character_read(p+0x188,alternate)||alternate||!character_read(p+0x3B8,remap)||remap
        ||!character_read(p+0x3A0,effect)||!character_effect_supported(effect,flags)
        ||!character_read(value.state+0x5020,value.room))return false;
    unsigned offset=0;
    if(!character_read(value.asset+0x10,offset)||!offset||value.bin!=value.asset+offset)return false;
    // This identity is rotation-only; include the native costume in its owner
    // stamp so same-address preview rebuilds reset elapsed-time integration.
    value.bindings=unsigned(costume)+1;
    out=value;return true;
}
struct CharacterMenuRotation {
    CharacterIdentity owner{};
    std::uint64_t tick=0,frame=0;
} character_menu_rotation;
void character_menu_rotate(void* actor){
    std::uintptr_t preview=0;
    if(!main_base||!actor||!character_read(main_base+0x1272140,preview)
        ||preview!=reinterpret_cast<std::uintptr_t>(actor))return;
    CharacterIdentity identity;
    if(!character_menu_rotation_identity(actor,identity)){character_menu_rotation={};return;}
    const auto now=svcGetSystemTick(),frame=view_frame.load(std::memory_order_acquire);
    auto& rotation=character_menu_rotation;
    if(!(rotation.owner==identity)||!rotation.tick){rotation={identity,now,frame};return;}
    if(frame==rotation.frame)return;
    const auto previous=rotation.tick;rotation.tick=now;rotation.frame=frame;
    if(now<=previous||now-previous>native_tick_hz/10)return;
    std::int32_t right_x=0;
    if(!menu_read_input(now,right_x))return;
    const float axis=re4::shape_axis(right_x/32767.f,settings.right_deadzone,1,settings.stick_curve);
    float yaw=0;
    if(!axis||!std::isfinite(axis)||!character_read(identity.actor+0xB4,yaw)
        ||!std::isfinite(yaw)||std::fabs(yaw)>1e6f)return;
    // Native4BCC60 updates this exact preview through2B7C60. Its2B8700
    // evaluation rebuilds the actor matrix from B0/B4/B8 before skin/draw.
    // Keep the angle on release; native initialization resets a new preview.
    yaw=std::remainder(yaw+axis*2.09439510239f*float(now-previous)/float(native_tick_hz),6.28318530718f);
    character_store(actor,0xB4,yaw);
}
