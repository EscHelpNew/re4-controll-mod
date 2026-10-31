#define main re4_overlay_entry
#include "../source/overlay/main.cpp"
#undef main
#include "re4/installer_ui.hpp"
#include <cassert>
#include <map>
#include <cstdlib>
#undef assert
#define assert(condition) do { if(!(condition)){std::fprintf(stderr,"profile UI line %d: %s\n",__LINE__,#condition);std::exit(1);} } while(false)

namespace {
std::map<std::string,re4::Settings> files;
bool writable=true;
unsigned sends=0;
tsl::elm::ListItem* row(tsl::elm::OverlayFrame* frame,const std::string& label){
    for(auto* e:static_cast<tsl::elm::List*>(frame->content())->children)
        if(auto* item=dynamic_cast<tsl::elm::ListItem*>(e))if(item->label==label)return item;
    assert(false);return nullptr;
}
}
namespace overlay_storage {bool ready(){return true;}bool mount(){return true;}void close(){}}
namespace re4 {
bool save_settings_file(const char* path,const Settings& value){
    if(!writable || !valid_settings(value))return false;
    files[path]=value;return true;
}
bool load_settings_file(const char* path,Settings& value){
    const auto found=files.find(path);if(found==files.end())return false;
    value=found->second;return true;
}
}
void GameLink::initialize(){}
void GameLink::close(){}
bool GameLink::connect(){return true;}
bool GameLink::apply(const re4::Settings& s){++sends;return re4::valid_settings(s);}
int main(){
    using namespace re4;
    assert(profile_kind(settings,ProfileDomain::Controls)==ProfileKind::Default);
    settings=default_controls_profile();note.clear();ControlsGui controls;
    auto* root=static_cast<tsl::elm::OverlayFrame*>(controls.createUI());
    auto* list=static_cast<tsl::elm::List*>(root->content());
    assert(list->children.size()==unsigned(ControlGroup::Count)+1);
    auto* profile=static_cast<tsl::elm::ListItem*>(list->children[0]);
    assert(profile->label=="Controls profile" && profile->value=="Default");
    assert(profile->click(HidNpadButton_Left)); // Missing Custom leaves active and storage untouched.
    assert(files.empty() && sends==0 && note.find("empty")!=std::string::npos);
    assert(profile->click(HidNpadButton_A));
    assert(settings.re6_response && profile->value=="RE6" && tsl::navigation.empty());
    assert(settings.move_while_reloading);
    assert(files.count(std::string(overlay_storage::settings_path)+".controls-custom")==0);
    const auto sent=sends;
    assert(profile->click(HidNpadButton_Right)); // Missing Custom must not silently select Default.
    assert(profile->value=="RE6" && settings.re6_response && sends==sent);
    settings.camera_gain=0.45f;settings.texture_filtering=TextureFilter::Aniso4;
    settings.gyro_bias_x=0.3f;settings.calibration_request=17;
    assert(save_profiled_settings(overlay_storage::settings_path,settings));controls.update();
    assert(profile->value=="Custom");
    assert(profile->click(HidNpadButton_A));
    assert(profile->value=="Default" && !settings.analog_enabled && !settings.remap_enabled && !settings.gyro_enabled);
    assert(!settings.move_while_reloading);
    assert(settings.texture_filtering==TextureFilter::Aniso4 && settings.gyro_bias_x==0.3f && settings.calibration_request==17);
    assert(profile->click(HidNpadButton_Left));
    assert(profile->value=="Custom" && settings.camera_gain==0.45f && settings.re6_response);
    assert(settings.move_while_reloading);
    writable=false;const auto before=sends;
    assert(profile->click(HidNpadButton_A));
    assert(settings.camera_gain==0.45f && profile->value=="Custom" && sends==before && note.find("save")!=std::string::npos);
    writable=true;
    settings.move_while_reloading=false;
    assert(save_profiled_settings(overlay_storage::settings_path,settings));
    assert(profile->click(HidNpadButton_A)); // Default -> RE6 -> Custom restores the manual reload choice.
    assert(profile->click(HidNpadButton_A));
    assert(settings.move_while_reloading);
    assert(profile->click(HidNpadButton_A));
    assert(!settings.move_while_reloading && settings.camera_gain==0.45f);
    for(auto language:{Language::English,Language::Russian}){
        settings.language=language;controls.update();
        assert(profile->label==control_spec(ControlId::Profile).label.get(language));
        assert(profile->value==profile_name(ProfileKind::Custom,language));
        controls.focused=profile;controls.update();
        assert(row(root,control_group_spec(ControlGroup::Profiles).label.get(language))->click(HidNpadButton_A));
        auto* reset=static_cast<tsl::elm::OverlayFrame*>(tsl::navigation.back()->root);
        assert(static_cast<tsl::elm::List*>(reset->content())->children.size()==1);
        assert(row(reset,control_spec(ControlId::Reset).label.get(language)));
        tsl::goBack();
    }
    delete root;
    InstallerModel model;model.page=InstallerPage::Controls;
    assert(model.item_count()==8 && model.item_value(0)=="Default" && model.selected_control()==ControlId::Profile);
    assert(model.handle(UiEvent::Confirm)==InstallerAction::RE6Controls);
    assert(!model.settings.re6_response && model.page==InstallerPage::Controls); // Dispatch persists the concrete choice.
    assert(model.handle(UiEvent::Left)==InstallerAction::CustomControls);
    model.settings=re6_profile();
    assert(model.handle(UiEvent::Right)==InstallerAction::CustomControls);
    assert(model.handle(UiEvent::Left)==InstallerAction::DefaultControls);
    model.settings.camera_gain=0.4f;
    assert(model.handle(UiEvent::Confirm)==InstallerAction::DefaultControls);
    for(auto language:{Language::English,Language::Russian}){
        model.settings.language=language;
        assert(model.item_label(0)==control_spec(ControlId::Profile).label.get(language));
        assert(std::string(model.selected_help()).find("RE9")==std::string::npos);
    }
    model.profile_note="Custom profile is empty";
    assert(std::string(model.selected_help())==model.profile_note);
    model.handle(UiEvent::Down);assert(model.profile_note.empty());
    model.page=InstallerPage::Graphics;model.selected=unsigned(GraphicsId::Profile);
    assert(model.handle(UiEvent::Right)==InstallerAction::CustomGraphics);
    model.settings.shadow_mode=ShadowMode::Off;
    assert(model.handle(UiEvent::Confirm)==InstallerAction::DefaultGraphics);
    std::puts("profile UI: three inline profiles, missing Custom, save failures, bilingual navigation and domain scope PASS");
}
