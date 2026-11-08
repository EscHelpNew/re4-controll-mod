#define main native_inventory_text_baseline_main
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#include "inventory_damage_module_tests.cpp"
#pragma GCC diagnostic pop
#undef main
#include "re4/native_inventory_text.hpp"
namespace {
#include "../source/game/inventory_damage_native_text.inc"
std::vector<re4::NativeInventoryGlyph> drawn;
void draw_native(const re4::NativeInventoryGlyph* p){drawn.push_back(*p);}
}
int main(){
    Fixture f;
    alignas(32) std::array<std::uint8_t,8192> core{};
    std::array<std::uint8_t,128> state{};
    std::array<std::uint8_t,256> font{};
    std::array<std::array<std::int8_t,2>,256> metrics{};
    for(auto& m:metrics)m={0,40};
    re4::NativeInventoryGlyph style;style.x=320;style.y=300;style.width=18;style.height=24;
    style.font=reinterpret_cast<std::uintptr_t>(font.data());style.rgba=0xCCDDEE80;
    const auto engine=f.image.data()+0xB00648,manager=f.image.data()+0x124FFB0;
    f.put(engine,0x38,font.data());f.put(engine,0xF8,&style);f.put(engine,0x100,&style+1);engine[0x30]=18;
    f.put(f.image.data(),0xB1AE08,state.data());f.put(state.data(),0x60,core.data());
    f.put(core.data(),0x54,0x200u);f.put(core.data(),0x58,0x1E00u);
    auto* bank=core.data()+0x200;f.put(manager,0x20,bank);f.put(manager,0x44,1u);f.put(manager,0,std::int32_t(1));
    f.put(bank,4,0x110u);
    f.put(font.data(),0,std::uint8_t(1));f.put(font.data(),0xF0,metrics.data());
    f.put(font.data(),0xF8,std::uint16_t(2048));f.put(font.data(),0xFA,std::uint16_t(512));font[0xFC]=font[0xFD]=64;
    const unsigned ids[]={35,46,16,3};const char16_t* words[]={u"Пистолет",u"Винтовка",u"Арбалет",u"Матильда"};
    for(unsigned n=0;n<4;++n){
        const unsigned offset=0x1000+n*64;f.put(bank,8+ids[n]*8,std::uint64_t(offset));
        auto* text=bank+offset;unsigned at=0;f.put(text,at,std::uint16_t(0));at+=2;
        for(const auto* p=words[n];*p;++p){f.put(text,at,std::uint16_t(0x80+unsigned(*p)-0x400+70));at+=2;}
        for(unsigned code:{0x80u,14u,8u,1u}){f.put(text,at,std::uint16_t(code));at+=2;}
    }
    for(auto range:std::vector<MemoryInfo>{{reinterpret_cast<std::uintptr_t>(core.data()),core.size(),1},
        {reinterpret_cast<std::uintptr_t>(state.data()),state.size(),1},{reinterpret_cast<std::uintptr_t>(font.data()),font.size(),1},
        {reinterpret_cast<std::uintptr_t>(metrics.data()),sizeof(metrics),1},{reinterpret_cast<std::uintptr_t>(&style),sizeof(style),1}})
        game_test::memory_maps.push_back(range);
    static unsigned fallback=0;fallback=0;
    inventory_damage_glyph_native.draw=draw_native;
    inventory_damage_glyph_native.prefix=+[](const char*,float,float,float,unsigned){++fallback;return true;};
    native_value=1.2f;f.invoke();drawn.clear();
    CHECK(inventory_damage_draw_native(true)&&fallback==0&&drawn.size()==9);
    const unsigned expected[]={86,136,118,128,118,13,4,21,5};
    for(unsigned i=0;i<9;++i)CHECK(drawn[i].glyph==expected[i]&&drawn[i].font==style.font
        &&drawn[i].rgba==style.rgba&&drawn[i].height==21&&drawn[i].y==327);
    // A different translation or reused bank must never emit guessed glyphs.
    f.put(bank+0x1040,8,std::uint16_t(200));drawn.clear();
    CHECK(inventory_damage_draw_native(true)&&fallback==1&&drawn.size()==3);
    // English uses proven native Latin codes and the same styling too.
    drawn.clear();CHECK(inventory_damage_draw_native(false)&&fallback==1&&drawn.size()==10);
    CHECK(drawn[0].glyph==39&&drawn[6].glyph==13);
    // Real native non-direct language table, including its skipped entries.
    f.put(bank+0x1040,8,std::uint16_t(0x80+136));
    std::memmove(bank+0x80,bank,0x1180);f.put(bank,0,6u);
    f.put(bank,8,0x80u);f.put(bank,16,0x80u);f.put(bank,12,0x80u);
    f.put(manager,0x44,0u);inventory_damage_glyph_native.prefix=nullptr;
    for(auto selection:std::array<std::array<int,2>,3>{{{{1,0}},{{2,0}},{{2,1}}}}){
        f.put(manager,0,std::int32_t(selection[0]));f.put(manager,0x53,std::uint8_t(selection[1]));
        drawn.clear();CHECK(inventory_damage_draw_native(true)&&drawn.size()==9);
    }
    std::puts("native caption callback: localized native prefix/digits share font, color, size; unknown translation fallback PASS");
}
