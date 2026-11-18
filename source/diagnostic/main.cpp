#define SDL_MAIN_HANDLED
#include <switch.h>
#include <SDL.h>
#include "re4/installer_ui.hpp"
#include "re4/calibration.hpp"
#include "re4/gyro_preview.hpp"
#include "re4/home_launcher.hpp"
#include "re4/version.hpp"
#include "re4/launch_mode.hpp"
#include "re4/startup_options.hpp"
#include "re4/startup_updates.hpp"
#include "re4/local_update_server.hpp"
#include "re4/app_journal.hpp"
#include "re4/support_report.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#include "re4/input.hpp"
#include "re4/settings.hpp"
#include "re4/settings_file.hpp"
#include "re4/install.hpp"
#include "game_link.hpp"
#include "storage.hpp"
#include "app_exit.hpp"
extern "C" {
#include "remote_reload.h"
#include "reload_main.h"
#include "reload_project.h"
#include "reload_user_channel.h"
#include "reload_core.h"
}

namespace {
constexpr float tau = 6.28318530718f;
bool persist_reload_token(const char* token) {
    if (!reload_token_valid(token)) return false;
    mkdir("sdmc:/switch", 0777);
    mkdir(NXDK_SD_ROOT, 0777);
    constexpr const char* path = NXDK_SD_ROOT "/token.txt";
    constexpr const char* incoming = NXDK_SD_ROOT "/token.txt.incoming";
    constexpr const char* backup = NXDK_SD_ROOT "/token.txt.previous";
    char existing[65]{};
    FILE* old = std::fopen(path, "rb");
    const bool had_old = old != nullptr;
    if (old) {
        const auto count = std::fread(existing, 1, sizeof(existing), old);
        std::fclose(old);
        if ((count == 64 || (count == 65 && existing[64] == '\n'))
            && !std::memcmp(existing, token, 64)) return true;
    }
    FILE* file = std::fopen(incoming, "wb");
    if (!file) return false;
    bool ok = std::fwrite(token, 1, 64, file) == 64;
    if (std::fflush(file) || fsync(fileno(file))) ok = false;
    if (std::fclose(file)) ok = false;
    if (!ok) return false;
    if (had_old) {
        struct stat st{};
        if (!stat(backup, &st)) return false; // Preserve the prior credential backup.
        if (std::rename(path, backup)) return false;
    }
    if (!std::rename(incoming, path)) return true;
    if (had_old) std::rename(backup, path);
    return false;
}
void persist_launch_token(int argc,char** argv){
    if(!argv)return;
    for(int i=1;i<argc;++i)if(argv[i]&&!std::strncmp(argv[i],"--token=",8)){
        (void)persist_reload_token(argv[i]+8);return;
    }
}
struct Sensors {
    HidSixAxisSensorHandle handles[2]{};
    bool running[2]{};
    unsigned count = 0;
    u32 style = 0;
    u32 attributes = 0;
    HidNpadIdType selected_id=HidNpadIdType_No1;
    Result result = 0;
    void stop() {
        for (unsigned i = 0; i < 2; ++i) {
            if (running[i]) hidStopSixAxisSensor(handles[i]);
            running[i] = false;
        }
        count = 0;
    }
    bool select(const PadState& pad,HidNpadIdType id) {
        const auto next = padGetStyleSet(&pad);
        const auto connected = padGetAttributes(&pad) &
            (HidNpadAttribute_IsConnected | HidNpadAttribute_IsLeftConnected | HidNpadAttribute_IsRightConnected);
        if(padIsHandheld(&pad))id=HidNpadIdType_Handheld;
        if (next == style && connected == attributes && id==selected_id) return false;
        stop(); style = next; attributes = connected; selected_id=id; result = 0;
        HidNpadStyleTag tag;
        int wanted = 1;
        if (!padIsConnected(&pad)) return true;
        if (padIsHandheld(&pad)) { id = HidNpadIdType_Handheld; tag = HidNpadStyleTag_NpadHandheld; }
        else if (style & HidNpadStyleTag_NpadFullKey) tag = HidNpadStyleTag_NpadFullKey;
        else if (style & HidNpadStyleTag_NpadJoyDual) { tag = HidNpadStyleTag_NpadJoyDual; wanted = 2; }
        else { result = MAKERESULT(Module_Libnx, LibnxError_NotFound); return true; }
        result = hidGetSixAxisSensorHandles(handles, wanted, id, tag);
        if (R_FAILED(result)) return true;
        for (int i = 0; i < wanted; ++i) {
            const auto started = hidStartSixAxisSensor(handles[i]);
            if (R_FAILED(started)) { result = started; continue; }
            running[i] = true;
            ++count;
        }
        return true;
    }
    size_t read(HidSixAxisSensorState* samples) {
        if (!count) return 0;
        // Prefer the right Joy-Con for split controllers, then the left.
        unsigned index = (style & HidNpadStyleTag_NpadJoyDual)
            && (attributes & HidNpadAttribute_IsRightConnected) && running[1] ? 1 : 0;
        if (!running[index]) index ^= 1;
        return hidGetSixAxisSensorStates(handles[index], samples, 16);
    }
};


}

