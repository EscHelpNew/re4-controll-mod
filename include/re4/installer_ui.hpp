#pragma once
#include "control_menu.hpp"
#include "update.hpp"
#include "game_target.hpp"
#include "re6/settings.hpp"
#include <cstddef>
#include <string>

namespace re4 {
enum class InstallerPage { Home, Controls, Diagnostics, Updates, Mapping, MappingTargets, Graphics, ControlGroup, GyroTest, Game, CharacterAppearance };
enum class UiEvent { Up,Down,Left,Right,Confirm,Back,Exit,QuickInstall,QuickOverlay,QuickDisable,Calibrate,ResetPreview,PreviousGame,NextGame,OpenGyroTest };
enum class InstallerAction { None,InstallGame,InstallOverlay,DisableGame,SaveSettings,Exit,Calibrate,ResetPreview,CheckUpdates,InstallUpdate,CancelUpdate,
    DefaultControls,CustomControls,DefaultGraphics,CustomGraphics,RE6Controls,SaveRe6Graphics,QualityGraphics,SaveGyroTest };
enum class InstallerStatus { Ready,Saved,Installed,OverlayInstalled,Disabled,GameRunning,Conflict,Failed,Calibrating,Calibrated,NoSensor };
struct DiagnosticSnapshot {
    bool connected=false,network=false;
    unsigned style=0,sensors=0,sensor_result=0,samples=0,calibration_remaining=0;
    float raw[4]{},processed[4]{},gyro_rate[3]{},gyro_preview[2]{};
};
struct HomeItem {Bilingual label,help;};
const std::array<HomeItem,10>& installer_home_items();
class InstallerModel {
    struct GameHomeState {
        unsigned selected=0;
        InstallerStatus status=InstallerStatus::Ready;
        std::string detail;
    };
    GameHomeState game_home_[2];
    Settings gyro_test_draft_;
    InstallerPage gyro_test_origin_=InstallerPage::Diagnostics;
    unsigned gyro_test_origin_selected_=0;
    void open_gyro_test();
    void close_gyro_test();
public:
    GameTarget game=GameTarget::RE4;
    Settings settings=default_controls_profile();
    re6::Settings re6_settings;
    InstallerPage page=InstallerPage::Home;
    unsigned selected=0,home_selected=0;
    unsigned controls_selected=0,mapping_source=0;
    re4::ControlGroup control_group=re4::ControlGroup::Sticks;
    InstallerPage graphics_origin=InstallerPage::Home;
    InstallerStatus status=InstallerStatus::Ready;
    DiagnosticSnapshot diagnostic;
    UpdateSnapshot update;
    std::string detail;
    std::string profile_note;
    unsigned item_count()const;
    ControlId selected_control()const;
    UiEvent x_shortcut()const;
    UiEvent y_shortcut()const;
    InstallerAction handle(UiEvent);
    const char* status_text()const;
    const char* page_title()const;
    const char* selected_help()const;
    std::string item_label(unsigned index)const;
    std::string item_value(unsigned index)const;
    void installation_result(const char* result);
    bool allow_installation(bool process_identified,bool re4_running);
    bool save_re6_graphics(const char* path=re6::kSettingsPath);
    const Settings& preview_settings()const;
    bool save_gyro_test(const char* path=kSettingsPath);
    // Calibration changes the draft while testing; the caller saves ordinary diagnostics.
    void calibrated(float bias_x,float bias_y);
};

class InstallerRenderer {
    struct Impl;
    Impl* impl_=nullptr;
public:
    InstallerRenderer();
    ~InstallerRenderer();
    InstallerRenderer(const InstallerRenderer&)=delete;
    InstallerRenderer& operator=(const InstallerRenderer&)=delete;
    bool initialize(const void* font,std::size_t font_size,const char* logo_bmp,bool hidden=false,const char* re6_logo_bmp=nullptr);
    void draw(const InstallerModel&);
    bool save_preview(const char* bmp_path);
    void close();
    const char* error()const;
};
}
