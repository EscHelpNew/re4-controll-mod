#include "re5/profiles.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#undef assert
#define assert(c) do {if(!(c)){std::fprintf(stderr,"RE5 profile line %d: %s\n",__LINE__,#c);std::exit(1);}}while(false)
int main(){
 namespace fs=std::filesystem;using re4::ProfileDomain;using re4::ProfileKind;using re4::ProfileResult;
 const char* path="build/re5-profile-test/settings.bin";fs::create_directories("build/re5-profile-test");
 for(const auto suffix:{"",".previous",".incoming",".controls-1-custom",".controls-2-custom",".graphics-custom"})
  for(const auto backup:{"",".previous",".incoming"})fs::remove(std::string(path)+suffix+backup);
 re5::Settings active;active.language=re4::Language::Russian;
 active.players[0].gyro_bias_x=0.1f;active.players[1].gyro_bias_x=-0.2f;
 active.players[0].gyro_speed=0.25f;active.players[1].gyro_speed=2.5f;
 active.graphics.draw_distance=1.5f;
 assert(re5::save_edited_settings(path,active,ProfileDomain::Controls,0));
 assert(re5::save_edited_settings(path,active,ProfileDomain::Controls,1));
 assert(re5::save_edited_settings(path,active,ProfileDomain::Graphics,0));
 assert(re5::switch_profile(path,active,ProfileDomain::Controls,ProfileKind::RE6,0)==ProfileResult::Applied);
 assert(active.players[0].unified_controls&&active.players[0].gyro_enabled&&active.players[0].move_while_aiming);
 assert(active.players[1].gyro_speed==2.5f&&active.players[1].gyro_bias_x==-0.2f);
 assert(active.players[0].gyro_bias_x==0.1f&&active.graphics.draw_distance==1.5f&&active.language==re4::Language::Russian);
 assert(re5::switch_profile(path,active,ProfileDomain::Controls,ProfileKind::Default,1)==ProfileResult::Applied);
 assert(!active.players[1].gyro_enabled&&!active.players[1].analog_enabled&&active.players[1].gyro_bias_x==-0.2f);
 assert(re5::switch_profile(path,active,ProfileDomain::Controls,ProfileKind::Custom,0)==ProfileResult::Applied);
 assert(active.players[0].gyro_speed==0.25f&&active.players[1].gyro_speed==0.8f);
 assert(re5::switch_profile(path,active,ProfileDomain::Controls,ProfileKind::Custom,1)==ProfileResult::Applied);
 assert(active.players[1].gyro_speed==2.5f&&active.players[0].gyro_speed==0.25f);
 assert(re5::switch_profile(path,active,ProfileDomain::Graphics,ProfileKind::Quality,0)==ProfileResult::Applied);
 assert(active.graphics.filter==re4::TextureFilter::Aniso16&&active.graphics.draw_distance==1.75f&&active.graphics.aa==re5::AntiAliasing::Smaa);
 assert(active.players[0].gyro_speed==0.25f&&active.players[1].gyro_speed==2.5f);
 assert(re5::switch_profile(path,active,ProfileDomain::Graphics,ProfileKind::Default,1)==ProfileResult::Applied);
 assert(active.graphics.draw_distance==1&&active.graphics.filter==re4::TextureFilter::Native);
 assert(re5::switch_profile(path,active,ProfileDomain::Graphics,ProfileKind::Custom,1)==ProfileResult::Applied);
 assert(active.graphics.draw_distance==1.5f);
 // Manual canonical graphics remain recoverable as Custom, like RE4.
 active.graphics={};assert(re5::save_edited_settings(path,active,ProfileDomain::Graphics,1));
 assert(re5::switch_profile(path,active,ProfileDomain::Graphics,ProfileKind::Quality,0)==ProfileResult::Applied);
 assert(re5::switch_profile(path,active,ProfileDomain::Graphics,ProfileKind::Custom,0)==ProfileResult::Applied);
 assert(active.graphics.draw_distance==1&&active.graphics.filter==re4::TextureFilter::Native);
 assert(re5::switch_profile(path,active,ProfileDomain::Controls,ProfileKind::Quality,0)==ProfileResult::SaveFailed);
 assert(re5::switch_profile(path,active,ProfileDomain::Graphics,ProfileKind::RE6,0)==ProfileResult::SaveFailed);
 assert(re5::switch_profile(path,active,ProfileDomain::Controls,ProfileKind::RE6,2)==ProfileResult::SaveFailed);
 assert(re5::switch_profile("build/no-re5-profile/settings.bin",active,ProfileDomain::Controls,ProfileKind::Custom,0)==ProfileResult::MissingCustom);
 fs::remove(std::string(path)+".incoming");fs::create_directory(std::string(path)+".incoming");
 const float saved=active.players[0].gyro_speed;
 assert(re5::switch_profile(path,active,ProfileDomain::Controls,ProfileKind::RE6,0)==ProfileResult::SaveFailed);
 assert(active.players[0].gyro_speed==saved&&active.players[1].gyro_speed==2.5f);
 fs::remove(std::string(path)+".incoming");
 for(const auto suffix:{"",".previous",".incoming",".controls-1-custom",".controls-2-custom",".graphics-custom"})
  for(const auto backup:{"",".previous",".incoming"})fs::remove(std::string(path)+suffix+backup);
 std::puts("RE5 profiles: separate player Custom, calibration/language preservation and transactional switch PASS");
}
