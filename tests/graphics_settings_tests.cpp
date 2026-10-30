#include "re4/settings.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace {
using Bytes=std::array<std::uint8_t,re4::kSettingsBytes>;
void put32(std::uint8_t* out,std::uint32_t value) {
    for(unsigned i=0;i<4;++i)out[i]=std::uint8_t(value>>(8*i));
}
void putfloat(std::uint8_t* out,float value) {
    std::uint32_t bits;std::memcpy(&bits,&value,4);put32(out,bits);
}
void seal(Bytes& bytes) {
    std::uint32_t hash=2166136261u;
    for(unsigned i=0;i<124;++i){hash^=bytes[i];hash*=16777619u;}
    for(unsigned i=0;i<4;++i)bytes[124+i]=std::uint8_t(hash>>(8*i));
}
void native(const re4::Settings& s) {
    assert(s.texture_filtering==re4::TextureFilter::Native && s.shadow_mode==re4::ShadowMode::Native
        && s.anti_aliasing==re4::AntiAliasing::Native && s.draw_distance_scale==1);
}
void rejected(Bytes bytes) {
    seal(bytes);re4::Settings out;out.turn_gain=0.125f;out.texture_filtering=re4::TextureFilter::Aniso8;
    assert(!re4::decode_settings(bytes.data(),bytes.size(),out));
    assert(out.turn_gain==0.125f && out.texture_filtering==re4::TextureFilter::Aniso8);
}
}
int main() {
    using namespace re4;Settings s;native(s);Bytes bytes{};
    assert(s.gyro_speed==0.8f && encode_settings(s,bytes.data(),bytes.size()));
    assert(bytes[4]==10 && bytes[88]==0 && bytes[89]==0 && bytes[90]==0 && bytes[91]==0);
    assert(bytes[92]==0 && bytes[93]==0 && bytes[94]==0x80 && bytes[95]==0x3f);
    for(unsigned i=96;i<124;++i)if(i<100 || i>=104)assert(bytes[i]==0);
    for(unsigned f=0;f<=6;++f)for(unsigned shadow=0;shadow<=1;++shadow)for(unsigned aa=0;aa<=1;++aa)
        for(float distance:{0.5f,1.f,2.f}) {
            s.texture_filtering=static_cast<TextureFilter>(f);s.shadow_mode=static_cast<ShadowMode>(shadow);
            s.anti_aliasing=static_cast<AntiAliasing>(aa);s.draw_distance_scale=distance;
            assert(valid_settings(s) && encode_settings(s,bytes.data(),bytes.size()));
            assert(bytes[88]==f && bytes[89]==shadow && bytes[90]==aa);
            Settings decoded;assert(decode_settings(bytes.data(),bytes.size(),decoded));
            assert(decoded.texture_filtering==s.texture_filtering && decoded.shadow_mode==s.shadow_mode
                && decoded.anti_aliasing==s.anti_aliasing && decoded.draw_distance_scale==distance);
        }
    for(float invalid:{0.f,0.499f,2.001f,std::numeric_limits<float>::quiet_NaN(),
                       std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()}) {
        auto bad=bytes;std::memcpy(bad.data()+92,&invalid,4);rejected(bad);
        auto options=s;options.draw_distance_scale=invalid;const auto before=bytes;
        assert(!valid_settings(options) && !encode_settings(options,bytes.data(),bytes.size()) && bytes==before);
    }
    for(unsigned offset:{88,89,90})for(unsigned value:{7,127,255}) {
        auto bad=bytes;bad[offset]=std::uint8_t(value);rejected(bad);
    }
    for(unsigned offset=91;offset<124;++offset) {
        if(offset>=92 && offset<96)continue;
        if(offset>=96 && offset<99)continue;
        if(offset>=100 && offset<104)continue;
        auto bad=bytes;bad[offset]=1;rejected(bad);
    }
    // A real v7 fixture preserves all v7-only controls while graphics migrate Native.
    Settings previous;previous.language=Language::Russian;previous.stick_curve=StickCurve::Hybrid;
    previous.gyro_enabled=true;previous.gyro_speed=3.5f;previous.gyro_deadzone_dps=0.75f;
    previous.gyro_bias_x=-0.5f;previous.gyro_bias_y=0.25f;
    previous.right_aim_gain_x=0.25f;previous.right_aim_gain_y=0.75f;
    previous.free_camera=true;previous.camera_return_speed=0.375f;previous.remap_enabled=false;
    previous.button_map[0]=1;previous.button_map[1]=0;previous.button_map[19]=kUnboundButton;
    // Construct the legacy wire independently of the current encoder.
    bytes.fill(0);std::memcpy(bytes.data(),"R4CT",4);put32(bytes.data()+4,7);
    put32(bytes.data()+8,0x004b197fu);putfloat(bytes.data()+12,0.75f);
    const float old_fields[]={1,1,1,1,0.05f,0.05f,1,1,-0.5f,0.25f,3.5f};
    for(unsigned i=0;i<11;++i)putfloat(bytes.data()+16+4*i,old_fields[i]);
    putfloat(bytes.data()+60,0.375f);bytes[64]=1;bytes[65]=0;
    for(unsigned i=0;i<20;++i)bytes[68+i]=std::uint8_t(i);
    bytes[68]=1;bytes[69]=0;bytes[87]=255;seal(bytes);
    Settings migrated;s.texture_filtering=TextureFilter::Aniso16;migrated=s;
    assert(decode_settings(bytes.data(),bytes.size(),migrated));native(migrated);
    assert(migrated.language==Language::Russian && migrated.stick_curve==StickCurve::Hybrid);
    assert(migrated.gyro_enabled && migrated.gyro_speed==3.5f && migrated.gyro_deadzone_dps==0.75f);
    assert(migrated.gyro_bias_x==-0.5f && migrated.gyro_bias_y==0.25f);
    assert(migrated.right_aim_gain_x==0.25f && migrated.right_aim_gain_y==0.75f);
    assert(migrated.free_camera && migrated.camera_return_speed==0.375f && !migrated.remap_enabled);
    assert(migrated.button_map==previous.button_map);
    for(unsigned offset=88;offset<124;++offset){auto bad=bytes;bad[offset]=1;rejected(bad);}
    assert(encode_settings(migrated,bytes.data(),bytes.size()) && bytes[4]==10);
    assert(decode_settings(bytes.data(),bytes.size(),migrated));native(migrated);
    std::puts("graphics settings: strict v9 validation, roundtrip, v7 preservation and native migration PASS");
}
