#define main game_module_baseline_main
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#include "game_module_tests.cpp"
#pragma GCC diagnostic pop
#undef main

namespace {
float native_seen_yaw=0;
struct CaseFixture {
    Fixture f;
    alignas(32) std::uint8_t model[0x3C8]{},part[0x1A8]{},archive[0x200]{},bones[119*16]{};
    CaseFixture(){
        character_appearance_reset();character_renderer_ready.store(true);
        f.view_mode();settings.stick_curve=re4::StickCurve::Linear;
        settings.character_appearance=re4::CharacterAppearance::Leon;
        character_store(f.input_state.data(),0x70,static_cast<void*>(archive));
        character_store(archive,0x10,0x100u);archive[0x129]=119;
        character_store(model,0,main_base+0xAFF538);character_store(model,8,1u);model[0x11A]=119;
        character_store(model,0x180,static_cast<void*>(part));character_store(model,0x108,static_cast<void*>(bones));
        character_store(part,0x18,static_cast<void*>(archive+0x100));
        character_store(f.image.data(),0xAFF4A8+8,static_cast<void*>(model));
        character_store(f.image.data(),0xAFF4A8+0x10,1u);character_store(f.image.data(),0xAFF4A8+0x14,0x3C8u);
        character_store(f.image.data(),0x1272140,static_cast<void*>(model));
        for(const auto& region:std::vector<MemoryInfo>{{main_base,f.image.size(),1},
            {reinterpret_cast<std::uintptr_t>(f.actor),sizeof(f.actor),1},
            {reinterpret_cast<std::uintptr_t>(f.input_state.data()),f.input_state.size(),1},
            {reinterpret_cast<std::uintptr_t>(model),sizeof(model),1},{reinterpret_cast<std::uintptr_t>(part),sizeof(part),1},
            {reinterpret_cast<std::uintptr_t>(archive),sizeof(archive),1}})game_test::memory_maps.push_back(region);
        CharacterIdentity identity;assert(character_menu_identity(model,identity));
        game_test::tick=native_tick_hz;view_frame.store(100);
        modern_root_observer=+[](void* actor){native_seen_yaw=character_value<float>(actor,0xB4);};
    }
    // Native Ada factory494BF4: costume0/2 uses pl0b (81 bones),
    // costume1 pl0c and costume3 pl16 use100; both Ada campaigns use type2.
    void ada(unsigned costume,unsigned count){
        f.input_state[0x503C]=2;f.input_state[0x503D]=std::uint8_t(costume);
        model[0x11A]=std::uint8_t(count);archive[0x129]=std::uint8_t(count);
    }
    float yaw()const{return character_value<float>(model,0xB4);}
    void pad(int x,unsigned attributes=1){
        NpadState value{};value.attributes=attributes;value.sample=view_frame.load();
        value.rx=x;value.ry=23456;value.lx=-12345;value.ly=21000;value.buttons=0xA5;
        capture(value,0x20,2);
    }
    void step(int x,std::uint64_t delta=native_tick_hz/60,bool sample=true){
        game_test::tick+=delta;view_frame.fetch_add(1);if(sample)pad(x);
        const auto before=modern_original_roots;assert(ModernRoot::Callback(model,nullptr)==17);
        assert(modern_original_roots==before+1);
    }
};
float rotate(unsigned frames){
    CaseFixture x;x.step(32767);
    for(unsigned i=0;i<frames;++i)x.step(32767,native_tick_hz/frames);
    return x.yaw();
}
}
int main(){
    {
        CaseFixture x;x.step(32767);const auto before=x.yaw();x.step(32767);
        assert(x.yaw()>before); // RED: original callback never rotates the case body.
        assert(near(native_seen_yaw,x.yaw())); // Native model update receives new yaw.
        const auto held=x.yaw();x.step(0);assert(x.yaw()==held);
        x.step(3276);assert(x.yaw()==held); // Within configured deadzone.
        x.step(-32767);assert(x.yaw()<held);
        const auto left=x.yaw();x.step(16384);assert(x.yaw()>left&&x.yaw()-left<held-before);
        std::int32_t right_x=0;assert(menu_read_input(game_test::tick,right_x)&&right_x==16384);
        NpadState secondary{};secondary.attributes=1;secondary.rx=-32767;
        capture(secondary,1,1);assert(menu_read_input(game_test::tick,right_x)&&right_x==16384);
        secondary.attributes=0;capture(secondary,0,1);
        assert(menu_read_input(game_test::tick,right_x)&&right_x==16384); // Another style cannot disconnect the active handheld.

        const auto once=x.yaw();
        x.pad(-32767);ModernRoot::Callback(x.model,nullptr);assert(x.yaw()==once);
        const auto player=std::vector<std::uint8_t>(x.f.actor,x.f.actor+sizeof(x.f.actor));
        ModernRoot::Callback(x.f.actor,nullptr);assert(std::memcmp(player.data(),x.f.actor,player.size())==0);
    }
    // Regression: changing actual503C to Ada previously disabled rotation
    // even though the exact native case body owns the current Ada archiveBIN0.
    for(const auto costume:{0u,1u,2u,3u}){
        CaseFixture x;x.ada(costume,costume==1||costume==3?100:81);
        CharacterIdentity cosmetic;assert(!character_menu_identity(x.model,cosmetic));
        const auto actor=std::vector<std::uint8_t>(x.f.actor,x.f.actor+sizeof(x.f.actor));
        x.step(32767);x.step(32767);assert(x.yaw()>0);
        assert(near(native_seen_yaw,x.yaw()));
        const auto held=x.yaw();x.step(0);assert(x.yaw()==held);
        x.step(-32767);assert(x.yaw()<held);
        x.step(32767);const auto once=x.yaw();x.pad(-32767);
        ModernRoot::Callback(x.model,nullptr);assert(x.yaw()==once);
        ModernRoot::Callback(x.f.actor,nullptr);
        assert(std::memcmp(actor.data(),x.f.actor,actor.size())==0);
        // Rotation may write only the authenticated preview's yaw.
        const auto body=std::vector<std::uint8_t>(x.model,x.model+sizeof(x.model));
        x.step(32767);auto expected=body;
        std::memcpy(expected.data()+0xB4,x.model+0xB4,sizeof(float));
        assert(std::memcmp(expected.data(),x.model,expected.size())==0);
    }
    // An Ada-shaped actor or a mismatched archive must never broaden ownership.
    for(unsigned bad=0;bad<14;++bad){
        CaseFixture x;x.ada(3,100);x.step(32767);const auto before=x.yaw();
        if(bad==0)x.f.input_state[0x503C]=1;
        if(bad==1)x.f.input_state[0x503C]=3;
        if(bad==2)x.f.input_state[0x503C]=4;
        if(bad==3)x.f.input_state[0x503C]=5;
        if(bad==4)x.f.input_state[0x503D]=255;
        if(bad==5)x.model[0x11A]=81;
        if(bad==6)x.archive[0x129]=81;
        if(bad==7)character_store(x.archive,0x10,0x110u);
        if(bad==8)character_store(x.f.image.data(),0x1272140,static_cast<void*>(x.part));
        if(bad==9)character_store(x.f.image.data(),0xAFF4A8+0x14,0x3C0u);
        if(bad==10)character_store(x.model,0,main_base+0xAFF530);
        if(bad==11)x.model[0x118]=2;
        if(bad==12)character_store(x.model,0x188,static_cast<void*>(x.part));
        if(bad==13)character_renderer_ready.store(false);
        x.step(32767);assert(x.yaw()==before);
    }
    const auto at30=rotate(30),at60=rotate(60),at120=rotate(120);
    assert(near(at30,2.0943951f)&&near(at30,at60)&&near(at60,at120));
    for(unsigned appearance=0;appearance<=7;++appearance){
        CaseFixture x;settings.character_appearance=static_cast<re4::CharacterAppearance>(appearance);
        x.step(32767);x.step(32767);assert(x.yaw()>0);
    }
    {
        CaseFixture x;x.step(32767);x.step(32767);auto before=x.yaw();
        x.step(32767,native_tick_hz/5,false);assert(x.yaw()==before); // Stale input and long pause.
        x.step(32767);assert(x.yaw()>before);
        x.step(32767,native_tick_hz*6/100,false);before=x.yaw();
        x.step(32767,native_tick_hz*6/100,false);assert(x.yaw()==before); // Stale held sample with a normal frame gap.
        x.step(32767);
        before=x.yaw();game_test::tick+=native_tick_hz/60;view_frame.fetch_add(1);x.pad(32767,0);
        ModernRoot::Callback(x.model,nullptr);assert(x.yaw()==before);
        x.step(32767);assert(x.yaw()>before);
        before=x.yaw();character_menu_invalidate();x.step(32767);assert(x.yaw()==before);x.step(32767);assert(x.yaw()>before);
        before=x.yaw();game_test::tick-=native_tick_hz;view_frame.fetch_add(1);x.pad(32767);
        ModernRoot::Callback(x.model,nullptr);assert(x.yaw()==before);
        x.step(32767);assert(x.yaw()>before);
    }
    for(unsigned bad=0;bad<6;++bad){
        CaseFixture x;x.step(32767);const auto before=x.yaw();
        if(bad==0)character_store(x.f.image.data(),0xAFF4A8+0x14,0x3C0u);
        if(bad==1)character_store(x.model,0,main_base+0xAFF530);
        if(bad==2)x.model[0x118]=1;
        if(bad==3)character_store(x.f.image.data(),0x1272140,static_cast<void*>(x.part));
        if(bad==4)character_renderer_ready.store(false);
        if(bad==5)character_store(x.model,0xB4,std::numeric_limits<float>::quiet_NaN());
        x.step(32767);
        assert(bad==5?std::isnan(x.yaw()):x.yaw()==before);
    }
    std::puts("case rotation callback: native yaw, all appearances, timing, input isolation and identity guards PASS");
}
