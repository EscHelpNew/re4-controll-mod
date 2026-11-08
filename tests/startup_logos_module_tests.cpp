// Exercise the real title trampoline. The native engine remains external;
// the optional state-6 emulation below proves callback ordering, not console timing.
#define RE4_STARTUP_LOGOS_TEST_NATIVE
namespace { void test_StartupLogos(void*); }
#define main game_module_baseline_main
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#include "game_module_tests.cpp"
#pragma GCC diagnostic pop
#undef main

namespace {
unsigned startup_native_calls=0;
void* startup_argument=nullptr;
std::uint8_t* startup_observable_title=nullptr;
std::array<std::uint8_t,0xB0> startup_seen_title{};
bool startup_emulate_continuation=false;
void test_StartupLogos(void* title){
    ++startup_native_calls;startup_argument=title;
    if(title!=startup_observable_title||!title)return;
    std::memcpy(startup_seen_title.data(),title,startup_seen_title.size());
    // A host stand-in for native4E3B38's state change, only for the repeat test.
    // Fade, resource seeks and audio are native responsibilities, not simulated proof.
    if(startup_emulate_continuation&&startup_seen_title[1]==30){
        auto* bytes=static_cast<std::uint8_t*>(title);bytes[1]=6;
        const std::int32_t frame=400;std::memcpy(bytes+0x30,&frame,sizeof(frame));
    }
}
struct StartupFixture {
    Fixture f;
    alignas(8) std::array<std::uint8_t,0xB0> title{};
    template<class T>void set(void* p,unsigned offset,T value){std::memcpy(static_cast<std::uint8_t*>(p)+offset,&value,sizeof(value));}
    template<class T>T get(const void* p,unsigned offset){T value;std::memcpy(&value,static_cast<const std::uint8_t*>(p)+offset,sizeof(value));return value;}
    StartupFixture(){
        assert(!settings.skip_startup_logos);settings.skip_startup_logos=true;
        title.fill(0xA5);title[0]=4;title[1]=0;
        set(title.data(),0x30,std::int32_t(105));
        set(f.image.data(),0x1272A18,reinterpret_cast<std::uintptr_t>(title.data()));
        set(f.image.data(),0x124EC50,std::uint64_t(0xFFFFFFFFFFFFFFFFull)); // Held flags are irrelevant.
        set(f.image.data(),0x124EC58,std::uint64_t(0x1000));
        game_test::memory_maps={{main_base,f.image.size(),1},{reinterpret_cast<std::uintptr_t>(title.data()),title.size(),3}};
        startup_observable_title=title.data();startup_emulate_continuation=false;
    }
    void invoke(void* argument){
        startup_native_calls=0;startup_argument=nullptr;startup_seen_title.fill(0);
        StartupLogos::Callback(argument);
        assert(startup_native_calls==1&&startup_argument==argument);
    }
    void invoke(){invoke(title.data());}
    void unchanged(void* argument){
        const auto image=f.image;const auto original=title;
        invoke(argument);
        assert(f.image==image&&title==original);
        if(argument==title.data())assert(startup_seen_title==original);
    }
    void unchanged(){unchanged(title.data());}
    void skipped(){
        const auto image=f.image;auto expected=title;expected[1]=30;
        invoke();
        assert(startup_seen_title==expected); // State 30 must be visible before Orig.
        assert(title==expected&&f.image==image); // Only the native substate byte is ours.
    }
};
}

