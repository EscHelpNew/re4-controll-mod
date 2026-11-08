#include "re4/native_inventory_text.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
void check(bool value,unsigned line){if(!value){std::fprintf(stderr,"native inventory text line %u\n",line);std::exit(1);}}
#define CHECK(c) check((c),__LINE__)
int main(){
    std::array<std::array<std::int8_t,2>,22> metrics{};
    for(auto& metric:metrics)metric={0,40};
    metrics[21]={0,20};
    re4::NativeInventoryGlyph style;style.font=0x12345678;style.rgba=0xEEF0FFFF;style.visibility_override=1;
    re4::NativeInventoryNumber number;
    CHECK(re4::native_inventory_number(12,200,320,18,24,64,metrics,style,number));
    CHECK(number.count==3&&number.glyphs[0].glyph==4&&number.glyphs[1].glyph==21&&number.glyphs[2].glyph==5);
    CHECK(number.glyphs[0].width==10&&number.glyphs[0].height==21);
    CHECK(number.glyphs[0].x==200&&number.glyphs[1].x==210&&number.glyphs[2].x==215);
    CHECK(number.glyphs[0].y==320&&number.glyphs[0].font==style.font&&number.glyphs[0].rgba==style.rgba
        &&number.glyphs[0].visibility_override==1);
    CHECK(re4::native_inventory_number(0,0,0,18,24,64,metrics,style,number));
    CHECK(number.count==3&&number.glyphs[0].glyph==3&&number.glyphs[2].glyph==3);
    CHECK(re4::native_inventory_number(65535,0,0,18,24,64,metrics,style,number));
    const unsigned maximum[]={9,8,8,6,21,8};
    CHECK(number.count==6);for(unsigned i=0;i<6;++i)CHECK(number.glyphs[i].glyph==maximum[i]);
    const auto before=number;
    CHECK(!re4::native_inventory_number(65536,0,0,18,24,64,metrics,style,number));
    CHECK(!re4::native_inventory_number(12,500,0,18,24,64,metrics,style,number));
    CHECK(!re4::native_inventory_number(12,0,375,18,24,64,metrics,style,number));
    CHECK(!re4::native_inventory_number(12,0,0,18,24,0,metrics,style,number));
    metrics[21]={10,10};CHECK(!re4::native_inventory_number(12,0,0,18,24,64,metrics,style,number));
    CHECK(!std::memcmp(&number,&before,sizeof(number))); // No partial result from a bad final glyph.
    metrics[21]={0,20};re4::NativeInventoryCaption caption;
    for(bool russian:{false,true}){
        CHECK(re4::native_inventory_caption(12,200,320,18,24,64,metrics,style,russian,caption));
        CHECK(!std::strcmp(caption.prefix,russian?"Атака: ":"Attack: "));
        CHECK(caption.number.glyphs[0].x>caption.x+re4::serif_text_width(caption.prefix,caption.capheight));
        CHECK(caption.number.glyphs[0].height==21&&caption.capheight>11&&caption.capheight<11.2f);
    }
    re4::NativeInventoryGlyphCaption native_caption;
    std::array<std::array<std::int8_t,2>,7> prefix_metrics{};
    for(auto& m:prefix_metrics)m={0,40};
    CHECK(re4::native_inventory_glyph_caption(12,200,320,18,24,64,metrics,style,
        re4::native_inventory_english_prefix(),prefix_metrics,native_caption));
    CHECK(native_caption.count==10&&native_caption.glyphs[0].glyph==39&&native_caption.glyphs[7].glyph==4);
    for(unsigned i=0;i<native_caption.count;++i)CHECK(native_caption.glyphs[i].height==21
        &&native_caption.glyphs[i].y==320&&native_caption.glyphs[i].font==style.font&&native_caption.glyphs[i].rgba==style.rgba);
    const auto previous_caption=native_caption;prefix_metrics[6]={0,0};
    CHECK(!re4::native_inventory_glyph_caption(12,200,320,18,24,64,metrics,style,
        re4::native_inventory_english_prefix(),prefix_metrics,native_caption));
    CHECK(!std::memcmp(&previous_caption,&native_caption,sizeof(native_caption)));
    std::puts("native inventory text tests passed");
}
