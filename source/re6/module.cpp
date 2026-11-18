// RE6 opt-in graphics adapter. Exact native guards keep unsupported builds disabled.
// Exact build/instruction checks precede every trampoline installation.
#include "lib.hpp"
#include "nn/fs.hpp"
#include "re6/profile.hpp"
#include "re6/shared.hpp"
#include "re6/graphics.hpp"
#include <atomic>
#include <cstring>

namespace {
std::atomic<std::uint64_t> graphics_frames{0};
std::atomic<bool> graphics_active{false};
__attribute__((used,section(".data.re6controls"))) volatile re6::SharedControl graphics_control{};
re6::Settings graphics_settings;
std::uint32_t graphics_sequence=0;
std::atomic_flag graphics_settings_guard=ATOMIC_FLAG_INIT;
std::atomic<bool> graphics_loaded{false};
std::atomic<unsigned> graphics_request{0},graphics_latched{255},sampler_calls{0},sampler_overrides{0};
bool graphics_mounted=false;
unsigned graphics_attempts=0;
std::uint64_t graphics_retry=0;

bool read_graphics_file(const char* path){
 nn::fs::FileHandle file{};
 if(R_FAILED(nn::fs::OpenFile(&file,path,1)))return false;
 long size=0;std::uint8_t bytes[re6::kSettingsBytes];re6::Settings candidate;
 const bool valid=R_SUCCEEDED(nn::fs::GetFileSize(&size,file))&&size==long(sizeof(bytes))
   &&R_SUCCEEDED(nn::fs::ReadFile(file,0,bytes,sizeof(bytes)))&&re6::decode_settings(bytes,sizeof(bytes),candidate);
 nn::fs::CloseFile(file);
 if(valid)graphics_settings=candidate;
 return valid;
}
void poll_graphics_settings(){
 if(!graphics_active.load(std::memory_order_acquire)||graphics_settings_guard.test_and_set(std::memory_order_acquire))return;
 struct Unlock{~Unlock(){graphics_settings_guard.clear(std::memory_order_release);}} unlock;
 if(re6::consume_settings(graphics_control,graphics_settings,graphics_sequence)){
  graphics_control.settings_source=4;
 }else if(!graphics_loaded.load(std::memory_order_acquire)){
  const auto now=svcGetSystemTick();
  if(now<graphics_retry)return;
  graphics_retry=now+re6::kTickHz;++graphics_attempts;
  if(!graphics_mounted)graphics_mounted=R_SUCCEEDED(nn::fs::MountSdCardForDebug("r6ctl"));
  unsigned source=0;
  if(graphics_mounted){
   if(read_graphics_file("r6ctl:/config/re4-controls/re6-settings.bin"))source=1;
   else if(read_graphics_file("r6ctl:/config/re4-controls/re6-settings.bin.previous"))source=2;
  }
  if(!source&&!graphics_mounted&&graphics_attempts<8)return;
  graphics_control.settings_source=source?source:3;
  // Live publication always wins over a delayed filesystem attempt.
  // On a racing writer the seqlock stays odd and bootstrap retries next frame.
  if(!re6::publish_settings(graphics_control,graphics_settings,graphics_sequence))return;
  graphics_control.applied_sequence=graphics_sequence;
 }
 graphics_request.store(unsigned(graphics_settings.texture_filtering),std::memory_order_release);
 graphics_loaded.store(true,std::memory_order_release);
 // Bootstrap is the only module-side publisher. Expose the block only after
 // that publisher has stopped; the overlay is then the sole settings writer.
 __atomic_store_n(&graphics_control.status,1,__ATOMIC_RELEASE);
}

HOOK_DEFINE_TRAMPOLINE(Re6CameraUpdate){static void Callback(void* manager){
 if(!graphics_active.load(std::memory_order_acquire)){Orig(manager);return;}
 poll_graphics_settings();Orig(manager);
 graphics_control.frames=graphics_frames.fetch_add(1,std::memory_order_relaxed)+1;
 graphics_control.graphics_filter_latched=graphics_latched.load(std::memory_order_acquire);
 graphics_control.sampler_calls=sampler_calls.load(std::memory_order_relaxed);
 graphics_control.sampler_overrides=sampler_overrides.load(std::memory_order_relaxed);
}};
HOOK_DEFINE_TRAMPOLINE(Re6SamplerCreate){static void Callback(void* object,const std::uint8_t* descriptor){
 if(!graphics_active.load(std::memory_order_acquire)){Orig(object,descriptor);return;}
 poll_graphics_settings();sampler_calls.fetch_add(1,std::memory_order_relaxed);
 auto mode=graphics_latched.load(std::memory_order_acquire);
 if(mode==255&&graphics_loaded.load(std::memory_order_acquire)){
  unsigned pending=255;graphics_latched.compare_exchange_strong(pending,graphics_request.load(std::memory_order_acquire),std::memory_order_acq_rel);
  mode=graphics_latched.load(std::memory_order_acquire);
 }
 std::uint8_t local[0x38];
 if(descriptor&&mode!=255&&re6::filter_sampler_descriptor(descriptor,local,sizeof(local),re6::TextureFilter(mode))){
  sampler_overrides.fetch_add(1,std::memory_order_relaxed);Orig(object,local);
 }else Orig(object,descriptor);
}};

}
extern "C" void exl_main(void*,void*){
 u64 title=0;
 if(graphics_active.load(std::memory_order_acquire)||R_FAILED(svcGetInfo(&title,InfoType_ProgramId,CUR_PROCESS_HANDLE,0))||title!=re6::profile::title_id)return;
 const auto& main=exl::util::GetMainModuleInfo();
 if(!re6::profile::matches(reinterpret_cast<const std::uint8_t*>(main.m_Total.m_Start),main.m_Total.m_Size)
   ||!exl::util::HasModule(exl::util::ModuleIndex::Sdk))return;
 const auto& sdk=exl::util::GetModuleInfo(exl::util::ModuleIndex::Sdk);
 if(!re6::profile::sdk_matches(reinterpret_cast<const std::uint8_t*>(sdk.m_Total.m_Start),sdk.m_Total.m_Size))return;
 const auto initial=re6::initial_shared_control();
 auto* shared_bytes=reinterpret_cast<volatile std::uint8_t*>(&graphics_control);
 for(unsigned i=0;i<sizeof(initial);++i)shared_bytes[i]=reinterpret_cast<const std::uint8_t*>(&initial)[i];
 graphics_control.capabilities=re6::kCapabilityFiltering;
 exl::hook::Initialize();
 Re6CameraUpdate::InstallAtOffset(re6::profile::camera_update);
 Re6SamplerCreate::InstallAtOffset(0x1673C20);
 graphics_active.store(true,std::memory_order_release);
}
extern "C" NORETURN void exl_exception_entry(){EXL_ABORT("RE6 graphics exception");}