int main(int argc, char** argv) {
    const auto startup=re4::startup_options(argc,argv);
    mkdir("sdmc:/switch",0777);mkdir(NXDK_SD_ROOT,0777);
    u64 journal_pid=0;svcGetProcessId(&journal_pid,CUR_PROCESS_HANDLE);
    re4::AppJournal journal(NXDK_SD_ROOT,re4::kVersion,hosversionGet(),unsigned(appletGetAppletType()),journal_pid,startup.recovery);
    journal.note("early_bridge_begin");
    if (!startup.recovery&&game_reload_early_bridge()){journal.note("early_bridge_handoff");return 0;}
    journal.note("early_bridge_done");
    journal.note("exit_policy_begin");
    const re4::AppExitPolicy exit_policy(NXDK_TITLE_ID);
    journal.note("exit_policy_done");
    AppletStorage ordinary{};
    journal.note("user_channel_begin");
    if (!startup.recovery&&game_reload_take_user_channel(&ordinary)) appletStorageClose(&ordinary);
    journal.note("user_channel_done");
    journal.note("launch_token_begin");
    persist_launch_token(argc,argv);
    journal.note("launch_token_done");
    mkdir("sdmc:/config", 0777); mkdir("sdmc:/config/re4-controls", 0777);
    mkdir("sdmc:/switch", 0777); mkdir(NXDK_SD_ROOT, 0777);
    journal.note("storage_ready");
    // Application startup writes only the bounded AppJournal. Overlay diagnostic
    // preferences do not trigger process inspection or extra startup report files.
    // Networking is opt-in. No resident SDK receiver or curl worker on startup.
    Result sockets=MAKERESULT(Module_Libnx,LibnxError_NotInitialized);
    journal.note("pad_setup_begin");
    PadState pad;
    padConfigureInput(2, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);
    PadState second_pad;padInitialize(&second_pad,HidNpadIdType_No2);
    Sensors sensors;
    journal.note("pad_setup_done");
    re4::InstallerModel model;
    model.recovery_mode=startup.recovery;
    auto& settings=model.settings;
    journal.note("settings_load_begin");
    re4::load_settings_file(re4::kSettingsPath,settings);
    re6::load_settings_file(re6::kSettingsPath,model.re6_settings);
    re5::load_settings_file(re5::kSettingsPath,model.re5_settings);
    journal.note("settings_load_done");
    re4::InstallerRenderer renderer;
    PlFontData font{};
    journal.note("font_service_begin");
    const Result font_service=plInitialize(PlServiceType_User);
    journal.note("font_service_done",font_service);
    journal.note("shared_font_begin");
    const Result font_result=R_SUCCEEDED(font_service)?plGetSharedFontByType(&font,PlSharedFontType_Standard):font_service;
    const bool font_ready=R_SUCCEEDED(font_result);
    journal.note("shared_font_done",font_result);
    journal.note("romfs_begin");
    const Result logo_fs=romfsInit();
    journal.note("romfs_done",logo_fs);
    journal.note("renderer_begin");
    const bool graphics=font_ready && renderer.initialize(font.address,font.size,
        R_SUCCEEDED(logo_fs)?"romfs:/app-logo.bmp":nullptr,false,
        R_SUCCEEDED(logo_fs)?"romfs:/app-logo-re6.bmp":nullptr,
        R_SUCCEEDED(logo_fs)?"romfs:/app-logo-re5.bmp":nullptr);
    if(R_SUCCEEDED(logo_fs)){
        std::string home_detail;
        re4::UpdateFileOps operations;
        journal.note("self_images_begin");
        const bool self_ready=re4::bootstrap_self_images(argc>0 && argv && argv[0]?argv[0]:"",NXDK_SD_ROOT,
            re4::update_hash_file,operations,home_detail);
        journal.note("self_images_done",self_ready?0:1);
        if(self_ready&&!startup.recovery){
            journal.note("home_launcher_begin");
            const auto home_result=re4::install_own_home_launcher("romfs:/home-launcher.bin",home_detail);
            journal.note("home_launcher_done",unsigned(home_result));
        }
        model.detail=home_detail;
        romfsExit();
    }
    journal.note("renderer_ready",graphics?0:1);
    journal.note("telemetry_retirement_begin");
    const auto retired_telemetry=re4::retire_telemetry_overlay("sdmc:");
    if(std::strcmp(retired_telemetry,"Telemetry overlay already absent.")){
        if(!model.detail.empty())model.detail+="\n";
        model.detail+=retired_telemetry;
    }
    if(startup.recovery){
        const char* recovery_note=settings.language==re4::Language::Russian
            ?"Режим восстановления: сетевые службы и автоперезапуск отключены."
            :"Recovery mode: network services and automatic restart are disabled.";
        model.detail=std::string(recovery_note)+"\n"+model.detail;
    }
    journal.note("telemetry_retirement_done");
    if(!graphics){
        model.detail=renderer.error();renderer.close();
        consoleInit(nullptr);
    }
    re4::GyroPreview preview;
    std::unique_ptr<re4::UpdateWorker> updater;
    re4::LocalUpdateServer local_server;
    auto last_local_state=re4::LocalUpdateState::Off;
    bool exit_pending=false,first_frame=true,https_start_failed=false;
    std::string local_start_notice;
    re4::UpdateSnapshot https_start_failure;
    float aim_x=0,aim_y=0;
    re4::GyroCalibration calibration;
    journal.note("ui_ready");
    while(appletMainLoop()){
        local_server.poll();
        const auto local_snapshot=local_server.snapshot();
        auto local_display=local_snapshot;
        if(local_display.state==re4::LocalUpdateState::Off&&!local_start_notice.empty()){
            local_display.state=re4::LocalUpdateState::Failed;local_display.detail=local_start_notice;
        }
        model.local_update_result(local_display);
        if(local_snapshot.state!=last_local_state){
            journal.note("local_server_state",unsigned(local_snapshot.state));last_local_state=local_snapshot.state;
        }
        const auto update=https_start_failed&&!updater?https_start_failure
            :re4::startup_update_snapshot(updater.get(),settings.language,startup);
        if(model.page==re4::InstallerPage::Updates && update.stage==re4::UpdateStage::Available
            && model.update.stage!=update.stage)model.selected=1;
        model.update=update;
        bool sdl_exit=false;
        if(graphics){
            SDL_Event event;
            while(SDL_PollEvent(&event))if(event.type==SDL_QUIT)sdl_exit=true;
        }
        if(sdl_exit)exit_pending=true;
        if(first_frame)journal.note("first_input_begin");
        padUpdate(&pad);padUpdate(&second_pad);
        const auto down=padGetButtonsDown(&pad);
        const auto previous_page=model.page;
        const auto previous_game=model.game;const auto previous_player=model.re5_player;
        const auto previous_language=settings.language;
        re4::InstallerAction action=re4::InstallerAction::None;
        using Key=re4::UiEvent;
        if(down&HidNpadButton_Plus)action=model.handle(Key::Exit);
        else if(down&HidNpadButton_B)action=model.handle(Key::Back);
        else if(down&HidNpadButton_Up)action=model.handle(Key::Up);
        else if(down&HidNpadButton_Down)action=model.handle(Key::Down);
        else if(down&HidNpadButton_Left)action=model.handle(Key::Left);
        else if(down&HidNpadButton_Right)action=model.handle(Key::Right);
        else if(down&HidNpadButton_A)action=model.handle(Key::Confirm);
        else if(down&HidNpadButton_L)action=model.handle(Key::PreviousGame);
        else if(down&HidNpadButton_R)action=model.handle(Key::NextGame);
        else if(down&HidNpadButton_ZR)action=model.handle(Key::QuickOverlay);
        else if(down&HidNpadButton_Minus)action=model.handle(Key::QuickDisable);
        else if(down&HidNpadButton_Y)action=model.handle(model.y_shortcut());
        else if(down&HidNpadButton_X)action=model.handle(model.x_shortcut());
        else if(down&HidNpadButton_ZL)action=model.handle(Key::OpenGyroTest);
        if(action!=re4::InstallerAction::None)journal.note("user_action",unsigned(action));
        if((down&HidNpadButton_Plus)||action==re4::InstallerAction::Exit)exit_pending=true;
        if(exit_pending){
            if(!local_server.stopped()){local_server.request_stop();action=re4::InstallerAction::None;}
            else if(updater&&updater->busy()){updater->cancel();action=re4::InstallerAction::None;}
            else action=re4::InstallerAction::Exit;
        }
        if(action==re4::InstallerAction::Exit){
            journal.note("exit_requested");
            updater.reset();
            if(R_SUCCEEDED(sockets)){socketExit();sockets=MAKERESULT(Module_Libnx,LibnxError_NotInitialized);}
            journal.note("network_stopped");
            // As in 0.15.0: leave the loop and let libnx tear down services and
            // exit. For the own HOME title, __nx_applet_exit_mode=1 makes libnx
            // perform ISelfController::Exit and then really end the process.
            journal.note(exit_policy.own_home()?"home_exit_begin":"loader_exit_begin");
            break;
        }
        if(action==re4::InstallerAction::StartLocalUpdate){
            if(updater&&updater->busy()){
                re4::LocalUpdateSnapshot blocked;blocked.state=re4::LocalUpdateState::Failed;
                blocked.detail=settings.language==re4::Language::Russian
                    ?u8"\u0414\u043e\u0436\u0434\u0438\u0442\u0435\u0441\u044c \u0437\u0430\u0432\u0435\u0440\u0448\u0435\u043d\u0438\u044f \u043e\u0431\u043d\u043e\u0432\u043b\u0435\u043d\u0438\u044f GitHub."
                    :"Wait for the GitHub operation to finish.";
                local_start_notice=blocked.detail;model.local_update_result(blocked);
            }else{
                local_start_notice.clear();https_start_failed=false;
                updater.reset();
                if(R_SUCCEEDED(sockets)){socketExit();sockets=MAKERESULT(Module_Libnx,LibnxError_NotInitialized);}
                journal.note("local_server_start_begin");
                const bool started=local_server.start(NXDK_SD_ROOT,NXDK_APP_NAME,re4::kVersion,
                    NXDK_SD_ROOT "/token.txt",re4::AppJournal::report,&journal);
                journal.note("local_server_start_done",started?0:1);
                model.local_update_result(local_server.snapshot());
            }
        }
        if(action==re4::InstallerAction::StopLocalUpdate){
            local_start_notice.clear();
            journal.note("local_server_stop_requested");local_server.request_stop();
            model.local_update_result(local_server.snapshot());
        }
        if(action==re4::InstallerAction::CheckUpdates&&!startup.recovery&&local_server.stopped()&&!updater){
            https_start_failed=false;
            journal.note("https_socket_begin");sockets=socketInitializeDefault();journal.note("https_socket_done",sockets);
            if(R_SUCCEEDED(sockets)){journal.note("https_worker_begin");updater=re4::create_startup_updater(startup);journal.note("https_worker_done");}
            else{
                https_start_failed=true;https_start_failure.stage=re4::UpdateStage::Failed;
                https_start_failure.detail="Network initialization failed";model.update=https_start_failure;
            }
        }
        if(updater){
            if(action==re4::InstallerAction::CheckUpdates)updater->check();
            if(action==re4::InstallerAction::InstallUpdate)updater->install();
            if(action==re4::InstallerAction::CancelUpdate)updater->cancel();
        }
        if(action==re4::InstallerAction::InstallGame || action==re4::InstallerAction::InstallOverlay
           || action==re4::InstallerAction::DisableGame){
            journal.note("mod_action_begin",unsigned(action));
            u64 running_pid=0,running_title=0;
            const Result pm=pmdmntInitialize(),info=pminfoInitialize();
            const bool identified=R_SUCCEEDED(pm) && R_SUCCEEDED(info)
                && R_SUCCEEDED(pmdmntGetApplicationProcessId(&running_pid))
                && R_SUCCEEDED(pminfoGetProgramId(&running_title,running_pid));
            const auto target_title=re4::game_title_id(model.game);
            const bool allowed=model.allow_installation(identified,running_title==target_title);
            if(allowed && action==re4::InstallerAction::DisableGame)
                model.installation_result(re4::disable_game_files("sdmc:",model.game));
            else if(allowed && R_SUCCEEDED(romfsInit())){
                model.installation_result(action==re4::InstallerAction::InstallOverlay
                    ?re4::install_overlay_only("romfs:","sdmc:",model.game):re4::install_game_files("romfs:","sdmc:",model.game));
                romfsExit();
            }else if(allowed){model.status=re4::InstallerStatus::Failed;model.detail="Bundled game files unavailable";}
            if(R_SUCCEEDED(info))pminfoExit();
            if(R_SUCCEEDED(pm))pmdmntExit();
            journal.note("mod_action_done",unsigned(model.status));
        }
        if(action==re4::InstallerAction::SaveSettings){
            if(model.game==re4::GameTarget::RE5){
                if(model.save_re5_settings()&&previous_language!=settings.language)model.save_shared_language();
            }
            else model.status=(model.game==re4::GameTarget::RE4 && model.page==re4::InstallerPage::Graphics
                && model.selected!=unsigned(re4::GraphicsId::Reset)?re4::save_edited_graphics(re4::kSettingsPath,settings):
                re4::save_profiled_settings(re4::kSettingsPath,model.re4_configuration()))
                ?re4::InstallerStatus::Saved:re4::InstallerStatus::Failed;
        }
        if(action==re4::InstallerAction::SaveRe6Graphics)
            model.save_re6_graphics();
        if(action==re4::InstallerAction::SaveGyroTest)
            model.save_gyro_test();
        if(action==re4::InstallerAction::DefaultControls || action==re4::InstallerAction::CustomControls
            || action==re4::InstallerAction::RE6Controls
            || action==re4::InstallerAction::DefaultGraphics || action==re4::InstallerAction::CustomGraphics
            || action==re4::InstallerAction::QualityGraphics){
            const bool graphics_profile=action==re4::InstallerAction::DefaultGraphics || action==re4::InstallerAction::CustomGraphics
                || action==re4::InstallerAction::QualityGraphics;
            const bool custom=action==re4::InstallerAction::CustomControls || action==re4::InstallerAction::CustomGraphics;
            const auto domain=graphics_profile?re4::ProfileDomain::Graphics:re4::ProfileDomain::Controls;
            const auto kind=action==re4::InstallerAction::RE6Controls?re4::ProfileKind::RE6:
                action==re4::InstallerAction::QualityGraphics?re4::ProfileKind::Quality:
                custom?re4::ProfileKind::Custom:re4::ProfileKind::Default;
            const auto result=model.game==re4::GameTarget::RE5?model.select_re5_profile(domain,kind):
                re4::switch_profile(re4::kSettingsPath,settings,domain,kind);
            model.status=result==re4::ProfileResult::Applied || result==re4::ProfileResult::Unchanged?
                re4::InstallerStatus::Saved:re4::InstallerStatus::Failed;
            model.profile_note=re4::profile_result_text(result,settings.language);
        }
        if(action==re4::InstallerAction::ResetPreview){aim_x=aim_y=0;preview.reset();}
        if(action==re4::InstallerAction::Calibrate && sensors.count){
            calibration.begin();
            model.status=re4::InstallerStatus::Calibrating;
        }
        const bool player2=model.game==re4::GameTarget::RE5&&model.re5_player==1;
        const auto& test_pad=player2?second_pad:pad;
        if(first_frame)journal.note("sensors_select_begin");
        const bool sensor_changed=sensors.select(test_pad,player2?HidNpadIdType_No2:HidNpadIdType_No1);
        if(first_frame)journal.note("sensors_select_done",sensors.result);
        const bool page_changed=previous_page!=model.page||previous_game!=model.game||previous_player!=model.re5_player;
        if(sensor_changed || page_changed){
            if(calibration.remaining())model.status=sensors.count?re4::InstallerStatus::Ready:re4::InstallerStatus::NoSensor;
            preview.reset();calibration.cancel();aim_x=aim_y=0;
        }
        const bool gyro_test=model.page==re4::InstallerPage::GyroTest;
        if(model.page!=re4::InstallerPage::Diagnostics && !gyro_test)calibration.cancel();
        HidSixAxisSensorState samples[16]{};
        const size_t sample_count=sensors.read(samples);
        const auto& preview_settings=model.preview_settings();
        preview.begin_frame(armGetSystemTick(),preview_settings,gyro_test && sensors.count
            && !page_changed && !sensor_changed && !calibration.remaining());
        for(size_t i=sample_count;i>0;--i){
            const auto& sample=samples[i-1];
            const float yaw=-sample.angular_velocity.y*tau,pitch=-sample.angular_velocity.x*tau;
            if((sample.attributes&1) && calibration.add(sample.sampling_number,yaw,pitch)){
                model.calibrated(calibration.bias_x(),calibration.bias_y());
                if(!gyro_test){
                    const bool saved=model.game==re4::GameTarget::RE5?model.save_re5_settings():re4::save_profiled_settings(re4::kSettingsPath,settings);
                    model.status=saved?re4::InstallerStatus::Calibrated:re4::InstallerStatus::Failed;
                }
                preview.reset();aim_x=aim_y=0;
            }
            preview.push({sample.sampling_number,sample.delta_time,yaw,pitch},sample.attributes&1);
        }
        const auto movement=preview.finish_frame();
        const auto position=re4::clamp_gyro_preview_position({aim_x+movement.yaw,aim_y+movement.pitch});
        aim_x=position.yaw;aim_y=position.pitch;
        const auto left=padGetStickPos(&test_pad,0),right=padGetStickPos(&test_pad,1);
        auto& d=model.diagnostic;
        d.connected=padIsConnected(&test_pad);d.network=R_SUCCEEDED(sockets)||!local_server.stopped();
        d.style=sensors.style;d.sensors=sensors.count;d.sensor_result=sensors.result;
        d.samples=unsigned(sample_count);d.calibration_remaining=calibration.remaining();
        d.raw[0]=left.x/32767.f;d.raw[1]=left.y/32767.f;
        d.raw[2]=right.x/32767.f;d.raw[3]=right.y/32767.f;
        re4::process_stick_diagnostics(d.raw,settings,d.processed);
        d.gyro_rate[0]=samples[0].angular_velocity.x;d.gyro_rate[1]=samples[0].angular_velocity.y;
        d.gyro_rate[2]=samples[0].angular_velocity.z;d.gyro_preview[0]=aim_x;d.gyro_preview[1]=aim_y;
        if(!updater && previous_language!=settings.language)
            model.update=re4::startup_update_snapshot(updater.get(),settings.language,startup);
        if(first_frame)journal.note("first_input_done");
        if(first_frame)journal.note("first_present_begin");
        if(graphics)renderer.draw(model);
        else{
            std::printf("\x1b[HRE4 CONTROLS %s\n%s\n\n",re4::kVersion,model.page_title());
            for(unsigned i=0;i<model.item_count();++i)
                std::printf("%c %-40s %s\x1b[K\n",i==model.selected?'>':' ',model.item_label(i).c_str(),model.item_value(i).c_str());
            std::printf("\n%s\x1b[K\n%s\nUP/DOWN  A select  B back  + exit\x1b[J\n",model.status_text(),(model.page==re4::InstallerPage::Updates?model.update.detail:model.detail).c_str());
            consoleUpdate(nullptr);
        }
        if(first_frame){journal.note("first_present_done");first_frame=false;}
    }
    // Join the explicitly enabled receiver before SDL or socket cleanup.
    journal.note("cleanup_begin");
    // Keep the default focus mode here: NoSuspend plus any wait below made a
    // lingering process block HOME. libnx applies its own exit policy last.
    local_server.request_stop();
    while(!local_server.stopped()){local_server.poll();svcSleepThread(16'000'000);}
    journal.note("local_server_stopped");
    sensors.stop();
    if(updater)updater->close();
    renderer.close();
    if(R_SUCCEEDED(font_service))plExit();
    if(!graphics)consoleExit(nullptr);
    if(R_SUCCEEDED(sockets))socketExit();
    journal.note("clean_exit");
    return 0;
}
