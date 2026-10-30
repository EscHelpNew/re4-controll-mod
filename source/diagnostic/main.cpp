#define SDL_MAIN_HANDLED
#include <switch.h>
#include <SDL.h>
#include "re4/installer_ui.hpp"
#include "re4/calibration.hpp"
#include "re4/home_launcher.hpp"
#include "re4/version.hpp"
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
extern "C" {
#include "remote_reload.h"
#include "reload_main.h"
#include "reload_project.h"
#include "reload_user_channel.h"
#include "reload_core.h"
}

namespace {
constexpr float tau = 6.28318530718f;
void inspect_overlay_loader(FILE* report) {
    u64 pid = 0;
    const auto rc = pmdmntGetProcessId(&pid, 0x420000000007E51Aull);
    std::fprintf(report, "ovlloader process rc=%08X pid=%llX\n", rc, (unsigned long long)pid);
    for (const auto config : {"sdmc:/config/uberhand/config.ini", "sdmc:/config/ultrahand/config.ini", "sdmc:/config/tesla/config.ini"}) {
        FILE* file = std::fopen(config, "rb");
        if (!file) continue;
        char line[256];
        for (unsigned lines = 0; lines < 512 && std::fgets(line, sizeof(line), file); ++lines)
            if (std::strstr(line, "key_combo")) std::fprintf(report, "%s: %.150s", config, line);
        std::fclose(file);
    }
    for (const auto path : {"sdmc:/atmosphere/contents/420000000007E51A/exefs.nsp",
            "sdmc:/atmosphere/contents/420000000007E51A/flags/boot2.flag",
            "sdmc:/switch/.overlays/ovlmenu.ovl"}) {
        struct stat st{};
        if (stat(path, &st)) { std::fprintf(report, "absent %s\n", path); continue; }
        std::fprintf(report, "file %s bytes=%lld\n", path, (long long)st.st_size);
        FILE* file = std::fopen(path, "rb");
        if (!file) continue;
        if (std::strstr(path, "ovlmenu.ovl")) {
            unsigned char header[0x80]{};
            if (std::fread(header, 1, sizeof(header), file) == sizeof(header) && !std::memcmp(header + 16, "NRO0", 4)) {
                u32 nro_size = 0; std::memcpy(&nro_size, header + 24, 4);
                unsigned char aset[56]{};
                if (nro_size < u64(st.st_size) && !std::fseek(file, nro_size, SEEK_SET)
                    && std::fread(aset, 1, sizeof(aset), file) == sizeof(aset) && !std::memcmp(aset, "ASET", 4)) {
                    u64 offset = 0; std::memcpy(&offset, aset + 24, 8);
                    if (offset < u64(st.st_size) - nro_size && !std::fseek(file, nro_size + offset, SEEK_SET)) {
                        char name[129]{};
                        std::fread(name, 1, sizeof(name) - 1, file);
                        for (auto& c : name) if (static_cast<unsigned char>(c) < 32 && c) c = '?';
                        std::fprintf(report, "overlay_menu_name=%.128s\n", name);
                    }
                }
            }
        }
        std::fclose(file);
    }
}
void inspect_installed_re4(FILE* report) {
    const auto ncm_rc = ncmInitialize();
    std::fprintf(report, "ncm=%08X\n", ncm_rc);
    if (R_SUCCEEDED(ncm_rc)) {
        for (const auto storage : {NcmStorageId_SdCard, NcmStorageId_BuiltInUser}) {
            NcmContentMetaDatabase db{};
            const auto opened = ncmOpenContentMetaDatabase(&db, storage);
            std::fprintf(report, "meta storage=%u rc=%08X\n", unsigned(storage), opened);
            if (R_FAILED(opened)) continue;
            for (const auto title : {0x010099A00BC1E000ull, 0x010099A00BC1E800ull}) {
                NcmContentMetaKey key{};
                const auto rc = ncmContentMetaDatabaseGetLatestContentMetaKey(&db, &key, title);
                std::fprintf(report, "title=%016llX rc=%08X version=%u type=%u\n", title, rc, key.version, unsigned(key.type));
            }
            ncmContentMetaDatabaseClose(&db);
        }
        ncmExit();
    }
    const auto lr_rc = lrInitialize();
    const auto fs_rc = fsldrInitialize();
    std::fprintf(report, "installed_read lr=%08X fsp_ldr=%08X\n", lr_rc, fs_rc);
    if (R_SUCCEEDED(lr_rc)) {
        for (const auto storage : {NcmStorageId_SdCard, NcmStorageId_BuiltInUser}) {
            LrLocationResolver resolver{};
            Result rc = lrOpenLocationResolver(storage, &resolver);
            if (R_FAILED(rc)) continue;
            char path[FS_MAX_PATH]{};
            rc = lrLrResolveProgramPath(&resolver, 0x010099A00BC1E000ull, path);
            serviceClose(&resolver.s);
            std::fprintf(report, "storage=%u resolve=%08X\n", unsigned(storage), rc);
            if (R_FAILED(rc)) continue;
            std::fprintf(report, "program_path=%s\n", path);
            if (R_FAILED(fs_rc)) continue;
            FsCodeInfo info{};
            FsFileSystem fs{};
            rc = fsldrOpenCodeFileSystem(&info, 0x010099A00BC1E000ull, storage, path,
                FsContentAttributes_None, &fs);
            std::fprintf(report, "open_code=%08X\n", rc);
            if (R_FAILED(rc)) continue;
            FsFile main{};
            rc = fsFsOpenFile(&fs, "/main", FsOpenMode_Read, &main);
            if (R_SUCCEEDED(rc)) {
                unsigned char header[0x100]{};
                u64 count = 0;
                rc = fsFileRead(&main, 0, header, sizeof(header), FsReadOption_None, &count);
                std::fprintf(report, "main_header rc=%08X bytes=%llu build=", rc, (unsigned long long)count);
                if (R_SUCCEEDED(rc) && count >= 0x60 && !std::memcmp(header, "NSO0", 4))
                    for (unsigned i = 0x40; i < 0x60; ++i) std::fprintf(report, "%02X", header[i]);
                std::fprintf(report, "\n");
                fsFileClose(&main);
            } else std::fprintf(report, "open_main=%08X\n", rc);
            fsFsClose(&fs);
        }
    }
    if (R_SUCCEEDED(fs_rc)) fsldrExit();
    if (R_SUCCEEDED(lr_rc)) lrExit();
}
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
void configure_reload(int argc, char** argv) {
    for (int i = 0; i < argc; ++i) {
        if (!std::strncmp(argv[i], "--token=", 8)) {
            if (!persist_reload_token(argv[i] + 8))
                std::fprintf(stderr, "[reload] SD credential persistence unavailable\n");
            game_reload_configure(argc, argv); return;
        }
    }
    // The local SD token enables first launch from this project's forwarder.
    // Pass it explicitly to the unmodified kit, never embed it in an image.
    char token[67]{};
    char argument[73]{};
    char* args[64]{};
    FILE* file = std::fopen(NXDK_SD_ROOT "/token.txt", "rb");
    size_t count = file ? std::fread(token, 1, 66, file) : 0;
    const bool complete = file && std::fgetc(file) == EOF && !std::ferror(file);
    if (file) std::fclose(file);
    while (count && (token[count - 1] == '\n' || token[count - 1] == '\r')) --count;
    token[count] = 0;
    if (complete && count == 64 && reload_token_valid(token) && argc >= 0 && argc < 63) {
        for (int i = 0; i < argc; ++i) args[i] = argv[i];
        std::snprintf(argument, sizeof(argument), "--token=%s", token);
        args[argc] = argument;
        game_reload_configure(argc + 1, args);
    } else game_reload_configure(argc, argv);
    std::memset(token, 0, sizeof(token));
    std::memset(argument, 0, sizeof(argument));
}
struct Sensors {
    HidSixAxisSensorHandle handles[2]{};
    bool running[2]{};
    unsigned count = 0;
    u32 style = 0;
    u32 attributes = 0;
    Result result = 0;
    void stop() {
        for (unsigned i = 0; i < 2; ++i) {
            if (running[i]) hidStopSixAxisSensor(handles[i]);
            running[i] = false;
        }
        count = 0;
    }
    bool select(const PadState& pad) {
        const auto next = padGetStyleSet(&pad);
        const auto connected = padGetAttributes(&pad) &
            (HidNpadAttribute_IsConnected | HidNpadAttribute_IsLeftConnected | HidNpadAttribute_IsRightConnected);
        if (next == style && connected == attributes) return false;
        stop(); style = next; attributes = connected; result = 0;
        HidNpadStyleTag tag;
        auto id = HidNpadIdType_No1;
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
    if (game_reload_early_bridge()) return 0;
    AppletStorage ordinary{};
    if (game_reload_take_user_channel(&ordinary)) appletStorageClose(&ordinary);
    configure_reload(argc, argv);
    mkdir("sdmc:/config", 0777); mkdir("sdmc:/config/re4-controls", 0777);
    mkdir("sdmc:/switch", 0777); mkdir(NXDK_SD_ROOT, 0777);
    // Album/applet launch can inspect suspended RE4 without replacing it.
    const auto debug_init = pmdmntInitialize();
    if (R_SUCCEEDED(debug_init)) {
        GameLink game;
        game.initialize(); game.connect();
        FILE* report = std::fopen(NXDK_SD_ROOT "/reload-bridge.log", "wb");
        if (report) {
            std::fprintf(report, "RE4 inspection HOS=%08X applet=%u\n%s\n", hosversionGet(),
                unsigned(appletGetAppletType()), game.details.c_str());
            if (overlay_storage::mount()) {
                mkdir("r4ovl:/config", 0777); mkdir("r4ovl:/config/re4-controls", 0777);
                re4::Settings check;
                const char* check_path = "r4ovl:/config/re4-controls/storage-check.bin";
                const bool ok = re4::save_settings_file(check_path, {})
                    && re4::load_settings_file(check_path, check) && check.turn_gain == 1;
                std::fprintf(report, "overlay_storage_check=%s\n", ok ? "PASS" : "FAIL");
                std::remove(check_path);
                std::remove("r4ovl:/config/re4-controls/storage-check.bin.previous");
                overlay_storage::close();
            } else std::fprintf(report, "overlay_storage_check=MOUNT_FAIL\n");
            FILE* previous = std::fopen(NXDK_SD_ROOT "/game-inspection.txt", "rb");
            if (previous) {
                char text[1401]{};
                const auto count = std::fread(text, 1, sizeof(text) - 1, previous);
                std::fclose(previous);
                std::fprintf(report, "last_overlay_inspection:\n%.*s\n", int(count), text);
            }
            inspect_overlay_loader(report);
            inspect_installed_re4(report);
            std::fclose(report);
        }
        game.close(); pmdmntExit();
    }
    const Result sockets = socketInitializeDefault();
    if (R_SUCCEEDED(sockets)) {
        nxlinkStdioForDebug();
        u64 process_id = 0;
        svcGetProcessId(&process_id, CUR_PROCESS_HANDLE);
        char instance[24];
        std::snprintf(instance, sizeof(instance), "%s-p%llX",re4::kVersion, (unsigned long long)process_id);
        game_reload_start(NXDK_APP_NAME, instance);
    }
    PadState pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);
    Sensors sensors;
    re4::InstallerModel model;
    auto& settings=model.settings;
    re4::load_settings_file(re4::kSettingsPath,settings);
    re4::InstallerRenderer renderer;
    PlFontData font{};
    const Result font_service=plInitialize(PlServiceType_User);
    const bool font_ready=R_SUCCEEDED(font_service) && R_SUCCEEDED(plGetSharedFontByType(&font,PlSharedFontType_Standard));
    const Result logo_fs=romfsInit();
    const bool graphics=font_ready && renderer.initialize(font.address,font.size,
        R_SUCCEEDED(logo_fs)?"romfs:/app-logo.bmp":nullptr);
    if(R_SUCCEEDED(logo_fs)){
        std::string home_detail;
        re4::UpdateFileOps operations;
        if(re4::bootstrap_self_images(argc>0 && argv && argv[0]?argv[0]:"",NXDK_SD_ROOT,
            re4::update_hash_file,operations,home_detail))
            re4::install_own_home_launcher("romfs:/home-launcher.bin",home_detail);
        model.detail=home_detail;
        romfsExit();
    }
    if(!graphics){
        model.detail=renderer.error();renderer.close();
        consoleInit(nullptr);
    }
    re4::GyroFilter integrator;
    re4::UpdateWorker updater;
    float aim_x=0,aim_y=0;
    re4::GyroCalibration calibration;
    unsigned frame=0;
    while(appletMainLoop()){
        game_reload_main_poll();
        const auto update=updater.snapshot();
        if(model.page==re4::InstallerPage::Updates && update.stage==re4::UpdateStage::Available
            && model.update.stage!=update.stage)model.selected=1;
        model.update=update;
        bool sdl_exit=false;
        if(graphics){
            SDL_Event event;
            while(SDL_PollEvent(&event))if(event.type==SDL_QUIT)sdl_exit=true;
        }
        if(sdl_exit)break;
        padUpdate(&pad);
        const auto down=padGetButtonsDown(&pad);
        const auto previous_page=model.page;
        re4::InstallerAction action=re4::InstallerAction::None;
        using Key=re4::UiEvent;
        if(down&HidNpadButton_Plus)action=model.handle(Key::Exit);
        else if(down&HidNpadButton_B)action=model.handle(Key::Back);
        else if(down&HidNpadButton_Up)action=model.handle(Key::Up);
        else if(down&HidNpadButton_Down)action=model.handle(Key::Down);
        else if(down&HidNpadButton_Left)action=model.handle(Key::Left);
        else if(down&HidNpadButton_Right)action=model.handle(Key::Right);
        else if(down&HidNpadButton_A)action=model.handle(Key::Confirm);
        else if(down&HidNpadButton_R)action=model.handle(Key::QuickInstall);
        else if(down&HidNpadButton_ZR)action=model.handle(Key::QuickOverlay);
        else if(down&HidNpadButton_Minus)action=model.handle(Key::QuickDisable);
        else if(down&HidNpadButton_Y)action=model.handle(Key::Calibrate);
        else if(down&HidNpadButton_X)action=model.handle(Key::ResetPreview);
        if(action==re4::InstallerAction::Exit)break;
        if(action==re4::InstallerAction::CheckUpdates)updater.check();
        if(action==re4::InstallerAction::InstallUpdate)updater.install();
        if(action==re4::InstallerAction::CancelUpdate)updater.cancel();
        if(action==re4::InstallerAction::InstallGame || action==re4::InstallerAction::InstallOverlay
           || action==re4::InstallerAction::DisableGame){
            u64 running_pid=0,running_title=0;
            const Result pm=pmdmntInitialize(),info=pminfoInitialize();
            const bool identified=R_SUCCEEDED(pm) && R_SUCCEEDED(info)
                && R_SUCCEEDED(pmdmntGetApplicationProcessId(&running_pid))
                && R_SUCCEEDED(pminfoGetProgramId(&running_title,running_pid));
            const bool allowed=model.allow_installation(identified,running_title==0x010099A00BC1E000ull);
            if(allowed && action==re4::InstallerAction::DisableGame)
                model.installation_result(re4::disable_game_files("sdmc:"));
            else if(allowed && R_SUCCEEDED(romfsInit())){
                model.installation_result(action==re4::InstallerAction::InstallOverlay
                    ?re4::install_overlay_only("romfs:","sdmc:"):re4::install_game_files("romfs:","sdmc:"));
                romfsExit();
            }else if(allowed){model.status=re4::InstallerStatus::Failed;model.detail="Bundled game files unavailable";}
            if(R_SUCCEEDED(info))pminfoExit();
            if(R_SUCCEEDED(pm))pmdmntExit();
        }
        if(action==re4::InstallerAction::SaveSettings)
            model.status=re4::save_profiled_settings(re4::kSettingsPath,settings)
                ?re4::InstallerStatus::Saved:re4::InstallerStatus::Failed;
        if(action==re4::InstallerAction::DefaultControls || action==re4::InstallerAction::CustomControls
            || action==re4::InstallerAction::DefaultGraphics || action==re4::InstallerAction::CustomGraphics){
            const bool graphics_profile=action==re4::InstallerAction::DefaultGraphics || action==re4::InstallerAction::CustomGraphics;
            const bool custom=action==re4::InstallerAction::CustomControls || action==re4::InstallerAction::CustomGraphics;
            const auto result=re4::switch_profile(re4::kSettingsPath,settings,
                graphics_profile?re4::ProfileDomain::Graphics:re4::ProfileDomain::Controls,
                custom?re4::ProfileKind::Custom:re4::ProfileKind::Default);
            model.status=result==re4::ProfileResult::Applied || result==re4::ProfileResult::Unchanged?
                re4::InstallerStatus::Saved:re4::InstallerStatus::Failed;
            model.profile_note=re4::profile_result_text(result,settings.language);
        }
        if(action==re4::InstallerAction::ResetPreview)aim_x=aim_y=0;
        if(action==re4::InstallerAction::Calibrate && sensors.count){
            calibration.begin();
            model.status=re4::InstallerStatus::Calibrating;
        }
        const bool sensor_changed=sensors.select(pad);
        const bool page_changed=previous_page!=model.page;
        if(sensor_changed || page_changed){
            if(calibration.remaining())model.status=sensors.count?re4::InstallerStatus::Ready:re4::InstallerStatus::NoSensor;
            integrator.reset();calibration.cancel();aim_x=aim_y=0;
        }
        if(model.page!=re4::InstallerPage::Diagnostics)calibration.cancel();
        HidSixAxisSensorState samples[16]{};
        const size_t sample_count=sensors.read(samples);
        re4::AimDelta movement;
        for(size_t i=sample_count;i>0;--i){
            const auto& sample=samples[i-1];
            const float yaw=-sample.angular_velocity.y*tau,pitch=-sample.angular_velocity.x*tau;
            if((sample.attributes&1) && calibration.add(sample.sampling_number,yaw,pitch)){
                settings.gyro_bias_x=calibration.bias_x();settings.gyro_bias_y=calibration.bias_y();
                model.status=re4::save_profiled_settings(re4::kSettingsPath,settings)
                    ?re4::InstallerStatus::Calibrated:re4::InstallerStatus::Failed;
            }
            const bool active=model.page==re4::InstallerPage::Diagnostics && !page_changed && !sensor_changed
                && settings.gyro_enabled && (sample.attributes&1) && !calibration.remaining() && (padGetButtons(&pad)&HidNpadButton_ZL);
            const auto delta=integrator.update({sample.sampling_number,sample.delta_time,
                yaw-settings.gyro_bias_x,pitch-settings.gyro_bias_y},active,
                settings.gyro_speed*settings.gyro_gain_x*(settings.invert_x?-1:1),
                settings.gyro_speed*settings.gyro_gain_y*(settings.invert_y?-1:1),settings.gyro_deadzone_dps);
            movement.yaw+=delta.yaw;movement.pitch+=delta.pitch;
        }
        aim_x+=movement.yaw;aim_y+=movement.pitch;
        const auto left=padGetStickPos(&pad,0),right=padGetStickPos(&pad,1);
        auto& d=model.diagnostic;
        d.connected=padIsConnected(&pad);d.network=R_SUCCEEDED(sockets);
        d.style=sensors.style;d.sensors=sensors.count;d.sensor_result=sensors.result;
        d.samples=unsigned(sample_count);d.calibration_remaining=calibration.remaining();
        d.raw[0]=left.x/32767.f;d.raw[1]=left.y/32767.f;
        d.raw[2]=right.x/32767.f;d.raw[3]=right.y/32767.f;
        re4::process_stick_diagnostics(d.raw,settings,d.processed);
        d.gyro_rate[0]=samples[0].angular_velocity.x;d.gyro_rate[1]=samples[0].angular_velocity.y;
        d.gyro_rate[2]=samples[0].angular_velocity.z;d.gyro_preview[0]=aim_x;d.gyro_preview[1]=aim_y;
        if(graphics)renderer.draw(model);
        else{
            std::printf("\x1b[HRE4 CONTROLS %s\n%s\n\n",re4::kVersion,model.page_title());
            for(unsigned i=0;i<model.item_count();++i)
                std::printf("%c %-40s %s\x1b[K\n",i==model.selected?'>':' ',model.item_label(i).c_str(),model.item_value(i).c_str());
            std::printf("\n%s\x1b[K\n%s\nUP/DOWN  A select  B back  + exit\x1b[J\n",model.status_text(),model.detail.c_str());
            consoleUpdate(nullptr);
        }
        if(++frame%120==0)std::fprintf(stderr,
            "[input] style=%08lX raw=%ld,%ld,%ld,%ld gyro_samples=%u sensor_rc=%08lX\n",
            (unsigned long)sensors.style,(long)left.x,(long)left.y,(long)right.x,(long)right.y,
            unsigned(sample_count),(unsigned long)sensors.result);
    }
    sensors.stop();
    updater.close();
    renderer.close();
    if(R_SUCCEEDED(font_service))plExit();
    if(!graphics)consoleExit(nullptr);
    game_reload_main_finish();
    if(R_SUCCEEDED(sockets))socketExit();
    return 0;
}
