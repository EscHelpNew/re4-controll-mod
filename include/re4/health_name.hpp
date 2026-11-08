#pragma once
#include "re4/character_appearance.hpp"
#include "re4/native_inventory_text.hpp"
#include "re4/weapon_popup_viewport.hpp"
#include <algorithm>
#include <cmath>

namespace re4 {
struct HealthNameWord {std::array<std::uint16_t,24> glyphs{};unsigned count=0;};
struct HealthNameAlphabet {
    std::array<char16_t,64> letters{};std::array<std::uint16_t,64> glyphs{};unsigned count=0;
    bool add(char16_t letter,std::uint16_t glyph){
        if(letter==u' ')return glyph==0;
        if(!glyph||glyph>=4096)return false;
        for(unsigned i=0;i<count;++i){
            if(letters[i]==letter)return glyphs[i]==glyph;
            if(glyphs[i]==glyph)return false;
        }
        if(count==letters.size())return false;
        letters[count]=letter;glyphs[count++]=glyph;return true;
    }
    bool word(const char16_t* text,HealthNameWord& out)const{
        HealthNameWord value;
        for(unsigned j=0;text[j];++j){
            if(value.count==value.glyphs.size())return false;
            if(text[j]==u' '){value.glyphs[value.count++]=0;continue;}
            unsigned i=0;while(i<count&&letters[i]!=text[j])++i;
            if(i==count)return false;
            value.glyphs[value.count++]=glyphs[i];
        }
        if(!value.count)return false;
        out=value;return true;
    }
};
inline constexpr unsigned health_name_calibration_ids[]={35,46,16,3,82,41,53};
inline constexpr const char16_t* health_name_calibration_words[]={
    u"\u041f\u0438\u0441\u0442\u043e\u043b\u0435\u0442", // Пистолет
    u"\u0412\u0438\u043d\u0442\u043e\u0432\u043a\u0430", // Винтовка
    u"\u0410\u0440\u0431\u0430\u043b\u0435\u0442", // Арбалет
    u"\u041c\u0430\u0442\u0438\u043b\u044c\u0434\u0430", // Матильда
    u"\u041b\u0443\u043a \u041a\u0440\u0430\u0443\u0437\u0435\u0440\u0430", // Лук Краузера
    u"\u0421\u043b\u043e\u043c\u0430\u043d\u043d\u0430\u044f \u0431\u0430\u0431\u043e\u0447\u043a\u0430",
    u"\u0420\u0430\u043a\u0435\u0442\u043e\u043c\u0451\u0442" // Ракетомёт
};
inline bool health_name_word(CharacterAppearance appearance,bool russian,const HealthNameAlphabet& alphabet,HealthNameWord& out){
    constexpr const char* latin[]={"Leon","Ashley","Ada","Luis","Krauser","Merchant","Ramon","Ganado"};
    constexpr const char16_t* localized[]={
        u"\u041b\u0435\u043e\u043d",u"\u042d\u0448\u043b\u0438",u"\u0410\u0434\u0430",
        u"\u041b\u0443\u0438\u0441",u"\u041a\u0440\u0430\u0443\u0437\u0435\u0440",
        u"\u0422\u043e\u0440\u0433\u043e\u0432\u0435\u0446",u"\u0420\u0430\u043c\u043e\u043d",
        u"\u0413\u0430\u043d\u0430\u0434\u043e"};
    if(!valid_character_appearance(appearance))return false;
    const unsigned index=unsigned(appearance);
    if(russian&&alphabet.word(localized[index],out))return true;
    // Original English MDT/common_p proves these Latin atlas indices; this is
    // the same native face when a translation cannot prove every name letter.
    HealthNameWord value;
    for(const char* p=latin[index];*p;++p){
        const auto c=unsigned(*p);unsigned glyph=0;
        if(c>='A'&&c<='Z')glyph=39+c-'A';
        else if(c>='a'&&c<='z')glyph=65+c-'a';
        else if(c!=' ')return false;
        value.glyphs[value.count++]=std::uint16_t(glyph);
    }
    out=value;return true;
}
struct HealthNameLine {std::array<NativeInventoryGlyph,24> glyphs{};unsigned count=0;};
inline bool health_name_layout(const HealthNameWord& word,
        const std::array<std::array<std::int8_t,2>,24>& metrics,unsigned cell_width,
        float left,float top,float right,float bottom,const NativeInventoryGlyph& style,
        const WeaponPopupViewport& viewport,HealthNameLine& out){
    if(!word.count||word.count>word.glyphs.size()||!cell_width||cell_width>128||!style.font||!(style.rgba&255)
        ||!std::isfinite(left)||!std::isfinite(top)||!std::isfinite(right)||!std::isfinite(bottom)
        ||left<0||top<0||right>512||bottom>448||right<=left||bottom<=top)return false;
    float spans[24]{},units=0;
    for(unsigned i=0;i<word.count;++i){
        const int span=word.glyphs[i]?int(metrics[i][1])-int(metrics[i][0]):int(cell_width)/2;
        if(span<=0||span>int(cell_width))return false;
        // Native case cell18x24 becomes18x28 in late HUD512x448.
        spans[i]=float(span)/cell_width*(18.f/28.f);units+=spans[i];
    }
    const float height=std::min(bottom-top,(right-left)/units);
    if(!std::isfinite(height)||height<1)return false;
    float cursor=(left+right-height*units)*.5f;
    const float y=(top+bottom-height)*.5f;
    HealthNameLine value;
    for(unsigned i=0;i<word.count;++i){
        const float next=cursor+height*spans[i];
        if(word.glyphs[i]){
            const auto a=viewport.position(cursor,y),b=viewport.position(next,y+height);
            if(!std::isfinite(a.x)||!std::isfinite(a.y)||!std::isfinite(b.x)||!std::isfinite(b.y))return false;
            const long x=std::lround(a.x),r=std::lround(b.x),t=std::lround(a.y*384.f/448),end=std::lround(b.y*384.f/448);
            if(x< -32768||x>32767||r< -32768||r>32767||t< -32768||t>32767||end< -32768||end>32767
                ||r<=x||r-x>255||end<=t||end-t>255)return false;
            auto packet=style;packet.glyph=word.glyphs[i];packet.x=std::uint16_t(std::int16_t(x));
            packet.y=std::uint16_t(std::int16_t(t));packet.width=std::uint8_t(r-x);packet.height=std::uint8_t(end-t);
            value.glyphs[value.count++]=packet;
        }
        cursor=next;
    }
    if(!value.count)return false;
    out=value;return true;
}
}
