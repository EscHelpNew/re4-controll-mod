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
    CHECK(!s.enemy_health_bars&&!s.enemy_health_numbers&&!s.grenade_landing_spot&&!s.skip_pickup_confirmation&&!s.skip_startup_logos&&!s.disable_aim_sway&&!s.dpad_weapon_switching);
    s.grenade_landing_spot=true;s.skip_pickup_confirmation=true;s.skip_startup_logos=true;s.disable_aim_sway=true;s.dpad_weapon_switching=true;
    s.enemy_health_bars=true;s.enemy_health_numbers=true;CHECK(encode_settings(s,bytes.data(),bytes.size())&&bytes[104]==1&&bytes[105]==1&&bytes[106]==1&&bytes[107]==1&&bytes[108]==1&&bytes[109]==1&&bytes[110]==1);
    Settings game_roundtrip;CHECK(decode_settings(bytes.data(),bytes.size(),game_roundtrip)&&game_roundtrip.enemy_health_bars&&game_roundtrip.enemy_health_numbers&&game_roundtrip.grenade_landing_spot&&game_roundtrip.skip_pickup_confirmation&&game_roundtrip.skip_startup_logos&&game_roundtrip.disable_aim_sway&&game_roundtrip.dpad_weapon_switching);
    auto deployed=bytes;deployed[4]=16;seal(deployed);
    CHECK(!decode_settings(deployed.data(),deployed.size(),game_roundtrip)); // v16 byte110 was reserved.
    deployed[110]=0;seal(deployed);game_roundtrip.dpad_weapon_switching=true;
    CHECK(decode_settings(deployed.data(),deployed.size(),game_roundtrip));
    CHECK(game_roundtrip.enemy_health_bars && game_roundtrip.enemy_health_numbers && game_roundtrip.grenade_landing_spot
        && game_roundtrip.skip_pickup_confirmation && game_roundtrip.skip_startup_logos && game_roundtrip.disable_aim_sway
        && !game_roundtrip.dpad_weapon_switching);
    for(unsigned offset=110;offset<124;++offset){auto bad=deployed;bad[offset]=1;seal(bad);CHECK(!decode_settings(bad.data(),bad.size(),game_roundtrip));}
    for(unsigned offset=106;offset<110;++offset){auto bad=deployed;bad[offset]=2;seal(bad);CHECK(!decode_settings(bad.data(),bad.size(),game_roundtrip));}
    auto v15=bytes;v15[4]=15;seal(v15);CHECK(!decode_settings(v15.data(),v15.size(),game_roundtrip));
    v15[106]=v15[107]=v15[108]=v15[109]=v15[110]=0;seal(v15);CHECK(decode_settings(v15.data(),v15.size(),game_roundtrip)&&game_roundtrip.enemy_health_bars&&game_roundtrip.enemy_health_numbers&&!game_roundtrip.grenade_landing_spot&&!game_roundtrip.skip_pickup_confirmation&&!game_roundtrip.skip_startup_logos&&!game_roundtrip.disable_aim_sway&&!game_roundtrip.dpad_weapon_switching);
    bytes[106]=bytes[107]=bytes[108]=bytes[109]=bytes[110]=0;seal(bytes);
    auto old=bytes;old[4]=13;seal(old);CHECK(!decode_settings(old.data(),old.size(),game_roundtrip));
    old[104]=old[105]=0;seal(old);CHECK(decode_settings(old.data(),old.size(),game_roundtrip)&&!game_roundtrip.enemy_health_bars&&!game_roundtrip.enemy_health_numbers&&!game_roundtrip.grenade_landing_spot&&!game_roundtrip.skip_pickup_confirmation&&!game_roundtrip.skip_startup_logos&&!game_roundtrip.disable_aim_sway&&!game_roundtrip.dpad_weapon_switching);
    auto previous=bytes;previous[4]=14;seal(previous);CHECK(!decode_settings(previous.data(),previous.size(),game_roundtrip));
    previous[105]=0;seal(previous);CHECK(decode_settings(previous.data(),previous.size(),game_roundtrip)&&game_roundtrip.enemy_health_bars&&!game_roundtrip.enemy_health_numbers&&!game_roundtrip.grenade_landing_spot&&!game_roundtrip.skip_pickup_confirmation&&!game_roundtrip.skip_startup_logos&&!game_roundtrip.disable_aim_sway&&!game_roundtrip.dpad_weapon_switching);
    for(unsigned version=7;version<=15;++version){
        CHECK(encode_settings(Settings{},old.data(),old.size()));old[4]=std::uint8_t(version);
        if(version==7)for(unsigned i=88;i<124;++i)old[i]=0;
        else if(version<10)for(unsigned i=100;i<124;++i)old[i]=0;
        seal(old);game_roundtrip.enemy_health_bars=true;game_roundtrip.enemy_health_numbers=true;game_roundtrip.grenade_landing_spot=true;game_roundtrip.skip_pickup_confirmation=true;game_roundtrip.skip_startup_logos=true;game_roundtrip.disable_aim_sway=true;game_roundtrip.dpad_weapon_switching=true;
        CHECK(decode_settings(old.data(),old.size(),game_roundtrip)&&!game_roundtrip.enemy_health_bars&&!game_roundtrip.enemy_health_numbers&&!game_roundtrip.grenade_landing_spot&&!game_roundtrip.skip_pickup_confirmation&&!game_roundtrip.skip_startup_logos&&!game_roundtrip.disable_aim_sway&&!game_roundtrip.dpad_weapon_switching);
    }
    for(auto domain:{ProfileDomain::Controls,ProfileDomain::Graphics}){
        const auto profile=domain==ProfileDomain::Controls?re6_profile():quality_graphics_profile();
        copy_profile(s,profile,domain);CHECK(s.enemy_health_bars&&s.enemy_health_numbers&&s.grenade_landing_spot&&s.skip_pickup_confirmation&&s.skip_startup_logos&&s.disable_aim_sway&&s.dpad_weapon_switching);
        auto other=s;other.enemy_health_bars=false;other.enemy_health_numbers=false;other.grenade_landing_spot=false;other.skip_pickup_confirmation=false;other.skip_startup_logos=false;other.disable_aim_sway=false;other.dpad_weapon_switching=false;CHECK(same_profile(s,other,domain));
    }
    for(unsigned offset:{106,107,108,109,110})for(unsigned value:{2u,255u}){
        auto bad=bytes;bad[offset]=std::uint8_t(value);seal(bad);
        CHECK(!decode_settings(bad.data(),bad.size(),game_roundtrip));
    }
    for(unsigned offset:{106,107,108,109,110}){auto bad=v15;bad[offset]=1;seal(bad);CHECK(!decode_settings(bad.data(),bad.size(),game_roundtrip));}
    for(unsigned offset=114;offset<124;++offset){auto bad=bytes;bad[offset]=1;seal(bad);CHECK(!decode_settings(bad.data(),bad.size(),game_roundtrip));}
    {
        // v19 byte112: extended weapon upgrade. Strict boolean, reserved before v19.
        Settings extended;CHECK(!extended.extended_weapon_upgrade);extended.extended_weapon_upgrade=true;
        Bytes wire{};CHECK(encode_settings(extended,wire.data(),wire.size())&&wire[4]==19&&wire[112]==1);
        Settings decoded;CHECK(decode_settings(wire.data(),wire.size(),decoded)&&decoded.extended_weapon_upgrade);
        extended.extended_weapon_upgrade=false;
        CHECK(encode_settings(extended,wire.data(),wire.size())&&wire[112]==0);
        decoded.extended_weapon_upgrade=true;CHECK(decode_settings(wire.data(),wire.size(),decoded)&&!decoded.extended_weapon_upgrade);
        for(unsigned value:{2u,255u}){auto bad=wire;bad[112]=std::uint8_t(value);seal(bad);CHECK(!decode_settings(bad.data(),bad.size(),decoded));}
        auto v18=wire;v18[4]=18;v18[112]=1;seal(v18);CHECK(!decode_settings(v18.data(),v18.size(),decoded));
        v18[112]=0;seal(v18);decoded.extended_weapon_upgrade=true;
        CHECK(decode_settings(v18.data(),v18.size(),decoded)&&!decoded.extended_weapon_upgrade);
        auto future=wire;future[4]=20;seal(future);CHECK(!decode_settings(future.data(),future.size(),decoded));
        // v19 byte113: case ammo counter, same strict boolean rule.
        Settings counter;counter.case_ammo_counter=true;
        CHECK(encode_settings(counter,wire.data(),wire.size())&&wire[112]==0&&wire[113]==1);
        CHECK(decode_settings(wire.data(),wire.size(),decoded)&&decoded.case_ammo_counter&&!decoded.extended_weapon_upgrade);
        for(unsigned value:{2u,255u}){auto bad=wire;bad[113]=std::uint8_t(value);seal(bad);CHECK(!decode_settings(bad.data(),bad.size(),decoded));}
        auto old=wire;old[4]=18;seal(old);CHECK(!decode_settings(old.data(),old.size(),decoded));
    }
    for(unsigned mask=0;mask<32;++mask){
        Settings flags;flags.grenade_landing_spot=mask&1;flags.skip_pickup_confirmation=mask&2;flags.skip_startup_logos=mask&4;flags.disable_aim_sway=mask&8;flags.dpad_weapon_switching=mask&16;
        CHECK(encode_settings(flags,bytes.data(),bytes.size()));
        CHECK(bytes[4]==19 && bytes[106]==bool(mask&1) && bytes[107]==bool(mask&2) && bytes[108]==bool(mask&4) && bytes[109]==bool(mask&8) && bytes[110]==bool(mask&16));
        CHECK(decode_settings(bytes.data(),bytes.size(),game_roundtrip));
        CHECK(game_roundtrip.grenade_landing_spot==flags.grenade_landing_spot && game_roundtrip.skip_pickup_confirmation==flags.skip_pickup_confirmation && game_roundtrip.skip_startup_logos==flags.skip_startup_logos && game_roundtrip.disable_aim_sway==flags.disable_aim_sway && game_roundtrip.dpad_weapon_switching==flags.dpad_weapon_switching);
    }
    // Published v16 has six Game booleans, followed by reserved byte110.
    for(unsigned mask=0;mask<64;++mask){
        auto legacy=deployed;
        for(unsigned field=0;field<6;++field)legacy[104+field]=bool(mask&(1u<<field));
        seal(legacy);game_roundtrip.dpad_weapon_switching=true;
        CHECK(decode_settings(legacy.data(),legacy.size(),game_roundtrip));
        CHECK(game_roundtrip.enemy_health_bars==bool(mask&1) && game_roundtrip.enemy_health_numbers==bool(mask&2)
            && game_roundtrip.grenade_landing_spot==bool(mask&4) && game_roundtrip.skip_pickup_confirmation==bool(mask&8)
            && game_roundtrip.skip_startup_logos==bool(mask&16) && game_roundtrip.disable_aim_sway==bool(mask&32)
            && !game_roundtrip.dpad_weapon_switching);
    }
    s={};
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
    CHECK(encode_settings(s,bytes.data(),bytes.size()) && bytes[4]==19);
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
    for(unsigned offset:{96,97,98,99,104,105,106,123}){
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
    std::puts("view settings: v18 response, game options, domain preservation and legacy migration PASS");
}
