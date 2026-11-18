#include "re4/installer_ui.hpp"
#include "re4/settings_file.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#undef assert
#define assert(c) do{if(!(c)){std::fprintf(stderr,"RE5 language line %d: %s\n",__LINE__,#c);std::exit(1);}}while(false)
int main(){
 using namespace re4;namespace fs=std::filesystem;
 fs::create_directories("build/re5-language-test");
 const char* common="build/re5-language-test/settings.bin";const char* own="build/re5-language-test/re5-settings.bin";
 auto cleanup=[&]{for(const auto* path:{common,own})for(const auto* suffix:{"",".previous",".incoming",".controls-custom",".controls-custom.previous",".controls-1-custom",".controls-1-custom.previous"})fs::remove(std::string(path)+suffix);};cleanup();
 InstallerModel m;m.settings.turn_gain=.4f;m.settings.gyro_bias_x=.125f;
 assert(save_settings_file(common,m.settings));
 m.handle(UiEvent::NextGame);assert(m.game==GameTarget::RE5);
 m.settings.gyro_speed=2.5f;m.selected=8;
 assert(m.handle(UiEvent::Confirm)==InstallerAction::SaveSettings&&m.settings.language==Language::Russian);
 assert(m.save_re5_settings(own));assert(m.save_shared_language(common));
 Settings original;assert(load_settings_file(common,original));
 assert(original.language==Language::Russian&&original.turn_gain==.4f&&original.gyro_bias_x==.125f&&original.gyro_speed==.8f);
 InstallerModel reopened;assert(load_settings_file(common,reopened.settings));assert(re5::load_settings_file(own,reopened.re5_settings));
 reopened.handle(UiEvent::NextGame);assert(reopened.game==GameTarget::RE5&&reopened.settings.language==Language::Russian);
 assert(reopened.item_label(8)=="\u042f\u0437\u044b\u043a"&&reopened.settings.gyro_speed==2.5f);
 assert(!reopened.save_shared_language("build/missing-re5-language-parent/settings.bin")&&reopened.status==InstallerStatus::Failed);
 assert(load_settings_file(common,original)&&original.turn_gain==.4f&&original.language==Language::Russian);
 cleanup();std::puts("RE5 interface language: persists across restart without overwriting RE4 controls PASS");
}
