#include "re6/shared.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
int main(){
    auto c=re6::initial_shared_control();assert(sizeof(c)==128 && re6::kCapabilityFiltering==1);
    assert(!std::memcmp(c.marker,"RE6CTL_SHARED1\0\0",16) && c.abi==1 && c.status==0 && c.sequence==0 && c.applied_sequence==0);
    assert(c.capabilities==0 && c.graphics_filter_latched==255 && c.reserved==0);
    assert(c.settings_source==0 && c.frames==0 && c.sampler_calls==0 && c.sampler_overrides==0);
    re6::Settings wanted,out;assert(re6::decode_settings(c.settings,64,out) && out.texture_filtering==re6::TextureFilter::Native);
    c.status=1;c.capabilities=re6::kCapabilityFiltering;wanted.language=re6::Language::Russian;wanted.texture_filtering=re6::TextureFilter::Af16x;wanted.anti_aliasing=re6::AntiAliasing::Fxaa3High;
    std::uint32_t writer=0,reader=0;
    assert(re6::publish_settings(c,wanted,writer) && writer==2 && c.applied_sequence==0);
    assert(re6::consume_settings(c,out,reader) && reader==2 && c.applied_sequence==2);
    assert(out.language==re6::Language::Russian && out.texture_filtering==re6::TextureFilter::Af16x&&out.anti_aliasing==re6::AntiAliasing::Fxaa3High);
    // A delayed bootstrap reader cannot overwrite a completed live writer.
    {const auto before=c;std::uint32_t stale=0;
        assert(!re6::publish_settings(c,re6::Settings{},stale));
        assert(stale==0&&!std::memcmp(&c,&before,sizeof(c)));}
    out={};c.applied_sequence=10;
    auto reject=[&]{const auto before=reader;assert(!re6::consume_settings(c,out,reader));assert(reader==before && c.applied_sequence==10 && out.language==re6::Language::English && out.texture_filtering==re6::TextureFilter::Native);};
    reject();reader=4;reject();reader=0x80000002u;reject();reader=0;
    c.sequence=3;reject();c.sequence=2;c.settings[12]^=1;reject();c.settings[12]^=1;
    c.marker[0]='4';reject();c.marker[0]='R';c.abi=2;reject();c.abi=1;c.reserved=1;reject();c.reserved=0;
    c.capabilities=2;reject();c.capabilities=1;c.status=3;reject();c.status=1;
    assert(re6::consume_settings(c,out,reader) && reader==2 && c.applied_sequence==2);
    const auto initial=c;wanted.texture_filtering=static_cast<re6::TextureFilter>(5);
    assert(!re6::publish_settings(c,wanted,writer) && writer==2 && !std::memcmp(&c,&initial,sizeof(c)));
    wanted={};c.sequence=3;assert(!re6::publish_settings(c,wanted,writer) && writer==2 && c.sequence==3);
    c.sequence=2;c.capabilities=2;assert(!re6::publish_settings(c,wanted,writer) && writer==2 && c.sequence==2);c.capabilities=1;
    c.sequence=0xfffffffcu;writer=reader=0xfffffffcu;wanted.texture_filtering=re6::TextureFilter::Af4x;
    assert(re6::publish_settings(c,wanted,writer) && writer==0xfffffffeu);assert(re6::consume_settings(c,out,reader) && reader==0xfffffffeu);
    wanted.texture_filtering=re6::TextureFilter::Af8x;assert(re6::publish_settings(c,wanted,writer) && writer==0 && c.sequence==0);
    assert(re6::consume_settings(c,out,reader) && reader==0 && c.applied_sequence==0 && out.texture_filtering==re6::TextureFilter::Af8x);
    c.sequence=0xfffffffeu;assert(!re6::consume_settings(c,out,reader) && out.texture_filtering==re6::TextureFilter::Af8x);
    std::puts("RE6 graphics shared ABI:128 bytes, strict snapshots, wrap and ack PASS");
}
