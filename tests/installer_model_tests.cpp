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
    assert(m.settings.language==Language::English && m.page==InstallerPage::Home && m.item_count()==9);
    for(unsigned i=0;i<9;++i)assert(!m.item_label(i).empty());
    assert(m.item_label(5)=="Graphics" && m.item_label(6)=="Updates" && m.item_label(7)=="Language" && m.item_label(8)=="Exit");
    assert(m.handle(UiEvent::Confirm)==InstallerAction::InstallGame);
    m.selected=3;assert(m.handle(UiEvent::Confirm)==InstallerAction::None && m.page==InstallerPage::Controls);
    assert(m.item_count()==7 && m.selected==0); // Controls opens categories, never the complete catalogue.
    assert(m.selected_control()==ControlId::Count);
    const auto pristine=m.settings;
    for(unsigned group=0;group<7;++group){
        m.selected=group;
        for(auto language:{Language::English,Language::Russian}){
            m.settings.language=language;
            assert(!m.item_label(group).empty() && m.item_value(group).empty());
            assert(std::strlen(m.selected_help())>20);
            assert(m.item_label(group)!=control_spec(ControlId::Language).label.get(language));
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
        assert(m.page==InstallerPage::Controls && m.selected==group);
    }
    m.settings.language=Language::English;
    assert(same_profile(m.settings,pristine,ProfileDomain::Controls));
    assert(same_profile(m.settings,pristine,ProfileDomain::Graphics));
    m.selected=unsigned(ControlGroup::Sticks);m.handle(UiEvent::Confirm);
    assert(m.selected_control()==ControlId::Curve);
    m.selected=2;
    assert(m.selected_control()==ControlId::Turn);
    m.settings.turn_gain=0.4f;
    assert(m.handle(UiEvent::Confirm)==InstallerAction::None && m.settings.turn_gain==0.4f);
    assert(m.handle(UiEvent::Right)==InstallerAction::SaveSettings && m.settings.turn_gain==0.5f);
    m.handle(UiEvent::Back);
    m.selected=unsigned(ControlGroup::Profiles);m.handle(UiEvent::Confirm);
    assert(m.selected_control()==ControlId::Profile && m.item_value(0)=="Custom");
    m.profile_note="A missing Custom profile";
    assert(std::strcmp(m.selected_help(),m.profile_note.c_str())==0);
    assert(m.handle(UiEvent::Confirm)==InstallerAction::DefaultControls);
    assert(m.settings.turn_gain==0.5f); // Application performs profile persistence.
    m.settings={};
    assert(m.handle(UiEvent::Right)==InstallerAction::CustomControls);
    m.settings.shadow_mode=ShadowMode::Off;
    assert(m.item_value(0)=="Default" && m.handle(UiEvent::Left)==InstallerAction::CustomControls);
    m.selected=1;m.profile_note="Should only be shown for a profile";
    assert(m.selected_control()==ControlId::Reset && std::strcmp(m.selected_help(),m.profile_note.c_str())!=0);
    m.settings.turn_gain=0.4f;m.settings.gyro_bias_x=1.2f;
    assert(m.handle(UiEvent::Left)==InstallerAction::None);
    assert(m.handle(UiEvent::Right)==InstallerAction::None);
    assert(m.settings.turn_gain==0.4f && m.settings.gyro_bias_x==1.2f);
    assert(m.handle(UiEvent::Confirm)==InstallerAction::SaveSettings && m.settings.turn_gain==1);
    m.handle(UiEvent::Back);
    m.selected=unsigned(ControlGroup::Mapping);m.handle(UiEvent::Confirm);
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
    assert(m.page==InstallerPage::Controls && m.selected==unsigned(ControlGroup::Mapping));
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
    assert(m.selected==unsigned(ControlGroup::Gyro)); // The opened group owns the return location.
    std::puts("installer navigation: grouped controls, page-safe actions, language, mapping, profiles and calibration PASS");
}
