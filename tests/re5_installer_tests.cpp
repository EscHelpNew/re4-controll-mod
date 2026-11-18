#include "re4/installer_ui.hpp"
#include "re5/settings.hpp"
#include "re5/profiles.hpp"
#include "re4/install.hpp"
#include <fstream>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#undef assert
#define assert(c) do{if(!(c)){std::fprintf(stderr,"RE5 installer line %d: %s\n",__LINE__,#c);std::exit(1);}}while(false)
unsigned review_failures=0;
void review_check(bool pass,const char* message){if(!pass){std::fprintf(stderr,"FINAL REVIEW: %s\n",message);++review_failures;}}
void review_regressions(){
 using namespace re4;namespace fs=std::filesystem;
 const fs::path folder="build/re5-final-review";fs::create_directories(folder);
 const auto path=(folder/"settings.bin").string();
 // Start from fresh owned settings files, retaining a real recovery copy on writes.
 for(const auto* suffix:{"",".previous",".incoming",".controls-1-custom",".controls-1-custom.previous",".controls-2-custom",".controls-2-custom.previous",".graphics-custom",".graphics-custom.previous"})fs::remove(path+suffix);
 InstallerModel m;m.settings.turn_gain=.4f;m.re6_settings.texture_filtering=re6::TextureFilter::Af4x;
 m.handle(UiEvent::NextGame);
 for(unsigned player=0;player<2;++player){
  if(m.re5_player!=player){m.page=InstallerPage::Home;m.selected=6;m.handle(UiEvent::Confirm);}
  for(bool reset:{false,true}){
   m.page=InstallerPage::ControlGroup;m.control_group=ControlGroup::Gyro;m.selected=1;
   m.settings.gyro_speed=1.1f+player;m.settings.turn_gain=.3f+.2f*player;m.settings.gyro_bias_x=.125f+.125f*player;
   assert(m.handle(UiEvent::Right)==InstallerAction::SaveSettings&&m.save_re5_settings(path.c_str()));
   const auto custom=m.settings;const auto other=m.re5_settings.players[player^1];const auto graphics=m.re5_settings.graphics;
   assert(m.select_re5_profile(ProfileDomain::Controls,ProfileKind::RE6,path.c_str())==ProfileResult::Applied);
   if(reset){m.page=InstallerPage::ControlGroup;m.control_group=ControlGroup::Profiles;m.selected=0;}
   else{m.page=InstallerPage::Home;m.selected=8;}
   assert(m.handle(UiEvent::Confirm)==InstallerAction::SaveSettings&&m.save_re5_settings(path.c_str()));
   {const auto result=m.select_re5_profile(ProfileDomain::Controls,ProfileKind::Custom,path.c_str());assert(result==ProfileResult::Applied||result==ProfileResult::Unchanged);}
   review_check(same_profile(m.settings,custom,ProfileDomain::Controls),reset?"R2 controls reset overwrote Custom":"R2 language save overwrote controls Custom");
   review_check(m.settings.gyro_bias_x==custom.gyro_bias_x,"R2 calibration was not retained");
   assert(same_profile(m.re5_settings.players[player^1],other,ProfileDomain::Controls)&&m.re5_settings.players[player^1].gyro_bias_x==other.gyro_bias_x);
   assert(re5::same_graphics(m.re5_settings.graphics,graphics));
  }
  // Ordinary diagnostic calibration remains an intentional Custom edit.
  m.page=InstallerPage::Diagnostics;m.calibrated(.375f+.125f*player,.25f);assert(m.save_re5_settings(path.c_str()));
  re5::Settings custom;assert(re5::load_settings_file((path+(player?".controls-2-custom":".controls-1-custom")).c_str(),custom));
  review_check(custom.players[player].gyro_bias_x==m.settings.gyro_bias_x,"R2 calibration save did not update Custom intentionally");
 }
 for(bool reset:{false,true}){
  m.page=InstallerPage::Graphics;m.selected=unsigned(GraphicsId::Shadows);m.settings.draw_distance_scale=1.5f;
  assert(m.handle(UiEvent::Confirm)==InstallerAction::SaveSettings&&m.save_re5_settings(path.c_str()));
  const auto custom=m.re5_settings.graphics;const auto controls=m.re5_settings.players;
  assert(m.select_re5_profile(ProfileDomain::Graphics,ProfileKind::Quality,path.c_str())==ProfileResult::Applied);
  if(reset){m.page=InstallerPage::Graphics;m.selected=unsigned(GraphicsId::Reset);}
  else{m.page=InstallerPage::Home;m.selected=8;}
  assert(m.handle(UiEvent::Confirm)==InstallerAction::SaveSettings&&m.save_re5_settings(path.c_str()));
  {const auto result=m.select_re5_profile(ProfileDomain::Graphics,ProfileKind::Custom,path.c_str());assert(result==ProfileResult::Applied||result==ProfileResult::Unchanged);}
  review_check(re5::same_graphics(m.re5_settings.graphics,custom),reset?"R2 graphics reset overwrote Custom":"R2 language save overwrote graphics Custom");
  for(unsigned i=0;i<2;++i)assert(same_profile(m.re5_settings.players[i],controls[i],ProfileDomain::Controls));
 }
 re5::Settings recovery;assert(re5::load_settings_file((path+".previous").c_str(),recovery));
 assert(m.re4_configuration().turn_gain==.4f&&m.re6_settings.texture_filtering==re6::TextureFilter::Af4x);
 // Feed successful results from the actual installer into the UI classifier.
 const auto payload=folder/"payload",sd=folder/"sd";fs::create_directories(payload);fs::create_directories(sd);
 auto put=[&](const char* name,const char* magic,unsigned offset){std::string bytes(256,'\0');bytes.replace(offset,4,magic);std::ofstream(payload/name,std::ios::binary).write(bytes.data(),bytes.size());};
 put("re5-subsdk9","NSO0",0);put("re5-main.npdm","META",0);put("re5-controls.ovl","NRO0",16);
 for(auto language:{Language::English,Language::Russian}){
  m.page=InstallerPage::Home;m.settings.language=language;
  m.installation_result(install_game_files(payload.string().c_str(),sd.string().c_str(),GameTarget::RE5));
  review_check(m.status==InstallerStatus::Installed,"R3 actual RE5 install success classified as failed");
  review_check(std::strstr(m.status_text(),"RE5")!=nullptr,"R3 install status does not name RE5");
  m.installation_result(install_overlay_only(payload.string().c_str(),sd.string().c_str(),GameTarget::RE5));
  review_check(m.status==InstallerStatus::OverlayInstalled,"R3 actual RE5 overlay success classified as failed");
  m.installation_result(disable_game_files(sd.string().c_str(),GameTarget::RE5));
  review_check(m.status==InstallerStatus::Disabled,"R3 actual RE5 disable success classified as failed");
  review_check(std::strstr(m.status_text(),"RE5")!=nullptr,"R3 disable status does not name RE5");
  assert(!m.allow_installation(true,true));review_check(std::strstr(m.status_text(),"Resident Evil 5")!=nullptr,"R3 running-game status names another game");
  for(const auto* foreign:{"Installed RE4 Controls","Installed RE4 overlay","Disabled RE4 patch","Installed RE6 module","Installed RE6 overlay","Disabled RE6 mod"}){
   m.installation_result(foreign);review_check(m.status==InstallerStatus::Failed,"R3 foreign-game success accepted for RE5");
  }
  m.page=InstallerPage::Graphics;m.profile_note.clear();m.selected=unsigned(GraphicsId::Profile);auto help=std::string(m.selected_help());
  review_check(help.find("SMAA")!=std::string::npos&&help.find("SSAA")==std::string::npos&&help.find("1.75")!=std::string::npos&&help.find("HOME > X")!=std::string::npos,"R6 graphics Quality help is not RE5-specific");
  m.selected=unsigned(GraphicsId::Shadows);help=m.selected_help();review_check(help.find("foot")==std::string::npos&&help.find("self")==std::string::npos&&help.find(language==Language::English?"verified stage-shadow":"проверенные проходы")!=std::string::npos,"R6 shadows help advertises unverified shadow categories");
  for(unsigned group=0;group<unsigned(ControlGroup::Count);++group){m.page=InstallerPage::ControlGroup;m.control_group=ControlGroup(group);
   for(m.selected=0;m.selected<m.item_count();++m.selected){help=m.selected_help();review_check(help.find("RE4")==std::string::npos&&help.find("Leon")==std::string::npos&&help.find("\u041b\u0435\u043e\u043d")==std::string::npos,"R6 RE5 controls inherit RE4/Leon help");}
  }
  m.page=InstallerPage::Mapping;m.selected=0;review_check(std::strstr(m.selected_help(),"RE4")==nullptr,"R6 RE5 mapping inherits RE4 help");
 }
 // Shared RE4 descriptions stay intact; RE5 makes the classic-only turn
 // contract explicit without coupling unified movement or aiming to it.
 m.game=GameTarget::RE5;m.page=InstallerPage::ControlGroup;m.control_group=ControlGroup::Sticks;m.selected=2;m.settings.language=Language::English;
 review_check(std::strstr(m.selected_help(),"Classic controls only")!=nullptr,"R4 RE5 turn help does not explicitly exclude unified controls");
 InstallerModel original;original.page=InstallerPage::Graphics;original.selected=unsigned(GraphicsId::Profile);assert(std::strstr(original.selected_help(),"SSAA"));
 original.page=InstallerPage::ControlGroup;original.control_group=ControlGroup::Camera;original.selected=4;assert(std::strstr(original.selected_help(),"Leon"));
 for(auto language:{Language::English,Language::Russian}){
  original.game=GameTarget::RE6;original.settings.language=language;original.installation_result("Disabled RE6 mod. Restart RE6.");
  review_check(original.status==InstallerStatus::Disabled&&std::strstr(original.status_text(),"RE6")!=nullptr,"R3 RE6 disable status must also name its actual game");
 }
 // Leave the normal suite's independent files; remove only our known files.
 for(const auto* suffix:{"",".previous",".incoming",".controls-1-custom",".controls-1-custom.previous",".controls-2-custom",".controls-2-custom.previous",".graphics-custom",".graphics-custom.previous"})fs::remove(path+suffix);
}
int main(){
 review_regressions();
 using namespace re4;InstallerModel m;
 m.settings.gyro_speed=0.25f;m.settings.left_deadzone=0.15f;m.selected=3;
 m.re6_settings.texture_filtering=re6::TextureFilter::Af4x;
 m.handle(UiEvent::NextGame);assert(m.game==GameTarget::RE5&&m.selected==0&&m.page==InstallerPage::Home);
 assert(m.settings.gyro_speed==0.8f&&m.re5_player==0);
 assert(m.item_label(6)=="Local player"&&m.item_value(6)=="Player 1");
 m.settings.gyro_speed=1.25f;m.settings.left_deadzone=0.25f;m.selected=6;
 m.handle(UiEvent::Confirm);assert(m.game==GameTarget::RE5&&m.page==InstallerPage::Home&&m.re5_player==1);
 assert(m.settings.gyro_speed==0.8f&&m.re5_settings.players[0].gyro_speed==1.25f);
 m.settings.gyro_speed=2.5f;m.handle(UiEvent::Left);
 assert(m.re5_player==0&&m.settings.gyro_speed==1.25f&&m.re5_settings.players[1].gyro_speed==2.5f);
 m.selected=4;m.handle(UiEvent::Confirm);assert(m.page==InstallerPage::Diagnostics);
 m.handle(UiEvent::OpenGyroTest);assert(m.page==InstallerPage::GyroTest);
 const auto before=m.preview_settings().gyro_speed;m.selected=1;m.handle(UiEvent::Right);
 assert(m.preview_settings().gyro_speed>before&&m.settings.gyro_speed==before);
 m.handle(UiEvent::NextGame);assert(m.game==GameTarget::RE5&&m.page==InstallerPage::GyroTest);
 m.handle(UiEvent::Back);assert(m.page==InstallerPage::Diagnostics&&m.settings.gyro_speed==before);
 m.handle(UiEvent::NextGame);assert(m.game==GameTarget::RE4&&m.selected==3&&m.settings.gyro_speed==0.25f&&m.settings.left_deadzone==0.15f);
 m.handle(UiEvent::PreviousGame);assert(m.game==GameTarget::RE5&&m.settings.gyro_speed==1.25f);
 m.handle(UiEvent::PreviousGame);assert(m.game==GameTarget::RE4&&m.selected==3);
 m.handle(UiEvent::NextGame);assert(m.game==GameTarget::RE5&&m.settings.gyro_speed==1.25f);
 assert(m.re6_settings.texture_filtering==re6::TextureFilter::Af4x);
 m.handle(UiEvent::Back); // RE5 Home: Back exits, no accidental RE4 page.
 m.page=InstallerPage::Home;m.selected=3;m.handle(UiEvent::Confirm);
 assert(m.page==InstallerPage::Controls);m.selected=0;
 assert(m.item_value(0)=="Custom");
 m.selected=unsigned(ControlGroup::Gyro)+1;m.handle(UiEvent::Confirm);
 assert(m.page==InstallerPage::ControlGroup&&m.control_group==ControlGroup::Gyro);
 m.selected=control_group_count(ControlGroup::Gyro);m.handle(UiEvent::Confirm);assert(m.page==InstallerPage::GyroTest);
 m.selected=1;m.handle(UiEvent::Right);
 const auto draft=m.preview_settings().gyro_speed;
 std::filesystem::create_directories("build/re5-ui-test");const char* path="build/re5-ui-test/settings.bin";
 for(const auto suffix:{"",".previous",".incoming",".controls-1-custom",".controls-1-custom.previous",".controls-1-custom.incoming"})std::filesystem::remove(std::string(path)+suffix);
 assert(m.save_gyro_test(path)&&m.page==InstallerPage::ControlGroup&&m.selected==control_group_count(ControlGroup::Gyro));
 re5::Settings loaded;assert(re5::load_settings_file(path,loaded));
 assert(loaded.players[0].gyro_speed==draft&&loaded.players[1].gyro_speed==2.5f&&m.settings.gyro_speed==draft);
 // A failed RE5 draft save must preserve its origin and both active player values.
 m.handle(UiEvent::Confirm);m.selected=1;m.handle(UiEvent::Right);
 assert(!m.save_gyro_test("build/missing-re5-parent/settings.bin"));
 assert(m.page==InstallerPage::GyroTest&&m.settings.gyro_speed==draft&&m.re5_settings.players[1].gyro_speed==2.5f);
 m.handle(UiEvent::Back);
 for(const auto suffix:{"",".previous",".incoming",".controls-1-custom",".controls-1-custom.previous",".controls-1-custom.incoming"})std::filesystem::remove(std::string(path)+suffix);
 assert(review_failures==0);
 std::puts("RE5 installer: RE4/RE5 navigation, separate player controls, draft ownership and save/cancel PASS");
}
