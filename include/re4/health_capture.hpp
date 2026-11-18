#pragma once
#include "re4/health_report.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
namespace re4 {
inline constexpr std::size_t kHealthCaptureOffset=864;
inline constexpr unsigned kHealthCaptureBytes=160,kHealthCaptureWords=32;
inline constexpr char kHealthCaptureMarker[16]="RE4CTL_HP1";
enum class HealthCaptureField:unsigned {
 ViewFrameLo,ViewFrameHi,TargetKeyLo,TargetKeyHi,Health,WeaponBefore,WeaponAfter,
 ComputedUnit,PublishedUnit,FailedGate,Flags,Type,Subtype,BodyStatus,Phase,Mode,Rank,
 WeaponIndex,VptrRva,UpdateRva,TableRva,Coefficient,ReadMask,LinkMask,PoolCount,PoolStride,
 Outcome,ControlFrameLo,ControlFrameHi,GuardRva,GuardExpected,GuardObserved
};
// Pointer-free, retained actual producer observation. ReadMask has one bit per
// observed scalar field. ViewFrame and ControlFrame are distinct frame domains.
struct HealthCaptureWire {
 char marker[16];std::uint32_t version,bytes,sequence,valid;
 std::uint32_t words[kHealthCaptureWords];
};
constexpr HealthCaptureWire initial_health_capture(){
 HealthCaptureWire v{};for(unsigned i=0;i<16;++i)v.marker[i]=kHealthCaptureMarker[i];
 v.version=1;v.bytes=kHealthCaptureBytes;return v;
}
inline bool health_capture_header_valid(const HealthCaptureWire& v){
 return !std::memcmp(v.marker,kHealthCaptureMarker,16)&&v.version==1&&v.bytes==kHealthCaptureBytes&&v.valid<=1;
}
struct HealthCaptureSample {
 std::uint32_t words[kHealthCaptureWords]{};
 constexpr HealthCaptureSample(){
  words[unsigned(HealthCaptureField::VptrRva)]=UINT32_MAX;
  words[unsigned(HealthCaptureField::UpdateRva)]=UINT32_MAX;
  words[unsigned(HealthCaptureField::TableRva)]=UINT32_MAX;
  words[unsigned(HealthCaptureField::GuardRva)]=UINT32_MAX;
 }
 std::uint32_t get(HealthCaptureField field)const{return words[unsigned(field)];}
 void set(HealthCaptureField field,std::uint32_t value){
  words[unsigned(field)]=value;
  if(field!=HealthCaptureField::ReadMask)words[unsigned(HealthCaptureField::ReadMask)]|=std::uint32_t(1)<<unsigned(field);
 }
};
// Single bounded try-lock: a competing producer drops this observation rather
// than blocking gameplay. The retained record is never cleared by aim ending.
inline bool publish_health_capture(volatile HealthCaptureWire& wire,std::atomic_flag& writer,const HealthCaptureSample& sample){
 if(writer.test_and_set(std::memory_order_acquire))return false;
 const auto before=wire.sequence;
 wire.sequence=before+1;
 std::atomic_thread_fence(std::memory_order_release);
 for(unsigned i=0;i<kHealthCaptureWords;++i)wire.words[i]=sample.words[i];
 wire.valid=1;
 std::atomic_thread_fence(std::memory_order_release);
 wire.sequence=before+2;
 writer.clear(std::memory_order_release);return true;
}
static_assert(sizeof(HealthCaptureWire)==kHealthCaptureBytes&&offsetof(HealthCaptureWire,sequence)==24&&offsetof(HealthCaptureWire,words)==32);
static_assert(std::is_standard_layout_v<HealthCaptureWire>&&std::is_trivially_copyable_v<HealthCaptureWire>);
}
