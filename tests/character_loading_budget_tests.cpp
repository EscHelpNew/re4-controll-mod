#define main budget_baseline_main
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#include "character_pack_budget_tests.cpp"
#pragma GCC diagnostic pop
#undef main
int main(){
    using S=re4::CharacterLoadingStage;
    std::uint32_t diagnostic=0;bool retry=false;
    const auto run=[&]{return character_pack_budget_load(reinterpret_cast<void*>(0x123),0x22,0x01000001,native_load,3,&retry,&diagnostic);};
    reset();allocatable=128u*1024*1024-1;
    assert(!run()&&!retry&&re4::character_loading_stage(diagnostic)==S::Headroom);
    assert(re4::character_loading_observed(diagnostic)==127);
    assert(!loads&&closes==1&&releases==1&&headroom_calls==1);
    reset();fail_reserve=true;
    assert(!run()&&re4::character_loading_stage(diagnostic)==S::Allocation);
    assert(!loads&&!headroom_calls&&closes==1&&releases==0);
    reset();missing=true;
    assert(!run()&&re4::character_loading_stage(diagnostic)==S::FileOpen);
    assert(!loads&&!reserves&&!closes);
    reset();yz2=true;
    assert(!run()&&re4::character_loading_stage(diagnostic)==S::CompressedVariant);
    assert(!loads&&!reserves&&closes==1);
    reset();for(unsigned i=0;i<64;++i)character_store(image.data(),0x11829B0+i*16+8,0x1000100u+i);
    assert(!run()&&re4::character_loading_stage(diagnostic)==S::RegistryCapacity);
    assert(!loads&&!opens&&!reserves);
    reset();character_store(image.data(),0x1184300+0x50,1u);
    assert(!run()&&retry&&diagnostic==0);assert(!loads&&!opens);
    reset();allocatable=128u*1024*1024;
    assert(run()&&!retry&&diagnostic==0);native_free();
    reset();character_store(image.data(),0x11829B0+8,0x01000001u);character_store(image.data(),0x11829B0,std::uintptr_t(1));
    allocatable=0;character_pack_budget_native.headroom=nullptr;
    assert(run()&&!retry&&diagnostic==0);assert(!loads&&!opens&&!reserves&&!headroom_calls);
    std::puts("character loading budget: bounded admission diagnostics preserve allocation, headroom, retry and resident ownership PASS");
}
