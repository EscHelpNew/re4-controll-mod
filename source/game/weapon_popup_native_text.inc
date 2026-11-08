// Native case names use flags20088 / AUTO font selector5 -> base font124FAB0,
// case cell18x24 (4CFC20..34), palette0 (2B17D4..DC -> B00614).
// The loaded MDT supplies ALL letters, so localized glyphs are never guessed.
struct WeaponPopupNameNative {
    std::size_t (*allocation_size)(const void*,const void*)=nullptr;
    bool (*file_size)(const char*,std::uint64_t&)=nullptr;
} weapon_popup_name_native;
struct WeaponPopupNameCache {
    std::uintptr_t base=0,source=0,allocator=0;
    std::uint64_t bytes=0;
    std::array<char,16> mount{};
} weapon_popup_name_cache;
void weapon_popup_native_reset(){weapon_popup_name_cache={};}
bool weapon_popup_alias_bound(std::uintptr_t manager,std::uintptr_t source,std::uint64_t& bytes){
    std::array<char,16> mount{};
    if(character_mount_ready.load(std::memory_order_acquire))std::memcpy(mount.data(),character_mount,sizeof(character_mount));
    std::uintptr_t allocator=0;
    if(!inventory_damage_field(main_base,0xB27538,allocator))return false;
    if(weapon_popup_name_cache.base!=main_base||weapon_popup_name_cache.source!=source
        ||weapon_popup_name_cache.allocator!=allocator||weapon_popup_name_cache.mount!=mount)weapon_popup_native_reset();
    if(!mount[0]||!std::memchr(mount.data(),0,mount.size())||!weapon_popup_name_native.allocation_size||!weapon_popup_name_native.file_size
        ||allocator!=main_base+0x1240E20||source<0x1000||(source&15)||!inventory_damage_region(source,16))return false;
    // GetSizeOf authenticates allocator capacity, not the exact file length.
    // Recheck capacity even on a cached file size (including address reuse).
    const auto capacity=weapon_popup_name_native.allocation_size(reinterpret_cast<const void*>(allocator),reinterpret_cast<const void*>(source));
    if(!capacity||capacity>8*1024*1024||source>UINTPTR_MAX-capacity||!inventory_damage_region(source,capacity))return false;
    auto exact=weapon_popup_name_cache.bytes;
    if(!exact){
        char path[64]{};const auto n=std::strlen(mount.data());constexpr char suffix[] = ":/bio4/font/trc.mdt";
        std::memcpy(path,mount.data(),n);std::memcpy(path+n,suffix,sizeof(suffix));
        if(!weapon_popup_name_native.file_size(path,exact))return false;
    }
    std::uintptr_t after=0,after_allocator=0;std::array<char,16> after_mount{};
    if(!character_mount_ready.load(std::memory_order_acquire))return false;
    std::memcpy(after_mount.data(),character_mount,sizeof(character_mount));
    if(!exact||exact>capacity||!inventory_damage_field(manager,0x30,after)||after!=source
        ||!inventory_damage_field(main_base,0xB27538,after_allocator)||after_allocator!=allocator||after_mount!=mount)return false;
    weapon_popup_name_cache={main_base,source,allocator,exact,mount};bytes=exact;return true;
}
bool weapon_popup_native_line(unsigned raw,const re4::WeaponPopupViewport& viewport,float opacity,re4::NativeWeaponPopupLine& out){
    if(raw>0x10F||!inventory_damage_glyph_native.draw||!main_base||main_base>UINTPTR_MAX-0x1250000)return false;
    const auto manager=main_base+0x124FFB0,font=main_base+0x124FAB0;
    std::uintptr_t state=0,core=0,source=0;std::uint32_t first=0,next=0,direct=0;
    std::int32_t language=0;std::uint8_t special=0;
    if(!inventory_damage_field(manager,0,language)||language<0||language>7)return false;
    const bool alias=!language&&(raw==33||raw==64);const unsigned source_at=alias?0x30:0x20,direct_at=alias?0x4C:0x44,special_at=alias?0x55:0x53;
    if(!inventory_damage_field(manager,source_at,source)||!inventory_damage_field(manager,direct_at,direct)||!inventory_damage_field(manager,special_at,special))return false;
    std::uintptr_t end=0;unsigned message=raw;
    if(alias){
        std::uint64_t bytes=0;std::uint16_t id=0;
        if(!weapon_popup_alias_bound(manager,source,bytes)||!inventory_damage_field(main_base,0x6EBDE0+(raw==64?8:0),id))return false;
        end=source+bytes;message=id;
    }else{
        if(!inventory_damage_field(main_base,0xB1AE08,state)||!inventory_damage_field(state,0x60,core)
            ||!inventory_damage_field(core,0x54,first)||!inventory_damage_field(core,0x58,next)
            ||!first||next<=first||next-first<16||next-first>8*1024*1024||core>UINTPTR_MAX-next
            ||source!=core+first||!inventory_damage_region(source,next-first))return false;
        end=core+next;
    }
    if(end-source<16)return false;
    auto bank=source;
    if(!direct){
        const unsigned index=!special&&language>1?2*unsigned(language)-1:unsigned(language);
        std::uint32_t offset=0;
        if(4+4*index>end-source-4||!inventory_damage_field(source,4+4*index,offset)||offset<8||offset>end-source-8)return false;
        bank=source+offset;
    }
    std::uint32_t count=0;std::uint64_t offset=0;
    if(!inventory_damage_field(bank,4,count)||!count||count>4096||message>=count||8+std::size_t(count)*8>end-bank
        ||!inventory_damage_field(bank,8+message*8,offset)||offset<8+std::uint64_t(count)*8||offset>end-bank-2)return false;
    std::array<std::uint16_t,69> codes{};const auto words=std::min<std::size_t>(codes.size(),(end-bank-offset)/2);
    if(!words||!inventory_damage_region(bank+offset,words*2))return false;
    std::memcpy(codes.data(),reinterpret_cast<const void*>(bank+offset),words*2);
    unsigned length=0;while(length<words&&codes[length]!=1&&(!alias||codes[length]!=14))++length;if(length==words)return false;++length;
    re4::NativeWeaponPopupName name;
    if(!(alias?re4::native_weapon_popup_alias_name(codes.data(),length,name):re4::native_weapon_popup_name(codes.data(),length,name)))return false;
    std::array<std::uint8_t,0x100> font_bytes{},after_font{};
    if(!inventory_damage_field(font,0,font_bytes)||!(font_bytes[0]&1))return false;
    std::uintptr_t table=0;std::uint16_t width=0,height=0;const unsigned cw=font_bytes[0xFC],ch=font_bytes[0xFD];
    std::memcpy(&table,font_bytes.data()+0xF0,sizeof(table));std::memcpy(&width,font_bytes.data()+0xF8,2);std::memcpy(&height,font_bytes.data()+0xFA,2);
    if(!cw||!ch||!width||!height||width%cw||height%ch)return false;
    const unsigned cells=unsigned(width/cw)*unsigned(height/ch);if(!cells||cells>4096)return false;
    std::array<std::array<std::int8_t,2>,64> metrics{};
    for(unsigned i=0;i<name.count;++i)if(name.glyphs[i]&&(name.glyphs[i]>=cells||!inventory_damage_field(table,name.glyphs[i]*2,metrics[i])))return false;
    std::uint32_t rgba=0,after_rgba=0;std::uintptr_t after=0,after_core=0;std::uint32_t after_direct=0;std::int32_t after_language=0;std::uint8_t after_special=0;
    if(!inventory_damage_field(main_base,0xB00614,rgba)||!(rgba&255)||!inventory_damage_field(manager,source_at,after)||after!=source
        ||!inventory_damage_field(manager,direct_at,after_direct)||after_direct!=direct||!inventory_damage_field(manager,0,after_language)||after_language!=language
        ||!inventory_damage_field(manager,special_at,after_special)||after_special!=special||(!alias&&(!inventory_damage_field(state,0x60,after_core)||after_core!=core))
        ||!inventory_damage_field(font,0,after_font)||after_font!=font_bytes||!inventory_damage_field(main_base,0xB00614,after_rgba)||after_rgba!=rgba)return false;
    re4::NativeInventoryGlyph style;style.font=font;style.rgba=rgba;
    // Owned HUD lifetime/scene gates already apply; do not inherit unrelated
    // text-channel suppression. Native packet byte18 explicitly supports this.
    style.visibility_override=1;
    const unsigned space=(unsigned(1u<<unsigned(language))&0xC1)?13:16;
    return re4::native_weapon_popup_layout(name,metrics,cw,space,style,viewport,opacity,out);
}
