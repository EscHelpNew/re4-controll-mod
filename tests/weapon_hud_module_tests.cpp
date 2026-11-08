#include "re4/native_weapon_popup.hpp"
#include "game_stubs/lib.hpp"
Result popup_query_memory(MemoryInfo*,std::uint32_t*,std::uintptr_t);
#define svcQueryMemory popup_query_memory
#define main game_module_baseline_main
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#include "game_module_tests.cpp"
#pragma GCC diagnostic pop
#undef main
#undef svcQueryMemory

namespace {
unsigned popup_batches=0,popup_expected=0,popup_emitted=0,popup_peak=0;
unsigned hud_required_calls=0;
std::uintptr_t popup_correction_address=0;
unsigned popup_matrix_queries=0;
bool popup_matrix_race=false;
std::vector<re4::ViewHudPoint> popup_vertices;
std::vector<std::array<unsigned,4>> popup_colors;
std::vector<re4::NativeInventoryGlyph> native_damage_glyphs;
std::vector<re4::NativeInventoryGlyph> native_popup_glyphs;
void hud_begin(unsigned primitive,unsigned format,unsigned count){
    assert((primitive==0x98&&count==4)||(primitive==0x90&&count%6==0));
    assert(hud_native_calls>=hud_required_calls); // Overlay belongs after the native display list.
    assert(format==0);++popup_batches;popup_expected=count;popup_emitted=0;
}
void hud_position(float x,float y,float z){
    assert(x>=0&&x<=512&&y>=0&&y<=448&&z==0);++popup_emitted;popup_vertices.push_back({x,y});
}
void hud_color(unsigned r,unsigned g,unsigned b,unsigned a){popup_peak=std::max(popup_peak,a);popup_colors.push_back({r,g,b,a});}
void hud_end(unsigned token){assert(token==0x34&&popup_emitted==popup_expected);}
void reset_draw(){popup_batches=popup_peak=0;popup_vertices.clear();popup_colors.clear();native_damage_glyphs.clear();native_popup_glyphs.clear();}
struct HudFixture:Fixture {
    std::vector<std::uint8_t> core=std::vector<std::uint8_t>(0x4000);
    std::array<std::array<std::int8_t,2>,512> popup_metrics{};
    template<class T>void put(void* p,std::size_t at,T value){std::memcpy(static_cast<std::uint8_t*>(p)+at,&value,sizeof(value));}
    HudFixture(){
        view_mode();settings.dpad_weapon_switching=true;publish_view_settings();
        native_gx_begin=hud_begin;native_gx_position=hud_position;native_gx_color=hud_color;native_gx_end=hud_end;
        native_gx_blend=+[](unsigned a,unsigned b,unsigned c,unsigned d){assert(a==1&&!d&&((b==4&&(c==1||c==5))||(b==1&&!c)));};
        weapon_popup_capture_native={}; // Rendering works with diagnostic capture unbound.
        reset_draw();game_test::tick=100;
        popup_correction_address=0;popup_matrix_queries=0;popup_matrix_race=false;
        const auto font=image.data()+0x124FAB0,manager=image.data()+0x124FFB0,bank=core.data()+0x100;
        put(input_state.data(),0x60,core.data());put(core.data(),0x54,std::uint32_t(0x100));put(core.data(),0x58,std::uint32_t(0x3000));
        put(manager,0,std::int32_t(1));put(manager,0x20,bank);put(manager,0x44,std::uint32_t(1));manager[0x53]=1;
        put(bank,4,std::uint32_t(0x110));
        for(unsigned raw=0;raw<0x110;++raw)put(bank,8+raw*8,std::uint64_t(0x1000));
        const std::array<std::uint16_t,7> name{0,0xA7,0x80,0x1AC,14,8,1};
        std::memcpy(bank+0x1000,name.data(),sizeof(name));
        for(auto& metric:popup_metrics)metric={0,32};
        font[0]=1;font[0xFC]=font[0xFD]=64;put(font,0xF8,std::uint16_t(2048));put(font,0xFA,std::uint16_t(1024));put(font,0xF0,popup_metrics.data());
        put(image.data(),0xB00614,std::uint32_t(0xE5D9CFF0));
        game_test::memory_maps={{main_base,image.size(),3},{reinterpret_cast<std::uintptr_t>(input_state.data()),input_state.size(),3},
            {reinterpret_cast<std::uintptr_t>(core.data()),core.size(),3},{reinterpret_cast<std::uintptr_t>(popup_metrics.data()),sizeof(popup_metrics),3}};
        inventory_damage_glyph_native.draw=+[](const re4::NativeInventoryGlyph* p){
            assert(hud_native_calls>=hud_required_calls);native_popup_glyphs.push_back(*p);popup_peak=std::max(popup_peak,p->rgba&255);};
    }
    void draw(){hud_required_calls=hud_native_calls+1;HudDraw::Callback(nullptr);}
    void correction(const std::array<float,16>& value){
        const auto software=reinterpret_cast<std::uintptr_t>(image.data()+0x20000);
        std::memcpy(image.data()+0x1184150,&software,sizeof(software));
        std::memcpy(reinterpret_cast<void*>(software+0x440180),value.data(),sizeof(value));
        popup_correction_address=software+0x440180;popup_matrix_queries=0;
    }
};
void* damage_node=nullptr;
void* damage_hovered(void*,void*){return damage_node;}
void damage_classify(unsigned,void* out){static_cast<std::uint8_t*>(out)[2]=1;}
unsigned damage_weapon(unsigned){return 4;}
float damage_power(unsigned,int){return 2.25f;}
struct DamageHudFixture:HudFixture {
    std::array<std::uint8_t,0x60> grid{},cursor{},node{};
    std::array<std::uint8_t,14> record{};
    std::array<std::uint8_t,0x100> font{};
    std::array<std::array<std::int8_t,2>,22> metrics{};
    re4::NativeInventoryGlyph packet{};
    template<class T>void put(void* p,unsigned at,T value){std::memcpy(static_cast<std::uint8_t*>(p)+at,&value,sizeof(value));}
    DamageHudFixture(){
        settings.dpad_weapon_switching=false;settings.language=re4::Language::Russian;publish_view_settings();
        const auto menu=image.data()+0x1270F70,manager=image.data()+0x124D918;
        put(menu,0x338,grid.data());put(grid.data(),0x58,cursor.data());put(node.data(),0x30,record.data());
        put(manager,0x20,record.data());put(manager,0x30,std::int32_t(1));
        put(record.data(),0,std::uint16_t(35));put(record.data(),2,std::uint16_t(1));record[4]=1;
        put(image.data(),0xB00648+0x1C,std::uint32_t(1));
        packet.x=320;packet.y=300;packet.width=18;packet.height=24;packet.rgba=0xEEDDCCFF;
        packet.font=reinterpret_cast<std::uintptr_t>(font.data());
        put(image.data(),0xB00648+0xF8,&packet);put(image.data(),0xB00648+0x100,&packet+1);
        image[0xB00648+0x30]=18;font[0]=1;font[0xFC]=font[0xFD]=64;
        put(font.data(),0xF0,metrics.data());put(font.data(),0xF8,std::uint16_t(512));put(font.data(),0xFA,std::uint16_t(512));
        for(auto& metric:metrics)metric={0,32};
        game_test::memory_maps={{main_base,image.size(),3},
            {reinterpret_cast<std::uintptr_t>(grid.data()),grid.size(),3},
            {reinterpret_cast<std::uintptr_t>(cursor.data()),cursor.size(),3},
            {reinterpret_cast<std::uintptr_t>(node.data()),node.size(),3},
            {reinterpret_cast<std::uintptr_t>(record.data()),record.size(),3},
            {reinterpret_cast<std::uintptr_t>(font.data()),font.size(),3},
            {reinterpret_cast<std::uintptr_t>(metrics.data()),sizeof(metrics),3},
            {reinterpret_cast<std::uintptr_t>(&packet),sizeof(packet),3}};
        damage_node=node.data();inventory_damage_enabled=true;
        inventory_damage_native={damage_hovered,damage_classify,damage_weapon,damage_power};
        inventory_damage_glyph_native.draw=+[](const re4::NativeInventoryGlyph* p){
            assert(hud_native_calls>=hud_required_calls);native_damage_glyphs.push_back(*p);};
        inventory_damage_glyph_native.prefix=+[](const char* text,float x,float y,float cap,unsigned rgba){
            assert(!std::strcmp(text,"Атака: "));
            return re4::draw_serif_text(native_hud_backend(),text,x,y,cap,rgba&255);
        };
        exl::hook::InlineFloatCtx context{};
        put(context.bytes,0x200+20*8,reinterpret_cast<std::uintptr_t>(menu));
        put(context.bytes,0x200+22*8,std::uintptr_t(320));put(context.bytes,0x200+23*8,std::uintptr_t(300));
        InventoryDamageName::Callback(&context);
    }
};
void viewport_forwarding_test(){
    struct Capture {unsigned magic=321,mask=0;float x=0,y=0,z=0;} capture;
    re4::ReticleDrawBackend original{&capture,
        +[](void* p){auto& c=*static_cast<Capture*>(p);assert(c.magic==321);c.mask|=1;},
        +[](void* p,unsigned a,unsigned b,unsigned c,unsigned d){auto& v=*static_cast<Capture*>(p);assert(v.magic==321&&a==1&&b==4&&c==5&&d==0);v.mask|=2;},
        +[](void* p,unsigned a,unsigned b,unsigned c,unsigned d,unsigned e){auto& v=*static_cast<Capture*>(p);assert(v.magic==321&&!a&&b==9&&c==1&&d==4&&!e);v.mask|=4;},
        +[](void* p,unsigned a,unsigned b,unsigned c){auto& v=*static_cast<Capture*>(p);assert(v.magic==321&&a==0x90&&!b&&c==6);v.mask|=8;},
        +[](void* p,float x,float y,float z){auto& v=*static_cast<Capture*>(p);assert(v.magic==321);v.mask|=16;v.x=x;v.y=y;v.z=z;},
        +[](void* p,unsigned r,unsigned g,unsigned b,unsigned a){auto& v=*static_cast<Capture*>(p);assert(v.magic==321&&r==1&&g==2&&b==3&&a==4);v.mask|=32;},
        +[](void* p,unsigned t){auto& v=*static_cast<Capture*>(p);assert(v.magic==321&&t==0x34);v.mask|=64;}};
    const std::array<float,16> affine{{1.25f,0,0,0,0,1.5f,0,0,0,0,1,0,.125f,-.1f,0,1}};
    re4::WeaponPopupViewport viewport;assert(re4::WeaponPopupViewport::correction(affine,viewport));
    re4::WeaponPopupViewportBackend adapter{original,viewport};const auto b=adapter.backend();
    b.prepare_hud(b.context);b.blend(b.context,1,4,5,0);b.vertex_format(b.context,0,9,1,4,0);
    b.begin(b.context,0x90,0,6);b.position(b.context,128,420,.25f);b.color(b.context,1,2,3,4);b.end(b.context,0x34);
    assert(capture.mask==127&&capture.z==.25f);
    assert(std::fabs(affine[0]*(2*capture.x/512-1)+affine[12]-(2*128.f/512-1))<.00001f);
    assert(std::fabs(affine[5]*(1-2*capture.y/448)+affine[13]-(1-2*420.f/448))<.00001f);
    for(unsigned i=0;i<16;++i){
        auto invalid=affine;
        if(i==0)invalid[i]=0;else if(i==5)invalid[i]=-1;
        else if(i==12||i==13)invalid[i]=std::numeric_limits<float>::quiet_NaN();else invalid[i]+=.01f;
        re4::WeaponPopupViewport fallback;
        assert(!re4::WeaponPopupViewport::correction(invalid,fallback));const auto point=fallback.position(128,420);
        assert(point.x==128&&point.y==420);
    }
}
}
Result popup_query_memory(MemoryInfo* out,std::uint32_t* page,std::uintptr_t address){
    const auto result=svcQueryMemory(out,page,address);
    if(!result&&address==popup_correction_address&&++popup_matrix_queries==2&&popup_matrix_race)
        reinterpret_cast<float*>(address)[5]=1.5f;
    return result;
}
int main(){
    viewport_forwarding_test();
    {
        HudFixture f;weapon_popup_commit(f.actor,35);game_test::tick=9'600'100;f.draw();
        const auto logical=popup_vertices;const auto colors=popup_colors;const auto batches=popup_batches;
        const std::array<float,16> captured{{1,0,0,0,0,1.33333325f,0,0,0,0,1,0,0,0,0,1}};
        assert(1.33333325f*(1-2*420.f/448)<-1); // Original lower-right label is clipped.
        f.correction(captured);reset_draw();f.draw();
        assert(popup_vertices.size()==logical.size()&&popup_colors==colors&&popup_batches==batches);
        for(unsigned i=0;i<logical.size();++i){
            const auto actual=popup_vertices[i],wanted=logical[i];
            const float clip_x=captured[0]*(2*actual.x/512-1)+captured[12];
            const float clip_y=captured[5]*(1-2*actual.y/448)+captured[13];
            assert(std::fabs(clip_x-(2*wanted.x/512-1))<.00001f);
            assert(std::fabs(clip_y-(1-2*wanted.y/448))<.00001f);
            assert(clip_x>=-1.00001f&&clip_x<=1.00001f&&clip_y>=-1.00001f&&clip_y<=1.00001f);
        }
        // Incoherent matrices and failed context reads retain original geometry.
        f.correction(captured);popup_matrix_race=true;reset_draw();f.draw();
        assert(popup_vertices.size()==logical.size());
        for(unsigned i=0;i<logical.size();++i)assert(popup_vertices[i].x==logical[i].x&&popup_vertices[i].y==logical[i].y);
        f.correction(captured);popup_matrix_race=false;
        const std::uintptr_t invalid=~std::uintptr_t(0)-7;std::memcpy(f.image.data()+0x1184150,&invalid,sizeof(invalid));
        reset_draw();f.draw();
        for(unsigned i=0;i<logical.size();++i)assert(popup_vertices[i].x==logical[i].x&&popup_vertices[i].y==logical[i].y);
    }
    {
        DamageHudFixture f;f.draw();assert(popup_batches&&native_damage_glyphs.size()==3&&hud_native_calls==1);
        // Current2.25 rounds to2.3; native14px packets enlarged1.5x to21px.
        assert(native_damage_glyphs[0].glyph==5&&native_damage_glyphs[1].glyph==21&&native_damage_glyphs[2].glyph==6);
        for(const auto& p:native_damage_glyphs)assert(p.font==f.packet.font&&p.rgba==f.packet.rgba&&p.height==21&&p.y==327);
        reset_draw();*reinterpret_cast<std::uint32_t*>(f.image.data()+0xB00648+0x1C)=0;f.draw();assert(native_damage_glyphs.empty());
        *reinterpret_cast<std::uint32_t*>(f.image.data()+0xB00648+0x1C)=1;f.record[6]=1;f.draw();assert(native_damage_glyphs.empty());
    }
    {
        HudFixture f;weapon_popup_commit(f.actor,35);
        assert((control.graphics_reserved&255)==3&&((control.graphics_reserved>>16)&255)>0);
        f.draw();assert(!popup_batches);
        game_test::tick=4'800'100;f.draw();assert(popup_batches&&popup_peak==120);
        const auto capture_sequence=control.popup_capture_sequence;
        reset_draw();game_test::tick=9'600'100;f.draw();assert(popup_batches&&popup_peak==240);
        assert(control.popup_capture_sequence==capture_sequence); // No producer-side snapshot write.
        assert(((control.graphics_reserved>>8)&255)==8&&(control.graphics_reserved>>24)>0);
        reset_draw();game_test::tick=57'600'100;f.draw();assert(!popup_batches);
    }
    {
        HudFixture f;weapon_popup_commit(f.actor,35);game_test::tick=9'600'100;f.draw();assert(popup_batches);
        reset_draw();weapon_popup_commit(f.actor,44);f.draw();assert(!popup_batches); // New animation resets old text now.
        game_test::tick+=9'600'000;f.draw();assert(popup_batches);
        reset_draw();settings.dpad_weapon_switching=false;publish_view_settings();f.draw();assert(!popup_batches);
    }
    for(unsigned gate=0;gate<5;++gate){
        HudFixture f;weapon_popup_commit(f.actor,35);game_test::tick+=9'600'000;
        if(gate==0)f.image[0x1131718+0x54]=16;
        if(gate==1)*reinterpret_cast<std::uint32_t*>(f.input_state.data()+0x7C)=1u<<13;
        if(gate==2)f.actor[0x114]=1;
        if(gate==3)*reinterpret_cast<void**>(f.image.data()+re4::profile::player_slot)=nullptr;
        if(gate==4)game_test::tick=99;
        f.draw();assert(!popup_batches);
    }
    // Every supported name, both languages, stays entirely in the viewport.
    for(unsigned id=0;id<0x110;++id)for(bool russian:{false,true})if(re4::weapon_name(id,russian)){
        HudFixture f;settings.language=russian?re4::Language::Russian:re4::Language::English;publish_view_settings();
        weapon_popup_commit(f.actor,id);game_test::tick+=9'600'000;f.draw();assert(popup_batches);
    }
    std::puts("weapon HUD module tests passed");
}
