#include "re4/graphics.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace {
using Descriptor=std::array<std::uint8_t,24>;
Descriptor descriptor(std::uint16_t flags) {
    Descriptor d{};for(unsigned i=0;i<d.size();++i)d[i]=std::uint8_t(0x80+i);
    d[5]=1;d[6]=std::uint8_t(flags);d[7]=std::uint8_t(flags>>8);return d;
}
void sampler_tests() {
    using namespace re4;
    const auto src=descriptor(0xa82a);const auto original=src;
    struct Case {TextureFilter filter;std::uint8_t low,aniso;};
    const Case cases[]={{TextureFilter::Bilinear,0x29,1},{TextureFilter::Trilinear,0x2a,1},
        {TextureFilter::Aniso2,0x6a,2},{TextureFilter::Aniso4,0x6a,4},
        {TextureFilter::Aniso8,0x6a,8},{TextureFilter::Aniso16,0x6a,16}};
    for(const auto& c:cases) {
        Descriptor out{};out.fill(0x5c);
        const bool changed=filter_sampler_descriptor(src.data(),out.data(),out.size(),c.filter);
        assert(changed==(c.filter!=TextureFilter::Trilinear));
        for(unsigned i=0;i<20;++i)assert(out[i]==(i==5?c.aniso:i==6?c.low:src[i]));
        for(unsigned i=20;i<24;++i)assert(out[i]==0x5c);
        assert(src==original);
        auto inplace=src;assert(filter_sampler_descriptor(inplace.data(),inplace.data(),20,c.filter)==changed);
        assert(std::memcmp(inplace.data(),out.data(),20)==0);
        std::array<std::uint8_t,25> overlap{};std::memcpy(overlap.data(),src.data(),20);
        assert(filter_sampler_descriptor(overlap.data(),overlap.data()+1,20,c.filter)==changed);
        assert(std::memcmp(overlap.data()+1,out.data(),20)==0);
    }
    for(unsigned flags=0;flags<1024;++flags) {
        auto input=descriptor(std::uint16_t(flags|0xd400));Descriptor out{};
        const bool eligible=((flags>>4)&3)==2 && ((flags>>2)&3)==2
            && ((flags&3)==1 || (flags&3)==2) && !(flags&0x80) && !(flags&0x300);
        const bool changed=filter_sampler_descriptor(input.data(),out.data(),20,TextureFilter::Aniso16);
        assert(changed==eligible);
        if(!eligible)assert(std::memcmp(input.data(),out.data(),20)==0);
        else {
            assert(out[5]==16 && out[6]==0x6a && out[7]==input[7]);
            for(unsigned i=0;i<20;++i)if(i!=5 && i!=6)assert(out[i]==input[i]);
        }
    }
    Descriptor out{};out.fill(0x5c);const auto sentinel=out;
    assert(!filter_sampler_descriptor(nullptr,out.data(),20,TextureFilter::Aniso16) && out==sentinel);
    assert(!filter_sampler_descriptor(src.data(),nullptr,20,TextureFilter::Aniso16));
    assert(!filter_sampler_descriptor(src.data(),out.data(),19,TextureFilter::Aniso16) && out==sentinel);
    assert(!filter_sampler_descriptor(src.data(),out.data(),20,TextureFilter::Native));
    assert(std::memcmp(src.data(),out.data(),20)==0);
    assert(!filter_sampler_descriptor(src.data(),out.data(),20,static_cast<TextureFilter>(255)));
    assert(std::memcmp(src.data(),out.data(),20)==0);
}
void snapshot_tests() {
    using namespace re4;GraphicsOptions native;
    assert(valid_graphics(native));
    assert(pack_graphics(native,false)==0x3f80000000000000ull);
    assert(pack_graphics(native)==0x3f80000001000000ull);
    assert(!graphics_ready(pack_graphics(native,false)) && graphics_ready(pack_graphics(native)));
    GraphicsOptions all{TextureFilter::Aniso16,ShadowMode::Off,AntiAliasing::HandheldSsaa,2};
    assert(pack_graphics(all)==0x4000000001010106ull);
    const auto decoded=unpack_graphics(0x4000000001010106ull);
    assert(decoded.filter==all.filter && decoded.shadows==all.shadows && decoded.aa==all.aa && decoded.draw_distance==2);
    for(std::uint64_t word:{0x4000000003010106ull,0x4000000001010107ull,0x4000000001010206ull,
                           0x4000000001020106ull,0x7f80000001010106ull,0x7fc0000001010106ull,
                           0x3effffff01010106ull,0x4000000101010106ull}) {
        const auto value=unpack_graphics(word);
        assert(value.filter==TextureFilter::Native && value.shadows==ShadowMode::Native
            && value.aa==AntiAliasing::Native && value.draw_distance==1);
    }
    for(float f:{0.49f,2.01f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}) {
        all.draw_distance=f;assert(!valid_graphics(all));
    }
    all.draw_distance=1;all.filter=static_cast<TextureFilter>(7);assert(!valid_graphics(all));
    all.filter=TextureFilter::Native;all.shadows=static_cast<ShadowMode>(2);assert(!valid_graphics(all));
    all.shadows=ShadowMode::Native;all.aa=static_cast<AntiAliasing>(2);assert(!valid_graphics(all));
}
void distance_shadow_tests() {
    using namespace re4;
    assert(culling_distance(100,1,500,1)==100);
    assert(culling_distance(100,1,500,0.5f)==50);
    assert(culling_distance(100,1,500,2)==200);
    assert(culling_distance(100,1,150,2)==150);
    assert(culling_distance(10,9,100,0.5f)==10);
    assert(culling_distance(10,9.5f,10,2)==10);
    assert(culling_distance(10,10,100,2)==10);
    assert(culling_distance(10,-1,100,2)==10);
    assert(culling_distance(10,0,0,2)==10);
    assert(culling_distance(10,0,100,0.49f)==10);
    assert(culling_distance(10,0,100,2.01f)==10);
    const float inf=std::numeric_limits<float>::infinity(),nan=std::numeric_limits<float>::quiet_NaN();
    assert(culling_distance(10,nan,100,2)==10 && culling_distance(10,0,inf,2)==10);
    assert(culling_distance(10,0,100,nan)==10);
    assert(std::isnan(culling_distance(nan,0,100,1)));
    assert(culling_distance(inf,0,100,2)==inf);
    const float huge=std::numeric_limits<float>::max();
    assert(culling_distance(huge/2,0,huge,2)==huge);
    assert(culling_distance(huge*0.75f,0,huge,2)==huge);
    for(unsigned i=0;i<100;++i)assert(culling_distance(100,1,500,2)==200);
    assert(shadow_flags(ShadowMode::Native,0x12345678u)==0x12345678u);
    assert(shadow_flags(ShadowMode::Off,0)==0x02048000u);
    assert(restore_shadow_flags(0xffffffffu,0)==0xfdfb7fffu);
    assert(restore_shadow_flags(0x80000001u,0x02048000u)==0x82048001u);
    assert(restore_shadow_flags(0x02048000u,0x80000001u)==0);
}
void resolution_tests() {
    using namespace re4;
    for(int display:{0,2})for(int back=0;back<4;++back)for(bool owned:{false,true})for(auto aa:{AntiAliasing::Native,AntiAliasing::HandheldSsaa}) {
        ResolutionState state{display,back,display,back};const auto plan=plan_resolution(aa,state,owned);
        if(display==0)assert(!plan.request && !plan.owned);
        else if(aa==AntiAliasing::Native && !owned)assert(!plan.request && !plan.owned);
        else {
            const int desired=aa==AntiAliasing::Native?3:1;
            assert(plan.request==(back!=desired) && plan.owned==(aa==AntiAliasing::HandheldSsaa));
            if(plan.request)assert(plan.display==2 && plan.back==desired);
        }
    }
    for(auto state:{ResolutionState{0,1,2,3},ResolutionState{2,3,0,1},ResolutionState{1,3,2,3},
                    ResolutionState{2,-1,2,3},ResolutionState{2,4,2,3},ResolutionState{2,3,2,4},
                    ResolutionState{2,3,1,3},ResolutionState{2,3,2,-1}}) {
        for(bool owned:{false,true})for(auto aa:{AntiAliasing::Native,AntiAliasing::HandheldSsaa}) {
            const auto plan=plan_resolution(aa,state,owned);assert(!plan.request && plan.owned==owned);
        }
    }
    auto pending=plan_resolution(AntiAliasing::HandheldSsaa,{2,3,2,1},false);
    assert(!pending.request && pending.owned);
    auto restore=plan_resolution(AntiAliasing::Native,{2,1,2,3},true);
    assert(!restore.request && !restore.owned);
    assert(!plan_resolution(static_cast<AntiAliasing>(255),{2,3,2,3},false).request);
    assert(resolution_back_index(AntiAliasing::HandheldSsaa,1280,720,2,3)==1);
    assert(resolution_back_index(AntiAliasing::Native,1280,720,2,3)==3);
    assert(resolution_back_index(AntiAliasing::HandheldSsaa,1920,1080,0,1)==1);
    assert(resolution_back_index(AntiAliasing::HandheldSsaa,1280,720,0,3)==3);
    assert(resolution_back_index(AntiAliasing::HandheldSsaa,1280,720,2,2)==2);
    assert(resolution_back_index(AntiAliasing::HandheldSsaa,1280,721,2,3)==3);
    assert(resolution_back_index(AntiAliasing::HandheldSsaa,1279,720,2,3)==3);
}
}
int main() {sampler_tests();snapshot_tests();distance_shadow_tests();resolution_tests();
    std::puts("graphics: sampler scope/preservation, snapshots, masks, frustum and resolution policy PASS");}
