#include "re4/settings.hpp"
#include "re4/profiles.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>

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
    CHECK(!s.first_person && !s.move_while_aiming && !s.move_while_knife
        && s.reticle==ReticleStyle::Off && s.reticle_size==ReticleSize::Small);
    CHECK(encode_settings(s,bytes.data(),bytes.size()) && bytes[4]==9);
    for(unsigned flags=0;flags<8;++flags)for(unsigned style=0;style<3;++style)for(unsigned size=0;size<3;++size){
        s.first_person=flags&1;s.move_while_aiming=flags&2;s.move_while_knife=flags&4;
        s.reticle=static_cast<ReticleStyle>(style);s.reticle_size=static_cast<ReticleSize>(size);
        CHECK(encode_settings(s,bytes.data(),bytes.size()));
        CHECK(bytes[96]==style && bytes[97]==flags && bytes[98]==size);
        Settings decoded;CHECK(decode_settings(bytes.data(),bytes.size(),decoded));
        CHECK(decoded.first_person==s.first_person && decoded.move_while_aiming==s.move_while_aiming
            && decoded.move_while_knife==s.move_while_knife && decoded.reticle==s.reticle && decoded.reticle_size==s.reticle_size);
        CHECK(same_profile(s,decoded,ProfileDomain::Controls));
    }
    for(unsigned offset:{96,97,98,99,100,123}){
        auto bad=bytes;bad[offset]=255;seal(bad);Settings out;out.camera_gain=0.3f;
        CHECK(!decode_settings(bad.data(),bad.size(),out) && out.camera_gain==0.3f);
    }
    // v8 restores every previous domain and starts all new gameplay modes disabled.
    s={};s.gyro_enabled=true;s.camera_gain=0.4f;s.texture_filtering=TextureFilter::Aniso4;
    s.language=Language::Russian;s.gyro_bias_y=0.25f;s.button_map[0]=1;
    CHECK(encode_settings(s,bytes.data(),bytes.size()));bytes[4]=8;seal(bytes);
    Settings migrated;CHECK(decode_settings(bytes.data(),bytes.size(),migrated));
    CHECK(migrated.gyro_enabled && migrated.camera_gain==0.4f && migrated.texture_filtering==TextureFilter::Aniso4
        && migrated.language==Language::Russian && migrated.gyro_bias_y==0.25f && migrated.button_map[0]==1);
    CHECK(!migrated.first_person && !migrated.move_while_aiming && !migrated.move_while_knife && migrated.reticle==ReticleStyle::Off);
    for(unsigned offset=96;offset<124;++offset){auto bad=bytes;bad[offset]=1;seal(bad);CHECK(!decode_settings(bad.data(),bad.size(),migrated));}
    s={};s.first_person=true;CHECK(profile_kind(s,ProfileDomain::Controls)==ProfileKind::Custom);
    CHECK(profile_kind(s,ProfileDomain::Graphics)==ProfileKind::Default);
    copy_profile(s,Settings{},ProfileDomain::Graphics);CHECK(s.first_person);
    copy_profile(s,Settings{},ProfileDomain::Controls);CHECK(!s.first_person);
    std::puts("view settings: v9, strict fields, disabled defaults, v8 migration and profile domains PASS");
}
