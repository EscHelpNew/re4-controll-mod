// RE5 latest and installed legacy adapters. Exact image/instruction checks precede hook installation.
#include "lib.hpp"
#include "nn/fs.hpp"
#include "re5/profile.hpp"
#include "re5/shared.hpp"
#include "re5/runtime_probe.hpp"
#include "re5/camera_probe.hpp"
#include "re5/camera_pose.hpp"
#include "re5/native_graphics.hpp"
#include "re5/runtime_input.hpp"
#include "re5/frame_snapshot.hpp"
#include "re5/player_input.hpp"
#include "re5/gait.hpp"
#include "re4/sensor.hpp"
#include "re4/unified.hpp"
#include "re5/reticle.hpp"
#include "re4/hook_context.hpp"
#include <atomic>
#include <cstring>
#include <limits>
#include <new>
namespace {
std::uintptr_t main_base=0,sdk_base=0;
std::atomic<bool> game_active{false},settings_loaded{false};
std::atomic_flag settings_guard=ATOMIC_FLAG_INIT;
__attribute__((used,section(".data.re5controls"))) volatile re5::SharedControl shared_control{};
__attribute__((used,section(".data.re5identity"))) volatile re5::ImageIdentity image_identity{};
__attribute__((used,section(".data.re5runtime"),aligned(8))) volatile re5::RuntimeProbe runtime_probe{};
__attribute__((used,section(".data.re5camera"),aligned(8))) volatile re5::CameraProbe camera_probe{};
std::atomic_flag camera_probe_guard=ATOMIC_FLAG_INIT;
std::atomic_flag runtime_probe_guard=ATOMIC_FLAG_INIT;
re5::RuntimeProbeRecorder runtime_probe_recorder{runtime_probe,runtime_probe_guard};
bool camera_probe_lease(void*,std::uint32_t request){return runtime_probe_recorder.active()&&request==__atomic_load_n(&runtime_probe.applied_sequence,__ATOMIC_ACQUIRE);}
re5::CameraProbeRecorder camera_probe_recorder{camera_probe,camera_probe_guard,nullptr,camera_probe_lease};
re5::Settings current_settings;
re5::SettingsSnapshot controls_snapshot;
std::uint32_t settings_sequence=0;
bool settings_mounted=false;
unsigned settings_attempts=0;
std::uint64_t settings_retry=0,game_frames=0;
std::atomic<std::uint64_t> graphics_word{0x3f80000000000000ULL};
std::atomic<unsigned> filter_latched{255},sampler_calls{0},sampler_overrides{0};
std::uint64_t pack_graphics(const re5::GraphicsOptions& value){
 std::uint32_t distance=0;std::memcpy(&distance,&value.draw_distance,4);
 return std::uint64_t(distance)<<32|std::uint64_t(value.aa)<<16|std::uint64_t(value.shadows)<<8|std::uint64_t(value.filter);
}
re5::GraphicsOptions graphics(){
 const auto word=graphics_word.load(std::memory_order_acquire);re5::GraphicsOptions out;
 out.filter=static_cast<re4::TextureFilter>(word&255);out.shadows=static_cast<re4::ShadowMode>((word>>8)&255);
 out.aa=static_cast<re5::AntiAliasing>((word>>16)&255);const auto distance=std::uint32_t(word>>32);std::memcpy(&out.draw_distance,&distance,4);return out;
}
bool readable(void*,std::uintptr_t address,std::size_t size){
 if(!address||!size||address>UINTPTR_MAX-size)return false;
 const auto end=address+size;
 while(address<end){
  MemoryInfo region{};std::uint32_t page=0;
  if(R_FAILED(svcQueryMemory(&region,&page,address))||!(region.perm&1)||address<region.addr||address-region.addr>=region.size)return false;
  const auto remaining=region.size-(address-region.addr),needed=end-address;
  address+=remaining<needed?remaining:needed;
 }
 return true;
}
bool read_memory(void* context,std::uintptr_t address,void* out,std::size_t size){
 if(!out||!readable(context,address,size))return false;
 std::memcpy(out,reinterpret_cast<const void*>(address),size);return true;
}
const re5::MemoryReader memory{nullptr,read_memory,readable};
bool read_settings_file(const char* path){
 nn::fs::FileHandle file{};if(R_FAILED(nn::fs::OpenFile(&file,path,1)))return false;
 long size=0;std::uint8_t bytes[re5::kSettingsBytes];re5::Settings candidate;
 const bool valid=R_SUCCEEDED(nn::fs::GetFileSize(&size,file))&&size==long(sizeof(bytes))
  &&R_SUCCEEDED(nn::fs::ReadFile(file,0,bytes,sizeof(bytes)))&&re5::decode_settings(bytes,sizeof(bytes),candidate);
 nn::fs::CloseFile(file);if(valid)current_settings=candidate;return valid;
}
void poll_settings(){
 if(!game_active.load(std::memory_order_acquire)||settings_guard.test_and_set(std::memory_order_acquire))return;
 struct Unlock{~Unlock(){settings_guard.clear(std::memory_order_release);}} unlock;
 if(re5::consume_settings(shared_control,current_settings,settings_sequence))shared_control.settings_source=4;
 else if(!settings_loaded.load(std::memory_order_acquire)){
  const auto now=svcGetSystemTick();if(now<settings_retry)return;
  settings_retry=now+19200000;++settings_attempts;
  if(!settings_mounted)settings_mounted=R_SUCCEEDED(nn::fs::MountSdCardForDebug("r5ctl"));
  unsigned source=0;
  if(settings_mounted){
   if(read_settings_file("r5ctl:/config/re4-controls/re5-settings.bin"))source=1;
   else if(read_settings_file("r5ctl:/config/re4-controls/re5-settings.bin.previous"))source=2;
  }
  if(!source&&!settings_mounted&&settings_attempts<8)return;
  shared_control.settings_source=source?source:3;
  if(!re5::publish_settings(shared_control,current_settings,settings_sequence))return;
  shared_control.applied_sequence=settings_sequence;
 }
 controls_snapshot.publish(current_settings);
 graphics_word.store(pack_graphics(current_settings.graphics),std::memory_order_release);
 settings_loaded.store(true,std::memory_order_release);
 __atomic_store_n(&shared_control.status,1,__ATOMIC_RELEASE);
}
float saved_s(const void* context,unsigned index){float value=0;std::memcpy(&value,static_cast<const std::uint8_t*>(context)+index*16,4);return value;}
void set_saved_s(void* context,unsigned index,float value){std::memcpy(static_cast<std::uint8_t*>(context)+index*16,&value,4);}
#include "runtime.inc"
#include "reticle.inc"
HOOK_DEFINE_TRAMPOLINE(Re5CameraUpdate){static void Callback(void* manager){
 poll_settings();
 if(game_active.load(std::memory_order_acquire)&&runtime_probe_recorder.needs_poll())runtime_probe_recorder.poll(svcGetSystemTick());
 Orig(manager);
 shared_control.frames=++game_frames;
 shared_control.graphics_filter_latched=filter_latched.load(std::memory_order_acquire);
 shared_control.sampler_calls=sampler_calls.load(std::memory_order_relaxed);
 shared_control.sampler_overrides=sampler_overrides.load(std::memory_order_relaxed);
}};
HOOK_DEFINE_TRAMPOLINE(Re5SamplerCreate){static void Callback(void* object,const std::uint8_t* descriptor){
 if(!game_active.load(std::memory_order_acquire)){Orig(object,descriptor);return;}
 poll_settings();sampler_calls.fetch_add(1,std::memory_order_relaxed);
 auto filter=filter_latched.load(std::memory_order_acquire);
 if(filter==255&&settings_loaded.load(std::memory_order_acquire)){
  unsigned empty=255;filter_latched.compare_exchange_strong(empty,unsigned(graphics().filter),std::memory_order_acq_rel);
  filter=filter_latched.load(std::memory_order_acquire);
 }
 std::uint8_t local[0x38];
 if(descriptor&&filter!=255&&re5::filter_sampler_descriptor(descriptor,local,sizeof(local),re4::TextureFilter(filter))){
  sampler_overrides.fetch_add(1,std::memory_order_relaxed);Orig(object,local);
 }else Orig(object,descriptor);
 shared_control.graphics_filter_latched=filter_latched.load(std::memory_order_acquire);
 shared_control.sampler_calls=sampler_calls.load(std::memory_order_relaxed);
 shared_control.sampler_overrides=sampler_overrides.load(std::memory_order_relaxed);
}};
HOOK_DEFINE_INLINE(Re5ProjectionFar){static void Callback(exl::hook::InlineFloatCtx* context){
 if(!context||!game_active.load(std::memory_order_acquire))return;
 float value=0;if(re5::scale_projection_far(memory,main_base,re4::saved_x(context,0),saved_s(context,0),saved_s(context,1),graphics().draw_distance,value))set_saved_s(context,1,value);
}};
HOOK_DEFINE_INLINE(Re5CullingFar){static void Callback(exl::hook::InlineFloatCtx* context){
 if(!context||!game_active.load(std::memory_order_acquire))return;
 float value=0;if(re5::scale_culling_far(memory,main_base,re4::saved_x(context,8),re4::saved_x(context,19),saved_s(context,2),graphics().draw_distance,value))set_saved_s(context,2,value);
}};
HOOK_DEFINE_INLINE(Re5PostAA){static void Callback(exl::hook::InlineFloatCtx* context){
 if(!context||!game_active.load(std::memory_order_acquire))return;
 unsigned value=0;if(re5::override_post_aa(memory,main_base,re4::saved_x(context,20),unsigned(re4::saved_x(context,23)),graphics().aa,value))re4::set_saved_x(context,23,value);
}};
void shadow_hook(exl::hook::InlineFloatCtx* context,unsigned unit){
 if(context&&game_active.load(std::memory_order_acquire)&&re5::disable_stage_shadow(memory,main_base,re4::saved_x(context,0),re4::saved_x(context,8),unsigned(re4::saved_x(context,19)),unit,graphics().shadows))re4::set_saved_x(context,1,0);
}
HOOK_DEFINE_INLINE(Re5Shadow0){static void Callback(exl::hook::InlineFloatCtx* context){shadow_hook(context,0);}};
HOOK_DEFINE_INLINE(Re5Shadow1){static void Callback(exl::hook::InlineFloatCtx* context){shadow_hook(context,1);}};
}
extern "C" void exl_main(void*,void*){
 u64 title=0;
 if(game_active.load(std::memory_order_acquire)||R_FAILED(svcGetInfo(&title,InfoType_ProgramId,CUR_PROCESS_HANDLE,0))||title!=re5::profile::title_id)return;
 const auto& main=exl::util::GetMainModuleInfo();
 const auto build=re5::profile::detect(reinterpret_cast<const std::uint8_t*>(main.m_Total.m_Start),main.m_Total.m_Size);
 if(build==re5::profile::Build::None||!exl::util::HasModule(exl::util::ModuleIndex::Sdk))return;
 const auto& sdk=exl::util::GetModuleInfo(exl::util::ModuleIndex::Sdk);
 if(!re5::profile::sdk_matches(reinterpret_cast<const std::uint8_t*>(sdk.m_Total.m_Start),sdk.m_Total.m_Size))return;
 re5::profile::selected_build=build;
 main_base=main.m_Total.m_Start;sdk_base=sdk.m_Total.m_Start;
 const auto initial=re5::initial_shared_control();auto* out=reinterpret_cast<volatile std::uint8_t*>(&shared_control);
 for(unsigned i=0;i<sizeof(initial);++i)out[i]=reinterpret_cast<const std::uint8_t*>(&initial)[i];
 shared_control.capabilities=re5::kCapabilityFiltering|re5::kCapabilityAntiAliasing|re5::kCapabilityDistance|re5::kCapabilityShadows|re5::kCapabilityGyro|re5::kCapabilitySticks|re5::kCapabilityMovement|re5::kCapabilityCamera|re5::kCapabilityReticle;
 exl::hook::Initialize();
 install_controls();
 install_reticle();
 Re5CameraUpdate::InstallAtOffset(re5::profile::offset<re5::profile::camera_update>());Re5SamplerCreate::InstallAtOffset(re5::profile::offset<re5::profile::sampler_create>());
 Re5ProjectionFar::InstallAtOffset(re5::profile::offset<0x23AE94>());Re5CullingFar::InstallAtOffset(re5::profile::offset<0x10437C4>());Re5PostAA::InstallAtOffset(re5::profile::offset<0xCFEB38>());
 Re5Shadow0::InstallAtOffset(re5::profile::offset<0x1A09D0>());Re5Shadow1::InstallAtOffset(re5::profile::offset<0x1A09E8>());
 // Publish the immutable proof only after exact main/SDK guards and all hooks.
 re5::ImageIdentity identity{};std::memcpy(identity.marker,re5::kImageIdentityMarker,16);identity.abi=1;
 const auto verified_profile=re5::profile::view(build);
 std::memcpy(identity.build_id,verified_profile.id->data(),32);
 std::memcpy(identity.sdk_build_id,re5::profile::sdk_build_id.data(),32);
 identity.main_base=main_base;identity.main_size=main.m_Total.m_Size;identity.guard_count=verified_profile.count;
 auto* identity_bytes=reinterpret_cast<volatile std::uint8_t*>(&image_identity);
 for(unsigned i=0;i<sizeof(identity);++i)identity_bytes[i]=reinterpret_cast<const std::uint8_t*>(&identity)[i];
 __atomic_store_n(&image_identity.status,1,__ATOMIC_RELEASE);
 re5::initialize_runtime_probe(runtime_probe);
 re5::initialize_camera_probe(camera_probe);
 game_active.store(true,std::memory_order_release);
}
extern "C" NORETURN void exl_exception_entry(){EXL_ABORT("RE5 controls exception");}
