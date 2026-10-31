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
    {
        settings={};note.clear();ControlsProfilesGui fresh;
        auto* choices=static_cast<tsl::elm::OverlayFrame*>(fresh.createUI());
        const auto& choice_rows=static_cast<tsl::elm::List*>(choices->content())->children;
        assert(choice_rows.size()==4);
        assert(static_cast<tsl::elm::ListItem*>(choice_rows[0])->label=="Default");
        assert(static_cast<tsl::elm::ListItem*>(choice_rows[1])->label=="RE9");
        assert(static_cast<tsl::elm::ListItem*>(choice_rows[2])->label=="RE6");
        assert(static_cast<tsl::elm::ListItem*>(choice_rows[3])->label=="Custom");
        const auto untouched=sends;
        assert(row(choices,"Custom")->click(HidNpadButton_A));
        assert(profile_kind(settings,ProfileDomain::Controls)==ProfileKind::Default
            && files.empty() && sends==untouched && note.find("empty")!=std::string::npos);
        assert(row(choices,"RE9")->click(HidNpadButton_A));
        assert(profile_kind(settings,ProfileDomain::Controls)==ProfileKind::RE9);
        assert(files.count(std::string(overlay_storage::settings_path)+".controls-custom")==0);
        assert(row(choices,"RE6")->click(HidNpadButton_A));
        assert(profile_kind(settings,ProfileDomain::Controls)==ProfileKind::RE6 && settings.re6_response);
        assert(row(choices,"RE6")->value=="Active");
        settings.camera_gain=0.45f;
        assert(save_profiled_settings(overlay_storage::settings_path,settings));
        assert(row(choices,"Default")->click(HidNpadButton_A));
        assert(profile_kind(settings,ProfileDomain::Controls)==ProfileKind::Default && !settings.re6_response);
        assert(row(choices,"Custom")->click(HidNpadButton_A));
        assert(settings.re6_response && settings.camera_gain==0.45f);
        delete choices;files.clear();note.clear();sends=0;
    }
    settings={};note.clear();MainGui main_gui;
    auto* root=static_cast<tsl::elm::OverlayFrame*>(main_gui.createUI());
    const auto initial_sends=sends;
    assert(!row(root,"Controls")->click(HidNpadButton_Right));
    assert(row(root,"Graphics")->click(HidNpadButton_A));
    auto* graphics=static_cast<tsl::elm::OverlayFrame*>(tsl::navigation.back()->root);
    auto* profile=row(graphics,"Graphics profile");
    assert(profile->value=="Default" && files.empty() && sends==initial_sends);
    assert(profile->click(HidNpadButton_A));
    assert(files.empty() && note.find("Custom")!=std::string::npos && sends==initial_sends);
    assert(row(graphics,"Texture filtering")->click(HidNpadButton_Right));
    assert(profile->value=="Custom");
    settings.gyro_enabled=true;settings.language=Language::Russian;settings.gyro_bias_x=0.3f;
    // Switching this domain must preserve unrelated settings and device preferences.
    assert(profile->click(HidNpadButton_A));
    assert(settings.texture_filtering==TextureFilter::Native && settings.gyro_enabled
        && settings.language==Language::Russian && settings.gyro_bias_x==0.3f);
    assert(profile->value=="По умолчанию");
    assert(profile->click(HidNpadButton_A));
    assert(settings.texture_filtering==TextureFilter::Bilinear && profile->value=="Пользовательский");
    tsl::goBack();settings.language=Language::English;
    assert(row(root,"Controls")->click(HidNpadButton_A));
    auto* groups=static_cast<tsl::elm::OverlayFrame*>(tsl::navigation.back()->root);
    assert(row(groups,"Profiles and reset")->click(HidNpadButton_A));
    auto* controls=static_cast<tsl::elm::OverlayFrame*>(tsl::navigation.back()->root);
    for(auto* e:static_cast<tsl::elm::List*>(controls->content())->children)
        if(auto* item=dynamic_cast<tsl::elm::ListItem*>(e))assert(item->label!="Graphics");
    profile=row(controls,"Controls profile");
    assert(profile->value=="Custom");
    const auto before_picker=sends;
    assert(profile->click(HidNpadButton_A));
    auto* picker=static_cast<tsl::elm::OverlayFrame*>(tsl::navigation.back()->root);
    assert(picker!=controls && sends==before_picker);
    assert(row(picker,"RE9"));
    assert(row(picker,"Default")->click(HidNpadButton_A));
    assert(!settings.gyro_enabled && settings.texture_filtering==TextureFilter::Bilinear
        && settings.gyro_bias_x==0.3f);
    assert(row(picker,"RE9")->click(HidNpadButton_A));
    assert(settings.third_person_distance==1.15f && settings.move_while_aiming && settings.move_while_knife
        && settings.gyro_speed==0.8f && settings.texture_filtering==TextureFilter::Bilinear);
    assert(row(picker,"Custom")->click(HidNpadButton_A) && settings.gyro_enabled);
    assert(settings.third_person_distance==1 && !settings.move_while_knife); // Pre-RE9 Custom was retained.
    const auto before_failure=sends;writable=false;
    assert(row(picker,"Default")->click(HidNpadButton_A));
    assert(settings.gyro_enabled && sends==before_failure && note.find("save")!=std::string::npos);
    auto* controls_gui=tsl::navigation.back().get();
    const auto profile_text_sets=row(picker,"RE9")->text_sets;
    for(unsigned frame=0;frame<31;++frame){
        controls_gui->update();assert(picker->subtitle==note);
        assert(row(picker,"RE9")->text_sets==profile_text_sets);
    }
    writable=true;
    tsl::goBack(); // Profile picker -> Profiles and reset.
    tsl::goBack();
    assert(row(groups,"Camera")->click(HidNpadButton_A));
    controls=static_cast<tsl::elm::OverlayFrame*>(tsl::navigation.back()->root);
    auto* first_person_toggle=dynamic_cast<tsl::elm::ToggleListItem*>(row(controls,"First-person view"));
    assert(first_person_toggle);first_person_toggle->changed(true);tsl::goBack();
    assert(row(groups,"Aiming")->click(HidNpadButton_A));
    controls=static_cast<tsl::elm::OverlayFrame*>(tsl::navigation.back()->root);
    auto* aim_move_toggle=dynamic_cast<tsl::elm::ToggleListItem*>(row(controls,"Move while aiming"));
    auto* knife_move_toggle=dynamic_cast<tsl::elm::ToggleListItem*>(row(controls,"Move with knife"));
    assert(aim_move_toggle && knife_move_toggle);
    aim_move_toggle->changed(true);knife_move_toggle->changed(true);tsl::goBack();
    assert(row(groups,"Reticle")->click(HidNpadButton_A));
    controls=static_cast<tsl::elm::OverlayFrame*>(tsl::navigation.back()->root);
    assert(row(controls,"Reticle")->click(HidNpadButton_Right));
    assert(settings.first_person && settings.move_while_aiming && settings.move_while_knife && settings.reticle==ReticleStyle::Dot);
    assert(files.at(overlay_storage::settings_path).reticle==ReticleStyle::Dot);
    tsl::goBack();assert(row(groups,"Profiles and reset")->click(HidNpadButton_A));
    controls=static_cast<tsl::elm::OverlayFrame*>(tsl::navigation.back()->root);profile=row(controls,"Controls profile");
    assert(profile->click(HidNpadButton_A));
    picker=static_cast<tsl::elm::OverlayFrame*>(tsl::navigation.back()->root);
    assert(row(picker,"Default")->click(HidNpadButton_A));
    assert(!settings.first_person && !settings.move_while_aiming && !settings.move_while_knife && settings.reticle==ReticleStyle::Off);
    assert(row(picker,"Custom")->click(HidNpadButton_A));
    assert(settings.first_person && settings.move_while_aiming && settings.move_while_knife && settings.reticle==ReticleStyle::Dot);
    tsl::goBack();tsl::goBack();tsl::goBack();delete root;

    InstallerModel model;model.page=InstallerPage::ControlGroup;model.control_group=ControlGroup::Profiles;
    model.selected=0;
    assert(model.item_value(model.selected)=="Default");
    assert(model.handle(UiEvent::Confirm)==InstallerAction::None);
    assert(model.item_count()==4 && model.item_label(1)=="RE9" && model.item_label(2)=="RE6");
    assert(model.item_value(0)=="Active");
    model.selected=1;
    assert(model.handle(UiEvent::Confirm)==InstallerAction::RE9Controls);
    model.settings=re9_profile();assert(model.item_value(1)=="Active");
    model.selected=2;assert(model.handle(UiEvent::Confirm)==InstallerAction::RE6Controls);
    model.settings=re6_profile();assert(model.item_value(2)=="Active");
    assert(std::string(model.selected_help()).find("measured")!=std::string::npos);
    model.selected=3;assert(model.handle(UiEvent::Confirm)==InstallerAction::CustomControls);
    assert(model.handle(UiEvent::Back)==InstallerAction::None);
    model.settings.camera_gain=0.3f;
    assert(model.item_value(model.selected)=="Custom");
    assert(model.handle(UiEvent::Left)==InstallerAction::None && model.item_count()==4);
    model.selected=0;assert(model.handle(UiEvent::Confirm)==InstallerAction::DefaultControls);
    model.page=InstallerPage::Graphics;model.selected=unsigned(GraphicsId::Profile);
    assert(model.handle(UiEvent::Right)==InstallerAction::CustomGraphics);
    model.settings.shadow_mode=ShadowMode::Off;
    assert(model.handle(UiEvent::Confirm)==InstallerAction::DefaultGraphics);
    model.profile_note="Custom profile is empty";
    assert(model.handle(UiEvent::Up)==InstallerAction::None);
    assert(model.profile_note.empty());
    for(auto language:{Language::English,Language::Russian}){
        model.settings=re6_profile();model.settings.language=language;model.page=InstallerPage::ControlGroup;
        model.control_group=ControlGroup::Camera;model.selected=0;
        assert(std::string(model.selected_help())==control_help_text(control_spec(ControlId::Camera),model.settings));
        model.control_group=ControlGroup::Aiming;model.selected=2;
        assert(std::string(model.selected_help())==control_help_text(control_spec(ControlId::RightAimX),model.settings));
    }
    std::puts("profile UI: independent domain selection, restore, errors and root navigation PASS");
}
