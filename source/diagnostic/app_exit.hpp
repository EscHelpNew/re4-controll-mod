#pragma once
#include <switch.h>

extern "C" {
extern u32 __nx_applet_exit_mode;
Result game_reload_request_exit(void);
}

namespace re4 {
class AppExitPolicy {
    bool own_home_=false;
    AppletType type_;
public:
    explicit AppExitPolicy(u64 own_title):type_(appletGetAppletType()){
        u64 current=0;
        own_home_=own_title && (type_==AppletType_Application || type_==AppletType_SystemApplication)
            && R_SUCCEEDED(svcGetInfo(&current,InfoType_ProgramId,CUR_PROCESS_HANDLE,0)) && current==own_title;
        // The own forwarder has no hbmenu to return to. Configure this only
        // after early_bridge, whose deliberate NRO-to-NRO return must survive.
        if(own_home_)__nx_applet_exit_mode=1;
    }
    bool own_home()const{return own_home_;}
    Result request_home_exit()const{
        if(!own_home_)return MAKERESULT(Module_Libnx,LibnxError_NotInitialized);
        // Match libnx's Application cleanup before ISelfController::Exit.
        const bool focus=type_==AppletType_Application;
        if(focus){
            const Result rc=appletSetFocusHandlingMode(AppletFocusHandlingMode_NoSuspend);
            if(R_FAILED(rc))return rc;
        }
        const Result rc=game_reload_request_exit();
        if(R_FAILED(rc)){
            // Keep the renderer and workers alive so the caller can display
            // the error and retry. This app otherwise uses libnx's default.
            if(focus)(void)appletSetFocusHandlingMode(AppletFocusHandlingMode_SuspendHomeSleep);
            return rc;
        }
        // Managed exit owns process termination. Never return to a loader or
        // tear down application services while its resident workers are live.
        for(;;)svcSleepThread(1000000000);
    }
};
}
