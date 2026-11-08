// Exercise the actual inline callback; native grant/inventory code stays native.
#define main game_module_baseline_main
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#include "game_module_tests.cpp"
#pragma GCC diagnostic pop
#undef main

namespace {
unsigned pickup_sound_calls=0,pickup_class=1,pickup_last_sound=0;
void pickup_classify(unsigned id,void* out){
    assert(id<=0x10F);static_cast<std::uint8_t*>(out)[2]=static_cast<std::uint8_t>(pickup_class);
}
unsigned pickup_sound(unsigned bank,unsigned sound,const void* position,unsigned a,unsigned b,const void* c){
    // Native Yes handler order: bit8 cue0xF always precedes bit9 cue0x11.
    assert(bank==0&&!position&&!a&&!b&&!c);
    assert(sound==0xF?pickup_last_sound!=0xF:sound==0x11&&pickup_last_sound==0xF);
    pickup_last_sound=sound;++pickup_sound_calls;return 17;
}
struct PickupFixture {
    Fixture f;
    alignas(8) std::array<std::uint8_t,0xA8> item{};
    alignas(8) std::array<std::uint8_t,0xC0> fades{};
    exl::hook::InlineFloatCtx context{};
    std::uint8_t* controller(){return f.image.data()+0xB00648;}
    template<class T>void set(void* p,unsigned offset,T value){std::memcpy(static_cast<std::uint8_t*>(p)+offset,&value,sizeof(value));}
    template<class T>T get(const void* p,unsigned offset){T value;std::memcpy(&value,static_cast<const std::uint8_t*>(p)+offset,sizeof(value));return value;}
    void reg(unsigned index,std::uintptr_t value){set(context.bytes,0x200+index*8,value);}
    PickupFixture(){
        settings.skip_pickup_confirmation=true;
        pickup_sound_calls=0;pickup_class=1;pickup_last_sound=0;
        pickup_confirmation_native={pickup_classify,pickup_sound};
        std::memset(context.bytes,0xA5,sizeof(context.bytes));
        std::memset(controller(),0x5A,0x150);
        set(controller(),0x1C,0x87654321u);set(controller(),0x24,0xF0F00F0Fu);controller()[0x149]=0;
        set(item.data(),0x80,reinterpret_cast<std::uintptr_t>(f.other_model));
        set(item.data(),0x94,std::uint16_t(1));set(item.data(),0x98,std::uint16_t(12));
        set(f.image.data(),0xB25D90,reinterpret_cast<std::uintptr_t>(controller()));
        set(f.image.data(),0xB260C8,reinterpret_cast<std::uintptr_t>(fades.data()));
        fades.fill(0x5A);
        set(fades.data(),0,std::uint32_t(0x20000000));set(fades.data(),4,std::uint32_t(0));
        set(fades.data(),8,std::uint32_t(0x20000000));set(fades.data(),0x18,std::uint16_t(1));
        set(fades.data(),0x1C,std::uint32_t(5));
        reg(19,reinterpret_cast<std::uintptr_t>(item.data()));reg(21,reinterpret_cast<std::uintptr_t>(controller()));reg(27,1);
        game_test::memory_maps={{main_base,f.image.size(),3},{reinterpret_cast<std::uintptr_t>(item.data()),item.size(),1}};
        game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(fades.data()),fades.size(),3});
    }
    void invoke(){PickupConfirmation::Callback(&context);}
    void unchanged(){
        const auto image=f.image;
        const auto before_item=item;
        const auto before_context=context;
        const auto before_fades=fades;
        invoke();
        assert(f.image==image&&item==before_item&&fades==before_fades&&!std::memcmp(&context,&before_context,sizeof(context)));
    }
};
}

