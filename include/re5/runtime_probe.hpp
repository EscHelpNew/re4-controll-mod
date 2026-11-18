#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
namespace re5 {
constexpr char kRuntimeProbeMarker[16]="RE5CTL_RUN1";
constexpr std::uint32_t kRuntimeProbeAbi=1,kRuntimeProbeBytes=192;
constexpr std::uint64_t kRuntimeProbeTicks=38400000; // Two seconds at 19.2 MHz.
enum class RuntimeCounter : unsigned {
 PadCalls,InputCalls,InputAccepted,InputRejectedThread,InputRejectedRoute,InputRejectedPad,
 FreshCalls,FreshRejectedThread,FreshRejectedRoute,FreshRejectedPad,FreshRejectedAge,
 CameraRejectedOwnership,CameraApplied,MovementPrepared,MovementRejected,BodyHeadingApplied
};
constexpr unsigned kRuntimeCounterCount=16;
constexpr const char* kRuntimeCounterNames[kRuntimeCounterCount]={
 "pad_calls","input_calls","input_accepted","input_rejected_thread","input_rejected_route","input_rejected_pad",
 "fresh_calls","fresh_rejected_thread","fresh_rejected_route","fresh_rejected_pad","fresh_rejected_age",
 "camera_rejected_ownership","camera_applied","movement_prepared","movement_rejected","body_heading_applied"
};
// Only request_sequence is written by the overlay. All other scalar fields are
// native-owned atomics; the payload remains a plain, fixed-size shared ABI.
struct alignas(8) RuntimeProbe {
 char marker[16];
 std::uint32_t abi,size,request_sequence,applied_sequence,active,reserved;
 std::uint64_t counters[kRuntimeCounterCount];
 std::uint64_t input_thread,camera_thread,pinned_thread;
};
static_assert(alignof(RuntimeProbe)==8&&sizeof(RuntimeProbe)==192);
static_assert(offsetof(RuntimeProbe,abi)==16&&offsetof(RuntimeProbe,size)==20);
static_assert(offsetof(RuntimeProbe,request_sequence)==24&&offsetof(RuntimeProbe,applied_sequence)==28);
static_assert(offsetof(RuntimeProbe,active)==32&&offsetof(RuntimeProbe,reserved)==36);
static_assert(offsetof(RuntimeProbe,counters)==40&&offsetof(RuntimeProbe,input_thread)==168);
static_assert(offsetof(RuntimeProbe,camera_thread)==176&&offsetof(RuntimeProbe,pinned_thread)==184);
inline void initialize_runtime_probe(volatile RuntimeProbe& probe){
 __atomic_store_n(&probe.active,0,__ATOMIC_RELEASE);
 __atomic_store_n(&probe.abi,kRuntimeProbeAbi,__ATOMIC_RELAXED);
 __atomic_store_n(&probe.size,kRuntimeProbeBytes,__ATOMIC_RELAXED);
 __atomic_store_n(&probe.request_sequence,0,__ATOMIC_RELAXED);
 __atomic_store_n(&probe.applied_sequence,0,__ATOMIC_RELAXED);
 __atomic_store_n(&probe.reserved,0,__ATOMIC_RELAXED);
 for(auto& counter:probe.counters)__atomic_store_n(&counter,0,__ATOMIC_RELAXED);
 __atomic_store_n(&probe.input_thread,0,__ATOMIC_RELAXED);
 __atomic_store_n(&probe.camera_thread,0,__ATOMIC_RELAXED);
 __atomic_store_n(&probe.pinned_thread,0,__ATOMIC_RELAXED);
 for(unsigned i=0;i<sizeof(kRuntimeProbeMarker);++i)__atomic_store_n(&probe.marker[i],kRuntimeProbeMarker[i],__ATOMIC_RELEASE);
}
enum class RuntimeProbeThread {None,Input,Camera};
// The native-owned lock serializes records against capture resets. Recording
// performs a single active load while disabled, without obtaining ticks/IDs.
class RuntimeProbeRecorder {
 volatile RuntimeProbe& probe_;
 std::atomic_flag& lock_;
 std::uint64_t started_=0;
 struct Guard {
  std::atomic_flag& lock;
  bool acquired=false;
  explicit Guard(std::atomic_flag& value):lock(value),acquired(!lock.test_and_set(std::memory_order_acquire)){}
  ~Guard(){if(acquired)lock.clear(std::memory_order_release);}
 };
public:
 constexpr RuntimeProbeRecorder(volatile RuntimeProbe& probe,std::atomic_flag& lock):probe_(probe),lock_(lock){}
 bool active()const{return __atomic_load_n(&probe_.active,__ATOMIC_ACQUIRE)!=0;}
 bool needs_poll()const{
  const auto request=__atomic_load_n(&probe_.request_sequence,__ATOMIC_ACQUIRE);
  return active()||(request&&request!=__atomic_load_n(&probe_.applied_sequence,__ATOMIC_ACQUIRE));
 }
 // Only the existing CameraUpdate calls poll. Repeated or zero requests leave
 // the current lease unchanged; a new nonzero request resets and reopens it.
 void poll(std::uint64_t now){
  Guard guard(lock_);if(!guard.acquired)return;
  const auto request=__atomic_load_n(&probe_.request_sequence,__ATOMIC_ACQUIRE);
  if(request&&request!=__atomic_load_n(&probe_.applied_sequence,__ATOMIC_ACQUIRE)){
   __atomic_store_n(&probe_.active,0,__ATOMIC_RELEASE);
   for(auto& counter:probe_.counters)__atomic_store_n(&counter,0,__ATOMIC_RELAXED);
   __atomic_store_n(&probe_.input_thread,0,__ATOMIC_RELAXED);
   __atomic_store_n(&probe_.camera_thread,0,__ATOMIC_RELAXED);
   __atomic_store_n(&probe_.pinned_thread,0,__ATOMIC_RELAXED);
   started_=now;
   __atomic_store_n(&probe_.active,1,__ATOMIC_RELEASE);
   __atomic_store_n(&probe_.applied_sequence,request,__ATOMIC_RELEASE);
  }else if(active()&&(now<started_||now-started_>=kRuntimeProbeTicks)){
   __atomic_store_n(&probe_.active,0,__ATOMIC_RELEASE);
  }
 }
 void record(RuntimeCounter counter,RuntimeProbeThread source=RuntimeProbeThread::None,
             std::uint64_t thread=0,std::uint64_t pinned=0){
  if(!active())return;
  Guard guard(lock_);if(!guard.acquired)return;if(!active())return;
  const auto index=static_cast<unsigned>(counter);if(index>=kRuntimeCounterCount)return;
  __atomic_fetch_add(&probe_.counters[index],std::uint64_t(1),__ATOMIC_RELAXED);
  if(source==RuntimeProbeThread::Input)__atomic_store_n(&probe_.input_thread,thread,__ATOMIC_RELAXED);
  else if(source==RuntimeProbeThread::Camera)__atomic_store_n(&probe_.camera_thread,thread,__ATOMIC_RELAXED);
  if(source!=RuntimeProbeThread::None)__atomic_store_n(&probe_.pinned_thread,pinned,__ATOMIC_RELAXED);
 }
};
}
