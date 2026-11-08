// Native4A0470 owns item grants, room-fit checks and inventory rearrangement.
// At4A0970 every fresh pickup message and its channel0 darkening are configured,
// before the first yield. The old4A0948 boundary ran before that fade setter.
// Close only its active primary channel, and answer Yes for confirmation paths;
// native continuation still owns the transaction and can grant it only once.
struct PickupConfirmationNative {
    void (*classify)(unsigned,void*)=nullptr;
    unsigned (*sound)(unsigned,unsigned,const void*,unsigned,unsigned,const void*)=nullptr;
} pickup_confirmation_native;
struct PickupConfirmationPending {
    std::uintptr_t item=0,model=0,base=0;
    std::uint64_t thread=0;
    std::uint16_t id=0;
} pickup_confirmation_pending;
std::atomic_flag pickup_confirmation_lock=ATOMIC_FLAG_INIT;
bool pickup_confirmation_ready(std::uint16_t item,std::uintptr_t model,unsigned selector,
    std::uint32_t primary,std::uint32_t secondary,std::uint8_t response){
    return item<=0x10F&&model&&selector<=1&&(primary&1)&&(secondary&1)&&response==0;
}
bool pickup_confirmation_region(std::uintptr_t address,std::size_t size,bool write,
    re4::ModernMoveMemory& memory){
    if(!address||size>std::numeric_limits<std::uintptr_t>::max()-address)return false;
    MemoryInfo info{};std::uint32_t page=0;
    if(R_FAILED(svcQueryMemory(&info,&page,address))||!(info.perm&1)||(write&&!(info.perm&2)))return false;
    memory=re4::ModernMoveMemory::span(reinterpret_cast<const void*>(info.addr),info.size);
    return memory.contains(address,size);
}
HOOK_DEFINE_INLINE(PickupConfirmation){
    static void Callback(exl::hook::InlineFloatCtx* context){
        if(pickup_confirmation_lock.test_and_set(std::memory_order_acquire))return;
        // A new native pickup supersedes any rejected/full-inventory attempt.
        pickup_confirmation_pending={};
        struct Unlock {~Unlock(){pickup_confirmation_lock.clear(std::memory_order_release);}} unlock;
        const bool enabled=settings.skip_pickup_confirmation;
        if(!enabled||!context||!main_base
            ||main_base>std::numeric_limits<std::uintptr_t>::max()-0xB00648)return;
        const auto controller=main_base+0xB00648;
        const auto selector=std::uint32_t(re4::saved_x(context,27));
        const auto item=re4::saved_x(context,19);
        if(re4::saved_x(context,21)!=controller||selector>1)return;
        // The native GOT entry must still name the researched global owner.
        EnemyHealthReader reader;std::uintptr_t owner=0;
        if(!reader.field(main_base,0xB25D90,owner)||owner!=controller)return;
        re4::ModernMoveMemory im,cm;
        if(!pickup_confirmation_region(item,0xA1,false,im)
            ||!pickup_confirmation_region(controller,0x14A,true,cm))return;
        std::uintptr_t model=0;std::uint16_t id=0;
        std::uint32_t primary=0,secondary=0;std::uint8_t response=0;
        if(!im.value(item+0x80,model)||!im.value(item+0x94,id)
            ||!cm.value(controller+0x1C,primary)||!cm.value(controller+0x24,secondary)
            ||!cm.value(controller+0x149,response)
            ||!pickup_confirmation_ready(id,model,selector,primary,secondary,response))return;
        // Match only bit0 of native close-all2B0B80. Preserve every other flag,
        // all secondary channels, native input, saved registers and item state.
        primary&=~1u;secondary&=~1u;
        std::memcpy(reinterpret_cast<void*>(controller+0x1C),&primary,sizeof(primary));
        std::memcpy(reinterpret_cast<void*>(controller+0x24),&secondary,sizeof(secondary));
        if(selector){
            response=1;
            std::memcpy(reinterpret_cast<void*>(controller+0x149),&response,sizeof(response));
            u64 thread=0;
            if(pickup_confirmation_native.classify&&pickup_confirmation_native.sound
                &&R_SUCCEEDED(svcGetThreadId(&thread,CUR_THREAD_HANDLE)))
                pickup_confirmation_pending={item,model,main_base,thread,id};
        }
        // Native4A096C has just set channel0 to a five-frame black alpha0x20 fade.
        // Its following yield otherwise presents one darkened gameplay frame,
        // even though the message is closed. Match that exact fresh channel;
        // native215240 disables a channel with this same halfword-only write.
        std::uintptr_t fade=0;re4::ModernMoveMemory fm;
        std::uint32_t color=0,end=0,start=0,type=0;std::uint16_t state=0;
        if(reader.field(main_base,0xB260C8,fade)&&pickup_confirmation_region(fade,0x30,true,fm)
            &&fm.value(fade,color)&&fm.value(fade+4,end)&&fm.value(fade+8,start)
            &&fm.value(fade+0x18,state)&&fm.value(fade+0x1C,type)
            &&color==0x20000000&&end==0&&start==0x20000000&&state==1&&type==5){
            state=0;std::memcpy(reinterpret_cast<void*>(fade+0x18),&state,sizeof(state));
        }
    }
};
HOOK_DEFINE_INLINE(PickupSuccess){
    static void Callback(exl::hook::InlineFloatCtx* context){
        if(!context||pickup_confirmation_lock.test_and_set(std::memory_order_acquire))return;
        auto pending=pickup_confirmation_pending;
        const auto item=re4::saved_x(context,19);
        u64 thread=0;
        const bool same=pending.item&&item==pending.item&&pending.base==main_base
            &&R_SUCCEEDED(svcGetThreadId(&thread,CUR_THREAD_HANDLE))&&thread==pending.thread;
        if(same)pickup_confirmation_pending={}; // Consume before any external/native call.
        pickup_confirmation_lock.clear(std::memory_order_release);
        if(!same||!settings.skip_pickup_confirmation
            ||!pickup_confirmation_native.classify||!pickup_confirmation_native.sound)return;
        re4::ModernMoveMemory im;std::uintptr_t model=0;std::uint16_t id=0;
        if(!pickup_confirmation_region(item,0xA1,false,im)
            ||!im.value(item+0x80,model)||!im.value(item+0x94,id)
            ||model!=pending.model||id!=pending.id||re4::weapon_cycle_grenade(id))return;
        std::array<std::uint8_t,6> type{};pickup_confirmation_native.classify(id,type.data());
        const bool ammo=type[2]==2;
        if(type[2]!=1&&type[2]!=3&&!ammo)return;
        // Native firearm message0x111 sets bit8: its Yes handler2B3B2C plays
        // sound(bank0,id0xF). Ammunition (class2, case4A05D8) uses message0x211:
        // bit8 plays0xF, then bit9 at2B3B50 plays0x11. Closed messages never
        // reach that handler. This boundary4A0C44 is reached only after native
        // successful take, including successful native inventory rearrangement;
        // no grant logic is replaced.
        pickup_confirmation_native.sound(0,0xF,nullptr,0,0,nullptr);
        if(ammo)pickup_confirmation_native.sound(0,0x11,nullptr,0,0,nullptr);
    }
};