int main(){
    assert(pickup_confirmation_ready(0,1,0,1,1,0));
    assert(pickup_confirmation_ready(0x10F,1,1,0xFFFFFFFFu,0xFFFFFFFFu,0));
    assert(!pickup_confirmation_ready(0x110,1,1,1,1,0));
    assert(!pickup_confirmation_ready(1,0,1,1,1,0));
    assert(!pickup_confirmation_ready(1,1,2,1,1,0));
    assert(!pickup_confirmation_ready(1,1,1,2,1,0));
    assert(!pickup_confirmation_ready(1,1,1,1,2,0));
    assert(!pickup_confirmation_ready(1,1,1,1,1,1));
    {
        PickupFixture h;h.set(h.item.data(),0x94,std::uint16_t(3));
        const auto context=h.context;const auto item=h.item;
        h.invoke();assert(pickup_sound_calls==0); // A Yes response alone has not granted a firearm.
        PickupSuccess::Callback(&h.context);
        assert(pickup_sound_calls==1); // Only native successful take restores native message sound0:0xF.
        PickupSuccess::Callback(&h.context);assert(pickup_sound_calls==1);
        assert(h.item==item&&!std::memcmp(&h.context,&context,sizeof(context)));
    }
    for(unsigned gate=0;gate<15;++gate){
        PickupFixture h;h.set(h.item.data(),0x94,std::uint16_t(3));
        if(gate==0)h.reg(27,0); // Pregranted/informational messages do not arm a sound.
        if(gate==1)settings.skip_pickup_confirmation=false;
        if(gate==2)pickup_confirmation_native.sound=nullptr;
        if(gate==3)pickup_confirmation_native.classify=nullptr;
        if(gate==4)h.set(h.item.data(),0x94,std::uint16_t(1));
        if(gate==5)h.set(h.item.data(),0x94,std::uint16_t(2));
        if(gate==6)h.set(h.item.data(),0x94,std::uint16_t(14));
        if(gate==7)pickup_class=6; // Recovery items use no message cue.
        if(gate==8)pickup_class=9; // Eggs/other native throwable class.
        h.invoke();assert(pickup_sound_calls==0);
        if(gate==9)settings.skip_pickup_confirmation=false;
        if(gate==10)h.set(h.item.data(),0x80,std::uintptr_t(0));
        if(gate==11)h.set(h.item.data(),0x94,std::uint16_t(4));
        if(gate==12)h.reg(19,reinterpret_cast<std::uintptr_t>(h.item.data()+8));
        if(gate==13)game_test::memory_maps[1].perm=0;
        if(gate==14)h.invoke(); // Rejection/full inventory followed by another entry cannot retain the old arm.
        PickupSuccess::Callback(&h.context);assert(pickup_sound_calls==0);
    }
    {
        PickupFixture h;h.set(h.item.data(),0x94,std::uint16_t(0x17));pickup_class=3;
        h.invoke();PickupSuccess::Callback(&h.context);assert(pickup_sound_calls==1);
    }
    {
        // Ammunition message0x211: native Yes plays0xF then0x11, once per take.
        PickupFixture h;h.set(h.item.data(),0x94,std::uint16_t(4));pickup_class=2;
        h.invoke();assert(pickup_sound_calls==0);
        PickupSuccess::Callback(&h.context);assert(pickup_sound_calls==2&&pickup_last_sound==0x11);
        PickupSuccess::Callback(&h.context);assert(pickup_sound_calls==2);
    }
    {
        PickupFixture h;h.set(h.item.data(),0x94,std::uint16_t(4));pickup_class=2;h.reg(27,0);
        h.invoke();PickupSuccess::Callback(&h.context);assert(pickup_sound_calls==0); // Pregranted ammo.
    }
    {
        PickupFixture h;h.set(h.item.data(),0x94,std::uint16_t(3));
        h.invoke(); // Native full inventory/rejection never reaches the success boundary.
        assert(pickup_sound_calls==0);
        h.reg(19,reinterpret_cast<std::uintptr_t>(h.item.data()+8));
        PickupSuccess::Callback(&h.context);assert(pickup_sound_calls==0);
        h.reg(19,reinterpret_cast<std::uintptr_t>(h.item.data()));
        PickupSuccess::Callback(&h.context);assert(pickup_sound_calls==1);
    }
    for(unsigned gate=0;gate<9;++gate){
        PickupFixture h;
        if(gate==0)h.set(h.fades.data(),0,std::uint32_t(0x40000000));
        if(gate==1)h.set(h.fades.data(),4,std::uint32_t(1));
        if(gate==2)h.set(h.fades.data(),8,std::uint32_t(0));
        if(gate==3)h.set(h.fades.data(),0x18,std::uint16_t(3));
        if(gate==4)h.set(h.fades.data(),0x1C,std::uint32_t(6));
        if(gate==5)game_test::memory_maps[2].perm=1;
        if(gate==6)game_test::memory_maps[2].size=0x2F;
        if(gate==7)h.set(h.f.image.data(),0xB260C8,std::uintptr_t(0));
        if(gate==8)h.set(h.f.image.data(),0xB260C8,std::numeric_limits<std::uintptr_t>::max());
        const auto fades=h.fades;h.invoke();
        assert(h.controller()[0x149]==1&&h.fades==fades); // Another effect or invalid owner is untouched.
    }
    {
        PickupFixture h;
        const auto image=h.f.image;
        const auto item=h.item;
        const auto context=h.context;
        h.invoke();
        auto expected_fades=std::array<std::uint8_t,0xC0>{};expected_fades.fill(0x5A);
        h.set(expected_fades.data(),0,std::uint32_t(0x20000000));h.set(expected_fades.data(),4,std::uint32_t(0));
        h.set(expected_fades.data(),8,std::uint32_t(0x20000000));h.set(expected_fades.data(),0x18,std::uint16_t(0));
        h.set(expected_fades.data(),0x1C,std::uint32_t(5));
        assert(h.fades==expected_fades); // Disable only the fresh pickup darkening before its first native yield.
        assert(h.get<std::uint32_t>(h.controller(),0x1C)==0x87654320u);
        assert(h.get<std::uint32_t>(h.controller(),0x24)==0xF0F00F0Eu);
        assert(h.controller()[0x149]==1);
        assert(h.item==item&&!std::memcmp(&context,&h.context,sizeof(context)));
        auto expected=image;expected[0xB00648+0x1C]=0x20;expected[0xB00648+0x24]=0x0E;expected[0xB00648+0x149]=1;
        assert(h.f.image==expected);
        h.unchanged(); // Never answer or close the same message twice.
    }
    {
        PickupFixture h;h.reg(27,0); // Native informational/pregranted item.
        auto expected=h.f.image;
        const auto item=h.item;
        const auto context=h.context;
        expected[0xB00648+0x1C]=0x20;expected[0xB00648+0x24]=0x0E;
        h.invoke();
        assert(h.f.image==expected&&h.controller()[0x149]==0);
        assert(h.item==item&&!std::memcmp(&context,&h.context,sizeof(context)));
        h.unchanged();
    }
    for(unsigned gate=0;gate<23;++gate){
        PickupFixture h;
        if(gate==0)settings.skip_pickup_confirmation=false;
        if(gate==1)h.reg(19,0);
        if(gate==2)h.reg(19,std::numeric_limits<std::uintptr_t>::max());
        if(gate==3)h.reg(19,1);
        if(gate==4)h.reg(21,0);
        if(gate==5)h.reg(21,reinterpret_cast<std::uintptr_t>(h.controller()+8));
        if(gate==6)h.reg(27,2);
        if(gate==7)h.reg(27,0xFFFFFFFFu);
        if(gate==8)h.set(h.item.data(),0x80,std::uintptr_t(0));
        if(gate==9)h.set(h.item.data(),0x94,std::uint16_t(0x110));
        if(gate==10)h.set(h.item.data(),0x94,std::uint16_t(0xFFFF));
        if(gate==11)h.set(h.controller(),0x1C,0x87654320u);
        if(gate==12)h.set(h.controller(),0x24,0xF0F00F0Eu);
        if(gate==13)h.controller()[0x149]=1;
        if(gate==14)h.controller()[0x149]=2;
        if(gate==15)h.set(h.f.image.data(),0xB25D90,std::uintptr_t(0));
        if(gate==16)h.set(h.f.image.data(),0xB25D90,reinterpret_cast<std::uintptr_t>(h.controller()+8));
        if(gate==17)game_test::memory_maps.clear();
        if(gate==18)game_test::memory_maps[1].size=0xA0; // Item range crosses its mapping.
        if(gate==19)game_test::memory_maps[1].perm=0;
        if(gate==20)game_test::memory_maps[0].perm=1; // Controller is readable, not writable.
        if(gate==21)game_test::memory_maps[0].perm=2; // Writable does not imply readable.
        if(gate==22){ // GOT readable, controller range ends one byte early.
            game_test::memory_maps[0]={main_base+0xB25D90,8,1};
            game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(h.controller()),0x149,3});
        }
        h.unchanged();
    }
    {
        PickupFixture h;
        settings.skip_pickup_confirmation=false;game_test::memory_maps.clear();
        h.reg(19,std::numeric_limits<std::uintptr_t>::max());h.reg(21,1);h.unchanged();
        PickupConfirmation::Callback(nullptr);
        settings.skip_pickup_confirmation=true;PickupConfirmation::Callback(nullptr);
    }
    {
        PickupFixture h;
        main_base=std::numeric_limits<std::uintptr_t>::max()-1;
        h.unchanged();
        main_base=0;h.unchanged();
    }
    {
        PickupFixture h;h.reg(27,0xDEADBEEF00000001ull);
        h.invoke();assert(h.controller()[0x149]==1); // Consume native w27, not its unused high word.
    }
    {
        PickupFixture h;
        // The native fields fit exactly: do not require or touch neighboring data.
        game_test::memory_maps={{main_base+0xB25D90,8,1},
            {reinterpret_cast<std::uintptr_t>(h.controller()),0x14A,3},
            {reinterpret_cast<std::uintptr_t>(h.item.data()),0xA1,1}};
        h.invoke();assert(h.controller()[0x149]==1);
    }
    std::puts("actual pickup confirmation callback: bounded native response, unchanged grants and registers PASS");
}
