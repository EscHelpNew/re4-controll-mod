#pragma once
#include "re5/camera_pose.hpp"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>
namespace re5 {
constexpr char kCameraProbeMarker[16]="RE5CTL_CAM1";
constexpr std::uint32_t kCameraProbeAbi=1,kCameraProbeOffset=688,kCameraProbeReasonCount=32;
enum class CameraProbeStage : std::uint32_t {Component=1,PostDesired=2};
enum class CameraProbeReason : std::uint32_t {
 None,NoActor,NoFreshFrame,FirstPerson,LegacyFree,NotMode,BadDistance,BadOwner,NativeRead,SourceInvalid,
 AnchorBusy,ReleaseMismatch,HeadingMissing,ActorBlocked,BodyIdentity,EpochMismatch,WorldRead,WorldChanged,
 WorldNotCommitted,HeadRead,AnchorHeight,MathRejected,SamePose,NotWritable,Applied,PostDesired,
 PostDesiredRead,CameraBusy,Reserved28,Reserved29,Reserved30,Reserved31
};
constexpr const char* kCameraProbeReasonNames[kCameraProbeReasonCount]={
 "none","no_actor","no_fresh_frame","first_person","legacy_free","not_mode","bad_distance","bad_owner","native_read","source_invalid",
 "anchor_busy","release_mismatch","heading_missing","actor_blocked","body_identity","epoch_mismatch","world_read","world_changed",
 "world_not_committed","head_read","anchor_height","math_rejected","same_pose","not_writable","applied","post_desired",
 "post_desired_read","camera_busy","reserved28","reserved29","reserved30","reserved31"};
enum CameraProbeFlags : std::uint32_t {
 CameraFrameValid=1u,CameraHeadingValid=2u,CameraBodyActive=4u,CameraBodyAnchor=8u,CameraArmValid=16u,
 CameraScope=32u,CameraModern=64u,CameraFirstPerson=128u,CameraInputAnchored=256u
};
enum CameraProbeAvailable : std::uint32_t {
 CameraHasFrame=1u,CameraHasNativeEyeTarget=2u,CameraHasNativeUp=4u,CameraHasResultEyeTarget=8u,CameraHasResultUp=16u,
 CameraHasBody=32u,CameraHasExpectedWorld=64u,CameraHasWorld=128u,CameraHasWorldAgain=256u,CameraHasArm=512u,
 CameraHasAnchor=1024u,CameraHasFov=2048u
};
// route packs controller in low16 and native style in high16. All floats are
// unscaled game values/radians. A record's request tag survives lease expiry.
struct alignas(8) CameraProbeInput {
 std::uint64_t call,tick,actor,command,frame,generation,look_epoch,look_release,validation_tick,pad_sample;
 std::uint32_t request_sequence,action,flags,route;
 std::array<std::int32_t,4> raw;
 std::array<float,4> shaped,filtered;
 float pitch,native_heading,look_yaw,look_pitch;
};
struct alignas(8) CameraProbeDecision {
 std::uint64_t call,tick,actor,command,camera,component,input_frame,input_tick,generation,look_epoch,look_release,body_frame,body_epoch,body_generation;
 std::uint32_t request_sequence,stage,reason,action,flags,available;
 float input_pitch,body_pitch,heading,native_heading,look_integral,body_integral,height,distance,forward_x,forward_z;
 std::array<float,16> expected_world,world,world_again;
 CameraPose native,result,arm;
 float fov;
 std::array<float,3> anchor;
 std::array<std::uint32_t,5> reserved;
};
struct alignas(8) CameraProbePlayer {
 CameraProbeInput last_input,last_nonzero_ry;
 CameraProbeDecision last_component,last_success,last_failure,last_post_desired;
 std::uint64_t reasons[kCameraProbeReasonCount];
};
struct alignas(8) CameraProbe {
 char marker[16];
 std::uint32_t abi,size,sequence,request_sequence;
 std::uint64_t calls,unrouted_calls;
 CameraProbePlayer players[2];
 // Failed native route resolution must never be attributed to player0.
 CameraProbeDecision last_unrouted;
};
constexpr std::uint32_t kCameraProbeBytes=5808;
static_assert(sizeof(CameraProbeInput)==160&&sizeof(CameraProbeDecision)==512&&sizeof(CameraProbePlayer)==2624);
static_assert(sizeof(CameraProbe)==kCameraProbeBytes&&alignof(CameraProbe)==8&&kCameraProbeBytes<=8192);
static_assert(offsetof(CameraProbe,abi)==16&&offsetof(CameraProbe,size)==20&&offsetof(CameraProbe,sequence)==24&&offsetof(CameraProbe,request_sequence)==28);
static_assert(offsetof(CameraProbe,players)==48&&offsetof(CameraProbe,last_unrouted)==5296);
static_assert(offsetof(CameraProbeDecision,expected_world)==176&&offsetof(CameraProbeDecision,native)==368&&offsetof(CameraProbeDecision,fov)==476&&offsetof(CameraProbeDecision,anchor)==480);
static_assert(std::is_standard_layout_v<CameraProbe>&&std::is_trivially_copyable_v<CameraProbe>);
inline void initialize_camera_probe(volatile CameraProbe& probe){
 auto* bytes=reinterpret_cast<volatile std::uint8_t*>(&probe);
 for(unsigned i=0;i<sizeof(probe);++i)bytes[i]=0;
 __atomic_store_n(&probe.abi,kCameraProbeAbi,__ATOMIC_RELAXED);__atomic_store_n(&probe.size,kCameraProbeBytes,__ATOMIC_RELAXED);
 for(unsigned i=0;i<sizeof(kCameraProbeMarker);++i)__atomic_store_n(&probe.marker[i],kCameraProbeMarker[i],__ATOMIC_RELEASE);
}
// The caller's existing Monitor lease authorizes records. Disabled calls do
// not obtain locks or inspect payloads. One native try-lock serializes all
// writers; the odd/even sequence brackets the entire exported block. Readers
// copy only an equal, even sequence before/after and never write this block.
class CameraProbeRecorder {
 volatile CameraProbe& probe_;std::atomic_flag& lock_;
 void* lease_context_;bool (*lease_valid_)(void*,std::uint32_t);
 template<class T>static void copy(volatile T& target,const T& source){
  auto* dst=reinterpret_cast<volatile std::uint8_t*>(&target);const auto* src=reinterpret_cast<const std::uint8_t*>(&source);
  for(unsigned i=0;i<sizeof(T);++i)dst[i]=src[i];
 }
 bool begin(bool active,std::uint32_t request){
  if(!active||!request||lock_.test_and_set(std::memory_order_acquire))return false;
  if(lease_valid_&&!lease_valid_(lease_context_,request)){lock_.clear(std::memory_order_release);return false;}
  const auto sequence=__atomic_load_n(&probe_.sequence,__ATOMIC_RELAXED);
  __atomic_store_n(&probe_.sequence,sequence+1,__ATOMIC_RELEASE);
  // Release on the odd store orders prior work only. This fence also orders
  // odd visibility before every following payload write on AArch64.
  __atomic_thread_fence(__ATOMIC_RELEASE);
  if(__atomic_load_n(&probe_.request_sequence,__ATOMIC_RELAXED)!=request){
   for(auto& player:probe_.players)for(auto& count:player.reasons)__atomic_store_n(&count,0,__ATOMIC_RELAXED);
   __atomic_store_n(&probe_.request_sequence,request,__ATOMIC_RELAXED);
  }
  return true;
 }
 void end(){const auto sequence=__atomic_load_n(&probe_.sequence,__ATOMIC_RELAXED);__atomic_store_n(&probe_.sequence,sequence+1,__ATOMIC_RELEASE);lock_.clear(std::memory_order_release);}
public:
 constexpr CameraProbeRecorder(volatile CameraProbe& probe,std::atomic_flag& lock,void* context=nullptr,bool (*valid)(void*,std::uint32_t)=nullptr):probe_(probe),lock_(lock),lease_context_(context),lease_valid_(valid){}
 bool input(bool active,std::uint32_t request,unsigned player,CameraProbeInput& value){
  if(player>=2||!begin(active,request))return false;
  value.call=__atomic_load_n(&probe_.calls,__ATOMIC_RELAXED)+1;value.request_sequence=request;
  copy(probe_.players[player].last_input,value);if(value.raw[3])copy(probe_.players[player].last_nonzero_ry,value);
  __atomic_store_n(&probe_.calls,value.call,__ATOMIC_RELAXED);end();return true;
 }
 bool decision(bool active,std::uint32_t request,unsigned player,CameraProbeDecision& value){
  if(!begin(active,request))return false;
  value.call=__atomic_load_n(&probe_.calls,__ATOMIC_RELAXED)+1;value.request_sequence=request;
  if(player>=2){copy(probe_.last_unrouted,value);__atomic_fetch_add(&probe_.unrouted_calls,std::uint64_t(1),__ATOMIC_RELAXED);}
  else{
   auto& slot=probe_.players[player];const auto reason=value.reason;
   if(reason<kCameraProbeReasonCount)__atomic_fetch_add(&slot.reasons[reason],std::uint64_t(1),__ATOMIC_RELAXED);
   if(value.stage==std::uint32_t(CameraProbeStage::PostDesired))copy(slot.last_post_desired,value);
   else{copy(slot.last_component,value);if(reason==std::uint32_t(CameraProbeReason::Applied))copy(slot.last_success,value);else copy(slot.last_failure,value);}
  }
  __atomic_store_n(&probe_.calls,value.call,__ATOMIC_RELAXED);end();return true;
 }
};
}