int main(){
    // Removing the write, changing its ordering, or touching any other title/input
    // byte must fail. Each of the four native accept edges is independently valid.
    for(const std::uint8_t substate:{0,1,2,10})for(const std::int32_t frame:{105,212,230,246,314,320})
        for(const std::uint64_t edge:{0x1000ull,0x40000000ull,0x80000000ull,0x200000000000ull}){
            StartupFixture h;h.title[1]=substate;h.set(h.title.data(),0x30,frame);
            h.set(h.f.image.data(),0x124EC58,edge);h.skipped();
        }
    {
        StartupFixture h;settings.skip_startup_logos=false;h.unchanged();
        game_test::memory_maps.clear();h.unchanged(reinterpret_cast<void*>(1));
    }
    for(const std::uint8_t stage:{0,1,2,3,5,6,7,255}){
        StartupFixture h;h.title[0]=stage;h.unchanged();
    }
    for(const std::uint8_t state:{3,4,5,6,9,11,29,30,31,255}){
        StartupFixture h;h.title[1]=state;h.unchanged();
    }
    for(const std::int32_t frame:{-2147483647-1,-1,0,104,321,400,616,2147483647}){
        StartupFixture h;h.set(h.title.data(),0x30,frame);h.unchanged();
    }
    for(unsigned bit=0;bit<64;++bit){
        if(bit==12||bit==30||bit==31||bit==45)continue;
        StartupFixture h;h.set(h.f.image.data(),0x124EC58,std::uint64_t(1)<<bit);h.unchanged();
    }
    {
        StartupFixture h;h.set(h.f.image.data(),0x124EC58,std::uint64_t(0));h.unchanged(); // Held only.
        h.set(h.f.image.data(),0x124EC58,std::uint64_t(0x0F000000));h.unchanged(); // Native direction autorepeat.
        h.set(h.f.image.data(),0x124EC58,std::uint64_t(0xFFFFFFFFFFFFFFFFull));h.skipped(); // Preserve full flags.
    }
    // Each failed ownership/mapping guard must still call Orig once, unchanged.
    for(unsigned gate=0;gate<20;++gate){
        StartupFixture h;
        if(gate==0){h.unchanged(nullptr);continue;}
        if(gate==1)h.set(h.f.image.data(),0x1272A18,std::uintptr_t(0));
        if(gate==2)h.set(h.f.image.data(),0x1272A18,reinterpret_cast<std::uintptr_t>(h.title.data()+8));
        if(gate==3){h.unchanged(h.title.data()+8);continue;}
        if(gate==4)game_test::memory_maps[1].perm=1; // Read-only title.
        if(gate==5)game_test::memory_maps[1].perm=2; // Write-only title.
        if(gate==6)game_test::memory_maps[1].perm=0;
        if(gate==7)game_test::memory_maps.pop_back();
        if(gate==8)game_test::memory_maps[1].size=0x33; // Frame ends one byte beyond span.
        if(gate==9)game_test::memory_maps[0].perm=0;
        if(gate==10)game_test::memory_maps[0].perm=2;
        if(gate==11)game_test::memory_maps.clear();
        if(gate==12){
            const auto bad=std::numeric_limits<std::uintptr_t>::max()-0x20;
            h.set(h.f.image.data(),0x1272A18,bad);
            game_test::memory_maps[1]={bad,0x34,3};h.unchanged(reinterpret_cast<void*>(bad));continue;
        }
        if(gate==13){h.set(h.f.image.data(),0x1272A18,std::uintptr_t(1));h.unchanged(reinterpret_cast<void*>(1));continue;}
        if(gate==14)game_test::memory_maps[0]={main_base+0x1272A18,7,1}; // Truncated owner.
        if(gate==15){
            game_test::memory_maps[0]={main_base+0x1272A18,8,1}; // Missing edge mapping.
        }
        if(gate==16){
            game_test::memory_maps[0]={main_base+0x1272A18,8,1};
            game_test::memory_maps.push_back({main_base+0x124EC58,7,1}); // Truncated edge.
        }
        if(gate==17){
            game_test::memory_maps[0]={main_base+0x1272A18,8,1};
            game_test::memory_maps.push_back({main_base+0x124EC58,8,2}); // Unreadable edge.
        }
        if(gate==18)main_base=std::numeric_limits<std::uintptr_t>::max()-1;
        if(gate==19)main_base=0;
        h.unchanged();
    }
    {
        StartupFixture h;
        // Exact independent spans suffice; do not demand access to adjacent fields.
        game_test::memory_maps={{main_base+0x1272A18,8,1},{main_base+0x124EC58,8,1},
            {reinterpret_cast<std::uintptr_t>(h.title.data()),0x34,3}};
        h.skipped();
    }
    {
        StartupFixture h;startup_emulate_continuation=true;
        const auto image=h.f.image;auto expected=h.title;expected[1]=30;
        h.invoke();assert(startup_seen_title==expected&&h.f.image==image);
        assert(h.title[1]==6&&h.get<std::int32_t>(h.title.data(),0x30)==400);
        h.unchanged(); // Same edge cannot skip a continuation/title phase again.
    }
    std::puts("actual startup-logo callback: bounded native transition, fresh edges, original once PASS");
}
