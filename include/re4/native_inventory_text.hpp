#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include "weapon_text.hpp"
#include "native_inventory_alphabet.hpp"

namespace re4 {
// Exact packet consumed by native2B28E0/2B2A30. The live font supplies the
// game's atlas artwork, including the localized face and baked glyph outline.
struct NativeInventoryGlyph {
    std::uint16_t x=0,y=0,glyph=0;
    std::uint8_t width=0,height=0;
    std::uint32_t rgba=0,reserved=0;
    std::uintptr_t font=0;
    std::uint8_t visibility_override=0;
    std::array<std::uint8_t,7> padding{};
};
static_assert(sizeof(NativeInventoryGlyph)==0x20);
static_assert(offsetof(NativeInventoryGlyph,font)==0x10);
static_assert(offsetof(NativeInventoryGlyph,visibility_override)==0x18);
struct NativeInventoryNumber {
    std::array<NativeInventoryGlyph,6> glyphs{};
    unsigned count=0;
};
// Native MDT numeric codes83..8C/95 become atlas glyph3..12/21 at2B120C.
// No letter/Unicode mapping is inferred from the base game's English assets.
inline bool native_inventory_number(unsigned tenths,unsigned x,unsigned y,
        unsigned name_width,unsigned name_height,unsigned cell_width,
        const std::array<std::array<std::int8_t,2>,22>& metrics,
        const NativeInventoryGlyph& style,NativeInventoryNumber& out){
    if(tenths>65535||x>511||y>383||!style.font||!cell_width||cell_width>128
        ||name_width<5||name_width>128||name_height<5||name_height>128)return false;
    const unsigned width=(((name_width*3+2)/5)*3+1)/2,height=(((name_height*3+2)/5)*3+1)/2;
    if(!width||!height||y+height>384)return false;
    unsigned reverse[4]{},digits=0,integer=tenths/10;
    do{reverse[digits++]=integer%10;integer/=10;}while(integer);
    unsigned codes[6]{},count=0;
    while(digits)codes[count++]=3+reverse[--digits];
    codes[count++]=21;codes[count++]=3+tenths%10;
    NativeInventoryNumber result;
    for(unsigned i=0;i<count;++i){
        const auto glyph=codes[i];const auto& metric=metrics[glyph];
        const int span=int(metric[1])-int(metric[0]);
        if(span<=0||span>int(cell_width))return false;
        const unsigned advance=unsigned(span)*width/cell_width;
        if(!advance||advance>255||x+advance>512)return false;
        auto packet=style;packet.x=std::uint16_t(x);packet.y=std::uint16_t(y);
        packet.glyph=std::uint16_t(glyph);packet.width=std::uint8_t(advance);packet.height=std::uint8_t(height);
        result.glyphs[result.count++]=packet;x+=advance;
    }
    out=result;return true;
}
struct NativeInventoryCaption {
    NativeInventoryNumber number;
    const char* prefix=nullptr;
    float x=0,y=0,capheight=0;
};
struct NativeInventoryGlyphCaption {
    std::array<NativeInventoryGlyph,16> glyphs{};
    unsigned count=0;
};
inline bool native_inventory_glyph_caption(unsigned tenths,unsigned x,unsigned y,
        unsigned name_width,unsigned name_height,unsigned cell_width,
        const std::array<std::array<std::int8_t,2>,22>& metrics,
        const NativeInventoryGlyph& style,const NativeInventoryPrefix& prefix,
        const std::array<std::array<std::int8_t,2>,7>& prefix_metrics,NativeInventoryGlyphCaption& out){
    NativeInventoryNumber number;
    if(!prefix.count||prefix.count>7||!native_inventory_number(tenths,x,y,name_width,name_height,cell_width,metrics,style,number))return false;
    const unsigned width=(((name_width*3+2)/5)*3+1)/2;
    NativeInventoryGlyphCaption value;
    for(unsigned i=0;i<prefix.count;++i){
        const int span=int(prefix_metrics[i][1])-int(prefix_metrics[i][0]);
        if(!prefix.glyphs[i]||prefix.glyphs[i]>=4096||span<=0||span>int(cell_width))return false;
        const unsigned advance=unsigned(span)*width/cell_width;
        if(!advance||x+advance>512)return false;
        auto packet=number.glyphs[0];packet.x=std::uint16_t(x);packet.glyph=prefix.glyphs[i];packet.width=std::uint8_t(advance);
        value.glyphs[value.count++]=packet;x+=advance;
    }
    x+=(width+2)/3;
    if(!native_inventory_number(tenths,x,y,name_width,name_height,cell_width,metrics,style,number))return false;
    for(unsigned i=0;i<number.count;++i)value.glyphs[value.count++]=number.glyphs[i];
    out=value;return true;
}
inline bool native_inventory_caption(unsigned tenths,unsigned x,unsigned y,
        unsigned name_width,unsigned name_height,unsigned cell_width,
        const std::array<std::array<std::int8_t,2>,22>& metrics,
        const NativeInventoryGlyph& style,bool russian,NativeInventoryCaption& out){
    NativeInventoryCaption result;
    if(!native_inventory_number(tenths,x,y,name_width,name_height,cell_width,metrics,style,result.number))return false;
    result.prefix=russian?"Атака: ":"Attack: ";
    // Prefix geometry and native packets have different logical Y scales.
    // The supported64px atlas has29px numeral ink, starting at row17.
    // Localized replacement artwork can differ; no letter indices are guessed.
    result.capheight=result.number.glyphs[0].height*(29.f/64.f)*(448.f/384.f);
    result.x=float(x);result.y=(y+result.number.glyphs[0].height*(17.f/64.f))*(448.f/384.f);
    if(!serif_text_runs(result.prefix,result.x,result.y,result.capheight,255,nullptr,
        +[](void*,float,float,float,float,unsigned){}))return false;
    const unsigned number_x=x+unsigned(std::ceil(serif_text_width(result.prefix,result.capheight)))+2;
    if(!native_inventory_number(tenths,number_x,y,name_width,name_height,cell_width,metrics,style,result.number))return false;
    out=result;return true;
}
}
