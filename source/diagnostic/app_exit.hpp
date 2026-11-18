#pragma once
#include <switch.h>

extern "C" {
extern u32 __nx_applet_exit_mode;
}

namespace re4 {
// The own HOME forwarder has no hbmenu to return to. Select libnx's managed
// exit (ISelfController::Exit after full libnx teardown, then process exit).
// Never exit manually while services are live and never park the main thread:
// a lingering NoSuspend process froze the screen and blocked HOME (0.17.2+).
class AppExitPolicy {
    bool own_home_=false;
public:
    explicit AppExitPolicy(u64 own_title){
        const AppletType type=appletGetAppletType();
        u64 current=0;
        own_home_=own_title && (type==AppletType_Application || type==AppletType_SystemApplication)
            && R_SUCCEEDED(svcGetInfo(&current,InfoType_ProgramId,CUR_PROCESS_HANDLE,0)) && current==own_title;
        // Configure this only after early_bridge, whose deliberate NRO-to-NRO
        // return must survive.
        if(own_home_)__nx_applet_exit_mode=1;
    }
    bool own_home()const{return own_home_;}
};
}
