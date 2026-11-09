// Extended weapon upgrade: one extra Firepower level per tunable gun.
// The native record nibble is never raised: Firepower nibble6 means "exclusive
// owned" to gameplay (handgun critical, mine homing...). Ownership is a mod
// mask, persisted per native save slot in a sidecar keyed by the slot CRC.
// Native evidence: data/analysis/extended_upgrade/findings.md.
struct ExtendedUpgradeNative {
    float (*power)(unsigned,int)=nullptr;                          // 293910 (UI units)
    void* (*sprite)(void*,unsigned,unsigned)=nullptr;              // 28C8B0
    void (*price)(unsigned,unsigned,int,const float*,unsigned)=nullptr; // 4D5FE0
} extended_upgrade_native;
std::uint16_t extended_upgrade_mask=0;
struct ExtendedUpgradePending {std::uintptr_t work=0,record=0;} extended_upgrade_pending;
struct ExtendedUpgradeSnapshot {std::uintptr_t buffer=0;std::uint16_t mask=0;};
std::array<ExtendedUpgradeSnapshot,4> extended_upgrade_snapshots{};
re4::ExtendedUpgradeStore extended_upgrade_store;
bool extended_upgrade_store_loaded=false;
bool extended_upgrade_store_dirty=false;
// A failed native-load sidecar read leaves ownership unknown, not empty.
// Keep the loaded identity even if a later native save produces a new CRC.
struct ExtendedUpgradeLoadedIdentity {unsigned slot=0;std::uint32_t crc=0;bool valid=false;} extended_upgrade_loaded_identity;
bool extended_upgrade_mask_known=true;
// Selection only. Geometry and fade are read at the native sprite draw, after
// 28FEA0 has evaluated this frame's parent transforms and colour animation.
struct ExtendedUpgradeSquare {
    std::uintptr_t previous=0,last=0,record=0;
    int entry=-1;
    std::array<std::uint8_t,14> identity{};
};
ExtendedUpgradeSquare extended_upgrade_square;
constexpr const char* kExtendedUpgradePath="r4ctl:/config/re4-controls/extended-upgrade.bin";
constexpr std::uintptr_t kExtendedInventory=0x124D918;

