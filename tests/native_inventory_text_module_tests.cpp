#define main inventory_damage_collector_tests_main
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#include "inventory_damage_module_tests.cpp"
#pragma GCC diagnostic pop
#undef main
#include "re4/native_inventory_text.hpp"
namespace {
#include "../source/game/inventory_damage_native_text.inc"
std::vector<re4::NativeInventoryGlyph> drawn;
unsigned prefixes=0;
void draw_native(const re4::NativeInventoryGlyph* packet){drawn.push_back(*packet);}
bool draw_prefix(const char* text,float x,float y,float cap,unsigned rgba){
    CHECK(!std::strcmp(text,"Attack: ")&&x==320&&y>380&&y<390&&cap>11&&cap<11.2f&&rgba==0xE8E9EAFF);
    ++prefixes;return true;
}
}
int main(){
    Fixture f;
    alignas(8) std::array<std::uint8_t,0x100> font{};
    std::array<std::array<std::int8_t,2>,22> metrics{};for(auto& pair:metrics)pair={0,40};metrics[21]={0,20};
    std::array<re4::NativeInventoryGlyph,3> native_name{};
    for(unsigned i=0;i<3;++i){auto& packet=native_name[i];packet.font=reinterpret_cast<std::uintptr_t>(font.data());
        packet.rgba=0xE8E9EAFF;packet.height=24;packet.width=12;packet.x=320+i*12;packet.y=300;packet.glyph=39+i;}
    f.put(font.data(),0,std::uint8_t(1));f.put(font.data(),0xF0,metrics.data());
    f.put(font.data(),0xF8,std::uint16_t(1024));f.put(font.data(),0xFA,std::uint16_t(1024));font[0xFC]=font[0xFD]=64;
    f.put(f.image.data(),0xB00648+0xF8,native_name.data());
    f.put(f.image.data(),0xB00648+0x100,native_name.data()+3);
    f.put(f.image.data(),0xB00648+0x30,std::uint8_t(18));f.put(f.image.data(),0xB00648+0x31,std::uint8_t(24));
    game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(font.data()),font.size(),1});
    game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(metrics.data()),sizeof(metrics),1});
    game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(native_name.data()),sizeof(native_name),1});
    inventory_damage_glyph_native.draw=draw_native;inventory_damage_glyph_native.prefix=draw_prefix;native_value=1.2f;f.invoke();
    const auto before_image=f.image;const auto before_font=font;const auto before_name=native_name;
    CHECK(inventory_damage_draw_native());
    CHECK(drawn.size()==3&&drawn[0].glyph==4&&drawn[1].glyph==21&&drawn[2].glyph==5);
    CHECK(prefixes==1&&drawn[0].x>350&&drawn[0].y==327&&drawn[0].height==21&&drawn[0].font==native_name[0].font&&drawn[0].rgba==0xE8E9EAFF);
    CHECK(f.image==before_image&&font==before_font&&!std::memcmp(native_name.data(),before_name.data(),sizeof(native_name)));
    const auto count=drawn.size();
    f.put(f.image.data(),0xB00648+0x100,reinterpret_cast<std::uintptr_t>(native_name.data())+33);
    CHECK(!inventory_damage_draw_native()&&drawn.size()==count); // Misaligned live packet span.
    f.put(f.image.data(),0xB00648+0x100,native_name.data()+3);
    font[0]=0;CHECK(!inventory_damage_draw_native()&&drawn.size()==count);font[0]=1;
    metrics[21]={0,0};CHECK(!inventory_damage_draw_native()&&drawn.size()==count);metrics[21]={0,20};
    native_name[2].font=0;CHECK(!inventory_damage_draw_native()&&drawn.size()==count);native_name[2]=before_name[2];
    game_test::memory_maps.erase(game_test::memory_maps.end()-2);
    CHECK(!inventory_damage_draw_native()&&drawn.size()==count); // Unreadable live metrics, no partial draw.
    std::puts("native inventory text module tests passed");
}
