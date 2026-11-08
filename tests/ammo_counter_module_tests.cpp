// Case ammo counter: pure geometry/count rules and the actual HUD callback
// against a fake native gauge, inventory and weapon-data table.
#define main game_module_baseline_main
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#include "game_module_tests.cpp"
#pragma GCC diagnostic pop
#undef main

namespace {
std::array<std::array<std::uint8_t,0x170>,4> gauge_sprites{}; // ids 0, 0x0B, plate, health arc7
unsigned plate_id=0x40,gauge_capacity=10,gauge_class=1;
void* gauge_sprite(void*,unsigned id,unsigned group){
    assert(group==0x21);
    if(id==0)return gauge_sprites[0].data();
    if(id==0x0B)return gauge_sprites[1].data();
    if(id==7)return gauge_sprites[3].data();
    if(id==plate_id)return gauge_sprites[2].data();
    return reinterpret_cast<void*>(main_base+0x124D630);
}
void gauge_classify(unsigned,void* out){static_cast<std::uint8_t*>(out)[2]=std::uint8_t(gauge_class);}
unsigned gauge_capacity_of(unsigned,int){return gauge_capacity;}
struct CounterFixture {
    Fixture f;
    alignas(16) std::vector<std::uint8_t> state=std::vector<std::uint8_t>(0x5100);
    alignas(16) std::array<std::uint8_t,0x40> manager{};
    alignas(16) std::array<std::uint8_t,0xA0> hud{};
    alignas(16) std::array<std::uint8_t,14*6> slots{};
    std::vector<std::uint8_t> software=std::vector<std::uint8_t>(0x4401C0);
    template<class T>void put(void* p,std::size_t offset,T value){std::memcpy(static_cast<std::uint8_t*>(p)+offset,&value,sizeof(value));}
    std::uintptr_t address(const void* p){return reinterpret_cast<std::uintptr_t>(p);}
    void stack(unsigned i,std::uint16_t raw,std::uint16_t quantity,std::uint8_t flags,std::uint8_t owner,std::uint32_t packed=0){
        auto* r=slots.data()+i*14;put(r,0,raw);put(r,2,quantity);r[4]=flags;r[5]=owner;put(r,6,packed);
    }
    void sprite(unsigned i,float x,float y,float half_w,float half_h,std::array<float,4> rgba){
        auto& s=gauge_sprites[i];s.fill(0);s[0]=9;put(s.data(),0x14,x);put(s.data(),0x24,y);put(s.data(),0x34,0.f);
        put(s.data(),8,1.f);put(s.data(),0x1C,1.f);put(s.data(),0x30,1.f);
        s[2]=0x21;s[3]=i==0?0:i==1?0x0B:i==2?0x40:7;s[4]=i==0?1:0;s[0x9C]=0x16;s[0x88]=i==3?0x20:0x27;
        const float corners[4][3]={{-half_w,half_h,0},{half_w,half_h,0},{-half_w,-half_h,0},{half_w,-half_h,0}};
        std::memcpy(s.data()+0xBC,corners,sizeof(corners));std::memcpy(s.data()+0x104,rgba.data(),16);
    }
    CounterFixture(){
        ammo_counter_gauge_draw={};
        settings.case_ammo_counter=true;health_color_probe=true;gx_colors.clear();
        plate_id=0x40;gauge_capacity=10;gauge_class=1;
        ammo_counter_native={gauge_sprite,gauge_classify,gauge_capacity_of};
        put(f.image.data(),0xB1AE08,address(state.data()));
        put(f.image.data(),0xB25D98,address(manager.data()));
        put(f.image.data(),0xB25D80,address(hud.data()));hud[0x90]=0x31;
        put(state.data(),0x168,30.f);
        // Weapon-data row0: Handgun 0x23, variant0 -> Handgun Ammo 0x04.
        put(f.image.data(),0xA07CC6,std::uint16_t(0x23));put(f.image.data(),0xA07CCC,std::uint16_t(0x04));
        const auto inv=f.image.data()+kExtendedInventory;
        put(inv,0x20,address(slots.data()));put(inv,0x30,std::int32_t(6));inv[0x1B]=0;put(inv,0x18,std::uint16_t(0x23));
        stack(0,0x23,1,1,0,0x00A80005);put(inv,0x10,address(slots.data()));
        stack(1,0x04,30,1,0);stack(2,0x04,70,1,0);stack(3,0x04,5,1,2);stack(4,0x18,10,1,0);stack(5,0x04,99,0,0);
        sprite(0,0,-100,80,80,{255,255,255,200});
        sprite(1,150,-150,10,20,{73,214,230,180});
        sprite(2,200,-180,60,15,{255,255,255,255});
        sprite(3,180,-130,20,20,{255,255,255,255});
        put(gauge_sprites[3].data(),0x68,address(gauge_sprites[0].data()));
        const float cot=1.f/std::tan(15.f*3.14159265f/180.f);
        put(state.data(),0x10C,re4::ViewMatrix4x4{{{cot*.75f,0,0,0},{0,cot,0,0},{0,0,-.00010001f,-100.010001f},{0,0,-1,0}}});
        put(f.image.data(),0x124D530,re4::ViewMatrix3x4{{{1,0,0,0},{0,1,0,0},{0,0,1,-240*cot}}});
        put(f.image.data(),0xB275B8,main_base+0x124D530);
        put(f.image.data(),0x1184150,address(software.data()));
        std::array<float,16> correction{};correction[0]=correction[5]=correction[10]=correction[15]=1;
        put(software.data(),0x440180,correction);
        game_test::memory_maps={{main_base,f.image.size(),3},{address(state.data()),state.size(),3},
            {address(manager.data()),manager.size(),3},{address(hud.data()),hud.size(),3},
            {address(slots.data()),slots.size(),3},{address(software.data()),software.size(),3}};
        for(auto& s:gauge_sprites)game_test::memory_maps.push_back({address(s.data()),s.size(),3});
    }
    unsigned draw(bool native_bar=true){
        if(native_bar)ExtendedSquareDraw::Callback(gauge_sprites[3].data());
        gx_vertices=0;gx_colors.clear();const auto before=hud_native_calls;
        HudDraw::Callback(nullptr);assert(hud_native_calls==before+1);return gx_vertices;
    }
};
}

