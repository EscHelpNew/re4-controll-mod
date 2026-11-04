#include "re4/installer_ui.hpp"
#include "re4/settings_file.hpp"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#undef assert
#define assert(condition) do { if(!(condition)){std::fprintf(stderr,"installer model line %d: %s\n",__LINE__,#condition);std::exit(1);} } while(false)
int main(){
    using namespace re4;
    InstallerModel m;
    // A diagnostic test edits a draft; cancelling must preserve live settings.
    {
        InstallerModel test;test.page=InstallerPage::Diagnostics;test.selected=0;
        test.diagnostic.sensors=1;
        assert(test.handle(test.x_shortcut())==InstallerAction::ResetPreview);
        assert(test.handle(test.y_shortcut())==InstallerAction::Calibrate);
        const auto original=test.settings.gyro_enabled;
        assert(test.handle(UiEvent::OpenGyroTest)==InstallerAction::None);
        assert(test.page==InstallerPage::GyroTest && test.item_count()==8);
        assert(test.handle(UiEvent::Confirm)==InstallerAction::None);
        assert(test.preview_settings().gyro_enabled!=original && test.settings.gyro_enabled==original);
        std::array<std::uint8_t,kSettingsBytes> before_reset{},after_reset{};
        assert(encode_settings(test.preview_settings(),before_reset.data(),before_reset.size()));
        test.selected=7;assert(test.selected_control()==ControlId::Count);
        assert(!test.item_label(7).empty() && test.item_value(7)=="A");
        assert(test.handle(UiEvent::Left)==InstallerAction::None);
        assert(test.handle(UiEvent::Right)==InstallerAction::None);
        assert(test.handle(test.y_shortcut())==InstallerAction::ResetPreview);
        assert(test.handle(test.x_shortcut())==InstallerAction::Calibrate);
        assert(test.page==InstallerPage::GyroTest && test.selected==7);
        assert(encode_settings(test.preview_settings(),after_reset.data(),after_reset.size()));
        assert(before_reset==after_reset && test.settings.gyro_enabled==original);
        test.handle(UiEvent::Back);
        assert(test.page==InstallerPage::Diagnostics && test.selected==0 && test.settings.gyro_enabled==original);
    }
    // Both entry points share a draft and return to the exact origin on save/cancel.
    {
        namespace fs=std::filesystem;
        const fs::path dir="build/installer-gyro-test";fs::create_directories(dir);
        const auto path=(dir/"settings.bin").string();
        InstallerModel test;test.settings.language=Language::Russian;
        test.settings.turn_gain=.4f;test.settings.left_deadzone=.17f;
        test.settings.texture_filtering=TextureFilter::Aniso4;
        assert(save_settings_file(path.c_str(),test.settings));
        std::ifstream input(path,std::ios::binary);
        const std::string before{std::istreambuf_iterator<char>(input),{}};input.close();
        test.page=InstallerPage::ControlGroup;test.control_group=ControlGroup::Gyro;
        test.selected=control_group_count(ControlGroup::Gyro);
        const auto origin=test.selected;
        assert(test.item_count()==8 && test.selected_control()==ControlId::Count);
        assert(!test.item_label(origin).empty() && std::strlen(test.selected_help())>20);
        test.handle(UiEvent::Confirm);assert(test.page==InstallerPage::GyroTest && test.item_count()==8);
        for(unsigned i=0;i<7;++i){
            test.selected=i;assert(test.selected_control()==control_group_item(ControlGroup::Gyro,i));
            assert(!test.item_label(i).empty() && !test.item_value(i).empty());
        }
        test.selected=0;test.handle(UiEvent::Confirm);
        test.selected=1;test.handle(UiEvent::Right);
        assert(std::fabs(test.preview_settings().gyro_speed-.9f)<.000001f && test.settings.gyro_speed==.8f);
        test.calibrated(.25f,-.5f);
        assert(test.preview_settings().gyro_bias_x==.25f && test.settings.gyro_bias_x==0);
        test.handle(UiEvent::NextGame);assert(test.page==InstallerPage::GyroTest && test.game==GameTarget::RE4);
        test.handle(UiEvent::PreviousGame);assert(test.page==InstallerPage::GyroTest);
        assert(test.handle(UiEvent::ResetPreview)==InstallerAction::ResetPreview);
        test.diagnostic.sensors=0;assert(test.handle(test.x_shortcut())==InstallerAction::None);
        assert(test.status==InstallerStatus::NoSensor);
        test.diagnostic.sensors=1;assert(test.handle(test.x_shortcut())==InstallerAction::Calibrate);
        test.selected=7;
        std::array<std::uint8_t,kSettingsBytes> before_reset{},after_reset{};
        assert(encode_settings(test.preview_settings(),before_reset.data(),before_reset.size()));
        assert(test.selected_control()==ControlId::Count && std::strlen(test.selected_help())>20);
        assert(test.handle(test.y_shortcut())==InstallerAction::ResetPreview);
        assert(encode_settings(test.preview_settings(),after_reset.data(),after_reset.size()));
        assert(before_reset==after_reset && test.page==InstallerPage::GyroTest);
        test.selected=test.item_count()-1;assert(test.selected_control()==ControlId::Count);
        assert(test.handle(UiEvent::Confirm)==InstallerAction::SaveGyroTest);
        const auto missing=(dir/"missing"/"settings.bin").string();
        assert(!test.save_gyro_test(missing.c_str()) && test.page==InstallerPage::GyroTest);
        assert(test.status==InstallerStatus::Failed && !test.settings.gyro_enabled);
        assert(test.preview_settings().gyro_enabled);
        std::ifstream unchanged(path,std::ios::binary);
        assert(std::string(std::istreambuf_iterator<char>(unchanged),{})==before);unchanged.close();
        test.calibrated(11,0);assert(!test.save_gyro_test(path.c_str()));
        test.calibrated(.25f,-.5f);
        assert(test.save_gyro_test(path.c_str()));
        assert(test.page==InstallerPage::ControlGroup && test.selected==origin);
        Settings loaded;assert(load_settings_file(path.c_str(),loaded));
        assert(loaded.gyro_enabled && loaded.gyro_bias_x==.25f && loaded.gyro_bias_y==-.5f);
        assert(loaded.turn_gain==.4f && loaded.left_deadzone==.17f && loaded.texture_filtering==TextureFilter::Aniso4);
        test.handle(UiEvent::Confirm);test.selected=0;test.handle(UiEvent::Confirm);
        test.calibrated(2,3);test.handle(UiEvent::Back);
        assert(test.page==InstallerPage::ControlGroup && test.selected==origin);
        assert(test.settings.gyro_enabled && test.settings.gyro_bias_x==.25f);
        test.page=InstallerPage::Diagnostics;test.selected=3;test.handle(UiEvent::OpenGyroTest);
        test.selected=0;test.handle(UiEvent::Confirm);
        assert(test.handle(UiEvent::Exit)==InstallerAction::Exit);
        assert(test.page==InstallerPage::Diagnostics && test.selected==3 && test.settings.gyro_enabled);
        assert(!test.save_gyro_test(path.c_str()));
        // Known test files only; retain the directory and any unrelated files.
        for(const auto* suffix:{"",".previous",".incoming",".controls-custom",".controls-custom.previous",".graphics-custom",".graphics-custom.previous"})
            fs::remove(path+suffix);
    }
    // L/R game selection must leave nested settings without installing.
    {
        InstallerModel navigation;
        assert(navigation.game==GameTarget::RE4);
        navigation.page=InstallerPage::ControlGroup;
        navigation.selected=2;
        navigation.home_selected=3;navigation.controls_selected=4;
        navigation.settings.turn_gain=0.4f;navigation.settings.gyro_bias_x=1.25f;
        navigation.settings.button_map[0]=255;navigation.settings.free_camera=true;
        navigation.settings.texture_filtering=TextureFilter::Aniso4;
        const auto saved_settings=navigation.settings;
        navigation.status=InstallerStatus::Installed;navigation.detail="RE4 install detail";
        assert(navigation.handle(UiEvent::NextGame)==InstallerAction::None);
        assert(navigation.game==GameTarget::RE6);
        assert(navigation.page==InstallerPage::Home && navigation.item_count()==5);
        assert(navigation.status==InstallerStatus::Ready && navigation.detail.empty());
        const char* labels[]={"Install RE6 module","Install RE6 overlay","Graphics","Language","Exit"};
        for(unsigned row=0;row<5;++row){
            navigation.selected=row;
            assert(navigation.item_label(row)==labels[row]);
            assert(std::strlen(navigation.selected_help())>20);
        }
        assert(navigation.item_label(5).empty() && navigation.item_value(5).empty());
        assert(navigation.selected_control()==ControlId::Count);
        navigation.selected=0;
        assert(navigation.handle(UiEvent::Confirm)==InstallerAction::InstallGame);
        navigation.selected=1;
        assert(navigation.handle(UiEvent::Confirm)==InstallerAction::InstallOverlay);
        navigation.selected=3;
        for(auto event:{UiEvent::QuickInstall,UiEvent::QuickOverlay,UiEvent::QuickDisable,UiEvent::Calibrate,UiEvent::ResetPreview})
            assert(navigation.handle(event)==InstallerAction::None);
        assert(navigation.handle(UiEvent::Confirm)==InstallerAction::SaveSettings);
        assert(navigation.settings.language==Language::Russian && navigation.item_label(3)=="Язык");
        assert(navigation.item_label(0)=="Установить модуль RE6" && navigation.item_label(1)=="Установить оверлей RE6");
        assert(navigation.item_label(2)=="Графика" && navigation.item_label(4)=="Выход" && navigation.item_value(3)=="Русский");
        assert(navigation.handle(UiEvent::Right)==InstallerAction::SaveSettings && navigation.settings.language==Language::English);
        assert(navigation.handle(UiEvent::Left)==InstallerAction::SaveSettings && navigation.settings.language==Language::Russian);
        navigation.selected=0;navigation.handle(UiEvent::Up);assert(navigation.selected==4);
        assert(navigation.handle(UiEvent::Confirm)==InstallerAction::Exit);
        navigation.handle(UiEvent::Down);assert(navigation.selected==0);
        assert(navigation.handle(UiEvent::Back)==InstallerAction::Exit);
        assert(navigation.handle(UiEvent::Exit)==InstallerAction::Exit);
        navigation.selected=2;navigation.status=InstallerStatus::Failed;navigation.detail="Missing RE6 module";
        assert(navigation.handle(UiEvent::PreviousGame)==InstallerAction::None);
        assert(navigation.game==GameTarget::RE4 && navigation.page==InstallerPage::Home && navigation.selected==3);
        assert(navigation.home_selected==3 && navigation.controls_selected==4);
        assert(navigation.settings.language==Language::Russian);
        assert(same_profile(navigation.settings,saved_settings,ProfileDomain::Controls));
        assert(same_profile(navigation.settings,saved_settings,ProfileDomain::Graphics));
        assert(navigation.settings.gyro_bias_x==1.25f);
        assert(navigation.status==InstallerStatus::Installed && navigation.detail=="RE4 install detail");
        assert(navigation.handle(UiEvent::PreviousGame)==InstallerAction::None);
        assert(navigation.game==GameTarget::RE6 && navigation.selected==2 && navigation.item_count()==5);
        assert(navigation.status==InstallerStatus::Failed && navigation.detail=="Missing RE6 module");
        navigation.installation_result("Installed RE4 Controls. Restart RE4.");
        assert(navigation.status==InstallerStatus::Failed);
        navigation.installation_result("Installed RE6 module. Restart RE6.");
        assert(navigation.status==InstallerStatus::Installed && std::strstr(navigation.status_text(),"RE4")==nullptr);
        navigation.installation_result("Installed RE6 overlay.");assert(navigation.status==InstallerStatus::OverlayInstalled);
        assert(!navigation.allow_installation(true,true) && std::strstr(navigation.status_text(),"Resident Evil 6"));
        // Even stale page state must never expose RE4 control mutations under RE6.
        navigation.page=InstallerPage::Diagnostics;navigation.diagnostic.sensors=1;
        assert(navigation.handle(UiEvent::Calibrate)==InstallerAction::None);
        assert(navigation.page==InstallerPage::Home && !navigation.settings.gyro_enabled);
        assert(navigation.handle(UiEvent::NextGame)==InstallerAction::None && navigation.game==GameTarget::RE4);
    }
    // RE6 graphics edit their own settings and expose only RE6's supported modes.
    {
        InstallerModel graphics;
        graphics.settings.turn_gain=0.4f;graphics.settings.gyro_bias_x=1.25f;
        graphics.settings.texture_filtering=TextureFilter::Aniso4;
        graphics.settings.anti_aliasing=AntiAliasing::HandheldSsaa;
        graphics.settings.button_map[0]=255;
        const auto re4_before=graphics.settings;
        graphics.re6_settings.language=re6::Language::Russian;
        assert(graphics.handle(UiEvent::NextGame)==InstallerAction::None);
        graphics.selected=2;
        assert(graphics.handle(UiEvent::Left)==InstallerAction::None);
        assert(graphics.handle(UiEvent::Right)==InstallerAction::None);
        assert(graphics.handle(UiEvent::Confirm)==InstallerAction::None);
        assert(graphics.page==InstallerPage::Graphics && graphics.item_count()==3 && graphics.selected==0);
        assert(graphics.item_label(0)=="Texture filtering" && graphics.item_value(0)=="Native");
        assert(graphics.item_label(1)=="Anti-aliasing" && graphics.item_value(1)=="FXAA3 (native)");
        assert(graphics.item_label(2)=="Reset graphics" && graphics.item_value(2)=="A");
        assert(graphics.item_label(3).empty() && graphics.item_value(3).empty());
        assert(graphics.selected_control()==ControlId::Count);
        assert(std::strstr(graphics.selected_help(),"restart"));
        for(auto event:{UiEvent::QuickInstall,UiEvent::QuickOverlay,UiEvent::QuickDisable,UiEvent::Calibrate,UiEvent::ResetPreview})
            assert(graphics.handle(event)==InstallerAction::None);
        const char* filters[]={"Native","AF 2x","AF 4x","AF 8x","AF 16x"};
        for(unsigned step=0;step<5;++step){
            assert(graphics.item_value(0)==filters[step]);
            assert(graphics.handle(UiEvent::Right)==InstallerAction::SaveRe6Graphics);
        }
        assert(graphics.re6_settings.texture_filtering==re6::TextureFilter::Native);
        assert(graphics.handle(UiEvent::Left)==InstallerAction::SaveRe6Graphics && graphics.item_value(0)=="AF 16x");
        assert(graphics.handle(UiEvent::Confirm)==InstallerAction::SaveRe6Graphics && graphics.item_value(0)=="Native");
        graphics.handle(UiEvent::Down);assert(graphics.selected==1);
        assert(std::strstr(graphics.selected_help(),"fixed"));
        for(auto event:{UiEvent::Confirm,UiEvent::Left,UiEvent::Right}){
            assert(graphics.item_value(1)=="FXAA3 (native)");
            assert(graphics.handle(event)==InstallerAction::None);
            assert(graphics.re6_settings.anti_aliasing==re6::AntiAliasing::Native);
        }
        graphics.selected=0;
        assert(graphics.handle(UiEvent::Right)==InstallerAction::SaveRe6Graphics && graphics.item_value(0)=="AF 2x");
        graphics.selected=2;
        assert(std::strstr(graphics.selected_help(),"language"));
        assert(graphics.handle(UiEvent::Left)==InstallerAction::None);
        assert(graphics.handle(UiEvent::Right)==InstallerAction::None);
        assert(graphics.re6_settings.anti_aliasing==re6::AntiAliasing::Native);
        assert(graphics.handle(UiEvent::Confirm)==InstallerAction::SaveRe6Graphics);
        assert(graphics.re6_settings.texture_filtering==re6::TextureFilter::Native && graphics.re6_settings.anti_aliasing==re6::AntiAliasing::Native);
        assert(graphics.handle(UiEvent::Confirm)==InstallerAction::None);
        assert(graphics.re6_settings.language==re6::Language::Russian);
        assert(graphics.settings.language==Language::English);
        graphics.settings.language=Language::Russian;
        assert(std::strcmp(graphics.page_title(),"ГРАФИКА")==0);
        const char* russian_labels[]={"Фильтрация текстур","Сглаживание","Сброс графики"};
        for(unsigned row=0;row<3;++row){
            graphics.selected=row;
            assert(graphics.item_label(row)==russian_labels[row]);
            assert(std::strlen(graphics.selected_help())>40);
        }
        assert(graphics.item_value(0)=="Штатная" && graphics.item_value(1)=="FXAA3 (штатное)");
        graphics.selected=1;
        assert(graphics.handle(UiEvent::Right)==InstallerAction::None && graphics.item_value(1)=="FXAA3 (штатное)");
        graphics.selected=0;graphics.handle(UiEvent::Up);assert(graphics.selected==2);
        graphics.handle(UiEvent::Down);assert(graphics.selected==0);
        assert(graphics.handle(UiEvent::Back)==InstallerAction::None && graphics.page==InstallerPage::Home && graphics.selected==2);
        graphics.handle(UiEvent::Confirm);graphics.selected=1;
        assert(graphics.handle(UiEvent::PreviousGame)==InstallerAction::None && graphics.game==GameTarget::RE4);
        assert(graphics.page==InstallerPage::Home && graphics.selected==0);
        assert(graphics.handle(UiEvent::NextGame)==InstallerAction::None && graphics.game==GameTarget::RE6);
        assert(graphics.page==InstallerPage::Home && graphics.selected==2);
        assert(graphics.re6_settings.anti_aliasing==re6::AntiAliasing::Native);
        assert(same_profile(graphics.settings,re4_before,ProfileDomain::Controls));
        assert(same_profile(graphics.settings,re4_before,ProfileDomain::Graphics));
        assert(graphics.settings.gyro_bias_x==1.25f && graphics.settings.button_map[0]==255);
        graphics.handle(UiEvent::Confirm);graphics.selected=999;
        assert(graphics.item_label(999).empty() && graphics.item_value(999).empty());
        assert(std::strlen(graphics.selected_help())>20);
        assert(graphics.handle(UiEvent::Confirm)==InstallerAction::None); // Invalid rows cannot mutate a valid row.
        for(auto stale:{InstallerPage::Controls,InstallerPage::ControlGroup,InstallerPage::Diagnostics,
            InstallerPage::Mapping,InstallerPage::MappingTargets,InstallerPage::Updates}){
            graphics.page=stale;graphics.selected=0;
            assert(graphics.item_count()==0 && graphics.item_label(0).empty() && graphics.item_value(0).empty());
            assert(std::strlen(graphics.selected_help())==0);
            assert(std::strcmp(graphics.page_title(),"УСТАНОВКА МОДА")==0);
            assert(graphics.handle(UiEvent::Confirm)==InstallerAction::None && graphics.page==InstallerPage::Home);
            assert(same_profile(graphics.settings,re4_before,ProfileDomain::Controls));
            assert(same_profile(graphics.settings,re4_before,ProfileDomain::Graphics));
        }
    }
    // The action backend persists RE6 separately, including save failures and reload.
    {
        namespace fs=std::filesystem;
        const char* root="build/installer-re6-graphics-test";
        const char* re4_path="build/installer-re6-graphics-test/re4.bin";
        const char* re6_path="build/installer-re6-graphics-test/re6.bin";
        const char* blocked="build/installer-re6-graphics-test/re6.bin.incoming";
        fs::create_directories(root);
        for(const auto* path:{re4_path,re6_path,blocked,"build/installer-re6-graphics-test/re6.bin.previous"})fs::remove(path);
        InstallerModel disk;
        disk.settings.turn_gain=0.3f;disk.settings.texture_filtering=TextureFilter::Aniso16;
        assert(re4::save_settings_file(re4_path,disk.settings));
        std::ifstream before_file(re4_path,std::ios::binary);
        const std::string re4_bytes((std::istreambuf_iterator<char>(before_file)),{});before_file.close();
        disk.game=GameTarget::RE6;disk.page=InstallerPage::Graphics;
        disk.re6_settings.language=re6::Language::Russian;
        disk.re6_settings.texture_filtering=re6::TextureFilter::Af8x;
        disk.re6_settings.anti_aliasing=re6::AntiAliasing::Fxaa3;
        assert(disk.save_re6_graphics(re6_path) && disk.status==InstallerStatus::Saved);
        InstallerModel loaded;
        assert(re6::load_settings_file(re6_path,loaded.re6_settings));
        assert(loaded.re6_settings.texture_filtering==re6::TextureFilter::Af8x);
        assert(loaded.re6_settings.anti_aliasing==re6::AntiAliasing::Native && loaded.re6_settings.language==re6::Language::Russian);
        disk.re6_settings.texture_filtering=re6::TextureFilter::Af16x;
        assert(fs::create_directory(blocked));
        assert(!disk.save_re6_graphics(re6_path) && disk.status==InstallerStatus::Failed);
        assert(re6::load_settings_file(re6_path,loaded.re6_settings));
        assert(loaded.re6_settings.texture_filtering==re6::TextureFilter::Af8x && loaded.re6_settings.anti_aliasing==re6::AntiAliasing::Native);
        std::ifstream after_file(re4_path,std::ios::binary);
        const std::string after((std::istreambuf_iterator<char>(after_file)),{});after_file.close();
        assert(after==re4_bytes && !fs::exists(std::string(re4_path)+".previous"));
        for(const auto* path:{re4_path,re6_path,blocked,"build/installer-re6-graphics-test/re6.bin.previous"})fs::remove(path);
        fs::remove(root);
    }
    assert(m.settings.language==Language::English && m.page==InstallerPage::Home && m.item_count()==9);
    for(unsigned i=0;i<9;++i)assert(!m.item_label(i).empty());
    assert(m.item_label(5)=="Graphics" && m.item_label(6)=="Updates" && m.item_label(7)=="Language" && m.item_label(8)=="Exit");
    assert(m.handle(UiEvent::Confirm)==InstallerAction::InstallGame);
    m.selected=3;assert(m.handle(UiEvent::Confirm)==InstallerAction::None && m.page==InstallerPage::Controls);
    assert(m.item_count()==8 && m.selected==0); // Controls opens categories, never the complete catalogue.
    assert(m.selected_control()==ControlId::Profile);
    const auto pristine=m.settings;
    for(unsigned group=0;group<7;++group){
        m.selected=group+1;
        for(auto language:{Language::English,Language::Russian}){
            m.settings.language=language;
            assert(!m.item_label(group+1).empty() && m.item_value(group+1).empty());
            assert(std::strlen(m.selected_help())>20);
            assert(m.item_label(group+1)!=control_spec(ControlId::Language).label.get(language));
        }
        assert(m.handle(UiEvent::Left)==InstallerAction::None);
        assert(m.handle(UiEvent::Right)==InstallerAction::None);
        assert(m.selected_control()==ControlId::Count);
        m.settings.language=Language::English;
        assert(m.handle(UiEvent::Confirm)==InstallerAction::None);
        assert(m.control_group==static_cast<ControlGroup>(group) && m.selected==0);
        if(group==unsigned(ControlGroup::Mapping)){
            assert(m.page==InstallerPage::Mapping && m.item_count()==22);
            assert(m.selected_control()==ControlId::Count);
        }else{
            assert(m.page==InstallerPage::ControlGroup);
            assert(m.item_count()==control_group_count(m.control_group)+(m.control_group==ControlGroup::Gyro?1:0));
            assert(m.selected_control()==control_group_item(m.control_group,0));
            for(unsigned row=0;row<control_group_count(m.control_group);++row){
                m.selected=row;
                assert(m.selected_control()==control_group_item(m.control_group,row));
                for(auto language:{Language::English,Language::Russian}){
                    m.settings.language=language;
                    assert(m.item_label(row)==control_spec(m.selected_control()).label.get(language));
                    assert(m.item_value(row)==control_value_text(control_spec(m.selected_control()),m.settings));
                    assert(std::strlen(m.selected_help())>20);
                    assert(std::strcmp(m.page_title(),control_group_spec(m.control_group).label.get(language))==0);
                }
                assert(m.handle(UiEvent::Down)==InstallerAction::None);
            }
            m.selected=999;
            assert(m.selected_control()==ControlId::Count);
            assert(m.item_label(999).empty() && m.item_value(999).empty());
            assert(m.handle(UiEvent::Confirm)==InstallerAction::None);
        }
        assert(m.handle(UiEvent::QuickInstall)==InstallerAction::None);
        assert(m.handle(UiEvent::QuickDisable)==InstallerAction::None);
        assert(m.handle(UiEvent::QuickOverlay)==InstallerAction::None);
        assert(m.handle(UiEvent::Back)==InstallerAction::None);
        assert(m.page==InstallerPage::Controls && m.selected==group+1);
    }
    m.settings.language=Language::English;
    assert(same_profile(m.settings,pristine,ProfileDomain::Controls));
    assert(same_profile(m.settings,pristine,ProfileDomain::Graphics));
    m.selected=1+unsigned(ControlGroup::Aiming);m.handle(UiEvent::Confirm);m.selected=6;
    assert(m.selected_control()==ControlId::MoveReloading && !m.settings.move_while_reloading);
    assert(m.handle(UiEvent::Confirm)==InstallerAction::SaveSettings && m.settings.move_while_reloading);
    assert(m.handle(UiEvent::Left)==InstallerAction::SaveSettings && !m.settings.move_while_reloading);
    m.handle(UiEvent::Back);
    m.selected=1+unsigned(ControlGroup::Sticks);m.handle(UiEvent::Confirm);
    assert(m.selected_control()==ControlId::Curve);
    m.selected=2;
    assert(m.selected_control()==ControlId::Turn);
    m.settings.turn_gain=0.4f;
    assert(m.handle(UiEvent::Confirm)==InstallerAction::None && m.settings.turn_gain==0.4f);
    assert(m.handle(UiEvent::Right)==InstallerAction::SaveSettings && m.settings.turn_gain==0.5f);
    m.handle(UiEvent::Back);
    m.selected=0;
    assert(m.selected_control()==ControlId::Profile && m.item_value(0)=="Custom");
    m.profile_note="A missing Custom profile";
    assert(std::strcmp(m.selected_help(),m.profile_note.c_str())==0);
    assert(m.handle(UiEvent::Confirm)==InstallerAction::DefaultControls && m.page==InstallerPage::Controls);
    assert(m.settings.turn_gain==0.5f);
    m.settings=default_controls_profile();
    assert(m.handle(UiEvent::Right)==InstallerAction::RE6Controls);
    assert(m.handle(UiEvent::Left)==InstallerAction::CustomControls);
    m.settings=re6_profile();
    assert(m.handle(UiEvent::Confirm)==InstallerAction::CustomControls);
    m.selected=1+unsigned(ControlGroup::Profiles);m.handle(UiEvent::Confirm);
    assert(m.selected_control()==ControlId::Reset);
    m.settings.turn_gain=0.4f;m.settings.gyro_bias_x=1.2f;
    assert(m.handle(UiEvent::Left)==InstallerAction::None);
    assert(m.handle(UiEvent::Right)==InstallerAction::None);
    assert(m.settings.turn_gain==0.4f && m.settings.gyro_bias_x==1.2f);
    assert(m.handle(UiEvent::Confirm)==InstallerAction::SaveSettings && m.settings.turn_gain==1);
    m.handle(UiEvent::Back);
    m.selected=1+unsigned(ControlGroup::Mapping);m.handle(UiEvent::Confirm);
    assert(m.item_label(0)=="Enable mapping" && m.item_count()==22);
    for(auto language:{Language::English,Language::Russian}){
        m.settings.language=language;
        for(unsigned source=0;source<20;++source){
            m.selected=source+1;
            assert(m.item_label(m.selected)==button_name(source));
            assert(m.item_value(m.selected)==button_name(source));
            assert(m.handle(UiEvent::Confirm)==InstallerAction::None && m.page==InstallerPage::MappingTargets);
            for(unsigned target=0;target<21;++target){
                m.selected=target;
                assert(!m.item_label(target).empty() && std::strlen(m.selected_help())>20);
                if(target<20)assert(m.item_label(target)==button_name(target));
                assert(m.handle(UiEvent::Down)==InstallerAction::None && m.settings.button_map[source]==source);
                assert(m.handle(UiEvent::Left)==InstallerAction::None && m.settings.button_map[source]==source);
            }
            assert(m.handle(UiEvent::Back)==InstallerAction::None && m.page==InstallerPage::Mapping && m.selected==source+1);
        }
    }
    m.settings.language=Language::English;
    for(unsigned source=0;source<20;++source){
        m.selected=source+1;
        assert(m.item_label(m.selected)==button_name(source));
        assert(m.item_value(m.selected)==button_name(source));
        assert(m.handle(UiEvent::Confirm)==InstallerAction::None && m.page==InstallerPage::MappingTargets);
        assert(m.item_count()==21 && m.selected==source && m.item_label(20)=="Unbound");
        assert(m.handle(UiEvent::Down)==InstallerAction::None && m.settings.button_map[source]==source);
        assert(m.handle(UiEvent::Back)==InstallerAction::None && m.page==InstallerPage::Mapping && m.selected==source+1);
        m.handle(UiEvent::Confirm);m.selected=20;
        assert(m.handle(UiEvent::Confirm)==InstallerAction::SaveSettings && m.page==InstallerPage::Mapping);
        assert(m.settings.button_map[source]==255 && m.item_value(source+1)=="Unbound");
    }
    m.settings.gyro_speed=0.3f;m.settings.free_camera=true;m.settings.remap_enabled=false;m.selected=21;
    assert(m.handle(UiEvent::Left)==InstallerAction::None && !m.settings.remap_enabled);
    assert(m.handle(UiEvent::Right)==InstallerAction::None && m.settings.button_map[0]==255);
    assert(m.handle(UiEvent::Confirm)==InstallerAction::SaveSettings && m.settings.remap_enabled);
    assert(m.settings.button_map==identity_button_map() && m.settings.gyro_speed==0.3f && m.settings.free_camera);
    assert(m.handle(UiEvent::Confirm)==InstallerAction::None);
    m.settings.language=Language::Russian;
    assert(m.item_label(0)=="Включить назначения" && m.item_value(0)=="Вкл");
    m.selected=1;m.handle(UiEvent::Confirm);assert(m.item_label(20)=="Не назначено");
    m.handle(UiEvent::Back);m.handle(UiEvent::Back);
    assert(m.page==InstallerPage::Controls && m.selected==1+unsigned(ControlGroup::Mapping));
    assert(m.selected_control()==ControlId::Count);
    m.settings={};
    assert(m.handle(UiEvent::QuickInstall)==InstallerAction::None);
    assert(m.handle(UiEvent::Back)==InstallerAction::None && m.page==InstallerPage::Home && m.selected==3);
    m.selected=5;const auto unchanged=m.settings;
    assert(m.handle(UiEvent::Confirm)==InstallerAction::None && m.page==InstallerPage::Graphics && m.item_count()==6);
    assert(m.settings.camera_gain==unchanged.camera_gain && m.settings.texture_filtering==TextureFilter::Native);
    assert(m.handle(UiEvent::Down)==InstallerAction::None);
    assert(m.handle(UiEvent::Back)==InstallerAction::None && m.page==InstallerPage::Home && m.selected==5);
    m.selected=5;
    assert(m.handle(UiEvent::Confirm)==InstallerAction::None && m.page==InstallerPage::Graphics);
    assert(m.handle(UiEvent::Right)==InstallerAction::SaveSettings && m.settings.texture_filtering==TextureFilter::Bilinear);
    m.selected=2;assert(m.handle(UiEvent::Confirm)==InstallerAction::None);
    assert(m.handle(UiEvent::Right)==InstallerAction::SaveSettings && m.settings.draw_distance_scale==1.25f);
    m.selected=unsigned(GraphicsId::Profile);
    assert(m.handle(UiEvent::Confirm)==InstallerAction::DefaultGraphics);
    m.settings.turn_gain=0.3f;m.settings.gyro_bias_y=0.7f;
    m.selected=unsigned(GraphicsId::Reset);m.settings.free_camera=true;m.settings.button_map[0]=255;
    assert(m.handle(UiEvent::Left)==InstallerAction::None && m.settings.texture_filtering==TextureFilter::Bilinear);
    assert(m.handle(UiEvent::Confirm)==InstallerAction::SaveSettings && m.settings.texture_filtering==TextureFilter::Native);
    assert(m.settings.free_camera && m.settings.button_map[0]==255 && m.settings.draw_distance_scale==1);
    assert(m.settings.turn_gain==0.3f && m.settings.gyro_bias_y==0.7f);
    m.selected=unsigned(GraphicsId::Profile);
    assert(m.handle(UiEvent::Right)==InstallerAction::QualityGraphics);
    m.selected=unsigned(GraphicsId::Reset);
    assert(m.handle(UiEvent::Confirm)==InstallerAction::None);
    m.settings.language=Language::Russian;assert(m.item_label(0)=="Фильтрация текстур");
    assert(m.handle(UiEvent::Back)==InstallerAction::None && m.page==InstallerPage::Home && m.selected==5);
    m.selected=7;assert(m.handle(UiEvent::Left)==InstallerAction::SaveSettings && m.settings.language==Language::English);
    assert(m.handle(UiEvent::Confirm)==InstallerAction::SaveSettings && m.settings.language==Language::Russian);
    assert(m.handle(UiEvent::Right)==InstallerAction::SaveSettings && m.settings.language==Language::English);
    m.selected=8;assert(m.handle(UiEvent::Confirm)==InstallerAction::Exit);
        m.selected=4;m.handle(UiEvent::Confirm);assert(m.page==InstallerPage::Diagnostics);
    assert(m.handle(UiEvent::QuickDisable)==InstallerAction::None);
    assert(m.handle(UiEvent::Calibrate)==InstallerAction::None);
    m.diagnostic.sensors=1;assert(m.handle(UiEvent::Calibrate)==InstallerAction::Calibrate);
    assert(m.handle(UiEvent::Confirm)==InstallerAction::SaveSettings && m.settings.gyro_enabled);
    m.handle(UiEvent::Back);assert(m.page==InstallerPage::Home);
    assert(m.handle(UiEvent::QuickOverlay)==InstallerAction::InstallOverlay);
    assert(m.handle(UiEvent::Exit)==InstallerAction::Exit);
    m.installation_result("Installed RE4 overlay. Open the existing Uberhand menu.");
    assert(m.status==InstallerStatus::OverlayInstalled);
    m.installation_result("Installed RE4 Controls. Restart RE4.");
    assert(m.status==InstallerStatus::Installed);
    m.installation_result("Conflict: existing files belong to another mod");
    assert(m.status==InstallerStatus::Conflict);
    m.installation_result("Files installed; receipt write failed");
    assert(m.status==InstallerStatus::Failed); // Partial publication must never report success.
    assert(!m.allow_installation(false,false) && m.status==InstallerStatus::Failed);
    assert(!m.allow_installation(true,true) && m.status==InstallerStatus::GameRunning);
    assert(m.allow_installation(true,false));
    m.page=InstallerPage::Home;m.selected=6;
    m.handle(UiEvent::Confirm);assert(m.page==InstallerPage::Updates && m.item_count()==2);
    assert(m.handle(UiEvent::QuickInstall)==InstallerAction::None);
    assert(m.handle(UiEvent::Confirm)==InstallerAction::CheckUpdates);
    m.update.stage=UpdateStage::Checking;
    assert(m.handle(UiEvent::Confirm)==InstallerAction::None);
    m.update.stage=UpdateStage::Available;m.selected=1;
    assert(m.handle(UiEvent::Confirm)==InstallerAction::InstallUpdate);
    m.update.stage=UpdateStage::Downloading;
    assert(m.handle(UiEvent::Confirm)==InstallerAction::None);
    assert(m.handle(UiEvent::Back)==InstallerAction::CancelUpdate && m.page==InstallerPage::Home);
    m.page=InstallerPage::ControlGroup;m.control_group=static_cast<ControlGroup>(999);m.selected=0;
    assert(m.item_count()==0 && m.selected_control()==ControlId::Count);
    assert(m.item_label(0).empty() && m.item_value(0).empty());
    assert(m.handle(UiEvent::Confirm)==InstallerAction::None);
    assert(m.handle(UiEvent::Left)==InstallerAction::None);
    assert(m.handle(UiEvent::Right)==InstallerAction::None);
    m.page=InstallerPage::ControlGroup;m.control_group=ControlGroup::Gyro;m.controls_selected=0;
    assert(m.handle(UiEvent::Back)==InstallerAction::None && m.page==InstallerPage::Controls);
    assert(m.selected==1+unsigned(ControlGroup::Gyro)); // The opened group owns the return location.
    std::puts("installer navigation: grouped controls, page-safe actions, language, mapping, profiles and calibration PASS");
}
