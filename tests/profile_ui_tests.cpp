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
    auto* controls=static_cast<tsl::elm::OverlayFrame*>(tsl::navigation.back()->root);
    for(auto* e:static_cast<tsl::elm::List*>(controls->content())->children)
        if(auto* item=dynamic_cast<tsl::elm::ListItem*>(e))assert(item->label!="Graphics");
    profile=row(controls,"Controls profile");
    assert(profile->value=="Custom");
    assert(profile->click(HidNpadButton_A));
    assert(!settings.gyro_enabled && settings.texture_filtering==TextureFilter::Bilinear
        && settings.gyro_bias_x==0.3f && profile->value=="Default");
    assert(profile->click(HidNpadButton_Right) && settings.gyro_enabled);
    const auto before_failure=sends;writable=false;
    assert(profile->click(HidNpadButton_Left));
    assert(settings.gyro_enabled && sends==before_failure && note.find("save")!=std::string::npos);
    auto* controls_gui=tsl::navigation.back().get();
    const auto profile_text_sets=profile->text_sets;
    for(unsigned frame=0;frame<31;++frame){
        controls_gui->update();assert(row(controls,note)->label==note);
        assert(profile->text_sets==profile_text_sets); // Tesla resets marquee text on setText.
    }
    writable=true;
    auto* first_person_toggle=dynamic_cast<tsl::elm::ToggleListItem*>(row(controls,"First-person view"));
    auto* aim_move_toggle=dynamic_cast<tsl::elm::ToggleListItem*>(row(controls,"Move while aiming"));
    auto* knife_move_toggle=dynamic_cast<tsl::elm::ToggleListItem*>(row(controls,"Move with knife"));
    assert(first_person_toggle && aim_move_toggle && knife_move_toggle);
    first_person_toggle->changed(true);aim_move_toggle->changed(true);knife_move_toggle->changed(true);
    assert(row(controls,"Reticle")->click(HidNpadButton_Right));
    assert(settings.first_person && settings.move_while_aiming && settings.move_while_knife && settings.reticle==ReticleStyle::Dot);
    assert(files.at(overlay_storage::settings_path).reticle==ReticleStyle::Dot);
    assert(profile->click(HidNpadButton_A));
    assert(!settings.first_person && !settings.move_while_aiming && !settings.move_while_knife && settings.reticle==ReticleStyle::Off);
    assert(profile->click(HidNpadButton_A));
    assert(settings.first_person && settings.move_while_aiming && settings.move_while_knife && settings.reticle==ReticleStyle::Dot);
    tsl::goBack();delete root;

    InstallerModel model;model.page=InstallerPage::Controls;
    model.selected=unsigned(ControlId::Profile);
    assert(model.item_value(model.selected)=="Default");
    assert(model.handle(UiEvent::Confirm)==InstallerAction::CustomControls);
    model.settings.camera_gain=0.3f;
    assert(model.item_value(model.selected)=="Custom");
    assert(model.handle(UiEvent::Left)==InstallerAction::DefaultControls);
    model.page=InstallerPage::Graphics;model.selected=unsigned(GraphicsId::Profile);
    assert(model.handle(UiEvent::Right)==InstallerAction::CustomGraphics);
    model.settings.shadow_mode=ShadowMode::Off;
    assert(model.handle(UiEvent::Confirm)==InstallerAction::DefaultGraphics);
    model.profile_note="Custom profile is empty";
    assert(model.handle(UiEvent::Up)==InstallerAction::None);
    assert(model.profile_note.empty());
    std::puts("profile UI: independent domain selection, restore, errors and root navigation PASS");
}
