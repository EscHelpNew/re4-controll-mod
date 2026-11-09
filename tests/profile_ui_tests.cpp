#define main re4_overlay_entry
#include "../source/overlay/main.cpp"
#undef main
#include "re4/installer_ui.hpp"
#include <cassert>
namespace re6 {bool save_settings_file(const char*,const Settings& settings){return valid_settings(settings);}}
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
namespace overlay_debug {void start(const char*){}void share_link(const GameLink*){}void stop(){}void set_mode(re4::LaunchMode,const char*){}re4::LaunchMode mode(){return re4::LaunchMode::Normal;}State state(){return State::Off;}std::uint16_t port(){return 0;}}
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
bool GameLink::refresh_appearance(){appearance={};return false;}
bool GameLink::refresh_popup(){popup_diagnostic=0;return false;}
bool GameLink::refresh_money(){money={};return false;}
bool GameLink::request_money(){return false;}
int main(){
    using namespace re4;
    // Saving through the actual Game toggle survives a fresh overlay lifecycle.
    {
        settings=default_controls_profile();note.clear();MainGui home;
        auto* frame=static_cast<tsl::elm::OverlayFrame*>(home.createUI());
        assert(row(frame,"Game")->click(HidNpadButton_A));
        auto* game=static_cast<tsl::elm::OverlayFrame*>(tsl::navigation.back()->root);
        auto* health=dynamic_cast<tsl::elm::ToggleListItem*>(row(game,"Enemy health bars"));assert(health);
        health->changed(true);assert(files.at(overlay_storage::settings_path).enemy_health_bars);
        assert(row(game,"Health numbers")->click(HidNpadButton_A));
        assert(files.at(overlay_storage::settings_path).enemy_health_numbers);
        for(const auto label:{"Grenade explosion spot","Skip pickup confirmation","Skip startup logos","Remove aiming sway","D-pad weapon switching"})
            assert(row(game,label)->click(HidNpadButton_A));
        assert(files.at(overlay_storage::settings_path).grenade_landing_spot && files.at(overlay_storage::settings_path).skip_pickup_confirmation && files.at(overlay_storage::settings_path).skip_startup_logos && files.at(overlay_storage::settings_path).disable_aim_sway && files.at(overlay_storage::settings_path).dpad_weapon_switching);
        assert(row(game,"Character appearance")->click(HidNpadButton_A));
        auto* appearance=static_cast<tsl::elm::OverlayFrame*>(tsl::navigation.back()->root);
        assert(row(appearance,"Ashley")->click(HidNpadButton_A));
        assert(settings.character_appearance==CharacterAppearance::Ashley&&files.at(overlay_storage::settings_path).character_appearance==CharacterAppearance::Ashley);
        tsl::goBack();
        health->changed(false);
        assert(!files.at(overlay_storage::settings_path).enemy_health_bars && files.at(overlay_storage::settings_path).enemy_health_numbers);
        tsl::goBack();settings={};
        ControlsOverlay overlay;tsl::Overlay& lifecycle=overlay;
        lifecycle.initServices();assert(!settings.enemy_health_bars && settings.enemy_health_numbers && settings.grenade_landing_spot && settings.skip_pickup_confirmation && settings.skip_startup_logos && settings.disable_aim_sway && settings.dpad_weapon_switching);lifecycle.exitServices();
        assert(settings.character_appearance==CharacterAppearance::Ashley);
        assert(row(frame,"Game")->click(HidNpadButton_A));
        auto* restored=static_cast<tsl::elm::OverlayFrame*>(tsl::navigation.back()->root);
        assert(row(restored,"Health numbers")->value=="Unavailable");
        dynamic_cast<tsl::elm::ToggleListItem*>(row(restored,"Enemy health bars"))->changed(true);
        assert(row(restored,"Health numbers")->value=="On");
        tsl::goBack();
        settings.language=Language::Russian;home.update();assert(row(frame,"Игра"));
        delete frame;files.clear();sends=0;settings=default_controls_profile();note.clear();
    }
    // Profile application must preserve all independent Game preferences.
    for(bool bars:{false,true}){
        settings=default_controls_profile();settings.enemy_health_bars=bars;settings.enemy_health_numbers=true;settings.grenade_landing_spot=true;settings.skip_pickup_confirmation=true;settings.skip_startup_logos=true;settings.disable_aim_sway=true;settings.dpad_weapon_switching=true;
        settings.character_appearance=CharacterAppearance::Merchant;
        ControlsGui controls;auto* controls_frame=static_cast<tsl::elm::OverlayFrame*>(controls.createUI());
        assert(row(controls_frame,"Controls profile")->click(HidNpadButton_A));
        assert(settings.character_appearance==CharacterAppearance::Merchant);
        assert(settings.enemy_health_bars==bars && settings.enemy_health_numbers && settings.grenade_landing_spot && settings.skip_pickup_confirmation && settings.skip_startup_logos && settings.disable_aim_sway && settings.dpad_weapon_switching);
        GraphicsGui graphics;auto* graphics_frame=static_cast<tsl::elm::OverlayFrame*>(graphics.createUI());
        assert(row(graphics_frame,"Graphics profile")->click(HidNpadButton_Right));
        assert(settings.character_appearance==CharacterAppearance::Merchant&&files.at(overlay_storage::settings_path).character_appearance==CharacterAppearance::Merchant);
        assert(settings.enemy_health_bars==bars && settings.enemy_health_numbers && settings.grenade_landing_spot && settings.skip_pickup_confirmation && settings.skip_startup_logos && settings.disable_aim_sway && settings.dpad_weapon_switching);
        assert(files.at(overlay_storage::settings_path).enemy_health_bars==bars && files.at(overlay_storage::settings_path).enemy_health_numbers && files.at(overlay_storage::settings_path).grenade_landing_spot && files.at(overlay_storage::settings_path).skip_pickup_confirmation && files.at(overlay_storage::settings_path).skip_startup_logos && files.at(overlay_storage::settings_path).disable_aim_sway && files.at(overlay_storage::settings_path).dpad_weapon_switching);
        delete controls_frame;delete graphics_frame;
    }
    files.clear();sends=0;settings=default_controls_profile();note.clear();
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
    settings=default_controls_profile();note.clear();files.clear();sends=0;
    GraphicsGui graphics;
    auto* graphics_root=static_cast<tsl::elm::OverlayFrame*>(graphics.createUI());
    auto* graphics_profile=row(graphics_root,"Graphics profile");
    assert(graphics_profile->value=="Default");
    assert(graphics_profile->click(HidNpadButton_Right));
    assert(graphics_profile->value=="Quality" && settings.texture_filtering==TextureFilter::Aniso16);
    assert(settings.anti_aliasing==AntiAliasing::HandheldSsaa && settings.draw_distance_scale==1.75f);
    assert(settings.shadow_mode==ShadowMode::Native && !settings.analog_enabled && !settings.gyro_enabled);
    assert(!files.count(std::string(overlay_storage::settings_path)+".graphics-custom"));
    assert(graphics_profile->click(HidNpadButton_Left) && graphics_profile->value=="Default");
    const auto old_sends=sends;
    assert(graphics_profile->click(HidNpadButton_Left));
    assert(graphics_profile->value=="Default" && sends==old_sends && note.find("empty")!=std::string::npos);
    settings.draw_distance_scale=1.5f;settings.language=Language::Russian;
    assert(save_profiled_settings(overlay_storage::settings_path,settings));graphics.update();
    assert(graphics_profile->value=="Пользовательский");
    assert(graphics_profile->click(HidNpadButton_Left) && graphics_profile->value=="Качество");
    assert(graphics_profile->click(HidNpadButton_Right) && graphics_profile->value=="Пользовательский");
    assert(settings.draw_distance_scale==1.5f && settings.texture_filtering==TextureFilter::Native);
    writable=false;const auto unsent=sends;
    assert(graphics_profile->click(HidNpadButton_Right));
    assert(graphics_profile->value=="Пользовательский" && sends==unsent && settings.draw_distance_scale==1.5f);
    writable=true;
    // Editing the final field to match Quality must still save the last edit as Custom.
    settings=quality_graphics_profile();settings.texture_filtering=TextureFilter::Aniso8;
    graphics.update();
    assert(row(graphics_root,"Texture filtering")->click(HidNpadButton_Right));
    assert(settings.texture_filtering==TextureFilter::Aniso16);
    assert(graphics_profile->click(HidNpadButton_Left)); // Quality -> Default.
    assert(graphics_profile->click(HidNpadButton_Left)); // Restore that last edited Custom.
    assert(settings.texture_filtering==TextureFilter::Aniso16 && settings.anti_aliasing==AntiAliasing::HandheldSsaa);
    assert(settings.draw_distance_scale==1.75f);
    delete graphics_root;
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
    assert(model.handle(UiEvent::Right)==InstallerAction::QualityGraphics);
    model.settings.shadow_mode=ShadowMode::Off;
    assert(model.handle(UiEvent::Confirm)==InstallerAction::DefaultGraphics);
    model.settings=quality_graphics_profile();
    assert(model.item_value(unsigned(GraphicsId::Profile))=="Quality");
    assert(model.handle(UiEvent::Right)==InstallerAction::CustomGraphics);
    assert(model.handle(UiEvent::Left)==InstallerAction::DefaultGraphics);
    model.settings={};
    assert(model.handle(UiEvent::Right)==InstallerAction::QualityGraphics);
    std::puts("profile UI: three inline profiles, missing Custom, save failures, bilingual navigation and domain scope PASS");
}
