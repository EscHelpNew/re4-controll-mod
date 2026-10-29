#include "re4/installer_ui.hpp"
#include <cassert>
#include <cstring>
#include <cstdio>
int main(){
    using namespace re4;
    InstallerModel m;
    assert(m.settings.language==Language::English && m.page==InstallerPage::Home && m.item_count()==8);
    assert(m.handle(UiEvent::Confirm)==InstallerAction::InstallGame);
    m.selected=3;assert(m.handle(UiEvent::Confirm)==InstallerAction::None && m.page==InstallerPage::Controls);
    assert(m.item_count()==unsigned(ControlId::Count) && m.selected==0);
    assert(m.handle(UiEvent::QuickInstall)==InstallerAction::None); // Never install from a settings page.
    m.selected=unsigned(ControlId::Reset);m.settings.turn_gain=0.4f;m.settings.gyro_bias_x=1.2f;
    assert(m.handle(UiEvent::Left)==InstallerAction::None);
    assert(m.handle(UiEvent::Right)==InstallerAction::None);
    assert(m.settings.turn_gain==0.4f && m.settings.gyro_bias_x==1.2f);
    m.selected=0;
    assert(m.handle(UiEvent::Right)==InstallerAction::SaveSettings && m.settings.language==Language::Russian);
    assert(m.handle(UiEvent::Back)==InstallerAction::None && m.page==InstallerPage::Home && m.selected==3);
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
    m.page=InstallerPage::Home;m.selected=5;
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
