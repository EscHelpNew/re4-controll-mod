#pragma once
#include "recorder.hpp"
#include <cstdio>
#include <cstring>
#include <cerrno>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

namespace re6 {
// Separate diagnostic schema: this file never claims to contain game samples.
enum class CapturePhase {Attach,Metadata,Maps,Scan,Start,Read,Write,Stop};
struct CaptureStatus {
 const char* target="unknown";
 std::uint64_t target_title=0,actual_title=0,pid=0;
 BuildId expected_build{},actual_build{};
 CapturePhase phase=CapturePhase::Start;
 std::uint32_t ipc_result=0;
 RecorderState state=RecorderState::Idle;
 RecorderError error=RecorderError::None;
 std::uint64_t records=0,dropped=0,tick=0;
 unsigned errors=0;
 const char* detail="";const char* filename="";
};
inline constexpr std::size_t kCaptureStatusBytes=2048;
inline const char* capture_phase_name(CapturePhase phase){
 switch(phase){case CapturePhase::Attach:return "attach";case CapturePhase::Metadata:return "metadata";case CapturePhase::Maps:return "maps";case CapturePhase::Scan:return "scan";case CapturePhase::Start:return "start";case CapturePhase::Read:return "read";case CapturePhase::Write:return "write";case CapturePhase::Stop:return "stop";}return "unknown";
}
inline const char* capture_state_name(RecorderState state){
 switch(state){case RecorderState::Idle:return "idle";case RecorderState::Recording:return "recording";case RecorderState::Stopping:return "stopping";case RecorderState::Saved:return "saved";case RecorderState::Failed:return "failed";}return "unknown";
}
inline const char* capture_error_name(RecorderError error){
 switch(error){case RecorderError::None:return "none";case RecorderError::Attach:return "attach";case RecorderError::Open:return "open";case RecorderError::Write:return "write";case RecorderError::Finalize:return "finalize";case RecorderError::Read:return "read";case RecorderError::Changed:return "changed";case RecorderError::Stalled:return "stalled";}return "unknown";
}
inline std::size_t format_capture_status(const CaptureStatus& s,char* out,std::size_t size){
 if(!out||!size)return 0;
 char expected[65]{},actual[65]{};constexpr char hex[]="0123456789ABCDEF";
 for(unsigned i=0;i<32;++i){expected[2*i]=hex[s.expected_build[i]>>4];expected[2*i+1]=hex[s.expected_build[i]&15];actual[2*i]=hex[s.actual_build[i]>>4];actual[2*i+1]=hex[s.actual_build[i]&15];}
 std::size_t used=0;bool ok=true;
 auto append=[&](char c){if(used+1>=size){ok=false;return;}out[used++]=c;};
 auto text=[&](const char* value){for(;*value;++value)append(*value);};
 auto quoted=[&](const char* value,std::size_t limit){append('"');if(value)for(std::size_t i=0;i<limit&&value[i];++i){const auto c=static_cast<unsigned char>(value[i]);if(c=='"')append('"');append(c<32||c==127?' ':static_cast<char>(c));}append('"');};
 text("schema,target,target_title,actual_title,pid,expected_build,actual_build,phase,ipc_result,recorder_state,recorder_error,records,dropped,errors,tick,detail,filename\n");
 text("re6-capture-status-v1,");quoted(s.target,16);append(',');
 char numbers[512];const int count=std::snprintf(numbers,sizeof(numbers),"%016llX,%016llX,%llu,%s,%s,%s,%08X,%s,%s,%llu,%llu,%u,%llu,",
  (unsigned long long)s.target_title,(unsigned long long)s.actual_title,(unsigned long long)s.pid,expected,actual,capture_phase_name(s.phase),s.ipc_result,capture_state_name(s.state),capture_error_name(s.error),(unsigned long long)s.records,(unsigned long long)s.dropped,s.errors,(unsigned long long)s.tick);
 if(count<0||static_cast<std::size_t>(count)>=sizeof(numbers))return 0;
 text(numbers);quoted(s.detail,256);append(',');quoted(s.filename,128);append('\n');out[used]=0;return ok?used:0;
}
inline bool replace_capture_status(const char* temporary,const char* path){
#if defined(_WIN32)
 return MoveFileExA(temporary,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#elif defined(__SWITCH__)
 // Horizon rename cannot replace an existing file. Keep one complete recovery
 // copy while swapping; a failed second rename restores the previous status.
 if(std::rename(temporary,path)==0)return true;
 if(errno!=EEXIST)return false;
 char backup[512];const int count=std::snprintf(backup,sizeof(backup),"%s.previous",path);
 if(count<0||static_cast<std::size_t>(count)>=sizeof(backup))return false;
 if(std::remove(backup)!=0&&errno!=ENOENT)return false;
 if(std::rename(path,backup)!=0)return false;
 if(std::rename(temporary,path)==0)return true;
 std::rename(backup,path);return false;
#else
 return std::rename(temporary,path)==0;
#endif
}
inline bool write_capture_status(const char* path,const CaptureStatus& status){
 char bytes[kCaptureStatusBytes],temporary[512];const auto size=format_capture_status(status,bytes,sizeof(bytes));
 if(!path||!size)return false;
 const int count=std::snprintf(temporary,sizeof(temporary),"%s.tmp",path);
 if(count<0||static_cast<std::size_t>(count)>=sizeof(temporary))return false;
 FILE* file=std::fopen(temporary,"wb");if(!file)return false;
 bool ok=std::fwrite(bytes,1,size,file)==size;
 if(std::fflush(file))ok=false;
#if defined(_WIN32)
 if(_commit(_fileno(file)))ok=false;
#else
 if(fsync(fileno(file)))ok=false;
#endif
 if(std::fclose(file))ok=false;
 if(ok)ok=replace_capture_status(temporary,path);
 if(!ok)std::remove(temporary);
 return ok;
}
}
