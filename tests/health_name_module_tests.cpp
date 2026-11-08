// Actual sprite delegate and late-HUD callbacks; only native glyph IO is fake.
#include "re4/health_name.hpp"
#include "game_stubs/lib.hpp"
#include <cstring>
namespace health_viewport_test {
inline std::uintptr_t address=0;
inline unsigned hit=0,trigger=0;
inline bool mutate=false;
inline std::array<std::uint8_t,8> replacement{};
inline std::size_t bytes=0;
inline Result query(MemoryInfo* out,std::uint32_t* page,std::uintptr_t at){
    if(at==address&&++hit==trigger){
        if(!mutate)return 1;
        std::memcpy(reinterpret_cast<void*>(at),replacement.data(),bytes);
    }
    return svcQueryMemory(out,page,at);
}
template<class T>void change(std::uintptr_t at,T value){
    address=at;hit=0;trigger=2;mutate=true;bytes=sizeof(value);
    std::memcpy(replacement.data(),&value,bytes);
}
inline void fail(std::uintptr_t at,unsigned when){address=at;hit=0;trigger=when;mutate=false;}
inline void reset(){address=0;hit=trigger=0;}
}
#define svcQueryMemory health_viewport_test::query
#define RE4_EXTENDED_SQUARE_TEST_NATIVE
namespace {void test_ExtendedSquareDraw(void*);}
#define main game_module_baseline_main
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#include "game_module_tests.cpp"
#pragma GCC diagnostic pop
#undef main
#undef svcQueryMemory

