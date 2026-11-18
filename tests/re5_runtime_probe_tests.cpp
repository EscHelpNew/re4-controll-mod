#if !__has_include("re5/runtime_probe.hpp")
#include <cstdio>
int main(){std::fputs("FAIL: bounded RE5 runtime probe is unavailable\n",stderr);return 1;}
#else
#include "re5/runtime_probe.hpp"
#include "lib.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <type_traits>
#include "re5/runtime_input.hpp"
namespace exl::util {
enum class ModuleIndex { Sdk };
inline game_test::Main sdk_module;inline bool sdk_present=true;
inline bool HasModule(ModuleIndex){return sdk_present;}
inline const auto& GetModuleInfo(ModuleIndex){return sdk_module;}
}
namespace {
void test_Re5AimAxes(exl::hook::InlineFloatCtx*){}
void test_Re5AimDelta(exl::hook::InlineFloatCtx*){}
void test_Re5ScopeZoom(exl::hook::InlineFloatCtx*){}
void test_Re5Scheme8(exl::hook::InlineFloatCtx*){}
void test_Re5Scheme9(exl::hook::InlineFloatCtx*){}
void test_Re5Scheme10(exl::hook::InlineFloatCtx*){}
void test_Re5Scheme11(exl::hook::InlineFloatCtx*){}
void test_Re5MovementProposal(exl::hook::InlineFloatCtx*){}
void test_Re5MovementPose(exl::hook::InlineFloatCtx*){}
void test_Re5CameraPoseRestore(exl::hook::InlineFloatCtx*){}
void test_Re5CameraDistance(exl::hook::InlineFloatCtx*){}
void test_Re5CameraAxes(exl::hook::InlineFloatCtx*){}
void test_Re5CameraReturn(exl::hook::InlineFloatCtx*){}
void test_Re5CameraRig(exl::hook::InlineFloatCtx*){}
void test_Re5ActorLook(exl::hook::InlineFloatCtx*){}
void test_Re5FullPad(re5::PadState*,const std::uint32_t&){}
void test_Re5HandheldPad(re5::PadState*,const std::uint32_t&){}
void test_Re5DualPad(re5::PadState*,const std::uint32_t&){}
unsigned input_original_calls=0,camera_original_calls=0;
void test_Re5LocalInput(void*,void*){++input_original_calls;}
void test_Re5Polar(void*){}
void test_Re5NativeGyro(float*,void*){}
void test_Re5RayCapture(exl::hook::InlineFloatCtx*){}
void test_Re5CockpitDraw(void*,void*){}
void test_Re5CameraUpdate(void*){++camera_original_calls;}
void test_Re5SamplerCreate(void*,const std::uint8_t*){}
void test_Re5ProjectionFar(exl::hook::InlineFloatCtx*){}
void test_Re5CullingFar(exl::hook::InlineFloatCtx*){}
void test_Re5PostAA(exl::hook::InlineFloatCtx*){}
void test_Re5Shadow0(exl::hook::InlineFloatCtx*){}
void test_Re5Shadow1(exl::hook::InlineFloatCtx*){}
}
#include "../source/re5/module.cpp"
#define check(c) do{if(!(c)){std::fprintf(stderr,"RE5 runtime probe line %d: %s\n",__LINE__,#c);std::exit(1);}}while(false)
namespace {
using Counter=re5::RuntimeCounter;
std::uint64_t count(const volatile re5::RuntimeProbe& probe,Counter counter){
 return __atomic_load_n(&probe.counters[static_cast<unsigned>(counter)],__ATOMIC_ACQUIRE);
}
void request(volatile re5::RuntimeProbe& probe,std::uint32_t sequence){__atomic_store_n(&probe.request_sequence,sequence,__ATOMIC_RELEASE);}
void empty_counts(const volatile re5::RuntimeProbe& probe){for(unsigned i=0;i<16;++i)check(__atomic_load_n(&probe.counters[i],__ATOMIC_ACQUIRE)==0);}
template<class T>void put(std::vector<std::uint8_t>& bytes,std::size_t offset,T value){std::memcpy(bytes.data()+offset,&value,sizeof(value));}
void lease_behavior(){
 static_assert(std::is_trivial_v<re5::RuntimeProbe> && std::is_standard_layout_v<re5::RuntimeProbe>);
 volatile re5::RuntimeProbe probe{};re5::initialize_runtime_probe(probe);std::atomic_flag guard=ATOMIC_FLAG_INIT;re5::RuntimeProbeRecorder recorder{probe,guard};
 check(sizeof(probe)==192 && alignof(re5::RuntimeProbe)==8);
 check(probe.abi==1 && probe.size==192 && std::memcmp(const_cast<const char*>(probe.marker),"RE5CTL_RUN1\0\0\0\0\0",16)==0);
 check(!recorder.active()&&!recorder.needs_poll()&&probe.request_sequence==0&&probe.applied_sequence==0&&probe.reserved==0);
 recorder.record(Counter::PadCalls);recorder.record(Counter::InputCalls,re5::RuntimeProbeThread::Input,11,7);
 recorder.poll(100);empty_counts(probe);check(probe.input_thread==0&&probe.camera_thread==0&&probe.pinned_thread==0);
 request(probe,7);check(recorder.needs_poll());recorder.poll(100);
 check(recorder.active()&&probe.applied_sequence==7);empty_counts(probe);
 for(unsigned i=0;i<16;++i)recorder.record(static_cast<Counter>(i));
 recorder.record(Counter::InputCalls,re5::RuntimeProbeThread::Input,11,7);
 recorder.record(Counter::FreshCalls,re5::RuntimeProbeThread::Camera,22,7);
 check(count(probe,Counter::PadCalls)==1&&count(probe,Counter::InputCalls)==2&&count(probe,Counter::FreshCalls)==2);
 check(count(probe,Counter::MovementPrepared)==1&&count(probe,Counter::MovementRejected)==1);
 check(probe.input_thread==11&&probe.camera_thread==22&&probe.pinned_thread==7);
 recorder.poll(19200100);check(recorder.active()&&count(probe,Counter::InputCalls)==2);
 request(probe,0);recorder.poll(38400099);check(recorder.active());
 recorder.poll(38400100);check(!recorder.active()&&probe.active==0&&probe.applied_sequence==7);
 recorder.record(Counter::InputCalls,re5::RuntimeProbeThread::Input,99,99);check(count(probe,Counter::InputCalls)==2&&probe.input_thread==11);
 request(probe,7);recorder.poll(40000000);check(!recorder.active()&&count(probe,Counter::PadCalls)==1);
 request(probe,8);recorder.poll(40000100);check(recorder.active()&&probe.applied_sequence==8);
 empty_counts(probe);check(probe.input_thread==0&&probe.camera_thread==0&&probe.pinned_thread==0);
 recorder.poll(40000099);check(!recorder.active()); // A backwards clock fails closed.
 request(probe,9);recorder.poll(50000000);
 guard.test_and_set();recorder.record(Counter::PadCalls);check(count(probe,Counter::PadCalls)==0);
 request(probe,10);recorder.poll(50000100);check(probe.applied_sequence==9);guard.clear();recorder.poll(50000100);check(probe.applied_sequence==10);
 std::thread writers[4];for(auto& writer:writers)writer=std::thread([&]{for(unsigned i=0;i<20000;++i)recorder.record(Counter::PadCalls);});
 for(auto& writer:writers)writer.join();
 const auto recorded=count(probe,Counter::PadCalls);check(recorded>0&&recorded<=80000);recorder.poll(88400100);check(!recorder.active());
 recorder.record(Counter::PadCalls);check(count(probe,Counter::PadCalls)==recorded);
}
void runtime_classification(){
 std::vector<std::uint8_t> main(0x26AA000),actor_bytes(0x4200),wrapper_bytes(0x1000),players(0x100),pads(0x9000),camera_bytes(0x3000),manager(0x500);
 for(auto* region:{&main,&actor_bytes,&wrapper_bytes,&players,&pads,&camera_bytes,&manager})
  game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(region->data()),region->size(),3});
 main_base=reinterpret_cast<std::uintptr_t>(main.data());re5::profile::selected_build=re5::profile::Build::Latest;
 const auto actor=reinterpret_cast<std::uintptr_t>(actor_bytes.data()),wrapper=reinterpret_cast<std::uintptr_t>(wrapper_bytes.data()),camera=reinterpret_cast<std::uintptr_t>(camera_bytes.data());
 put(main,0x1CF8CC8,reinterpret_cast<std::uintptr_t>(players.data()));put(main,0x26A9288,reinterpret_cast<std::uintptr_t>(pads.data()));
 put(main,0x26A8550,reinterpret_cast<std::uintptr_t>(manager.data()));put(players,0x30,actor);
 put(actor_bytes,0x3F2C,std::uint8_t(0));put(actor_bytes,0x3F49,std::uint8_t(0));put(actor_bytes,0x3EE8,wrapper);put(wrapper_bytes,0x48,actor);
 put(pads,0x8C98,std::uint32_t(3));put(pads,0x8C94,std::uint32_t(3));
 put(pads,3*0xEE0+0x670,std::uint32_t(3));put(pads,3*0xEE0+0x674,std::uint32_t(1));
 put(actor_bytes,0x1A80,camera);put(camera_bytes,0,main_base+re5::profile::offset<0x1AD2510>());
 put(camera_bytes,0x130,actor);put(camera_bytes,0x198,camera+0xDB0);put(camera_bytes,0xDA0,camera+0xDB0);put(manager,0x48,camera);put(camera_bytes,0x68,10.f);
 current_settings={};current_settings.players[0].gyro_enabled=false;current_settings.players[0].free_camera=false;current_settings.players[0].third_person_distance=1.5f;
 check(controls_snapshot.publish(current_settings));game_active.store(true);settings_loaded.store(true);game_test::tick=100000000;
 re5::initialize_runtime_probe(runtime_probe);runtime_thread.store(1);
 re5::PadState pad;pad.attributes=1;
 observed_pad(&pad,3,1);Re5LocalInput::Callback(reinterpret_cast<void*>(wrapper+0x350),reinterpret_cast<void*>(actor));
 empty_counts(runtime_probe); // Normal callbacks do not record before an explicit request.
 request(runtime_probe,1);const auto frames=game_frames;Re5CameraUpdate::Callback(nullptr);
 check(runtime_probe.applied_sequence==1&&runtime_probe.active==1&&game_frames==frames+1&&camera_original_calls==1);
 auto input=[&]{Re5LocalInput::Callback(reinterpret_cast<void*>(wrapper+0x350),reinterpret_cast<void*>(actor));};
 game_test::thread_id=0;input();check(count(runtime_probe,Counter::InputRejectedThread)==1);game_test::thread_id=1;
 runtime_thread.store(1);settings_loaded.store(false);input();check(count(runtime_probe,Counter::InputRejectedRoute)==1);settings_loaded.store(true);
 game_test::tick+=1920001;input();check(count(runtime_probe,Counter::InputRejectedPad)==1);
 observed_pad(&pad,3,1);input();check(count(runtime_probe,Counter::InputAccepted)==1);
 input_lock.test_and_set();input();input_lock.clear();check(count(runtime_probe,Counter::InputRejectedThread)==2);
 check(count(runtime_probe,Counter::InputCalls)==5&&input_original_calls==6);
 check(runtime_probe.input_thread==1&&runtime_probe.pinned_thread==1);
 ControlFrame frame;game_test::thread_id=2;check(fresh_frame(actor,frame));check(count(runtime_probe,Counter::FreshRejectedThread)==0);
 game_test::thread_id=1;check(!fresh_frame(0,frame));check(count(runtime_probe,Counter::FreshRejectedRoute)==1);
 game_test::tick+=1920001;check(!fresh_frame(actor,frame));check(count(runtime_probe,Counter::FreshRejectedPad)==1);
 observed_pad(&pad,3,1);check(!fresh_frame(actor,frame));check(count(runtime_probe,Counter::FreshRejectedAge)==1);
 auto latest=player_runtime[0].frame;latest.tick=game_test::tick;latest.player.sticks.style=2;check(published_frames[0].publish(latest));
 check(!fresh_frame(actor,frame));check(count(runtime_probe,Counter::FreshRejectedRoute)==2);latest.player.sticks.style=1;check(published_frames[0].publish(latest));
 check(fresh_frame(actor,frame));check(count(runtime_probe,Counter::FreshCalls)==6);
 check(runtime_probe.camera_thread==1&&runtime_probe.pinned_thread==1);
 exl::hook::InlineFloatCtx context{};re4::set_saved_x(&context,19,camera);
 put(manager,0x48,std::uintptr_t(0));Re5CameraRig::Callback(&context);check(count(runtime_probe,Counter::CameraRejectedOwnership)==1);
 put(manager,0x48,camera);Re5CameraRig::Callback(&context);check(count(runtime_probe,Counter::CameraApplied)==0);
 const auto component=camera+0xDB0;put(camera_bytes,0xDB0+0x130,actor);put(camera_bytes,0xDB0+0x168,10.f);
 re4::set_saved_x(&context,19,component);Re5CameraDistance::Callback(&context);check(count(runtime_probe,Counter::CameraApplied)==1);
 float eye=0;std::memcpy(&eye,camera_bytes.data()+0xDB0+0x168,4);check(eye==15.f);
 std::memcpy(&eye,camera_bytes.data()+0x68,4);check(eye==10.f); // Persistent native pose remains untouched.
 const auto calls=count(runtime_probe,Counter::FreshCalls);game_test::tick=138400000;Re5CameraUpdate::Callback(nullptr);
 check(runtime_probe.active==0);fresh_frame(actor,frame);check(count(runtime_probe,Counter::FreshCalls)==calls);
 check(game_frames==frames+2&&camera_original_calls==2);
}
}
int main(){lease_behavior();runtime_classification();std::puts("RE5 runtime probe: default off, bounded lease, atomic counts and actual runtime rejection paths PASS");}
#endif
