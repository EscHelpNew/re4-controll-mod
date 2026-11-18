#pragma once
#include <atomic>
#include <type_traits>
namespace re5 {
// One coherent committed frame per local player. Both operations try once:
// native callbacks must never spin, including when a backend reenters a hook.
template<class Frame> struct FrameSnapshot {
 static_assert(std::is_trivially_copyable_v<Frame>,"Frame publication requires a trivially copyable payload");
 std::atomic_flag lock=ATOMIC_FLAG_INIT;
private:
 Frame frame_{};bool present_=false;
public:
 bool publish(const Frame& frame){
  if(lock.test_and_set(std::memory_order_acquire))return false;
  frame_=frame;present_=true;lock.clear(std::memory_order_release);return true;
 }
 bool read(Frame& frame){
  if(lock.test_and_set(std::memory_order_acquire))return false;
  const bool present=present_;if(present)frame=frame_;
  lock.clear(std::memory_order_release);return present;
 }
};
}
