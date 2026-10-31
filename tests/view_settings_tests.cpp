#include "re4/settings.hpp"
#include "re4/profiles.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>

#define CHECK(c) do {if(!(c)){std::fprintf(stderr,"view settings line %d: %s\n",__LINE__,#c);std::exit(1);}}while(false)
namespace {
using Bytes=std::array<std::uint8_t,re4::kSettingsBytes>;
void seal(Bytes& bytes){
    std::uint32_t hash=2166136261u;
    for(unsigned i=0;i<124;++i){hash^=bytes[i];hash*=16777619u;}
    for(unsigned i=0;i<4;++i)bytes[124+i]=std::uint8_t(hash>>(8*i));
}
}
int main(){
    using namespace re4;Settings s;Bytes bytes{};
    s.unified_controls=true;
    CHECK(encode_settings(s,bytes.data(),bytes.size()));
    Settings unified;CHECK(decode_settings(bytes.data(),bytes.size(),unified) && unified.unified_controls);
    s={};
    CHECK(s.third_person_distance==1);
    s.third_person_distance=1.35f;
    CHECK(encode_settings(s,bytes.data(),bytes.size()));
    Settings roundtrip;CHECK(decode_settings(bytes.data(),bytes.size(),roundtrip));
    CHECK(roundtrip.third_person_distance==1.35f);
    for(float invalid : {0.f,0.99f,2.01f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
        s.third_person_distance=invalid;CHECK(!valid_settings(s));
    }
    s.third_person_distance=2;CHECK(valid_settings(s));s={};
    CHECK(!s.first_person && !s.move_while_aiming && !s.move_while_knife && !s.move_while_reloading
        && s.reticle==ReticleStyle::Off && s.reticle_size==ReticleSize::Small);
    CHECK(encode_settings(s,bytes.data(),bytes.size()) && bytes[4]==13);
    // Independently exercise the new wire bit before using it through a preset.
    bytes[97]=16;seal(bytes);
    Settings response;CHECK(decode_settings(bytes.data(),bytes.size(),response));
    Bytes response_bytes{};CHECK(encode_settings(response,response_bytes.data(),response_bytes.size()));
    CHECK(response_bytes[97]==16);
    for(unsigned flags=0;flags<64;++flags)for(unsigned style=0;style<3;++style)for(unsigned size=0;size<3;++size){
        s.first_person=flags&1;s.move_while_aiming=flags&2;s.move_while_knife=flags&4;
        s.unified_controls=flags&8;
        s.re6_response=flags&16;
        s.move_while_reloading=flags&32;
        s.reticle=static_cast<ReticleStyle>(style);s.reticle_size=static_cast<ReticleSize>(size);
        CHECK(encode_settings(s,bytes.data(),bytes.size()));
        CHECK(bytes[96]==style && bytes[97]==flags && bytes[98]==size);
        Settings decoded;CHECK(decode_settings(bytes.data(),bytes.size(),decoded));
        CHECK(decoded.first_person==s.first_person && decoded.move_while_aiming==s.move_while_aiming
            && decoded.move_while_knife==s.move_while_knife && decoded.reticle==s.reticle && decoded.reticle_size==s.reticle_size
            && decoded.unified_controls==s.unified_controls && decoded.re6_response==s.re6_response
            && decoded.move_while_reloading==s.move_while_reloading);
        CHECK(same_profile(s,decoded,ProfileDomain::Controls));
    }
    for(unsigned offset:{96,97,98,99,104,123}){
        auto bad=bytes;bad[offset]=255;seal(bad);Settings out;out.camera_gain=0.3f;
        CHECK(!decode_settings(bad.data(),bad.size(),out) && out.camera_gain==0.3f);
    }
    for(unsigned bit : {64u,128u}){
        auto bad=bytes;bad[97]=std::uint8_t(bit);seal(bad);Settings out;out.re6_response=true;
        CHECK(!decode_settings(bad.data(),bad.size(),out) && out.re6_response);
    }
    // v8 restores every previous domain and starts all new gameplay modes disabled.
    s={};s.gyro_enabled=true;s.camera_gain=0.4f;s.texture_filtering=TextureFilter::Aniso4;
    s.language=Language::Russian;s.gyro_bias_y=0.25f;s.button_map[0]=1;
    CHECK(encode_settings(s,bytes.data(),bytes.size()));bytes[4]=8;
    for(unsigned i=100;i<104;++i)bytes[i]=0;
    seal(bytes);
    Settings migrated;CHECK(decode_settings(bytes.data(),bytes.size(),migrated));
    CHECK(migrated.gyro_enabled && migrated.camera_gain==0.4f && migrated.texture_filtering==TextureFilter::Aniso4
        && migrated.language==Language::Russian && migrated.gyro_bias_y==0.25f && migrated.button_map[0]==1);
    CHECK(!migrated.first_person && !migrated.move_while_aiming && !migrated.move_while_knife && migrated.reticle==ReticleStyle::Off);
    for(unsigned offset=96;offset<124;++offset){auto bad=bytes;bad[offset]=1;seal(bad);CHECK(!decode_settings(bad.data(),bad.size(),migrated));}
    // Actual v9 reserved bytes stay zero: migration preserves all old gameplay
    // fields and gives the existing installation native camera framing.
    s.first_person=true;s.move_while_knife=true;s.reticle=ReticleStyle::Crosshair;
    CHECK(encode_settings(s,bytes.data(),bytes.size()));bytes[4]=9;
    for(unsigned i=100;i<104;++i)bytes[i]=0;
    seal(bytes);
    CHECK(decode_settings(bytes.data(),bytes.size(),migrated));
    CHECK(migrated.third_person_distance==1 && migrated.first_person && migrated.move_while_knife
        && migrated.reticle==ReticleStyle::Crosshair && migrated.camera_gain==0.4f);
    CHECK(!migrated.unified_controls);
    for(unsigned offset=99;offset<124;++offset){auto bad=bytes;bad[offset]=1;seal(bad);CHECK(!decode_settings(bad.data(),bad.size(),migrated));}
    // v10 retains distance but cannot opt into the new input behavior.
    s={};s.third_person_distance=1.35f;CHECK(encode_settings(s,bytes.data(),bytes.size()));
    bytes[4]=10;seal(bytes);CHECK(decode_settings(bytes.data(),bytes.size(),migrated));
    CHECK(migrated.third_person_distance==1.35f && !migrated.unified_controls);
    bytes[97]|=8;seal(bytes);CHECK(!decode_settings(bytes.data(),bytes.size(),migrated));
    CHECK(encode_settings(s,bytes.data(),bytes.size()));
    for(float invalid : {0.f,0.99f,2.01f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
        auto bad=bytes;std::memcpy(bad.data()+100,&invalid,4);seal(bad);
        Settings unchanged;unchanged.third_person_distance=1.5f;
        CHECK(!decode_settings(bad.data(),bad.size(),unchanged) && unchanged.third_person_distance==1.5f);
    }
    s={};s.first_person=true;CHECK(profile_kind(s,ProfileDomain::Controls)==ProfileKind::Custom);
    CHECK(profile_kind(s,ProfileDomain::Graphics)==ProfileKind::Default);
    copy_profile(s,Settings{},ProfileDomain::Graphics);CHECK(s.first_person);
    copy_profile(s,Settings{},ProfileDomain::Controls);CHECK(!s.first_person);
    std::puts("view settings: v13 response, unified controls, distance, strict fields and legacy migration PASS");
}
