#include "app_exit.hpp"
#include <cstdlib>
#include <cstdio>
#include <vector>

#define CHECK(expr) do{if(!(expr)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#expr);std::exit(1);}}while(0)

namespace {
constexpr u64 own=0x0100E4C04E540000ull;
AppletType type=AppletType_Application;
u64 title=own;
Result info_rc=0;
std::vector<int> calls;
void reset(){type=AppletType_Application;title=own;info_rc=0;calls.clear();__nx_applet_exit_mode=0;}
}
extern "C" { u32 __nx_applet_exit_mode=0; }
AppletType appletGetAppletType(){return type;}
Result svcGetInfo(u64* out,unsigned kind,unsigned handle,u64 index){
    CHECK(kind==InfoType_ProgramId && handle==CUR_PROCESS_HANDLE && index==0);
    *out=title;return info_rc;
}
// Any manual focus change, exit dispatch or parked sleep is a regression.
Result appletSetFocusHandlingMode(AppletFocusHandlingMode){calls.push_back(1);return 0;}
extern "C" Result game_reload_request_exit(){calls.push_back(2);return 0;}
void svcSleepThread(std::int64_t){calls.push_back(4);}

int main(){
    // Own HOME title: libnx's managed exit only, nothing done by the policy.
    reset();re4::AppExitPolicy normal(own);
    CHECK(normal.own_home() && __nx_applet_exit_mode==1 && calls.empty());
    reset();type=AppletType_SystemApplication;re4::AppExitPolicy system(own);
    CHECK(system.own_home() && __nx_applet_exit_mode==1 && calls.empty());
    // Foreign hbmenu hosts and library applets retain their loader contract.
    for(const int scenario:{0,1,2,3}){
        reset();
        if(scenario==0)title=0x010099A00BC1E000ull;
        if(scenario==1)type=AppletType_LibraryApplet;
        if(scenario==2)info_rc=0x99;
        re4::AppExitPolicy foreign(scenario==3?0:own);
        CHECK(!foreign.own_home() && __nx_applet_exit_mode==0 && calls.empty());
    }
    std::puts("App exit: own HOME uses libnx managed exit without focus override or parked thread; foreign loader preserved PASS");
}
