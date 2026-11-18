// Resolve only native, reciprocal boss health ownership. This runs on the game
// capture path; no enemy pointers or native calls cross into the HUD renderer.
std::uintptr_t enemy_health_resolve_owner(EnemyHealthReader& reader,std::uintptr_t aimed,
        std::uintptr_t pool,unsigned count,unsigned stride){
    if(!re4::enemy_health_pool_member(pool,count,stride,aimed))return 0;
    std::uint8_t id=0,subtype=0;
    if(!reader.field(aimed,0x118,id))return 0;
    if(id!=0x31&&id!=0x38)return aimed;
    if(!main_base)return 0;
    std::uintptr_t aimed_vptr=0;
    if(!reader.field(aimed,0,aimed_vptr))return 0;
    const auto vptr=main_base+(id==0x31?0xAEA170:0xAEBC60);
    if(aimed_vptr!=vptr){
        // Loader118 is independent of module class. A known different class
        // owns its own HP; its update identity is authenticated by the provider.
        for(const auto& native_class:re4::kEnemyHealthClasses)
            if(aimed_vptr==main_base+native_class.vptr)return aimed;
        return 0;
    }
    if(!reader.field(aimed,0x119,subtype))return 0;
    const auto actor_valid=[&](std::uintptr_t actor,unsigned expected_subtype,bool visible,std::uintptr_t known_vptr=0){
        std::uintptr_t actual_vptr=known_vptr;unsigned flags=0;std::uint8_t actual_id=0,actual_subtype=0;
        return re4::enemy_health_pool_member(pool,count,stride,actor)
            &&(known_vptr||reader.field(actor,0,actual_vptr))&&actual_vptr==vptr
            &&reader.field(actor,8,flags)&&(flags&0x201)==1&&(!visible||!(flags&0x10000000))
            &&reader.field(actor,0x118,actual_id)&&actual_id==id
            &&reader.field(actor,0x119,actual_subtype)&&actual_subtype==expected_subtype;
    };
    const auto health_valid=[&](std::uintptr_t actor){
        std::int16_t current=0,maximum=0;
        return reader.field(actor,0x3C2,current)&&reader.field(actor,0x3C4,maximum)
            &&re4::enemy_health_values(current,maximum);
    };
    if(!actor_valid(aimed,subtype,true,aimed_vptr))return 0;
    const unsigned core_subtype=id==0x31?1:3;
    if(subtype==core_subtype)return health_valid(aimed)?aimed:0;
    unsigned link=0,reciprocal=0;
    if(id==0x31){
        if(subtype)return 0;
        // Native12C2A0 forwards damage to BCC;12C610 mirrors only currentHP.
        // Native1335AC pairs body[BCC]=core and core[BC4]=body.
        link=0xBCC;reciprocal=0xBC4;
    }else {
        if(subtype>4)return 0;
        // Native162CD0 records the five parts;163E28 makes upper core3's
        // defeat kill body0. Tentacles and lower-part counters are separate.
        constexpr unsigned slots[]={0xAE4,0xAEC,0xAF4,0,0xB04};
        link=0xAFC;reciprocal=slots[subtype];
    }
    std::uintptr_t owner=0,back=0,again=0;
    if(!reader.field(aimed,link,owner)||owner==aimed
        ||!actor_valid(owner,core_subtype,false)||!health_valid(owner)
        ||!reader.field(owner,reciprocal,back)||back!=aimed)return 0;
    // Bit28 excludes an actor from native ray selection, not from owning HP:
    // native12EC30 reads the linked core's health without a visibility test.
    // The aimed actor stays visible; a linked core supplies counters only.
    if(!reader.field(aimed,link,again)||again!=owner
        ||!reader.field(owner,reciprocal,again)||again!=aimed
        ||!actor_valid(aimed,subtype,true)||!actor_valid(owner,core_subtype,false))return 0;
    return owner;
}
