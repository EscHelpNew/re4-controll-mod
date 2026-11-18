#define main game_module_baseline_main
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#include "game_module_tests.cpp"
#pragma GCC diagnostic pop
#undef main

namespace {
struct BossHealthFixture {
    Fixture f;
    std::vector<std::uint8_t> pool=std::vector<std::uint8_t>(7*0x10C8);
    std::array<std::array<std::uint8_t,0x1F0>,7> bones{};
    template<class T>void set(void* p,unsigned offset,T value){std::memcpy(static_cast<std::uint8_t*>(p)+offset,&value,sizeof(value));}
    std::uint8_t* actor(unsigned slot){return pool.data()+slot*0x10C8;}
    std::uintptr_t address(unsigned slot){return reinterpret_cast<std::uintptr_t>(actor(slot));}
    void model(unsigned slot,unsigned id,unsigned subtype,std::uintptr_t vptr,int current,int maximum){
        auto* value=actor(slot);set(value,0,main_base+vptr);set(value,8,1u);
        value[0x118]=std::uint8_t(id);value[0x119]=std::uint8_t(subtype);value[0x11A]=1;
        set(value,0x3C2,std::int16_t(current));set(value,0x3C4,std::int16_t(maximum));
        set(value,0xA4,re4::ViewVec3{float(slot*20),0,-1000});
        set(value,0x108,reinterpret_cast<std::uintptr_t>(bones[slot].data()));
        set(bones[slot].data(),0x80,re4::ViewVec3{float(slot*20),float(100+slot*10),-1000});
    }
    BossHealthFixture(){
        f.view_mode();f.aim();settings.enemy_health_bars=true;settings.enemy_health_numbers=false;health_color_probe=true;
        set(f.image.data(),0xAFF39C,2);
        set(f.image.data()+0xAE6EF8,8,reinterpret_cast<std::uintptr_t>(pool.data()));
        set(f.image.data()+0xAE6EF8,0x10,7u);set(f.image.data()+0xAE6EF8,0x14,0x10C8u);
        game_test::memory_maps={{reinterpret_cast<std::uintptr_t>(f.image.data()),f.image.size(),1},
            {reinterpret_cast<std::uintptr_t>(f.weapon),sizeof(f.weapon),1},
            {reinterpret_cast<std::uintptr_t>(pool.data()),pool.size(),1},
            {reinterpret_cast<std::uintptr_t>(bones.data()),sizeof(bones),1}};
        publish_view_settings();
    }
    void saddler(){
        model(1,0x31,0,0xAEA170,2500,10000);model(2,0x31,1,0xAEA170,2500,5000);
        set(actor(1),0xBCC,address(2));set(actor(2),0xBC4,address(1));
    }
    void salazar(unsigned subtype){
        model(1,0x38,subtype,0xAEBC60,200,200);model(2,0x38,3,0xAEBC60,2500,5000);
        set(actor(1),0xAFC,address(2));
        const unsigned reciprocal[]={0xAE4,0xAEC,0xAF4,0,0xB04};
        assert(subtype<5&&subtype!=3);set(actor(2),reciprocal[subtype],address(1));
    }
    bool capture(unsigned slot=1){
        set(f.weapon,0x418,address(slot));WeaponLaser::Callback(f.weapon,1,0,0);
        return enemy_health_point.frame.load()!=0;
    }
    std::uintptr_t resolve(unsigned slot=1){
        EnemyHealthReader reader;return enemy_health_resolve_owner(reader,address(slot),
            reinterpret_cast<std::uintptr_t>(pool.data()),7,0x10C8);
    }
    unsigned current()const{return enemy_health_point.health.load()&0xFFFF;}
    unsigned maximum()const{return enemy_health_point.health.load()>>16;}
};
}
int main(){
    {
        BossHealthFixture h;h.saddler();assert(h.capture());
        assert(h.current()==2500&&h.maximum()==5000); // Body's10000 max belongs to a different native owner.
        assert(h.resolve()==h.address(2));
        assert(view_float_value(enemy_health_point.y.load())==210); // Keep the aimed body's anchor.
        const auto key=enemy_health_point.target_key.load();assert(key);
        const auto before=h.pool;assert(h.capture(2)&&h.current()==2500&&h.maximum()==5000);
        assert(enemy_health_point.target_key.load()==key&&h.pool==before);
    }
    for(unsigned subtype:{0u,1u,2u,4u}){
        BossHealthFixture h;h.salazar(subtype);assert(h.capture());
        assert(h.current()==2500&&h.maximum()==5000); // Tentacle200HP is not the boss's core health.
        assert(h.resolve()==h.address(2));
        const auto key=enemy_health_point.target_key.load();assert(h.capture(2));
        assert(enemy_health_point.target_key.load()==key);
    }
    for(bool salazar:{false,true}){
        // Native's HP owner can be excluded from ray selection while the aimed
        // part is visible. It supplies counters, without becoming a draw target.
        BossHealthFixture h;if(salazar)h.salazar(1);else h.saddler();
        h.set(h.actor(2),8,0x10000001u);
        const auto before=h.pool;assert(h.capture());
        assert(h.current()==2500&&h.maximum()==5000&&h.pool==before);
        assert(!h.capture(2)); // The same hidden core cannot itself be aimed.
    }
    for(bool salazar:{false,true})for(unsigned bad=0;bad<24;++bad){
        BossHealthFixture h;if(salazar)h.salazar(1);else h.saddler();
        const unsigned link=salazar?0xAFC:0xBCC,back=salazar?0xAEC:0xBC4;
        if(bad==0)h.set(h.actor(1),link,std::uintptr_t(0));
        if(bad==1)h.set(h.actor(1),link,h.address(1));
        if(bad==2)h.set(h.actor(1),link,h.address(7));
        if(bad==3)h.set(h.actor(1),link,h.address(2)+1);
        if(bad==4){
            h.model(0,salazar?0x38:0x31,salazar?3:1,salazar?0xAEBC60:0xAEA170,2500,5000);
            h.set(h.actor(0),back,h.address(1));h.set(h.actor(1),link,h.address(0));
        }
        if(bad==5)h.actor(2)[0x118]=0x10; // Slot reused by another class.
        if(bad==6)h.actor(2)[0x119]=salazar?4:0;
        if(bad==7)h.set(h.actor(2),0,main_base+0xAEA178);
        if(bad==8)h.set(h.actor(2),8,0u);
        if(bad==9)h.set(h.actor(2),8,0x201u);
        if(bad==10)h.set(h.actor(2),0x3C2,std::int16_t(0));
        if(bad==11)h.set(h.actor(2),0x3C2,std::int16_t(-1));
        if(bad==12)h.set(h.actor(2),0x3C4,std::int16_t(0));
        if(bad==13)h.set(h.actor(2),0x3C4,std::int16_t(-1));
        if(bad==14)h.set(h.actor(2),back,std::uintptr_t(0));
        if(bad==15)h.set(h.actor(2),back,h.address(4)); // Reused owner links to another instance.
        if(bad==16)h.set(h.actor(1),0,main_base+0xAEA178);
        if(bad==17)h.actor(1)[0x119]=salazar?5:2;
        if(bad==18)h.set(h.actor(1),8,0x10000001u);
        if(bad==19)h.set(h.f.image.data()+0xAE6EF8,0x10,257u);
        if(bad==20)h.set(h.f.image.data()+0xAE6EF8,0x14,0x10C0u);
        if(bad==21)game_test::memory_maps[2]={h.address(1),0x10C8,1}; // Owner unreadable.
        if(bad==22)h.set(h.actor(1),link,std::numeric_limits<std::uintptr_t>::max());
        if(bad==23){
            h.model(3,salazar?0x38:0x31,salazar?3:1,salazar?0xAEBC60:0xAEA170,2500,5000);
            h.set(h.actor(1),link,h.address(3)); // Valid new core, no reciprocal owner.
        }
        const auto before=h.pool;const bool captured=h.capture();
        if(captured)std::fprintf(stderr,"boss invalid family%u gate%u\n",unsigned(salazar),bad);
        assert(!captured&&h.pool==before);
    }
    for(const auto id:{0x10u,0x2Bu,0x2Cu,0x2Fu,0x32u,0x35u,0x39u,0x3Fu,0x4Fu}){
        BossHealthFixture h;h.model(1,id,0,0x123456,75,100);
        const auto before=h.pool;assert(h.resolve()==h.address(1)&&h.capture());
        assert(h.current()==75&&h.maximum()==100&&h.pool==before);
    }
    // Exact self cores use their own native current/max without requiring a
    // different body to exist. Dead and wrongly typed cores cannot leak through.
    for(bool salazar:{false,true}){
        BossHealthFixture h;h.model(1,salazar?0x38:0x31,salazar?3:1,salazar?0xAEBC60:0xAEA170,75,100);
        assert(h.resolve()==h.address(1)&&h.capture()&&h.current()==75&&h.maximum()==100);
        h.set(h.actor(1),0x3C2,std::int16_t(0));assert(!h.resolve()&&!h.capture());
    }
    std::puts("boss health owners: Saddler/Salazar core snapshots, reciprocal guards, hidden counters and ordinary targets PASS");
}