int main(){
    using namespace re4;
    {
        CounterFixture h;
        // Old cached root/digit/word records are not evidence of a native draw.
        assert(h.draw(false)==0);
        assert(h.draw()>0);++view_frame;
        assert(h.draw(false)==0);
        assert(h.draw()>0);
    }
    {
        CounterFixture h;h.put(gauge_sprites[3].data(),0x110,64.f);
        assert(h.draw()>0);for(const auto& c:gx_colors)assert(c[3]==64);
        // Parent changes after the arc callback must invalidate even this frame.
        gauge_sprites[0][0]=1;assert(h.draw(false)==0);gauge_sprites[0][0]=9;
        h.put(gauge_sprites[0].data(),0x110,0.f);assert(h.draw(false)==0);
        h.put(gauge_sprites[0].data(),0x110,255.f);assert(h.draw()>0);
    }
    {
        auto hidden=[](auto&& change){CounterFixture h;assert(h.draw()>0);change(h);assert(h.draw()==0);};
        hidden([](CounterFixture&){gauge_sprites[3][0]=0xFF;});
        hidden([](CounterFixture&){gauge_sprites[3][0x88]=0xFF;});
        hidden([](CounterFixture&){gauge_sprites[3][0x9E]=1;});
        hidden([](CounterFixture& h){h.put(gauge_sprites[3].data(),0x110,0.f);});
        hidden([](CounterFixture& h){h.put(gauge_sprites[3].data(),0x14,1000.f);});
        hidden([](CounterFixture& h){h.put(gauge_sprites[0].data(),0x68,h.address(gauge_sprites[0].data()));});
        hidden([](CounterFixture& h){h.put(h.f.image.data(),0xB25D98,std::uintptr_t(0));});
    }
    // Pure rules: native CountItem semantics and supported weapons.
    const AmmoCounterRecord records[]={{4,30,1,0},{4,70,1,0},{4,5,1,2},{0x18,10,1,0},{4,99,0,0}};
    assert(ammo_counter_total(records,5,4,0)==100&&ammo_counter_total(records,5,0x18,0)==10&&ammo_counter_total(records,5,7,0)==0);
    assert(ammo_counter_supported(0x23,4,10)&&!ammo_counter_supported(0x10,0x11,10)&&!ammo_counter_supported(0x52,0x72,10));
    assert(!ammo_counter_supported(0x34,0x6A,0x8000)&&!ammo_counter_supported(0x35,0xFF,1)&&!ammo_counter_supported(0x35,0xFFFF,1));
    // Geometry: number only, centered in a fixed three-digit field above the HUD.
    AmmoCounterQuad quads[kAmmoCounterMaxQuads];
    assert(ammo_counter_quads(100,400,380,20,quads)==2+6+6);
    assert(ammo_counter_quads(8,400,380,20,quads)==7);
    assert(ammo_counter_quads(12345,400,380,20,quads)==2+5+5+4+5); // actual five-digit quantity, no truncation
    assert(ammo_counter_digits(0)==1&&ammo_counter_digits(9)==1&&ammo_counter_digits(10)==2&&ammo_counter_digits(9999)==4&&ammo_counter_digits(10000)==5);
    {
        const unsigned n=ammo_counter_quads(100,400,380,20,quads);float max_x=0,min_x=512;
        for(unsigned i=0;i<n;++i)for(const auto& p:quads[i].p){max_x=std::max(max_x,p.x);min_x=std::min(min_x,p.x);assert(p.y>=360-1.5f&&p.y<=380+1.5f);} // half stroke beyond the cap height
        assert(max_x<=400+.06f*20*kAmmoCounterAspect&&max_x>=400-1.f&&min_x>=400-ammo_counter_width(3,20)-1.f);
    }
    {
        // The middle digit of 888 occupies exactly the same location as 8;
        // both share the original three-digit field center, with no zero padding.
        AmmoCounterQuad single[kAmmoCounterMaxQuads],triple[kAmmoCounterMaxQuads],pair[kAmmoCounterMaxQuads];
        assert(ammo_counter_quads(8,400,380,20,single)==7);
        assert(ammo_counter_quads(888,400,380,20,triple)==21);
        assert(ammo_counter_quads(88,400,380,20,pair)==14);
        for(unsigned s=0;s<7;++s)for(unsigned p=0;p<4;++p){
            assert(std::fabs(single[s].p[p].x-triple[7+s].p[p].x)<.0001f);
            assert(single[s].p[p].y==triple[7+s].p[p].y);
            assert(std::fabs(pair[s].p[p].x-triple[s].p[p].x-ammo_counter_digit_advance(20)*.5f)<.0001f);
        }
        assert(ammo_counter_quads(0,400,380,20,single)==6);
    }
    assert(!ammo_counter_quads(100,520,380,20,quads)&&!ammo_counter_quads(100,400,10,20,quads)&&!ammo_counter_quads(1,400,380,NAN,quads));
    {
        CounterFixture h;
        // 30+70 of Handgun Ammo with the inventory owner; other owner/type/invalid excluded.
        assert(h.draw()==(2+6+6)*4);
        for(const auto& c:gx_colors)assert(c[0]==73&&c[1]==214&&c[2]==230&&c[3]==180);
    }
    {
        CounterFixture h;
        // 290D28..290D4C: state+7C bit16 skips native render layer0x16,
        // including the gauge created at 1E7A8..1E7D0. Its sprite flags and
        // cached alpha remain visible, so those alone cannot gate our draw.
        assert(h.draw()>0);
        const auto sprites_before=gauge_sprites;
        h.put(h.state.data(),0x7C,std::uint32_t(1u<<16));
        assert(h.draw()==0);
        assert(gauge_sprites==sprites_before);
        std::uint32_t native_flags=0;std::memcpy(&native_flags,h.state.data()+0x7C,sizeof(native_flags));
        assert(native_flags==(1u<<16));
        // The separate frame gate override still cannot bypass this renderer gate.
        h.put(h.state.data(),0x78,std::uint32_t(1u<<20));h.state[0x5092]=4;
        assert(h.draw()==0);
        h.put(h.state.data(),0x7C,std::uint32_t(0));
        assert(h.draw()>0);
        // Adjacent bits do not trigger the native gauge-layer suppression.
        for(unsigned bit:{12u,14u,15u,17u}){
            h.put(h.state.data(),0x7C,std::uint32_t(1u<<bit));assert(h.draw()>0);
        }
    }
    {
        // Every gate hides the counter without touching native state.
        // Each case owns its whole fake image (Fixture rebinds main_base).
        auto hidden=[&](auto&& change){CounterFixture g;change(g);assert(g.draw()==0);};
        hidden([](CounterFixture& g){settings.case_ammo_counter=false;(void)g;});
        hidden([](CounterFixture& g){g.put(g.state.data(),0x7C,std::uint32_t(1u<<13));});
        hidden([](CounterFixture& g){g.put(g.state.data(),0x78,std::uint32_t(1u<<20));});
        hidden([](CounterFixture& g){g.put(g.manager.data(),0x30,std::uint32_t(0x40000000));});
        hidden([](CounterFixture& g){g.hud[0x90]=0xFF;});
        hidden([](CounterFixture& g){g.hud[0x90]=0x80;});
        hidden([](CounterFixture& g){gauge_sprites[0][0]=8;(void)g;});
        hidden([](CounterFixture& g){gauge_sprites[1][0]=1;(void)g;});
        hidden([](CounterFixture& g){g.put(gauge_sprites[1].data(),0x110,0.f);});
        hidden([](CounterFixture& g){plate_id=0x41;(void)g;});
        hidden([](CounterFixture& g){gauge_capacity=0x8000;(void)g;});
        hidden([](CounterFixture& g){gauge_class=3;(void)g;});
        hidden([](CounterFixture& g){(g.f.image.data()+kExtendedInventory)[0x1B]=1;});
        hidden([](CounterFixture& g){g.put(g.f.image.data()+kExtendedInventory,0x18,std::uint16_t(0x10));});
        hidden([](CounterFixture& g){g.put(g.f.image.data(),0xA07CCC,std::uint16_t(0xFFFF));});
        hidden([](CounterFixture& g){g.put(g.f.image.data()+kExtendedInventory,0x10,std::uintptr_t(0));});
        hidden([](CounterFixture& g){g.put(g.f.image.data(),0xB25D98,std::uintptr_t(0));});
    }
    {
        CounterFixture h;
        // Gate state byte5092 bit2 re-enables the frame gate.
        h.put(h.state.data(),0x78,std::uint32_t(1u<<20));h.state[0x5092]=4;assert(h.draw()>0);
        // Character plate map: Ashley's (503C=1) plate is sprite 0x41.
        h.state[0x503C]=1;plate_id=0x41;assert(h.draw()>0);h.state[0x503C]=6;assert(h.draw()==0);
    }
    {
        CounterFixture h;
        // Empty case: red with the native fade alpha.
        h.stack(1,0x04,0,1,0);h.stack(2,0x04,0,1,0);
        assert(h.draw()==6*4);
        for(const auto& c:gx_colors)assert(c[0]==194&&c[1]==80&&c[2]==74&&c[3]==180);
    }
    std::puts("case ammo counter: native count, layer visibility, gates, fade and geometry PASS");
}
