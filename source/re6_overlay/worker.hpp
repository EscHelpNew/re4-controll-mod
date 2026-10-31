#pragma once
#include <switch.h>
#include <atomic>
#include <cstdio>
#include "re6/recorder.hpp"
#include "re6/capture_status.hpp"

namespace telemetry {
enum class Target {RE6,RE4};
struct Snapshot {
 re6::RecorderState state=re6::RecorderState::Idle;
 re6::RecorderError error=re6::RecorderError::None;
 std::uint64_t records=0,dropped=0;unsigned errors=0;Result result=0;
 bool busy=false,diagnostic_failed=false;char detail[96]{},filename[128]{};
};
class Worker {
 Thread thread_{};bool created_=false;Mutex mutex_=0;Snapshot snapshot_{};
 std::atomic<unsigned> command_{0};std::atomic<bool> exit_{false};
 Service service_{};re6::Recorder recorder_{};FILE* file_=nullptr;
 char part_[256]{},final_[256]{},file_buffer_[16384]{};
 std::uint64_t last_flush_=0;Result last_result_=0;unsigned errors_=0;
 re6::CaptureStatus diagnostic_{};bool diagnostic_requested_=false;
 static void entry(void*);void run();void begin_recording(Target);
 bool connect(Target,std::uint64_t&,re6::BuildId&,std::uint64_t&);
 void publish(const char* detail=nullptr,bool busy=false,bool force_diagnostic=false);
 static bool read(void*,std::uint64_t,void*,std::size_t);
 static bool begin(void*,const re6::Header&);
 static bool write(void*,const char*,std::size_t);
 static bool finish(void*,bool);
 static bool cancelled(void*);
public:
 bool initialize();void close();
 bool start(Target);void stop();Snapshot snapshot();
};
// Private mount aliases the same SD location as
// sdmc:/switch/re4-controls/telemetry, avoiding Tesla helper unmounts.
bool mount_storage();void close_storage();
inline constexpr char kDirectory[]="r6log:/switch/re4-controls/telemetry";
inline constexpr char kLanguagePath[]="r6log:/config/re4-telemetry-language.txt";
}
