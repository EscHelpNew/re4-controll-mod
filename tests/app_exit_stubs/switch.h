#pragma once
#include <cstdint>
using u32=std::uint32_t;
using u64=std::uint64_t;
using Result=u32;
enum AppletType { AppletType_None=-2,AppletType_Application=0,AppletType_LibraryApplet=2,AppletType_SystemApplication=4 };
enum AppletFocusHandlingMode { AppletFocusHandlingMode_SuspendHomeSleep=0,AppletFocusHandlingMode_NoSuspend=1 };
constexpr unsigned InfoType_ProgramId=18,CUR_PROCESS_HANDLE=0xffff8001;
constexpr unsigned Module_Libnx=345,LibnxError_NotInitialized=1;
#define MAKERESULT(module,description) ((module)|((description)<<9))
#define R_FAILED(rc) ((rc)!=0)
#define R_SUCCEEDED(rc) ((rc)==0)
AppletType appletGetAppletType();
Result svcGetInfo(u64*,unsigned,unsigned,u64);
Result appletSetFocusHandlingMode(AppletFocusHandlingMode);
void svcSleepThread(std::int64_t);