namespace {
#include "../source/game/health_name.inc"
std::vector<re4::NativeInventoryGlyph> health_packets;
unsigned health_original_words=0;
std::uintptr_t health_gauge_root=0,health_gauge_arc=0,health_word_address=0;
void* health_gauge_sprite(void*,unsigned id,unsigned group){
    assert(group==0x21);return reinterpret_cast<void*>(id==0?health_gauge_root:id==7?health_gauge_arc:main_base+0x124D630);
}
void test_ExtendedSquareDraw(void* sprite){
    if(reinterpret_cast<std::uintptr_t>(sprite)==health_word_address)++health_original_words;
}
void health_glyph(const re4::NativeInventoryGlyph* packet){health_packets.push_back(*packet);}
struct HealthFixture {
    Fixture f;
    alignas(16) std::array<std::uint8_t,0x170> sprite{};
    alignas(16) std::array<std::uint8_t,0x170> root{},arc{};
    alignas(16) std::array<std::uint8_t,0x40> chain{},bin{};
    std::array<std::array<std::int8_t,2>,1024> metrics{};
    std::vector<std::uint8_t> core=std::vector<std::uint8_t>(0x4000);
    std::vector<std::uint8_t> software=std::vector<std::uint8_t>(0x4401C0);
    template<class T>void put(void* p,std::size_t at,T value){std::memcpy(static_cast<std::uint8_t*>(p)+at,&value,sizeof(value));}
    std::uintptr_t address(const void* p){return reinterpret_cast<std::uintptr_t>(p);}
    HealthFixture(){
        ammo_counter_gauge_draw={};
        health_viewport_test::reset();
        character_appearance_reset();f.view_mode();health_name_clear();health_packets.clear();health_original_words=0;
        inventory_damage_glyph_native.draw=health_glyph;
        character_renderer_ready=true;
        f.actor[0x11A]=119;bin[0x29]=119;
        put(f.actor,0x180,address(chain.data()));put(chain.data(),0x18,address(bin.data()));put(f.actor,0x108,address(bin.data()));
        put(f.input_state.data(),0x168,30.f);put(f.input_state.data(),0x60,address(core.data()));
        put(f.image.data(),0xB25D98,main_base+0x124D568);
        // Native FCC0/290FE8 use this projection and HUD view, not the scene view.
        const float cot=1.f/std::tan(15.f*3.14159265f/180.f);
        const re4::ViewMatrix4x4 projection{{{cot*.75f,0,0,0},{0,cot,0,0},
            {0,0,-.00010001f,-100.010001f},{0,0,-1,0}}};
        put(f.input_state.data(),0x10C,projection);
        const re4::ViewMatrix3x4 hud_view{{{1,0,0,0},{0,1,0,0},{0,0,1,-240*cot}}};
        put(f.image.data(),0x124D530,hud_view);put(f.image.data(),0xB275B8,main_base+0x124D530);
        sprite[0]=13;sprite[2]=0x21;sprite[3]=0x40;sprite[0x88]=0x39;sprite[0x9C]=0x16;
        re4::ViewMatrix3x4 world{{{1,0,0,210},{0,1,0,-170},{0,0,1,0}}};put(sprite.data(),8,world);
        root[0]=arc[0]=9;root[2]=arc[2]=0x21;root[4]=1;arc[3]=7;arc[0x88]=0x20;root[0x9C]=arc[0x9C]=0x16;
        put(arc.data(),8,world);put(arc.data(),0x68,address(root.data()));
        const float arc_corners[4][3]={{-10,10,0},{10,10,0},{10,-10,0},{-10,-10,0}};
        std::memcpy(arc.data()+0xBC,arc_corners,sizeof(arc_corners));
        put(root.data(),0x104,std::array<float,4>{255,255,255,255});put(arc.data(),0x104,std::array<float,4>{255,255,255,255});
        health_gauge_root=address(root.data());health_gauge_arc=address(arc.data());health_word_address=address(sprite.data());ammo_counter_native.sprite=health_gauge_sprite;
        const float corners[4][3]={{-40,15,0},{40,15,0},{-40,-15,0},{40,-15,0}};
        std::memcpy(sprite.data()+0xBC,corners,sizeof(corners));put(sprite.data(),0x104,std::array<float,4>{173,182,191,128});
        auto* font=f.image.data()+0x124FAB0;font[0]=1;font[0xFC]=16;font[0xFD]=24;
        put(font,0xF0,address(metrics.data()));put(font,0xF8,std::uint16_t(512));put(font,0xFA,std::uint16_t(768));
        for(auto& metric:metrics)metric={0,10};
        put(core.data(),0x54,std::uint32_t(0x200));put(core.data(),0x58,std::uint32_t(core.size()));
        auto* manager=f.image.data()+0x124FFB0;put(manager,0x20,address(core.data()+0x200));put(manager,0x44,1u);
        put(core.data()+0x200,4,0x110u);
        game_test::memory_maps={{main_base,f.image.size(),3},{address(f.actor),sizeof(f.actor),3},
            {address(f.input_state.data()),f.input_state.size(),3},{address(chain.data()),chain.size(),3},
            {address(bin.data()),bin.size(),3},{address(sprite.data()),sprite.size(),3},
            {address(metrics.data()),sizeof(metrics),3},{address(core.data()),core.size(),3}};
        game_test::memory_maps.push_back({address(root.data()),root.size(),3});game_test::memory_maps.push_back({address(arc.data()),arc.size(),3});
        viewport();game_test::memory_maps.push_back({address(software.data()),software.size(),3});
        apply(re4::CharacterAppearance::Merchant);
    }
    void viewport(){
        put(f.image.data(),0x1184150,address(software.data()));put(software.data(),0,0u);
        std::array<float,16> correction{};correction[0]=correction[5]=correction[10]=correction[15]=1;
        put(software.data(),0x440180,correction);
    }
    void apply(re4::CharacterAppearance appearance){
        auto& bank=character_banks[0];bank.phase=CharacterPhase::Active;bank.appearance=appearance;
        bank.token=77;bank.drawn=77;bank.fault=0;assert(character_identity(f.actor,bank.identity));
        character_desired=appearance;settings.character_appearance=appearance;
    }
    std::string latin()const{
        std::string result;for(const auto& p:health_packets){
            if(p.glyph>=39&&p.glyph<=64)result+=char('A'+p.glyph-39);
            else if(p.glyph>=65&&p.glyph<=90)result+=char('a'+p.glyph-65);
            else result+='?';
        }return result;
    }
    bool capture(bool native_bar=true){
        if(native_bar)ExtendedSquareDraw::Callback(arc.data());
        const auto before=health_original_words;ExtendedSquareDraw::Callback(sprite.data());
        assert(health_original_words==before||health_original_words==before+1);
        return health_original_words==before;
    }
    void draw(){const auto before=hud_native_calls;HudDraw::Callback(nullptr);assert(hud_native_calls==before+1);}
    void russian_bank(){
        auto* bank=core.data()+0x200;std::size_t cursor=0xA00;
        re4::HealthNameAlphabet alphabet;
        for(unsigned word=0;word<std::size(re4::health_name_calibration_ids);++word){
            const auto* expected=re4::health_name_calibration_words[word];
            put(bank,8+re4::health_name_calibration_ids[word]*8,std::uint64_t(cursor));
            put(bank,cursor,std::uint16_t(0));cursor+=2;
            for(unsigned i=0;expected[i];++i){
                std::uint16_t glyph=0;
                if(expected[i]!=u' '){
                    unsigned j=0;while(j<alphabet.count&&alphabet.letters[j]!=expected[i])++j;
                    glyph=j<alphabet.count?alphabet.glyphs[j]:std::uint16_t(128+alphabet.count);
                    assert(alphabet.add(expected[i],glyph));
                }
                put(bank,cursor,std::uint16_t(glyph+0x80));cursor+=2;
            }
            for(auto code:{14u,8u,1u}){put(bank,cursor,std::uint16_t(code));cursor+=2;}
        }
        settings.language=re4::Language::Russian;
    }
};
}
int main(){
    using re4::CharacterAppearance;
    {
        HealthFixture h;assert(!h.capture(false)&&health_original_words==1);
        assert(h.capture());h.draw();assert(h.latin()=="Merchant");
        ++view_frame;health_packets.clear();assert(!h.capture(false));h.draw();assert(health_packets.empty());
    }
    {
        HealthFixture h;h.put(h.arc.data(),0x110,32.f);assert(h.capture());
        h.put(h.arc.data(),0x110,8.f);h.draw();
        assert(h.latin()=="Merchant");for(const auto& p:health_packets)assert((p.rgba&255)==8);
    }
    {
        // Cached visible words cannot outlive native root hiding, effective
        // alpha reaching zero, the gauge sliding offscreen, or scene disposal.
        for(unsigned problem=0;problem<6;++problem){
            HealthFixture h;assert(h.capture());
            if(problem==0)h.root[0]=1;
            else if(problem==1)h.put(h.root.data(),0x110,0.f);
            else if(problem==2)h.put(h.arc.data(),0x110,0.f);
            else if(problem==3)h.put(h.arc.data(),0x14,1000.f);
            else if(problem==4)h.put(h.root.data(),0x68,h.address(h.root.data()));
            else h.put(h.f.image.data(),0xB25D98,std::uintptr_t(0));
            h.draw();assert(health_packets.empty());
            h.root[0]=9;h.put(h.root.data(),0x110,255.f);h.put(h.arc.data(),0x110,255.f);
            h.put(h.arc.data(),0x14,210.f);h.put(h.root.data(),0x68,std::uintptr_t(0));
            h.put(h.f.image.data(),0xB25D98,main_base+0x124D568);
            h.draw();assert(health_packets.empty());
        }
        HealthFixture h;h.root[0]=1;assert(!h.capture()&&health_original_words==1);
    }
    {
        // Failure or a changing cache at capture must preserve the native word,
        // even when the later draw can read a valid nonidentity C again.
        for(unsigned problem=0;problem<9;++problem){
            HealthFixture h;const auto correction=h.address(h.software.data())+0x440180;
            if(problem<2)health_viewport_test::fail(correction,problem+1);
            else if(problem==2)health_viewport_test::change(correction,.9f);
            else if(problem==3)h.put(h.f.image.data(),0x1184150,std::uintptr_t(0));
            else if(problem==4)h.put(h.software.data(),0,1u);
            else if(problem==5)health_viewport_test::change(h.address(h.software.data()),1u);
            else if(problem==6)health_viewport_test::change(main_base+0x1184150,std::uintptr_t(0));
            else if(problem==7)h.put(h.software.data(),0x440180,0.f);
            else h.put(h.software.data(),0x440180,std::numeric_limits<float>::quiet_NaN());
            assert(!h.capture()&&health_original_words==1);
            health_viewport_test::reset();h.viewport();h.put(h.software.data(),0x440180+5*4,4.f/3);
            h.draw();assert(health_packets.empty());
        }
    }
    {
        // A valid capture cannot authorize drawing under an unproved late C.
        for(unsigned problem=0;problem<7;++problem){
            HealthFixture h;assert(h.capture());const auto correction=h.address(h.software.data())+0x440180;
            if(problem<2)health_viewport_test::fail(correction,problem+1);
            else if(problem==2)health_viewport_test::change(correction,.9f);
            else if(problem==3)health_viewport_test::change(h.address(h.software.data()),1u);
            else if(problem==4)health_viewport_test::change(main_base+0x1184150,std::uintptr_t(0));
            else if(problem==5)h.put(h.software.data(),0,1u);
            else h.put(h.software.data(),0x440180,0.f);
            h.draw();assert(health_packets.empty());
            health_viewport_test::reset();h.viewport();h.draw();assert(health_packets.empty());
        }
    }
    {
        HealthFixture h;h.apply(CharacterAppearance::Ada);
        // Original core UWF root217,-130 + plaque13,20 + Leon word21,-42.
        // Texture anchor1 is reset by290C20 to local quad(0,0)..(48,-26).
        const re4::ViewMatrix3x4 native_word{{{1,0,0,251},{0,1,0,-152},{0,0,1,0}}};
        h.put(h.sprite.data(),8,native_word);h.sprite[0x96]=1;
        const float corners[4][3]={{0,0,0},{48,0,0},{48,-26,0},{0,-26,0}};
        std::memcpy(h.sprite.data()+0xBC,corners,sizeof(corners));
        std::vector<std::uint8_t> software(0x4401C0);
        h.put(h.f.image.data(),0x1184150,h.address(software.data()));
        // Actual previously observed native C: Y correction4/3. Native sprites
        // inherit it; cancelling it only for the label puts the label above.
        std::array<float,16> correction{};correction[0]=correction[10]=correction[15]=1;correction[5]=4.f/3;
        h.put(software.data(),0x440180,correction);
        game_test::memory_maps.push_back({h.address(software.data()),software.size(),3});
        assert(h.capture());h.draw();assert(h.latin()=="Ada");
        const auto& first=health_packets.front();const auto& last=health_packets.back();
        const double glyph_center=224*(1-double(correction[5])*(1-(2*double(first.y)+first.height)/384));
        const double native_center=224+double(correction[5])*165*(448.0/480);
        assert(std::fabs(glyph_center-native_center)<1.6);
        assert(std::abs(int(first.x)+int(last.x)+int(last.width)-952)<=2);
        // An off-centre native P must be followed too; FOV reconstruction loses it.
        auto projection=re4::ViewMatrix4x4{};std::memcpy(&projection,h.f.input_state.data()+0x10C,sizeof(projection));
        projection.m[1][2]=-.03f;h.put(h.f.input_state.data(),0x10C,projection);
        health_packets.clear();assert(h.capture());h.draw();
        const auto& shifted=health_packets.front();
        const double shifted_center=224*(1-double(correction[5])*(1-(2*double(shifted.y)+shifted.height)/384));
        assert(std::fabs(shifted_center-(native_center-224*double(correction[5])*.03))<1.6);
    }
    {
        HealthFixture h;const auto before=h.sprite;
        assert(h.capture());h.draw();assert(h.latin()=="Merchant");assert(h.sprite==before);
        for(const auto& p:health_packets)assert(p.font==main_base+0x124FAB0&&(p.rgba&255)==128&&p.visibility_override==1);
        const auto count=health_packets.size();h.draw();assert(health_packets.size()==count);
    }
    {
        constexpr const char* names[]={"", "Ashley", "Ada", "Luis", "Krauser", "Merchant", "Ramon", "Ganado"};
        for(unsigned i=1;i<8;++i){HealthFixture h;h.apply(static_cast<CharacterAppearance>(i));
            settings.language=re4::Language::Russian; // Unknown/English MDT must use the SAME font's canonical Latin letters.
            assert(h.capture());h.draw();assert(h.latin()==names[i]);}
    }
    {
        // A fully cross-validated live Russian alphabet is used where complete;
        // untranslated/unknown names still receive canonical Latin glyphs.
        for(auto appearance:{CharacterAppearance::Ada,CharacterAppearance::LuisSera,
                CharacterAppearance::Krauser,CharacterAppearance::RamonSalazar}){
            HealthFixture h;h.apply(appearance);h.russian_bank();re4::HealthNameAlphabet alphabet;
            assert(health_name_alphabet(alphabet));re4::HealthNameWord expected;
            assert(re4::health_name_word(appearance,true,alphabet,expected));
            if(appearance==CharacterAppearance::LuisSera)assert(alphabet.word(u"\u041b\u0443\u0438\u0441",expected));
            if(appearance==CharacterAppearance::RamonSalazar)assert(alphabet.word(u"\u0420\u0430\u043c\u043e\u043d",expected));
            assert(h.capture());h.draw();unsigned j=0;
            for(unsigned i=0;i<expected.count;++i)if(expected.glyphs[i])assert(health_packets[j++].glyph==expected.glyphs[i]);
            assert(j==health_packets.size()&&health_packets[0].glyph>=128);
        }
        HealthFixture h;h.apply(CharacterAppearance::Ashley);h.russian_bank();
        assert(h.capture());h.draw();assert(h.latin()=="Ashley"); // No guessed uppercase Э.
    }
    {
        HealthFixture h;h.apply(CharacterAppearance::Ada);h.russian_bank();
        re4::HealthNameAlphabet alphabet;assert(health_name_alphabet(alphabet));re4::HealthNameWord name;
        assert(re4::health_name_word(CharacterAppearance::Ada,true,alphabet,name));
        h.metrics[name.glyphs[0]]={0,0}; // Translated glyph missing in this font: Latin remains available.
        assert(h.capture());h.draw();assert(h.latin()=="Ada");
    }
    {
        HealthFixture h;h.russian_bank();
        // Corrupt one repeated letter in an independently indexed calibration word.
        const auto bank=h.core.data()+0x200;std::uint64_t at=0;std::memcpy(&at,bank+8+35*8,8);
        h.put(bank,std::size_t(at)+2+7*2,std::uint16_t(0x80+999));
        re4::HealthNameAlphabet alphabet;assert(!health_name_alphabet(alphabet));
        assert(h.capture());h.draw();assert(h.latin()=="Merchant");
    }
    {
        // Applied bank wins while another appearance is requested but not drawn.
        HealthFixture h;settings.character_appearance=CharacterAppearance::Ashley;
        assert(h.capture());h.draw();assert(h.latin()=="Merchant");
    }
    {
        auto visible=[](auto&& change){HealthFixture h;change(h);assert(!h.capture());h.draw();assert(health_packets.empty());};
        visible([](HealthFixture& h){h.apply(CharacterAppearance::Leon);});
        visible([](HealthFixture&){character_banks[0].phase=CharacterPhase::Probe;});
        visible([](HealthFixture&){character_banks[0].drawn=76;});
        visible([](HealthFixture&){character_banks[0].fault=1;});
        visible([](HealthFixture&){character_banks[0].identity.room=5;});
        visible([](HealthFixture&){character_renderer_ready=false;});
        visible([](HealthFixture& h){h.put(h.f.input_state.data(),0x7C,1u<<16);});
        visible([](HealthFixture& h){h.put(h.f.input_state.data(),0x7C,1u<<13);});
        visible([](HealthFixture& h){h.sprite[3]=0x41;});
        visible([](HealthFixture& h){h.sprite[2]=0x1C;});
        visible([](HealthFixture& h){h.sprite[3]=0xFF;}); // Untagged metal/plaque/background always reaches Orig.
        visible([](HealthFixture& h){h.sprite[0]=0xFF;});
        visible([](HealthFixture& h){h.sprite[0x9E]=1;}); // Another native draw path has a different projection.
        visible([](HealthFixture& h){h.put(h.f.image.data(),0xB275B8,std::uintptr_t(0));});
        visible([](HealthFixture& h){h.put(h.f.input_state.data(),0x10C,re4::ViewMatrix4x4{});});
        visible([](HealthFixture& h){h.put(h.sprite.data(),0x110,0.f);});
        visible([](HealthFixture& h){h.f.image[0x124FAB0]=0;});
        visible([](HealthFixture&){inventory_damage_glyph_native.draw=nullptr;});
        visible([](HealthFixture&){native_ortho=nullptr;});
    }
    {
        HealthFixture h;assert(h.capture());++view_frame;h.draw();assert(health_packets.empty());
        assert(h.capture());character_banks[0].phase=CharacterPhase::Retiring;h.draw();assert(health_packets.empty());
    }
    {
        HealthFixture h;assert(h.capture());auto* font=h.f.image.data()+0x124FAB0;
        font[0xFC]=8;h.draw();assert(health_packets.empty()); // Resource identity change invalidates queued letters.
        font[0xFC]=16;assert(h.capture());h.metrics[health_name_capture.line.glyphs[0].glyph]={0,9};
        h.draw();assert(health_packets.empty());
    }
    {
        HealthFixture h;assert(h.capture());h.put(h.f.input_state.data(),0x7C,1u<<16);
        h.draw();assert(health_packets.empty());h.put(h.f.input_state.data(),0x7C,0u);
        h.draw();assert(health_packets.empty()); // A discarded capture cannot leak after gauge restoration.
        assert(h.capture());h.draw();assert(h.latin()=="Merchant");
    }
    {
        HealthFixture h;assert(h.capture());character_teardown=true;h.draw();assert(health_packets.empty());
        character_teardown=false;h.draw();assert(health_packets.empty());
        h.f.image[0x124FAB0]=0;assert(!h.capture()&&health_original_words==1); // Failed preflight preserves Orig.
    }
    {
        HealthFixture h;assert(h.capture());h.draw();const auto first=health_packets;
        const re4::ViewMatrix3x4 smaller{{{.5f,0,0,210},{0,.5f,0,-170},{0,0,.5f,0}}};
        h.put(h.sprite.data(),8,smaller);health_packets.clear();assert(h.capture());h.draw();
        assert(health_packets.size()==first.size());
        assert(health_packets[0].height<first[0].height);
        const auto& a=health_packets.front();const auto& b=health_packets.back();
        assert(std::abs(int(a.x)+int(b.x)+int(b.width)-848)<=2); // Center remains native worldX210 -> HUD424.
        h.put(h.sprite.data(),0x110,31.f);health_packets.clear();assert(h.capture());h.draw();
        for(const auto& packet:health_packets)assert((packet.rgba&255)==31);
    }
    {
        // Viewport correction is sampled at late consumption, then converted to
        // native512x384 once. The glyph wrapper owns its own projection.
        HealthFixture h;std::vector<std::uint8_t> software(0x4401C0);
        h.put(h.f.image.data(),0x1184150,h.address(software.data()));
        std::array<float,16> correction{};correction[0]=correction[5]=correction[10]=correction[15]=1;
        h.put(software.data(),0x440180,correction);
        game_test::memory_maps.push_back({h.address(software.data()),software.size(),3});
        assert(h.capture());const auto initial=health_name_capture.line.glyphs[0];
        correction[12]=.125f;h.put(software.data(),0x440180,correction);h.draw();
        assert(int(health_packets[0].x)==int(initial.x)-32);
        assert(health_packets[0].y==initial.y&&health_packets[0].height==initial.height);
    }
    std::puts("health name: native corrected position, first names, applied owner, glyphs, fallback and visibility PASS");
}