template<class T>bool extended_read(std::uintptr_t address,T& out){return modern_move_read(address,out);}
bool extended_upgrade_preset_mode(){
    std::uintptr_t state=0;std::uint8_t flags=0;
    // state+79 bit4 selects the preset level table (Separate Ways / minigames).
    return !extended_read(main_base+0xB1AE08,state)||!state||!extended_read(state+0x79,flags)||(flags&0x10);
}
bool extended_upgrade_active(){return settings.extended_weapon_upgrade&&main_base&&!extended_upgrade_preset_mode();}
// Returns the table entry for a live, valid inventory record.
int extended_upgrade_record(std::uintptr_t record,std::uint32_t& packed){
    std::uint16_t raw=0;std::uint8_t flags=0;
    if(!record||!extended_read(record,raw)||!extended_read(record+4,flags)||!(flags&1)
        ||!extended_read(record+6,packed))return -1;
    return re4::extended_upgrade_by_raw(raw);
}
bool extended_upgrade_purchasable(std::uintptr_t record){
    std::uint32_t packed=0;const int entry=extended_upgrade_record(record,packed);
    return entry>=0&&extended_upgrade_mask_known&&extended_upgrade_active()&&re4::extended_upgrade_at_max(re4::kExtendedUpgradeWeapons[entry],packed)
        &&!re4::extended_upgrade_owned(extended_upgrade_mask,entry);
}
// Owned extra in effect for this record (mask, setting and native maximum).
int extended_upgrade_applied(std::uintptr_t record){
    std::uint32_t packed=0;const int entry=extended_upgrade_record(record,packed);
    return entry>=0&&extended_upgrade_active()&&re4::extended_upgrade_at_max(re4::kExtendedUpgradeWeapons[entry],packed)
        &&re4::extended_upgrade_owned(extended_upgrade_mask,entry)?entry:-1;
}
// Drop ownership once no owned record of that weapon remains at its native
// maximum: a sold weapon or a fresh New Game starts again from scratch.
void extended_upgrade_prune(){
    if(!extended_upgrade_mask_known||!extended_upgrade_mask||!main_base)return;
    const auto inv=main_base+kExtendedInventory;
    std::uintptr_t slots=0;std::int32_t count=0;std::uint8_t owner=0;
    if(!extended_read(inv+0x20,slots)||!extended_read(inv+0x30,count)||count<1||count>384
        ||!extended_read(inv+0x1B,owner))return;
    std::uint16_t keep=0;
    for(std::int32_t i=0;i<count;++i){
        const auto record=slots+std::uintptr_t(i)*14;std::uint32_t packed=0;std::uint8_t record_owner=0;
        const int entry=extended_upgrade_record(record,packed);
        if(entry>=0&&extended_read(record+5,record_owner)&&record_owner==owner
            &&re4::extended_upgrade_at_max(re4::kExtendedUpgradeWeapons[entry],packed))keep|=std::uint16_t(1u<<unsigned(entry));
    }
    extended_upgrade_mask&=keep;
}
bool extended_upgrade_load_store(){
    if(extended_upgrade_store_loaded)return true;
    if(!file_mounted)return false;
    nn::fs::FileHandle handle{};
    const auto opened=nn::fs::OpenFile(&handle,kExtendedUpgradePath,1);
    // Atmosphere fs_results.hpp: module2, PathNotFound description1 => 0x202.
    // Other I/O failures and corrupt data cannot authorize an empty store.
    if(opened==0x202){extended_upgrade_store={};extended_upgrade_store_loaded=true;return true;}
    if(R_FAILED(opened))return false;
    long size=0;std::uint8_t bytes[re4::kExtendedUpgradeFileBytes]{};
    re4::ExtendedUpgradeStore loaded;
    const bool valid=R_SUCCEEDED(nn::fs::GetFileSize(&size,handle))&&size==long(sizeof(bytes))
        &&R_SUCCEEDED(nn::fs::ReadFile(handle,0,bytes,sizeof(bytes)))
        &&re4::extended_upgrade_decode(bytes,sizeof(bytes),loaded);
    nn::fs::CloseFile(handle);
    if(!valid)return false;
    extended_upgrade_store=loaded;extended_upgrade_store_loaded=true;return true;
}
bool extended_upgrade_resolve_mask(){
    if(extended_upgrade_mask_known)return true;
    const auto identity=extended_upgrade_loaded_identity;
    if(!identity.valid||!extended_upgrade_load_store())return false;
    std::uint16_t mask=0;
    extended_upgrade_mask=re4::extended_upgrade_get(extended_upgrade_store,identity.slot,identity.crc,mask)?mask:0;
    extended_upgrade_mask_known=true;return true;
}
bool extended_upgrade_write_store(const re4::ExtendedUpgradeStore& candidate){
    if(!file_mounted||!extended_upgrade_store_loaded)return false;
    std::uint8_t bytes[re4::kExtendedUpgradeFileBytes];re4::extended_upgrade_encode(candidate,bytes);
    nn::fs::CreateFile(kExtendedUpgradePath,sizeof(bytes)); // Fails harmlessly when present.
    nn::fs::FileHandle handle{};
    if(R_FAILED(nn::fs::OpenFile(&handle,kExtendedUpgradePath,2)))return false;
    const auto written=nn::fs::WriteFile(handle,0,bytes,sizeof(bytes),nn::fs::WriteOption::CreateOption(nn::fs::WriteOptionFlag_Flush));
    nn::fs::CloseFile(handle);
    return R_SUCCEEDED(written);
}
// Native save menu: ctx+1D slot, ctx+4D8 slot work, work+EAF8 slot CRC32.
bool extended_upgrade_slot(std::uintptr_t ctx,unsigned& slot,std::uint32_t& crc){
    std::int8_t index=0;std::uintptr_t work=0;
    if(!extended_read(ctx+0x1D,index)||index<0||unsigned(index)>=re4::kExtendedUpgradeSlots
        ||!extended_read(ctx+0x4D8,work)||!work||!extended_read(work+0xEAF8,crc))return false;
    slot=unsigned(index);return true;
}

