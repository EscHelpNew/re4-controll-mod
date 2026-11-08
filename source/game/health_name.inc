#pragma once
// Invoked by the shared290F00 sprite hook, never installed as another hook.
struct HealthNameOwner {
    CharacterIdentity identity{};re4::CharacterAppearance appearance=re4::CharacterAppearance::Leon;
    std::uint64_t token=0;
    bool operator==(const HealthNameOwner& b)const{return identity==b.identity&&appearance==b.appearance&&token==b.token;}
};
struct HealthNameCapture {
    HealthNameOwner owner{};re4::HealthNameLine line{};std::uint64_t frame=0;
    re4::HealthNameWord word{};re4::NativeInventoryGlyph style{};
    std::array<float,4> bounds{};
    std::array<std::array<std::int8_t,2>,24> word_metrics{};
    std::array<std::uint8_t,0x100> font{};
    std::array<std::uint8_t,0x58> manager{};
    std::array<std::array<std::int8_t,2>,24> metrics{};
} health_name_capture;
void health_name_clear(){health_name_capture={};}
bool health_name_owner(HealthNameOwner& out){
    const auto actor=modern_move_slot(re4::profile::player_slot);
    CharacterIdentity identity;
    if(!character_identity(reinterpret_cast<void*>(actor),identity)
        ||character_render_lock.test_and_set(std::memory_order_acquire))return false;
    bool found=false;HealthNameOwner value;
    for(const auto& b:character_banks){
        const auto token=b.token.load(std::memory_order_acquire);
        if(b.phase.load(std::memory_order_acquire)!=CharacterPhase::Active||!token||b.fault.load(std::memory_order_acquire)
            ||b.drawn.load(std::memory_order_acquire)!=token||!(b.identity==identity)
            ||!re4::valid_character_appearance(b.appearance)||b.appearance==re4::CharacterAppearance::Leon)continue;
        if(found){found=false;break;}
        value={identity,b.appearance,token};found=true;
    }
    character_render_lock.clear(std::memory_order_release);
    if(found)out=value;
    return found;
}
bool health_name_visible(std::uintptr_t& state){
    std::uint32_t flags=0,frame_flags=0,groups=0;std::uint8_t gate=0;std::uintptr_t manager=0;
    return main_base&&native_hud_ready()&&inventory_damage_glyph_native.draw
        &&modern_move_read(main_base+0xB1AE08,state)&&state
        &&modern_move_read(state+0x7C,flags)&&!(flags&((1u<<13)|(1u<<16)))
        &&modern_move_read(state+0x78,frame_flags)&&modern_move_read(state+0x5092,gate)
        &&(!(frame_flags&(1u<<20))||(gate&4))
        &&modern_move_read(main_base+0xB25D98,manager)&&manager
        &&modern_move_read(manager+0x30,groups)&&!(groups&0x40000000u);
}
bool health_name_alphabet(re4::HealthNameAlphabet& out){
    const auto manager=main_base+0x124FFB0;
    std::uintptr_t state=0,core=0,source=0;std::uint32_t first=0,next=0,direct=0;
    std::int32_t language=0;std::uint8_t special=0;
    if(!modern_move_read(main_base+0xB1AE08,state)||!state||!modern_move_read(state+0x60,core)||!core
        ||!modern_move_read(core+0x54,first)||!modern_move_read(core+0x58,next)
        ||!first||next<=first||next-first<16||next-first>8*1024*1024||core>UINTPTR_MAX-next
        ||!modern_move_read(manager+0x20,source)||source!=core+first||!inventory_damage_region(source,next-first)
        ||!modern_move_read(manager+0x44,direct)||!modern_move_read(manager,language)||language<0||language>7
        ||!modern_move_read(manager+0x53,special))return false;
    const auto end=core+next;auto bank=source;
    if(!direct){
        const unsigned index=!special&&language>1?2*unsigned(language)-1:unsigned(language);
        std::uint32_t offset=0;
        if(4+4*index>end-source-4||!modern_move_read(source+4+4*index,offset)||offset<8||offset>end-source-8)return false;
        bank=source+offset;
    }
    std::uint32_t count=0;
    if(!modern_move_read(bank+4,count)||count<83||count>4096||8+std::size_t(count)*8>end-bank)return false;
    re4::HealthNameAlphabet value;
    for(unsigned word=0;word<std::size(re4::health_name_calibration_ids);++word){
        std::uint64_t offset=0;
        if(!modern_move_read(bank+8+re4::health_name_calibration_ids[word]*8,offset)
            ||offset<8+std::uint64_t(count)*8||offset>end-bank-2)return false;
        std::array<std::uint16_t,69> codes{};const auto words=std::min<std::size_t>(codes.size(),(end-bank-offset)/2);
        if(!inventory_damage_region(bank+offset,words*2))return false;
        std::memcpy(codes.data(),reinterpret_cast<const void*>(bank+offset),words*2);
        unsigned length=0;while(length<words&&codes[length]!=1)++length;if(length==words)return false;
        re4::NativeWeaponPopupName name;
        if(!re4::native_weapon_popup_name(codes.data(),length+1,name))return false;
        const auto* expected=re4::health_name_calibration_words[word];unsigned expected_length=0;
        while(expected[expected_length])++expected_length;
        if(name.count!=expected_length)return false;
        for(unsigned i=0;i<name.count;++i)if(!value.add(expected[i],name.glyphs[i]))return false;
    }
    out=value;return true;
}
bool health_name_sprite(void* pointer){
    ammo_counter_gauge_observe(pointer);
    const auto address=reinterpret_cast<std::uintptr_t>(pointer);
    std::array<std::uint8_t,4> tag{};
    if(!address||!modern_move_read(address,tag)||tag[2]!=0x21||tag[3]<0x40||tag[3]>0x45)return false;
    health_name_clear();std::uintptr_t state=0;std::uint8_t character=0;
    constexpr std::uint8_t plates[]={0x40,0x41,0x42,0x45,0x43,0x44};
    HealthNameCapture value;
    float gauge_alpha=0;
    if(tag[0]==0xFF||(tag[0]&9)!=9||!health_name_visible(state)||!ammo_counter_gauge_visible(gauge_alpha)
        ||!modern_move_read(state+0x503C,character)||character>=std::size(plates)||tag[3]!=plates[character]
        ||!health_name_owner(value.owner))return false;
    std::array<std::uint8_t,0x170> sprite{};
    if(!modern_move_read(address,sprite)||sprite[0x88]==0xFF||sprite[0x9C]!=0x16||sprite[0x9E])return false;
    re4::ViewMatrix3x4 matrix,hud_view;re4::ViewMatrix4x4 projection;
    float corners[4][3]{},rgba[4]{};std::uintptr_t hud_view_address=0;
    std::memcpy(&matrix,sprite.data()+8,sizeof(matrix));std::memcpy(corners,sprite.data()+0xBC,sizeof(corners));
    std::memcpy(rgba,sprite.data()+0x104,sizeof(rgba));
    // FCC0 installs state+10C;290FE8..290FF8 multiplies the native HUD
    // view by sprite+8. Keep that exact projection, including off-centre P.
    if(!modern_move_read(state+0x10C,projection)||!modern_move_read(main_base+0xB275B8,hud_view_address)
        ||!hud_view_address||!modern_move_read(hud_view_address,hud_view))return false;
    for(float c:rgba)if(!std::isfinite(c)||c<0||c>255)return false;
    if(rgba[3]<1)return false;
    rgba[3]=std::min(rgba[3],gauge_alpha);
    re4::WeaponPopupViewport viewport;
    if(!weapon_popup_viewport_sample(viewport))return false;
    float left=512,top=448,right=0,bottom=0;
    for(const auto& corner:corners){
        re4::ViewVec3 point;re4::ViewHudPoint screen;
        if(!re4::view_transform_point(matrix,{corner[0],corner[1],corner[2]},point)
            ||!re4::project_view_point(hud_view,projection,point,screen))return false;
        // Native sprites inherit C*P*V*M. Capture their final screen bounds
        // WITH C; the late glyph inverse must cancel only its own current C.
        // Inverting C on the uncorrected anchor alone raised the old labels.
        const float x=float((viewport.sx*(2*double(screen.x)/512-1)+viewport.tx+1)*256);
        const float y=float((1-(viewport.sy*(1-2*double(screen.y)/448)+viewport.ty))*224);
        if(!std::isfinite(x)||!std::isfinite(y))return false;
        left=std::min(left,x);top=std::min(top,y);right=std::max(right,x);bottom=std::max(bottom,y);
    }
    const auto font=main_base+0x124FAB0;
    if(!modern_move_read(font,value.font)||!(value.font[0]&1)
        ||!modern_move_read(main_base+0x124FFB0,value.manager))return false;
    std::uintptr_t table=0;std::uint16_t atlas_width=0,atlas_height=0;
    std::memcpy(&table,value.font.data()+0xF0,8);std::memcpy(&atlas_width,value.font.data()+0xF8,2);std::memcpy(&atlas_height,value.font.data()+0xFA,2);
    const unsigned cw=value.font[0xFC],ch=value.font[0xFD];
    if(!table||!cw||!ch||!atlas_width||!atlas_height||atlas_width%cw||atlas_height%ch)return false;
    const unsigned cells=unsigned(atlas_width/cw)*unsigned(atlas_height/ch);if(!cells||cells>4096)return false;
    re4::HealthNameAlphabet alphabet;
    const bool russian=settings.language==re4::Language::Russian;
    if(russian&&!health_name_alphabet(alphabet))alphabet={};
    value.style.font=font;value.style.visibility_override=1;
    value.style.rgba=(unsigned(rgba[0])<<24)|(unsigned(rgba[1])<<16)|(unsigned(rgba[2])<<8)|unsigned(rgba[3]);
    value.bounds={left,top,right,bottom};bool ready=false;
    for(unsigned attempt=0;attempt<(russian?2u:1u)&&!ready;++attempt){
        value.word_metrics={};
        if(!re4::health_name_word(value.owner.appearance,russian&&!attempt,alphabet,value.word))return false;
        bool valid=true;
        for(unsigned i=0;i<value.word.count;++i)if(value.word.glyphs[i]&&(value.word.glyphs[i]>=cells
            ||!modern_move_read(table+value.word.glyphs[i]*2,value.word_metrics[i]))){valid=false;break;}
        ready=valid&&re4::health_name_layout(value.word,value.word_metrics,cw,left,top,right,bottom,
            value.style,viewport,value.line);
    }
    if(!ready)return false;
    for(unsigned i=0;i<value.line.count;++i)if(!modern_move_read(table+value.line.glyphs[i].glyph*2,value.metrics[i]))return false;
    HealthNameOwner after;std::array<std::uint8_t,0x100> after_font{};std::array<std::uint8_t,0x58> after_manager{};
    if(!health_name_owner(after)||!(after==value.owner)||!modern_move_read(font,after_font)||after_font!=value.font
        ||!modern_move_read(main_base+0x124FFB0,after_manager)||after_manager!=value.manager)return false;
    value.frame=view_frame.load(std::memory_order_acquire);health_name_capture=value;return true;
}
void draw_health_name_hud(){
    const auto value=health_name_capture;health_name_clear();
    std::uintptr_t state=0;HealthNameOwner owner;float gauge_alpha=0;
    if(!value.frame||value.frame!=view_frame.load(std::memory_order_acquire)||!value.line.count
        ||!health_name_visible(state)||!ammo_counter_gauge_visible(gauge_alpha)||!health_name_owner(owner)||!(owner==value.owner))return;
    std::array<std::uint8_t,0x100> font{};std::array<std::uint8_t,0x58> manager{};
    if(!modern_move_read(main_base+0x124FAB0,font)||font!=value.font
        ||!modern_move_read(main_base+0x124FFB0,manager)||manager!=value.manager)return;
    std::uintptr_t table=0;std::memcpy(&table,font.data()+0xF0,8);
    for(unsigned i=0;i<value.line.count;++i){
        std::array<std::int8_t,2> metric{};
        if(!modern_move_read(table+value.line.glyphs[i].glyph*2,metric)||metric!=value.metrics[i])return;
    }
    // Sample C at the actual late draw, after all earlier HUD work. The native
    // wrapper changes P/model only; its512x384 Y conversion is applied once.
    auto style=value.style;style.rgba=(style.rgba&~255u)|std::min(style.rgba&255u,unsigned(gauge_alpha));
    re4::HealthNameLine line;re4::WeaponPopupViewport viewport;
    if(!weapon_popup_viewport_sample(viewport)
        ||!re4::health_name_layout(value.word,value.word_metrics,font[0xFC],value.bounds[0],value.bounds[1],
        value.bounds[2],value.bounds[3],style,viewport,line))return;
    for(unsigned i=0;i<line.count;++i)inventory_damage_glyph_native.draw(&line.glyphs[i]);
}
