#pragma once
#include "re5/settings.hpp"
#include "re4/input.hpp"
#include <atomic>
#include <cmath>
#include <cstring>
namespace re5 {
// Atomic payload words avoid data races even if a reader observes a publication
// in progress. The settings codec validates the coherent snapshot before use.
class SettingsSnapshot {
 std::atomic<unsigned> sequence_{0};std::atomic<std::uint64_t> words_[kSettingsBytes/8]{};
public:
 bool publish(const Settings& settings){
  std::uint8_t bytes[kSettingsBytes];if(!encode_settings(settings,bytes,sizeof(bytes)))return false;
  const auto before=sequence_.fetch_add(1,std::memory_order_acq_rel);
  for(unsigned i=0;i<kSettingsBytes/8;++i){std::uint64_t word;std::memcpy(&word,bytes+i*8,8);words_[i].store(word,std::memory_order_relaxed);}
  sequence_.store(before+2,std::memory_order_release);return true;
 }
 bool read(Settings& settings)const{
  const auto before=sequence_.load(std::memory_order_acquire);if(!before||(before&1))return false;
  std::uint8_t bytes[kSettingsBytes];for(unsigned i=0;i<kSettingsBytes/8;++i){auto word=words_[i].load(std::memory_order_relaxed);std::memcpy(bytes+i*8,&word,8);}
  std::atomic_thread_fence(std::memory_order_acquire);
  return sequence_.load(std::memory_order_acquire)==before&&decode_settings(bytes,sizeof(bytes),settings);
 }
};
struct PadState {std::uint64_t sample=0,buttons=0;std::int32_t lx=0,ly=0,rx=0,ry=0;std::uint32_t attributes=0,reserved=0;};
static_assert(sizeof(PadState)==40);
struct Axes {float lx=0,ly=0,rx=0,ry=0;};
inline Axes shape(const PadState& p,const re4::Settings& s){
 auto axis=[&](int value,float deadzone){return re4::shape_axis(float(value)/32767.f,deadzone,1,s.stick_curve);};
 return {axis(p.lx,s.left_deadzone),axis(p.ly,s.left_deadzone),axis(p.rx,s.right_deadzone),axis(p.ry,s.right_deadzone)};
}
inline void polar(float x,float y,float& angle,float& magnitude){
 magnitude=std::fmin(std::hypot(x,y),1.f);angle=magnitude>0?-std::atan2(x,y)*57.295779513f:0;
}
inline bool aiming_action(unsigned action){return action>=0x1B&&action<=0x27;}
inline bool active_aim(unsigned action){return (action>=0x1B&&action<=0x1E)||(action>=0x22&&action<=0x27);}
}
