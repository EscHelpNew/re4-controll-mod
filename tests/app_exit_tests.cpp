#include "app_exit.hpp"
#include <cstdlib>
#include <cstdio>
#include <vector>

#define CHECK(expr) do{if(!(expr)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#expr);std::exit(1);}}while(0)

namespace {
constexpr u64 own=0x0100E4C04E540000ull;
AppletType type=AppletType_Application;
u64 title=own;
Result info_rc=0,focus_rc=0,exit_rc=0;
std::vector<int> calls;
struct ManagedWait {};
void reset(){type=AppletType_Application;title=own;info_rc=focus_rc=exit_rc=0;calls.clear();__nx_applet_exit_mode=0;}
}
extern "C" { u32 __nx_applet_exit_mode=0; }
AppletType appletGetAppletType(){return type;}
Result svcGetInfo(u64* out,unsigned kind,unsigned handle,u64 index){
    CHECK(kind==InfoType_ProgramId && handle==CUR_PROCESS_HANDLE && index==0);
    *out=title;return info_rc;
}
Result appletSetFocusHandlingMode(AppletFocusHandlingMode mode){calls.push_back(mode==AppletFocusHandlingMode_NoSuspend?1:3);return focus_rc;}
extern "C" Result game_reload_request_exit(){calls.push_back(2);return exit_rc;}
void svcSleepThread(std::int64_t duration){CHECK(duration>0);calls.push_back(4);throw ManagedWait{};}

int main(){
    // Without a receiver/token, an own-title NRO must still exit the process.
    reset();re4::AppExitPolicy normal(own);
    CHECK(normal.own_home() && __nx_applet_exit_mode==1);
    bool waited=false;
    try{normal.request_home_exit();}catch(const ManagedWait&){waited=true;}
    CHECK(waited && (calls==std::vector<int>{1,2,4}));
    // Rejected Exit leaves caller/UI alive, restores its normal focus policy,
    // and can be retried without dismantling any application resources.
    reset();re4::AppExitPolicy rejected(own);exit_rc=0x1234;
    CHECK(rejected.request_home_exit()==0x1234 && (calls==std::vector<int>{1,2,3}));
    calls.clear();exit_rc=0;waited=false;
    try{rejected.request_home_exit();}catch(const ManagedWait&){waited=true;}
    CHECK(waited && (calls==std::vector<int>{1,2,4}));
    reset();re4::AppExitPolicy focus_failed(own);focus_rc=0x4321;
    CHECK(focus_failed.request_home_exit()==0x4321 && (calls==std::vector<int>{1}));
    // Foreign hbmenu hosts and library applets retain their loader contract.
    for(const int scenario:{0,1,2,3}){
        reset();
        if(scenario==0)title=0x010099A00BC1E000ull;
        if(scenario==1)type=AppletType_LibraryApplet;
        if(scenario==2)info_rc=0x99;
        re4::AppExitPolicy foreign(scenario==3?0:own);
        CHECK(!foreign.own_home() && __nx_applet_exit_mode==0);
        CHECK(R_FAILED(foreign.request_home_exit()) && calls.empty());
    }
    // SystemApplication has a managed exit but no Application-only focus API.
    reset();type=AppletType_SystemApplication;re4::AppExitPolicy system(own);
    CHECK(system.own_home() && __nx_applet_exit_mode==1);waited=false;
    try{system.request_home_exit();}catch(const ManagedWait&){waited=true;}
    CHECK(waited && (calls==std::vector<int>{2,4}));
    std::puts("App exit: own HOME managed exit, tokenless route, rejected-exit retry and foreign loader preservation PASS");
}
