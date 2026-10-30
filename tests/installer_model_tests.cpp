#include "re4/installer_ui.hpp"
#include <cassert>
#include <cstring>
#include <cstdio>
int main(){
    using namespace re4;
    InstallerModel m;
    assert(m.settings.language==Language::English && m.page==InstallerPage::Home && m.item_count()==9);
    for(unsigned i=0;i<9;++i)assert(!m.item_label(i).empty());
    assert(m.item_label(5)=="Graphics" && m.item_label(6)=="Updates" && m.item_label(7)=="Language" && m.item_label(8)=="Exit");
    assert(m.handle(UiEvent::Confirm)==InstallerAction::InstallGame);
    m.selected=3;assert(m.handle(UiEvent::Confirm)==InstallerAction::None && m.page==InstallerPage::Controls);
    assert(m.item_count()==unsigned(ControlId::Count) && m.selected==0);
    m.selected=unsigned(ControlId::ButtonMapping);
    assert(m.handle(UiEvent::Confirm)==InstallerAction::None);
    assert(m.item_label(0)=="Enable mapping" && m.item_count()==22);
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
    assert(m.page==InstallerPage::Controls && m.selected==unsigned(ControlId::ButtonMapping));
    m.settings={};m.selected=0;
    assert(m.handle(UiEvent::QuickInstall)==InstallerAction::None); // Never install from a settings page.
    m.selected=unsigned(ControlId::Reset);m.settings.turn_gain=0.4f;m.settings.gyro_bias_x=1.2f;
    assert(m.handle(UiEvent::Left)==InstallerAction::None);
    assert(m.handle(UiEvent::Right)==InstallerAction::None);
    assert(m.settings.turn_gain==0.4f && m.settings.gyro_bias_x==1.2f);
    m.selected=0;
    assert(m.handle(UiEvent::Right)==InstallerAction::SaveSettings && m.settings.language==Language::Russian);
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
    m.selected=unsigned(GraphicsId::Reset);m.settings.free_camera=true;m.settings.button_map[0]=255;
    assert(m.handle(UiEvent::Left)==InstallerAction::None && m.settings.texture_filtering==TextureFilter::Bilinear);
    assert(m.handle(UiEvent::Confirm)==InstallerAction::SaveSettings && m.settings.texture_filtering==TextureFilter::Native);
    assert(m.settings.free_camera && m.settings.button_map[0]==255 && m.settings.draw_distance_scale==1);
    assert(m.handle(UiEvent::Confirm)==InstallerAction::None);
    m.settings.language=Language::Russian;assert(m.item_label(0)=="Фильтрация текстур");
    assert(m.handle(UiEvent::Back)==InstallerAction::None && m.page==InstallerPage::Home && m.selected==5);
    m.selected=7;assert(m.handle(UiEvent::Left)==InstallerAction::SaveSettings && m.settings.language==Language::English);
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
    std::puts("installer navigation: page-safe actions, language, controls and calibration PASS");
}
