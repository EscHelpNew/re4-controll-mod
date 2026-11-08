#include "weapon_popup_native_text.inc"
// One successful equip producer; atomic snapshots cross the native HUD thread.
struct WeaponPopupEvent {
    std::uintptr_t actor=0;
    std::uint64_t tick=0,generation=0,sequence=0;
    unsigned raw_id=0;
};
struct WeaponPopupMailbox {
    std::atomic<std::uint64_t> sequence{0},tick{0},generation{0};
    std::atomic<std::uintptr_t> actor{0};
    std::atomic<unsigned> raw_id{0};
} weapon_popup_mailbox;
std::atomic<std::uint64_t> weapon_popup_generation{1};
// ABI22's previously unused word: producer, HUD gate, committed events,
// rendered frames (four bytes). Both native threads publish atomically.
void weapon_popup_trace(unsigned producer,unsigned draw,bool commit,bool rendered){
    auto* target=&control.graphics_reserved;
    auto previous=__atomic_load_n(target,__ATOMIC_RELAXED);
    for(;;){
        auto value=previous;
        if(producer)value=(value&~255u)|(producer&255u);
        if(draw)value=(value&~0xFF00u)|((draw&255u)<<8);
        if(commit&&((value>>16)&255)<255)value+=1u<<16;
        if(rendered&&(value>>24)<255)value+=1u<<24;
        if(__atomic_compare_exchange_n(target,&previous,value,true,__ATOMIC_RELAXED,__ATOMIC_RELAXED))return;
    }
}
void weapon_popup_clear(){weapon_popup_generation.fetch_add(1,std::memory_order_acq_rel);}
void weapon_popup_commit(void* actor,unsigned raw_id){
    if(!settings.dpad_weapon_switching||!actor||raw_id>0x10F)return;
    auto& m=weapon_popup_mailbox;
    const auto generation=weapon_popup_generation.load(std::memory_order_acquire);
    const auto sequence=m.sequence.fetch_add(1,std::memory_order_acq_rel);
    m.actor.store(reinterpret_cast<std::uintptr_t>(actor),std::memory_order_relaxed);
    m.raw_id.store(raw_id,std::memory_order_relaxed);
    m.tick.store(svcGetSystemTick(),std::memory_order_relaxed);
    m.generation.store(generation,std::memory_order_relaxed);
    m.sequence.store(sequence+2,std::memory_order_release);
    weapon_popup_trace(3,0,true);
}
bool weapon_popup_read(WeaponPopupEvent& event){
    const auto& m=weapon_popup_mailbox;
    const auto sequence=m.sequence.load(std::memory_order_acquire);
    if(!sequence||(sequence&1))return false;
    WeaponPopupEvent value;
    value.actor=m.actor.load(std::memory_order_relaxed);
    value.raw_id=m.raw_id.load(std::memory_order_relaxed);
    value.tick=m.tick.load(std::memory_order_relaxed);
    value.generation=m.generation.load(std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_acquire);
    if(sequence!=m.sequence.load(std::memory_order_acquire)||!value.actor
        ||value.generation!=weapon_popup_generation.load(std::memory_order_acquire))return false;
    value.sequence=sequence;event=value;return true;
}
// ABI23 observes CPU cached GPU state at the ordered text-draw boundary;
// it does not read raster output or change any native render state.
enum class WeaponPopupCaptureStatus : unsigned {
    None=0,Valid=1,BadContext=2,BadViewport=3,BadScissor=4,StaleEvent=5,BadViewportAndScissor=6
};
struct WeaponPopupCaptureNative {
    void* (*command)(std::uint64_t)=nullptr;
    void* (*context)(unsigned)=nullptr;
} weapon_popup_capture_native;
std::atomic<std::uint64_t> weapon_popup_capture_frame{~std::uint64_t(0)};
void weapon_popup_capture_publish(WeaponPopupCaptureStatus status,const float (&viewport)[4],
        const std::int32_t (&scissor)[4],std::uint64_t tick,std::uint64_t event){
    auto* sequence=&control.popup_capture_sequence;
    auto before=__atomic_load_n(sequence,__ATOMIC_ACQUIRE);
    // Usually one native worker writes. If a different context overlaps it,
    // never block the renderer or allow two writers to cross the snapshot.
    if((before&1)||!__atomic_compare_exchange_n(sequence,&before,before+1,false,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED))return;
    std::atomic_thread_fence(std::memory_order_release);
    __atomic_store_n(&control.popup_capture_status,unsigned(status),__ATOMIC_RELAXED);
    for(unsigned i=0;i<4;++i){
        __atomic_store(&control.popup_viewport[i],&viewport[i],__ATOMIC_RELAXED);
        __atomic_store_n(&control.popup_scissor[i],scissor[i],__ATOMIC_RELAXED);
    }
    __atomic_store_n(&control.popup_tick,tick,__ATOMIC_RELAXED);
    __atomic_store_n(&control.popup_event,event,__ATOMIC_RELAXED);
    __atomic_store_n(sequence,before+2,__ATOMIC_RELEASE);
}
void weapon_popup_capture_command(void* command){
    const auto enqueue=character_value<std::uint64_t>(command,0x10);
    const auto event=character_value<std::uint64_t>(command,0x18);
    const auto generation=character_value<std::uint64_t>(command,0x20);
    const auto start=character_value<std::uint64_t>(command,0x28);
    float viewport[4]{};std::int32_t scissor[4]{};
    auto status=WeaponPopupCaptureStatus::BadContext;
    const auto context_id=character_value<std::uint16_t>(command,0xA);
    auto* software=weapon_popup_capture_native.context?weapon_popup_capture_native.context(context_id):nullptr;
    unsigned index=~0u;std::uintptr_t graphics=0;
    if(software&&modern_move_read(reinterpret_cast<std::uintptr_t>(software),index)&&index==0
        &&modern_move_read(main_base+0x1180D40+0x140,graphics)&&graphics&&graphics<=~std::uintptr_t(0)-0xFB0){
        // Native271138 passes gfxF70 to647000: descriptor+10 is the
        // float4 viewport; descriptor+30 is the int4 scissor (count1/index0).
        bool vp=true,sc=true;
        for(unsigned i=0;i<4;++i){
            vp=modern_move_read(graphics+0xF80+i*4,viewport[i])&&vp;
            sc=modern_move_read(graphics+0xFA0+i*4,scissor[i])&&sc;
        }
        // Native64E084..B0 installs an unbounded integer rect when scissor
        // is disabled. Preserve that exact sentinel for diagnostic readers.
        const bool disabled=re4::native_scissor_disabled(scissor);
        for(unsigned i=0;i<4;++i){
            vp=vp&&std::isfinite(viewport[i])&&std::fabs(viewport[i])<=65536;
            sc=sc&&(disabled||(scissor[i]>=-65536&&scissor[i]<=65536));
        }
        if(!vp)for(auto& value:viewport)value=0;
        if(!sc)for(auto& value:scissor)value=0;
        status=!vp&&!sc?WeaponPopupCaptureStatus::BadViewportAndScissor
            :!vp?WeaponPopupCaptureStatus::BadViewport:!sc?WeaponPopupCaptureStatus::BadScissor:WeaponPopupCaptureStatus::Valid;
    }
    const auto now=svcGetSystemTick();
    if(status==WeaponPopupCaptureStatus::Valid){
        WeaponPopupEvent current;
        if(now<enqueue||!weapon_popup_read(current)||current.sequence!=event||current.generation!=generation||current.tick!=start)
            status=WeaponPopupCaptureStatus::StaleEvent;
    }
    weapon_popup_capture_publish(status,viewport,scissor,now,event);
}
void weapon_popup_capture_queue(const WeaponPopupEvent& event,std::uint64_t now){
    if(!weapon_popup_capture_native.command||!weapon_popup_capture_native.context)return;
    const auto frame=view_frame.load(std::memory_order_relaxed);
    auto previous=weapon_popup_capture_frame.load(std::memory_order_relaxed);
    if(previous==frame||!weapon_popup_capture_frame.compare_exchange_strong(previous,frame,std::memory_order_relaxed))return;
    auto* command=weapon_popup_capture_native.command(0x30);if(!command)return;
    // Preserve the allocator's size/context header at8/A. Payload is copied
    // event metadata only: neither stack pointers nor actor pointers escape.
    character_store(command,0,&weapon_popup_capture_command);
    character_store(command,0x10,now);character_store(command,0x18,event.sequence);
    character_store(command,0x20,event.generation);character_store(command,0x28,event.tick);
}
void weapon_popup_validate_scene(void* actor){
    WeaponPopupEvent event;
    if(weapon_popup_read(event)&&(!settings.dpad_weapon_switching
        ||event.actor!=reinterpret_cast<std::uintptr_t>(actor)||!view_scene(actor))){weapon_popup_trace(0,3);weapon_popup_clear();}
}
bool weapon_popup_viewport_sample(re4::WeaponPopupViewport& out){
    constexpr auto maximum=std::numeric_limits<std::uintptr_t>::max();
    constexpr std::uintptr_t slot=0x1184150,offset=0x440180;
    if(!main_base||main_base>maximum-slot-sizeof(std::uintptr_t))return false;
    std::uintptr_t software=0,after=0;unsigned id=~0u,id_after=~0u;
    if(!modern_move_read(main_base+slot,software)||software<0x1000||(software&7)
        ||software>maximum-offset-16*sizeof(float)||!modern_move_read(software,id)||id)return false;
    std::array<float,16> first{},second{};
    if(!modern_move_read(software+offset,first))return false;
    std::atomic_signal_fence(std::memory_order_seq_cst);
    if(!modern_move_read(software+offset,second)||std::memcmp(first.data(),second.data(),sizeof(first))
        ||!modern_move_read(software,id_after)||id_after||!modern_move_read(main_base+slot,after)||after!=software)return false;
    // HUD prepare changes P/model but preserves this native clip correction.
    // This identity-checked cache sample is not an ordered GPU observation.
    return re4::WeaponPopupViewport::correction(second,out);
}
re4::WeaponPopupViewport weapon_popup_viewport(){
    re4::WeaponPopupViewport result;
    weapon_popup_viewport_sample(result);return result;
}
void draw_weapon_popup_hud(){
    WeaponPopupEvent event;
    if(!native_hud_ready()){weapon_popup_trace(0,2);return;}
    if(!weapon_popup_read(event))return; // Keep the gate that invalidated the last event.
    const auto options=view_settings_snapshot.load(std::memory_order_acquire);
    auto* actor=*reinterpret_cast<void**>(main_base+re4::profile::player_slot);
    const auto* state=*reinterpret_cast<const std::uint8_t**>(main_base+0xB1AE08);
    if(!(options&1024)||reinterpret_cast<std::uintptr_t>(actor)!=event.actor||!view_scene(actor)
        ||!state||(*reinterpret_cast<const std::uint32_t*>(state+0x7C)&(1u<<13))){weapon_popup_trace(0,3);weapon_popup_clear();return;}
    const auto now=svcGetSystemTick();
    const float opacity=re4::weapon_popup_opacity(event.tick,now);
    if(opacity<=0){weapon_popup_trace(0,now==event.tick?4:5);return;}
    const auto viewport=weapon_popup_viewport();re4::NativeWeaponPopupLine line;
    if(!weapon_popup_native_line(event.raw_id,viewport,opacity,line)){weapon_popup_trace(0,7);return;}
    const float left=line.left,width=line.width,top=line.top;
    const auto elapsed=now-event.tick;
    // The cloud gathers from a slightly wider shape and disperses outward at
    // the end, with the exact same fade envelope as the inscription.
    const float spread=1+(elapsed<9'600'000?.12f:.18f)*(1-opacity);
    re4::WeaponPopupViewportBackend popup_backend{native_hud_backend(),viewport};
    const auto backend=popup_backend.backend();
    const bool mist=re4::draw_weapon_mist(backend,{left+width*.5f,top},width,opacity,spread);
    for(unsigned i=0;i<line.count;++i)inventory_damage_glyph_native.draw(&line.glyphs[i]);
    const bool text=line.count!=0;
    weapon_popup_trace(0,mist&&text?8:7,false,mist&&text);
    if(text)weapon_popup_capture_queue(event,now);
}
void draw_inventory_damage_hud(){
    inventory_damage_draw_native(view_settings_snapshot.load(std::memory_order_acquire)&256);
}