// 2AE9E0 "can still be tuned": keep a weapon with an owned exclusive selectable
// while its extra level is still available.
HOOK_DEFINE_TRAMPOLINE(ExtendedCanTune){
    static std::uint64_t Callback(void* shop,void* record){
        const auto result=Orig(shop,record);
        if(result&1)return result;
        extended_upgrade_prune();
        return extended_upgrade_purchasable(reinterpret_cast<std::uintptr_t>(record))?1:result;
    }
};
// 4DAF14: w0 = native "fully tuned". Row0 must reach the ordinary per-row path.
HOOK_DEFINE_INLINE(ExtendedFullyTuned){
    static void Callback(exl::hook::InlineFloatCtx* context){
        if(!context||!(re4::saved_x(context,0)&1))return;
        const auto work=re4::saved_x(context,25);std::int32_t row=-1;std::uintptr_t record=0;
        if(!extended_read(work+0x34,row)||row||!extended_read(work+0x28,record))return;
        extended_upgrade_prune();
        if(extended_upgrade_purchasable(record))re4::set_saved_x(context,0,0);
    }
};
// 4DB3E8: w23 next level, w24 chapter limit, w0 native max, TW+30 price.
HOOK_DEFINE_INLINE(ExtendedRowPrice){
    static void Callback(exl::hook::InlineFloatCtx* context){
        extended_upgrade_pending={};
        if(!context)return;
        const auto work=re4::saved_x(context,25),record=re4::saved_x(context,22);
        std::int32_t row=-1;std::uintptr_t selected=0;std::uint16_t raw=0;
        if(!extended_read(work+0x34,row)||row||!extended_read(work+0x28,selected)||selected!=record
            ||!extended_upgrade_purchasable(record)||!extended_read(record,raw))return;
        const int entry=re4::extended_upgrade_by_raw(raw);if(entry<0)return;
        const std::int32_t price=std::int32_t(re4::kExtendedUpgradeWeapons[entry].price);
        std::memcpy(reinterpret_cast<void*>(work+0x30),&price,sizeof(price));
        re4::set_saved_x(context,24,0x7F);re4::set_saved_x(context,0,0xFF);
        extended_upgrade_pending={work,record};
    }
};
// 4DBF5C: confirm-enter has written TW+38..44. Keep every native level as is.
HOOK_DEFINE_INLINE(ExtendedConfirmEnter){
    static void Callback(exl::hook::InlineFloatCtx* context){
        if(!context||!extended_upgrade_pending.work)return;
        const auto work=re4::saved_x(context,22),confirm=re4::saved_x(context,20);
        std::int32_t row=-1;std::uintptr_t record=0;
        if(work!=extended_upgrade_pending.work||!extended_read(work+0x34,row)||row
            ||!extended_read(work+0x28,record)||record!=extended_upgrade_pending.record){extended_upgrade_pending={};return;}
        for(unsigned stat=0;stat<4;++stat){
            std::int8_t current=0;if(!extended_read(confirm+0x20+stat,current)){extended_upgrade_pending={};return;}
            const std::int32_t level=current;std::memcpy(reinterpret_cast<void*>(work+0x38+stat*4),&level,sizeof(level));
        }
    }
};
// 4DC2D4: reached only after Yes, native nibble rewrite and money deduction.
HOOK_DEFINE_INLINE(ExtendedCommit){
    static void Callback(exl::hook::InlineFloatCtx* context){
        if(!context||!extended_upgrade_pending.work)return;
        std::uintptr_t work=0,record=0;std::int32_t row=-1;std::uint16_t raw=0;
        const auto pending=extended_upgrade_pending;extended_upgrade_pending={};
        if(!extended_read(re4::saved_x(context,20)+0x3F8,work)||work!=pending.work||!extended_read(work+0x34,row)||row
            ||!extended_read(work+0x28,record)||record!=pending.record||!extended_read(record,raw))return;
        const int entry=re4::extended_upgrade_by_raw(raw);
        if(entry>=0)extended_upgrade_mask|=std::uint16_t(1u<<unsigned(entry));
    }
};
// 4DB460(record,raw,show): native level squares and current value digits.
HOOK_DEFINE_TRAMPOLINE(ExtendedLevelBar){
    static void Callback(void* record,unsigned raw,unsigned show){
        extended_upgrade_square={};
        const auto address=reinterpret_cast<std::uintptr_t>(record);
        const int applied=record?extended_upgrade_applied(address):-1;
        std::uintptr_t table=0;float* value=nullptr;float before=0;std::uint32_t packed=0;
        if(applied>=0&&extended_read(main_base+0xB26760,table)&&table&&extended_read(address+6,packed)){
            // 293910 reads float[weapon][nibble]; scale it only for this native call.
            value=reinterpret_cast<float*>(table+re4::kExtendedUpgradeWeapons[applied].index*0x1C+(packed&15u)*4);
            re4::ModernMoveMemory memory;
            if(!modern_move_region(reinterpret_cast<std::uintptr_t>(value),4,true,memory))value=nullptr;
            else{before=*value;*value=before*re4::extended_upgrade_ratio(re4::kExtendedUpgradeWeapons[applied]);}
        }
        Orig(record,raw,show);
        if(value)*value=before;
        const int entry=re4::extended_upgrade_by_raw(raw);
        if(!(show&1)||entry<0||!extended_upgrade_active()||!extended_upgrade_native.sprite)return;
        auto* manager=reinterpret_cast<void*>(modern_move_slot(0xB26C90));if(!manager)return;
        const auto& w=re4::kExtendedUpgradeWeapons[entry];
        ExtendedUpgradeSquare square;
        square.previous=reinterpret_cast<std::uintptr_t>(extended_upgrade_native.sprite(manager,0x65+w.max-2,0x1C));
        square.last=reinterpret_cast<std::uintptr_t>(extended_upgrade_native.sprite(manager,0x65+w.max-1,0x1C));
        if(!square.previous||!square.last||square.previous==main_base+0x124D630||square.last==main_base+0x124D630)return;
        square.entry=entry;square.record=address;
        if(record){
            std::uint32_t selected_packed=0;
            if(extended_upgrade_record(address,selected_packed)!=entry||!extended_read(address,square.identity))return;
        }
        extended_upgrade_square=square;
    }
};
// 290F00 is the sprite callback stored at *(main+ B275C0), queued by
// 290DD0/4F4790. Drawing alongside the source inherits native group visibility,
// layer, projection, quad, texture, blend mode and this frame's exact fade.
HOOK_DEFINE_TRAMPOLINE(ExtendedSquareDraw){
    static void Callback(void* sprite){
        if(health_name_sprite(sprite))return;
        Orig(sprite);
        const auto selected=extended_upgrade_square;
        if(reinterpret_cast<std::uintptr_t>(sprite)!=selected.last||selected.entry<0
            ||!extended_upgrade_active()||!extended_upgrade_native.sprite)return;
        if(selected.record){
            std::array<std::uint8_t,14> identity{};
            if(!extended_read(selected.record,identity)||identity!=selected.identity)return;
        }
        alignas(16) std::array<std::uint8_t,0x170> copy{};
        std::uint8_t previous_flags=0;
        if(!extended_read(selected.last,copy)||copy[0]==0xFF||(copy[0]&9)!=9||copy[0x9E]!=0
            ||!extended_read(selected.previous,previous_flags)||previous_flags==0xFF||(previous_flags&9)!=9)return;
        float distance=0;
        for(unsigned offset:{0x14u,0x24u,0x34u}){
            float previous=0,last=0;
            if(!extended_read(selected.previous+offset,previous))return;
            std::memcpy(&last,copy.data()+offset,sizeof(last));
            if(!std::isfinite(previous)||!std::isfinite(last)||std::fabs(previous)>4000.f||std::fabs(last)>4000.f)return;
            const float step=last-previous;distance+=step*step;
            last+=step;std::memcpy(copy.data()+offset,&last,sizeof(last));
        }
        if(!(distance>1.f&&distance<14400.f))return;
        const bool owned=selected.record&&extended_upgrade_mask_known&&extended_upgrade_applied(selected.record)==selected.entry;
        std::uintptr_t manager=0;
        if(!extended_read(main_base+0xB26C90,manager)||!manager)return;
        const auto color=reinterpret_cast<std::uintptr_t>(extended_upgrade_native.sprite(reinterpret_cast<void*>(manager),owned?2:3,0x1C));
        std::array<std::uint8_t,4> rgba{};
        if(!color||color==main_base+0x124D630||!extended_read(color+0xFC,rgba))return;
        std::uintptr_t parent=0;std::array<float,4> tint{255,255,255,255},effective{};
        std::memcpy(&parent,copy.data()+0x68,sizeof(parent));
        std::memcpy(effective.data(),copy.data()+0x104,sizeof(effective));
        if(parent&&!extended_read(parent+0x104,tint))return;
        for(unsigned channel=0;channel<4;++channel)
            if(!std::isfinite(tint[channel])||tint[channel]<0||tint[channel]>255
                ||!std::isfinite(effective[channel])||effective[channel]<0||effective[channel]>255)return;
        // 290494..2904F0 multiplies each local channel by the parent's effective
        // channel /255, truncating. Preserve the source alpha, including its
        // local colour animation, so a fade never leaves an opaque extra slot.
        for(unsigned channel=0;channel<3;++channel)effective[channel]=float(unsigned(float(rgba[channel])*tint[channel]/255.f));
        std::memcpy(copy.data()+0xFC,rgba.data(),sizeof(rgba));
        std::memcpy(copy.data()+0x104,effective.data(),sizeof(effective));
        Orig(copy.data());
    }
};
// 4D4FC0(TW,shop,show): next value and price. Native hides row0 at its maximum.
HOOK_DEFINE_TRAMPOLINE(ExtendedPanel){
    static void Callback(void* work,void* shop,unsigned show){
        Orig(work,shop,show);
        std::uintptr_t record=0;std::uint16_t raw=0;std::uint32_t packed=0;
        if(!(show&1)||!work||!extended_read(reinterpret_cast<std::uintptr_t>(work)+0x28,record)
            ||!extended_upgrade_purchasable(record)||!extended_read(record,raw)||!extended_read(record+6,packed))return;
        const auto& n=extended_upgrade_native;
        auto* manager=reinterpret_cast<void*>(modern_move_slot(0xB26C90));
        const int entry=re4::extended_upgrade_by_raw(raw);
        if(entry<0||!manager||!n.sprite||!n.power||!n.price)return;
        const auto& w=re4::kExtendedUpgradeWeapons[entry];
        re4::ExtendedUpgradeDigits digits;
        if(!re4::extended_upgrade_digits(n.power(raw,int(packed&15u)+1)*re4::extended_upgrade_ratio(w),digits))return;
        auto get=[&](unsigned id){
            const auto sprite=reinterpret_cast<std::uint8_t*>(n.sprite(manager,id,0x1D));
            return sprite&&reinterpret_cast<std::uintptr_t>(sprite)!=main_base+0x124D630?sprite:nullptr;
        };
        auto set=[&](unsigned id,int frame,bool visible){
            auto* s=get(id);if(!s)return;
            if(frame>=0){s[0x9B]|=2;s[0x8A]=std::uint8_t(frame);}
            s[0]=visible?(s[0]|8):(s[0]&~8);
        };
        // Row0 sprites from 4D51A4..4D5200 and 4D5574..4D5614.
        set(0x37,-1,true);
        set(0x39,w.max+1,true);
        set(0x36,int(digits.hundreds),digits.show_hundreds);
        set(0x35,int(digits.tens),true);
        set(0x34,int(digits.ones),true);
        auto* anchor=get(0x40);if(!anchor)return;
        std::uintptr_t parent=0;float local[3]{},origin[3]{};
        if(!extended_read(reinterpret_cast<std::uintptr_t>(anchor)+0xA4,local)
            ||!extended_read(reinterpret_cast<std::uintptr_t>(anchor)+0x68,parent)||!parent
            ||!extended_read(parent+0xB0,origin))return;
        const float position[4]{local[0]+origin[0],local[1]+origin[1],local[2]+origin[2],0};
        n.price(0x80,0,int(w.price),position,1);
    }
};
// 1CCD44: s0 = native firepower for weapon index w20. Scale only the equipped
// record's weapon with an owned extra level.
HOOK_DEFINE_INLINE(ExtendedDamage){
    static void Callback(exl::hook::InlineFloatCtx* context){
        if(!context||!extended_upgrade_mask)return;
        std::uintptr_t record=0;
        if(!extended_read(main_base+kExtendedInventory+0x10,record))return;
        const int entry=extended_upgrade_applied(record);
        if(entry<0||re4::kExtendedUpgradeWeapons[entry].index!=(re4::saved_x(context,20)&0xFFFFFFFFu))return;
        re4::scale_saved_s0(context,re4::extended_upgrade_ratio(re4::kExtendedUpgradeWeapons[entry]));
    }
};
// 21E100 snapshot take / 21DAF0 restore: checkpoints roll the mask back with
// the native money and upgrade nibbles.
HOOK_DEFINE_TRAMPOLINE(ExtendedSnapshotTake){
    static std::uint64_t Callback(void* buffer,std::uint64_t a,std::uint64_t b,std::uint64_t c){
        const auto result=Orig(buffer,a,b,c);
        if(!extended_upgrade_mask_known)return result;
        extended_upgrade_prune();
        const auto key=reinterpret_cast<std::uintptr_t>(buffer);
        for(auto& s:extended_upgrade_snapshots)if(s.buffer==key){s.mask=extended_upgrade_mask;return result;}
        for(auto& s:extended_upgrade_snapshots)if(!s.buffer){s={key,extended_upgrade_mask};return result;}
        extended_upgrade_snapshots[0]={key,extended_upgrade_mask};
        return result;
    }
};
HOOK_DEFINE_TRAMPOLINE(ExtendedSnapshotRestore){
    static std::uint64_t Callback(void* buffer,std::uint64_t a,std::uint64_t b,std::uint64_t c){
        const auto result=Orig(buffer,a,b,c);
        if(!extended_upgrade_mask_known)return result;
        const auto key=reinterpret_cast<std::uintptr_t>(buffer);
        for(const auto& s:extended_upgrade_snapshots)if(s.buffer==key){extended_upgrade_mask=s.mask;break;}
        return result;
    }
};
// 25BE48: makeSaveData has written this slot's CRC; record the current mask.
HOOK_DEFINE_INLINE(ExtendedSave){
    static void Callback(exl::hook::InlineFloatCtx* context){
        unsigned slot=0;std::uint32_t crc=0;
        if(!context||!extended_upgrade_slot(re4::saved_x(context,19),slot,crc))return;
        money_on_save(slot,crc);
        if(!extended_upgrade_load_store()||!extended_upgrade_resolve_mask())return;
        extended_upgrade_prune();
        std::uint16_t previous=0;
        if(!extended_upgrade_store_dirty&&re4::extended_upgrade_get(extended_upgrade_store,slot,crc,previous)&&previous==extended_upgrade_mask)return;
        auto candidate=extended_upgrade_store;
        re4::extended_upgrade_put(candidate,slot,crc,extended_upgrade_mask);
        // Failed open/write leaves the last durable cache unchanged, so the
        // next native save retries. This is not an atomic filesystem replace.
        if(extended_upgrade_write_store(candidate)){extended_upgrade_store=candidate;extended_upgrade_store_dirty=false;}
        else extended_upgrade_store_dirty=true;
    }
};
// 25B5C0: the loaded slot has just been applied by 21DAF0.
HOOK_DEFINE_INLINE(ExtendedLoad){
    static void Callback(exl::hook::InlineFloatCtx* context){
        unsigned slot=0;std::uint32_t crc=0;
        if(!context)return;
        extended_upgrade_pending={};
        extended_upgrade_snapshots={};
        extended_upgrade_mask=0;extended_upgrade_mask_known=false;extended_upgrade_loaded_identity={};
        money_on_load(20,0); // Invalid slot metadata must also invalidate the old campaign.
        if(!extended_upgrade_slot(re4::saved_x(context,19),slot,crc))return;
        money_on_load(slot,crc);
        extended_upgrade_loaded_identity={slot,crc,true};
        extended_upgrade_resolve_mask();
    }
};
void draw_extended_upgrade_hud(){
    // The extra square is emitted by ExtendedSquareDraw at the native sprite
    // draw boundary. Retain the existing HUD entry point without a late overlay.
}
// Attache hover readout: same ratio as gameplay damage.
float extended_upgrade_display_ratio(std::uintptr_t record){
    if(!extended_upgrade_mask)return 1.f;
    const int entry=extended_upgrade_applied(record);
    return entry<0?1.f:re4::extended_upgrade_ratio(re4::kExtendedUpgradeWeapons[entry]);
}
void extended_upgrade_install(){
    extended_upgrade_native.power=reinterpret_cast<decltype(extended_upgrade_native.power)>(main_base+0x293910);
    extended_upgrade_native.sprite=reinterpret_cast<decltype(extended_upgrade_native.sprite)>(main_base+0x28C8B0);
    extended_upgrade_native.price=reinterpret_cast<decltype(extended_upgrade_native.price)>(main_base+0x4D5FE0);
    ExtendedCanTune::InstallAtOffset(0x2AE9E0);
    ExtendedFullyTuned::InstallAtOffset(0x4DAF14);
    ExtendedRowPrice::InstallAtOffset(0x4DB3E8);
    ExtendedConfirmEnter::InstallAtOffset(0x4DBF5C);
    ExtendedCommit::InstallAtOffset(0x4DC2D4);
    ExtendedLevelBar::InstallAtOffset(0x4DB460);
    ExtendedSquareDraw::InstallAtOffset(0x290F00);
    ExtendedPanel::InstallAtOffset(0x4D4FC0);
    ExtendedDamage::InstallAtOffset(0x1CCD44);
    ExtendedSnapshotTake::InstallAtOffset(0x21E100);
    ExtendedSnapshotRestore::InstallAtOffset(0x21DAF0);
    ExtendedSave::InstallAtOffset(0x25BE48);
    ExtendedLoad::InstallAtOffset(0x25B5C0);
}
