#include "re4/installer_ui.hpp"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#undef assert
#define assert(condition) do { if(!(condition)){std::fprintf(stderr,"installer model line %d: %s\n",__LINE__,#condition);std::exit(1);} } while(false)
int main(){
    using namespace re4;
    InstallerModel m;
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
        assert(navigation.page==InstallerPage::Home && navigation.item_count()==4);
        assert(navigation.status==InstallerStatus::Ready && navigation.detail.empty());
        const char* labels[]={"Install RE6 module","Install RE6 overlay","Language","Exit"};
        for(unsigned row=0;row<4;++row){
            navigation.selected=row;
            assert(navigation.item_label(row)==labels[row]);
            assert(std::strlen(navigation.selected_help())>20);
        }
        assert(navigation.item_label(4).empty() && navigation.item_value(4).empty());
        assert(navigation.selected_control()==ControlId::Count);
        navigation.selected=0;
        assert(navigation.handle(UiEvent::Confirm)==InstallerAction::InstallGame);
        navigation.selected=1;
        assert(navigation.handle(UiEvent::Confirm)==InstallerAction::InstallOverlay);
        navigation.selected=2;
        for(auto event:{UiEvent::QuickInstall,UiEvent::QuickOverlay,UiEvent::QuickDisable,UiEvent::Calibrate,UiEvent::ResetPreview})
            assert(navigation.handle(event)==InstallerAction::None);
        assert(navigation.handle(UiEvent::Confirm)==InstallerAction::SaveSettings);
        assert(navigation.settings.language==Language::Russian && navigation.item_label(2)=="Язык");
        assert(navigation.item_label(0)=="Установить модуль RE6" && navigation.item_label(1)=="Установить оверлей RE6");
        assert(navigation.item_label(3)=="Выход" && navigation.item_value(2)=="Русский");
        assert(navigation.handle(UiEvent::Right)==InstallerAction::SaveSettings && navigation.settings.language==Language::English);
        assert(navigation.handle(UiEvent::Left)==InstallerAction::SaveSettings && navigation.settings.language==Language::Russian);
        navigation.selected=0;navigation.handle(UiEvent::Up);assert(navigation.selected==3);
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
        assert(navigation.game==GameTarget::RE6 && navigation.selected==2 && navigation.item_count()==4);
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
            assert(m.item_count()==control_group_count(m.control_group));
            assert(m.selected_control()==control_group_item(m.control_group,0));
            for(unsigned row=0;row<m.item_count();++row){
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
    assert(m.handle(UiEvent::Right)==InstallerAction::CustomGraphics);
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
