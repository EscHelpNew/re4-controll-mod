// Native2B28E0 supplies its own512x384 projection and identity transform.
struct InventoryDamageGlyphNative {
    void (*draw)(const re4::NativeInventoryGlyph*)=nullptr;
    bool (*prefix)(const char*,float,float,float,unsigned)=nullptr;
} inventory_damage_glyph_native;
bool inventory_damage_native_prefix(std::uintptr_t font,bool russian,re4::NativeInventoryPrefix& out){
    std::uintptr_t current_font=0;
    if(!inventory_damage_field(main_base+0xB00648,0x38,current_font)||current_font!=font)return false;
    if(!russian){out=re4::native_inventory_english_prefix();return true;}
    const auto manager=main_base+0x124FFB0;
    std::uintptr_t state=0,core=0,source=0;
    std::uint32_t first=0,next=0,direct=0;std::int32_t language=0;std::uint8_t special=0;
    if(!inventory_damage_field(main_base,0xB1AE08,state)||!inventory_damage_field(state,0x60,core)
        ||!inventory_damage_field(core,0x54,first)||!inventory_damage_field(core,0x58,next)
        ||!first||next<=first||next-first<16||next-first>8*1024*1024||core>UINTPTR_MAX-next
        ||!inventory_damage_field(manager,0x20,source)||source!=core+first
        ||!inventory_damage_region(source,next-first)||!inventory_damage_field(manager,0x44,direct)
        ||!inventory_damage_field(manager,0,language)||language<0||language>7
        ||!inventory_damage_field(manager,0x53,special))return false;
    const auto end=core+next;auto bank=source;
    if(!direct){
        const unsigned index=!special&&language>1?2*unsigned(language)-1:unsigned(language);
        std::uint32_t offset=0;
        if(4+4*index>end-source-4||!inventory_damage_field(source,4+4*index,offset)
            ||offset<8||offset>end-source-8)return false;
        bank=source+offset;
    }
    std::uint32_t count=0;
    if(!inventory_damage_field(bank,4,count)||count<47||count>4096
        ||8+std::size_t(count)*8>end-bank)return false;
    std::array<re4::NativeInventoryName,4> names{};constexpr unsigned ids[]={35,46,16,3};
    for(unsigned word=0;word<4;++word){
        std::uint64_t offset=0;
        if(!inventory_damage_field(bank,8+ids[word]*8,offset)
            ||offset<8+std::uint64_t(count)*8||offset>end-bank-2)return false;
        std::uint16_t codes[17]{};unsigned length=0;
        for(;length<17;++length){
            if(length*2>end-bank-offset-2||!inventory_damage_field(bank+offset,length*2,codes[length]))return false;
            if(codes[length]==1){++length;break;}
        }
        if(!re4::native_inventory_name(codes,length,names[word]))return false;
    }
    std::uintptr_t after=0,after_font=0;std::uint32_t after_direct=0;std::int32_t after_language=0;std::uint8_t after_special=0;
    if(!inventory_damage_field(manager,0x20,after)||after!=source||!inventory_damage_field(manager,0x44,after_direct)||after_direct!=direct
        ||!inventory_damage_field(manager,0,after_language)||after_language!=language
        ||!inventory_damage_field(manager,0x53,after_special)||after_special!=special
        ||!inventory_damage_field(main_base+0xB00648,0x38,after_font)||after_font!=font)return false;
    return re4::native_inventory_russian_prefix(names,out);
}
bool inventory_damage_draw_native(bool russian=false){
    InventoryDamageReadout value;
    if(!inventory_damage_glyph_native.draw||!inventory_damage_snapshot(value))return false;
    const auto engine=main_base+0xB00648;
    std::uintptr_t first=0,last=0;
    if(!inventory_damage_field(engine,0xF8,first)||!inventory_damage_field(engine,0x100,last)
        ||!first||last<=first||(last-first)%sizeof(re4::NativeInventoryGlyph)
        ||last-first>0x4000||!inventory_damage_region(first,last-first))return false;
    re4::NativeInventoryGlyph style;
    if(!inventory_damage_field(first,0,style)||!style.font||!style.height)return false;
    unsigned bottom=0;
    for(auto address=first;address<last;address+=sizeof(re4::NativeInventoryGlyph)){
        re4::NativeInventoryGlyph packet;
        if(!inventory_damage_field(address,0,packet)||packet.font!=style.font||!packet.height
            ||packet.x>=512||packet.y>=384||unsigned(packet.y)+packet.height>384)return false;
        const unsigned packet_bottom=unsigned(packet.y)+packet.height;
        if(packet_bottom>bottom)bottom=packet_bottom;
    }
    std::uint8_t loaded=0,cell_width=0,cell_height=0,name_width=0;
    std::uint16_t atlas_width=0,atlas_height=0;std::uintptr_t table=0;
    if(!inventory_damage_region(style.font,0x100)||!inventory_damage_field(style.font,0,loaded)||!(loaded&1)
        ||!inventory_damage_field(style.font,0xF0,table)||!inventory_damage_field(style.font,0xF8,atlas_width)
        ||!inventory_damage_field(style.font,0xFA,atlas_height)||!inventory_damage_field(style.font,0xFC,cell_width)
        ||!inventory_damage_field(style.font,0xFD,cell_height)||!cell_width||!cell_height
        ||atlas_width%cell_width||atlas_height%cell_height
        ||unsigned(atlas_width/cell_width)*unsigned(atlas_height/cell_height)<22
        ||!inventory_damage_field(engine,0x30,name_width))return false;
    std::array<std::array<std::int8_t,2>,22> metrics{};
    if(!inventory_damage_field(table,0,metrics))return false;
    re4::NativeInventoryPrefix prefix;
    if(inventory_damage_native_prefix(style.font,russian,prefix)){
        std::array<std::array<std::int8_t,2>,7> prefix_metrics{};
        const unsigned cells=unsigned(atlas_width/cell_width)*unsigned(atlas_height/cell_height);
        bool valid=true;
        for(unsigned i=0;i<prefix.count;++i)if(prefix.glyphs[i]>=cells
            ||!inventory_damage_field(table,prefix.glyphs[i]*2,prefix_metrics[i])){valid=false;break;}
        re4::NativeInventoryGlyphCaption native_caption;
        if(valid&&re4::native_inventory_glyph_caption(value.tenths,unsigned(value.x),bottom+3,
            name_width,style.height,cell_width,metrics,style,prefix,prefix_metrics,native_caption)){
            for(unsigned i=0;i<native_caption.count;++i)inventory_damage_glyph_native.draw(&native_caption.glyphs[i]);
            return true;
        }
    }
    if(!inventory_damage_glyph_native.prefix)return false;
    re4::NativeInventoryCaption caption;
    // Glyph packet coordinates are native512x384. The wrapper owns projection;
    // applying the late HUD's448/384 conversion here would move this line twice.
    if(!re4::native_inventory_caption(value.tenths,unsigned(value.x),bottom+3,
        name_width,style.height,cell_width,metrics,style,russian,caption))return false;
    if(!inventory_damage_glyph_native.prefix(caption.prefix,caption.x,caption.y,caption.capheight,style.rgba))return false;
    for(unsigned i=0;i<caption.number.count;++i)inventory_damage_glyph_native.draw(&caption.number.glyphs[i]);
    return true;
}
