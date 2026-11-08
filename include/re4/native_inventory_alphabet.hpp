#pragma once
#include <array>
#include <cstdint>
namespace re4 {
struct NativeInventoryName {std::array<std::uint16_t,12> glyphs{};unsigned count=0;};
struct NativeInventoryPrefix {std::array<std::uint16_t,7> glyphs{};unsigned count=0;};
inline bool native_inventory_name(const std::uint16_t* codes,unsigned count,NativeInventoryName& out){
    if(!codes||count<5||count>17||codes[0]!=0||codes[count-3]!=14||codes[count-2]!=8||codes[count-1]!=1)return false;
    NativeInventoryName value;unsigned end=count-3;
    if(end>1&&codes[end-1]==0x80)--end;
    for(unsigned i=1;i<end;++i){
        if(codes[i]<=0x80||codes[i]>=0x1080||value.count==value.glyphs.size())return false;
        value.glyphs[value.count++]=std::uint16_t(codes[i]-0x80);
    }
    if(!value.count)return false;
    out=value;return true;
}
inline bool native_inventory_russian_prefix(const std::array<NativeInventoryName,4>& names,NativeInventoryPrefix& out){
    // Identify a supported translation by length, repeated letters and a
    // one-to-one alphabet across four independently indexed native messages.
    // Unknown translations never supply guessed Cyrillic atlas indices.
    constexpr const char16_t* expected[]={u"Пистолет",u"Винтовка",u"Арбалет",u"Матильда"};
    char16_t letters[32]{};std::uint16_t glyphs[32]{};unsigned mapped=0;
    for(unsigned word=0;word<4;++word){
        unsigned length=0;while(expected[word][length])++length;
        if(names[word].count!=length)return false;
        for(unsigned i=0;i<length;++i){
            const auto letter=expected[word][i];const auto glyph=names[word].glyphs[i];
            if(!glyph||glyph>=4096)return false;
            unsigned at=0;
            for(;at<mapped;++at){
                if(letters[at]==letter){if(glyphs[at]!=glyph)return false;break;}
                if(glyphs[at]==glyph)return false;
            }
            if(at==mapped){letters[mapped]=letter;glyphs[mapped++]=glyph;}
        }
    }
    NativeInventoryPrefix value;constexpr char16_t label[]=u"Атака";
    for(unsigned i=0;i<5;++i){
        unsigned at=0;while(at<mapped&&letters[at]!=label[i])++at;
        if(at==mapped)return false;
        value.glyphs[value.count++]=glyphs[at];
    }
    value.glyphs[value.count++]=13; // Proven native MDT008D colon.
    out=value;return true;
}
inline NativeInventoryPrefix native_inventory_english_prefix(){return {{{39,84,84,65,67,75,13}},7};}
}
