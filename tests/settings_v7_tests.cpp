#include "re4/settings.hpp"
#include "re4/shared.hpp"
#include <cassert>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace {
using Bytes = std::array<std::uint8_t,128>;
void put32(std::uint8_t* p, std::uint32_t value) {
    for(unsigned i=0;i<4;++i)p[i]=std::uint8_t(value>>(8*i));
}
std::uint32_t get32(const std::uint8_t* p) {
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|(std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
void putfloat(std::uint8_t* p,float f) { std::uint32_t value;std::memcpy(&value,&f,4);put32(p,value); }
void seal(Bytes& bytes,unsigned length=128) {
    std::uint32_t hash=2166136261u;
    for(unsigned i=0;i<length-4;++i){hash^=bytes[i];hash*=16777619u;}
    put32(bytes.data()+length-4,hash);
}
Bytes legacy(unsigned version) {
    Bytes b{};std::memcpy(b.data(),"R4CT",4);put32(b.data()+4,version);
    const unsigned flags=11|(version>=4?0x70:0)|(version>=6?(25<<8)|(75<<16):0);
    put32(b.data()+8,flags);
    if(version>=5)putfloat(b.data()+12,0.75f);else put32(b.data()+12,42);
    const float gains[]={0.25f,0.5f,0.75f,1,0.1f,0.2f,1.5f,2.5f,-0.25f,0.5f};
    for(unsigned i=0;i<10;++i)putfloat(b.data()+16+i*4,gains[i]);
    putfloat(b.data()+56,version==1?0.f:version<=4?0.3f:2.5f);
    seal(b,64);return b;
}
// Historical 128-byte fixtures are built from the published layouts, without
// relying on the latest encoder to produce an older version's reserved bytes.
Bytes extended_legacy(unsigned version) {
    auto b=legacy(6);put32(b.data()+4,version);
    std::fill(b.begin()+60,b.end(),0);
    putfloat(b.data()+60,0.375f);b[64]=1;b[65]=1;
    for(unsigned i=0;i<20;++i)b[68+i]=std::uint8_t(i);
    b[72]=1;
    if(version>=8){b[88]=1;putfloat(b.data()+92,1.5f);}
    if(version>=9){b[96]=2;b[97]=7;b[98]=2;}
    if(version>=10)putfloat(b.data()+100,1.35f);
    if(version>=11)b[97]|=8;
    seal(b);return b;
}
void rejected(const Bytes& bytes,std::size_t length=128) {
    re4::Settings out;out.turn_gain=0.125f;out.camera_return_speed=0.5f;
    out.free_camera=true;out.remap_enabled=false;out.button_map.fill(re4::kUnboundButton);
    assert(!re4::decode_settings(bytes.data(),length,out));
    assert(out.turn_gain==0.125f && out.camera_return_speed==0.5f && out.free_camera && !out.remap_enabled);
    for(auto entry:out.button_map)assert(entry==re4::kUnboundButton);
}
}
int main() {
    using namespace re4;
    Settings defaults;
    assert(kSharedAbi==17 && std::strcmp(kSharedMarker,"RE4CTL_SHARED17")==0 && sizeof(SharedControl)==400);
    assert(!defaults.re6_response && !defaults.move_while_reloading);
    assert(defaults.gyro_speed == 0.8f);
    for(auto member:{&Settings::turn_gain,&Settings::camera_gain,&Settings::aim_gain_x,&Settings::aim_gain_y,
                    &Settings::right_aim_gain_x,&Settings::right_aim_gain_y,&Settings::gyro_gain_x,&Settings::gyro_gain_y,
                    &Settings::movement_gain,&Settings::camera_return_speed})assert(defaults.*member==1);
    assert(defaults.remap_enabled && !defaults.free_camera && defaults.button_map==identity_button_map());
    assert(defaults.left_deadzone==0.05f && defaults.right_deadzone==0.05f && defaults.gyro_deadzone_dps==0.5f);
    Bytes bytes{};
    assert(encode_settings(defaults,bytes.data(),bytes.size()));
    assert(bytes[4] == 13);
    assert(bytes[64]==0 && bytes[65]==1);
    for(unsigned i=0;i<20;++i)assert(bytes[68+i]==i);
    for(unsigned i:{66,67,88,89,100,123})assert(bytes[i]==0);
    auto resealed=bytes;seal(resealed);assert(resealed==bytes);
    Settings custom;custom.turn_gain=0.25f;custom.gyro_speed=3.5f;custom.gyro_deadzone_dps=0.75f;
    custom.language=Language::Russian;custom.stick_curve=StickCurve::Hybrid;
    custom.right_aim_gain_x=0.25f;custom.right_aim_gain_y=0.75f;
    custom.camera_return_speed=0.375f;custom.free_camera=true;custom.remap_enabled=false;
    custom.button_map[0]=1;custom.button_map[1]=0;custom.button_map[19]=kUnboundButton;
    assert(encode_settings(custom,bytes.data(),bytes.size()));
    Settings decoded;assert(decode_settings(bytes.data(),bytes.size(),decoded));
    assert(decoded.turn_gain==0.25f && decoded.gyro_speed==3.5f && decoded.gyro_deadzone_dps==0.75f);
    assert(decoded.language==Language::Russian && decoded.stick_curve==StickCurve::Hybrid);
    assert(decoded.right_aim_gain_x==0.25f && decoded.right_aim_gain_y==0.75f);
    assert(decoded.camera_return_speed==0.375f && decoded.free_camera && !decoded.remap_enabled);
    assert(decoded.button_map==custom.button_map);
    custom.re6_response=true;
    assert(encode_settings(custom,bytes.data(),bytes.size()) && bytes[97]==16);
    assert(decode_settings(bytes.data(),bytes.size(),decoded) && decoded.re6_response);
    custom.move_while_reloading=true;
    assert(encode_settings(custom,bytes.data(),bytes.size()) && bytes[97]==48);
    assert(decode_settings(bytes.data(),bytes.size(),decoded) && decoded.re6_response && decoded.move_while_reloading);
    auto re6_v12=extended_legacy(12);re6_v12[97]|=16;seal(re6_v12);
    assert(decode_settings(re6_v12.data(),re6_v12.size(),decoded) && decoded.re6_response && !decoded.move_while_reloading);
    auto bad_v12=re6_v12;bad_v12[97]|=32;seal(bad_v12);rejected(bad_v12);
    for(unsigned version=7;version<=11;++version){
        const auto old=extended_legacy(version);
        assert(decode_settings(old.data(),old.size(),decoded));
        assert(!decoded.re6_response && !decoded.move_while_reloading && decoded.unified_controls==(version>=11));
        assert(decoded.camera_gain==0.5f && decoded.right_aim_gain_x==0.25f && decoded.right_aim_gain_y==0.75f);
        assert(decoded.camera_return_speed==0.375f && decoded.free_camera && decoded.button_map[4]==1);
        assert(decoded.third_person_distance==(version>=10?1.35f:1.f));
        assert(decoded.texture_filtering==(version>=8?TextureFilter::Bilinear:TextureFilter::Native));
        assert(encode_settings(decoded,resealed.data(),resealed.size()) && resealed[4]==13 && !(resealed[97]&16));
        auto bad=old;bad[97]|=16;seal(bad);rejected(bad);
    }
    for(unsigned version=1;version<=6;++version) {
        const auto old=legacy(version);
        assert(decode_settings(old.data(),64,decoded));
        assert(decoded.turn_gain==0.25f && decoded.camera_gain==0.5f && decoded.aim_gain_x==0.75f && decoded.aim_gain_y==1);
        assert(decoded.left_deadzone==0.1f && decoded.right_deadzone==0.2f);
        assert(decoded.gyro_gain_x==1.5f && decoded.gyro_gain_y==2.5f);
        assert(decoded.gyro_bias_x==-0.25f && decoded.gyro_bias_y==0.5f);
        assert(decoded.analog_enabled && decoded.gyro_enabled && !decoded.invert_x && decoded.invert_y);
        assert(decoded.gyro_speed==(version>=5?2.5f:0.8f));
        assert(decoded.gyro_deadzone_dps==(version>=5?0.75f:0.5f));
        assert(decoded.calibration_request==(version>=5?0u:42u));
        assert(decoded.language==(version>=4?Language::Russian:Language::English));
        assert(decoded.stick_curve==(version>=4?StickCurve::Hybrid:StickCurve::Linear));
        assert(decoded.right_aim_gain_x==(version>=6?0.25f:1));
        assert(decoded.right_aim_gain_y==(version>=6?0.75f:1));
        assert(decoded.button_map==identity_button_map() && decoded.remap_enabled && !decoded.free_camera && decoded.camera_return_speed==1);
        assert(decoded.third_person_distance==1);
        assert(!decoded.re6_response && !decoded.move_while_reloading);
        assert(decoded.texture_filtering==TextureFilter::Native && decoded.shadow_mode==ShadowMode::Native
            && decoded.anti_aliasing==AntiAliasing::Native && decoded.draw_distance_scale==1);
        assert(encode_settings(decoded,resealed.data(),resealed.size()) && resealed[4]==13);
        rejected(old,128);
    }
    for(unsigned n=0;n<128;++n)rejected(bytes,n);
    rejected(bytes,129);
    for(unsigned offset=0;offset<128;++offset){auto bad=bytes;bad[offset]^=1;rejected(bad);}
    for(unsigned offset=66;offset<124;++offset) {
        if(offset>=68 && offset<91)continue;
        if(offset>=92 && offset<96)continue;
        if(offset>=96 && offset<99)continue;
        if(offset>=100 && offset<104)continue; // v9 gameplay fields have their own strict tests.
        auto bad=bytes;bad[offset]=1;seal(bad);rejected(bad);
    }
    for(unsigned offset:{64,65})for(unsigned value:{2,255}){auto bad=bytes;bad[offset]=std::uint8_t(value);seal(bad);rejected(bad);}
    for(unsigned offset=68;offset<88;++offset)for(unsigned value:{20,127,254}){auto bad=bytes;bad[offset]=std::uint8_t(value);seal(bad);rejected(bad);}
    for(unsigned bit:{7,15,23,24,31}){auto bad=bytes;put32(bad.data()+8,get32(bad.data()+8)|(1u<<bit));seal(bad);rejected(bad);}
    for(unsigned shift:{8,16}){auto bad=bytes;put32(bad.data()+8,(get32(bad.data()+8)&~(127u<<shift))|(101u<<shift));seal(bad);rejected(bad);}
    for(unsigned version:{0,1,6,14,255}){auto bad=bytes;put32(bad.data()+4,version);seal(bad);rejected(bad);}
    for(float value:{-0.01f,1.01f,std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
        auto bad=bytes;putfloat(bad.data()+60,value);seal(bad);rejected(bad);
        auto invalid=custom;invalid.camera_return_speed=value;
        assert(!valid_settings(invalid));auto untouched=bytes;
        assert(!encode_settings(invalid,untouched.data(),untouched.size()) && untouched==bytes);
    }
    for(float value:{0.f,1.f,std::nextafter(0.f,1.f),std::nextafter(1.f,0.f)}){
        auto settings=custom;settings.camera_return_speed=value;
        assert(valid_settings(settings) && encode_settings(settings,resealed.data(),resealed.size()));
        assert(decode_settings(resealed.data(),resealed.size(),decoded) && decoded.camera_return_speed==value);
    }
    auto invalid=custom;invalid.button_map[0]=20;assert(!valid_settings(invalid));
    assert(!encode_settings(invalid,resealed.data(),resealed.size()));
    assert(!encode_settings(defaults,nullptr,128) && !encode_settings(defaults,bytes.data(),64));
    assert(!decode_settings(nullptr,128,decoded));
    auto old=legacy(6);old[4]=7;seal(old,64);rejected(old,64);
    old=legacy(1);old[56]=1;seal(old,64);rejected(old,64);
    old=legacy(5);old[9]=1;seal(old,64);rejected(old,64);
    std::puts("settings v13: defaults, complete legacy migration, strict wire checks, rollback and ranges PASS");
}
