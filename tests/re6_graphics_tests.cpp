#include "re6/graphics.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>

namespace {
using Descriptor=std::array<std::uint8_t,60>;
Descriptor descriptor(std::uint32_t flags) {
    Descriptor value{};
    for(unsigned i=0;i<value.size();++i)value[i]=std::uint8_t(0x81+i);
    std::memcpy(value.data(),&flags,4);return value;
}
std::uint32_t flags(const Descriptor& value) {
    std::uint32_t result;std::memcpy(&result,value.data(),4);return result;
}
void filtering_preserves_every_unrelated_byte_and_flag() {
    struct Case {re6::TextureFilter choice;unsigned native_index;};
    const Case cases[]={{re6::TextureFilter::Af2x,2},{re6::TextureFilter::Af4x,4},
        {re6::TextureFilter::Af8x,9},{re6::TextureFilter::Af16x,16}};
    constexpr std::uint32_t changed_mask=0xffu|(0x1fu<<17);
    for(unsigned mode:{7u,8u})for(const auto& c:cases) {
        const auto input=descriptor(0xfedea500u|mode);const auto original=input;
        Descriptor output;output.fill(0x55);
        assert(re6::filter_sampler_descriptor(input.data(),output.data(),56,c.choice));
        assert(input==original);
        assert(flags(output)==((flags(input)&~changed_mask)|8u|(c.native_index<<17)));
        for(unsigned i=4;i<56;++i)assert(output[i]==input[i]);
        for(unsigned i=56;i<output.size();++i)assert(output[i]==0x55);
        auto inplace=input;
        assert(re6::filter_sampler_descriptor(inplace.data(),inplace.data(),56,c.choice));
        assert(std::memcmp(inplace.data(),output.data(),56)==0);
        std::array<std::uint8_t,61> overlap{};std::memcpy(overlap.data(),input.data(),56);
        assert(re6::filter_sampler_descriptor(overlap.data(),overlap.data()+1,56,c.choice));
        assert(std::memcmp(overlap.data()+1,output.data(),56)==0);
    }
}
void excluded_native_and_invalid_options_are_byte_exact() {
    for(unsigned mode=0;mode<256;++mode) {
        const auto input=descriptor(0x7cf1a500u|mode);Descriptor output{};
        const bool changed=re6::filter_sampler_descriptor(input.data(),output.data(),56,re6::TextureFilter::Af8x);
        assert(changed==(mode==7||mode==8));
        if(!changed)assert(std::memcmp(input.data(),output.data(),56)==0);
    }
    for(auto choice:{re6::TextureFilter::Native,static_cast<re6::TextureFilter>(255)}) {
        const auto input=descriptor(0xfedea507u);Descriptor output{};
        assert(!re6::filter_sampler_descriptor(input.data(),output.data(),56,choice));
        assert(std::memcmp(input.data(),output.data(),56)==0);
    }
    const auto already=descriptor(0x00400008u|(9u<<17));Descriptor output{};
    assert(!re6::filter_sampler_descriptor(already.data(),output.data(),56,re6::TextureFilter::Af8x));
    assert(std::memcmp(already.data(),output.data(),56)==0);
}
void boundaries_do_not_write() {
    const auto input=descriptor(7);Descriptor output;output.fill(0x55);const auto before=output;
    assert(!re6::filter_sampler_descriptor(nullptr,output.data(),56,re6::TextureFilter::Af8x));
    assert(output==before);
    assert(!re6::filter_sampler_descriptor(input.data(),nullptr,56,re6::TextureFilter::Af8x));
    for(unsigned size:{0u,4u,55u}) {
        assert(!re6::filter_sampler_descriptor(input.data(),output.data(),size,re6::TextureFilter::Af8x));
        assert(output==before);
    }
}
}
int main() {
    filtering_preserves_every_unrelated_byte_and_flag();
    excluded_native_and_invalid_options_are_byte_exact();boundaries_do_not_write();
    std::puts("RE6 graphics tests passed");
}
