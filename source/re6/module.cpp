// RE6 observer plus opt-in graphics. Gameplay/controller values are only read.
// Exact build/instruction checks precede every trampoline installation.
#include "lib.hpp"
#include "nn/fs.hpp"
#include "re6/profile.hpp"
#include "re6/shared.hpp"
#include "re6/graphics.hpp"
#include <atomic>
#include <cstring>
#include <limits>

namespace {
struct Re6NpadState {
 std::uint64_t sample,buttons;
 std::int32_t lx,ly,rx,ry;
 std::uint32_t attributes,reserved;
};
static_assert(sizeof(Re6NpadState)==40&&offsetof(Re6NpadState,attributes)==0x20);
using PadFunction=void (*)(Re6NpadState*,const std::uint32_t&);

// The collector discovers storage.header, not the beginning of ProducerRing
// (which also contains private state). No SDK/file I/O occurs in callbacks.
__attribute__((used,section(".data.re6observer"))) re6::ProducerRing<1024> observer_ring;
std::uintptr_t observer_main=0;
std::uint64_t observer_frame=0;
std::atomic<bool> observer_active{false};
std::atomic_flag observer_capture=ATOMIC_FLAG_INIT;
__attribute__((used,section(".data.re6controls"))) volatile re6::SharedControl graphics_control{};
re6::Settings graphics_settings;
std::uint32_t graphics_sequence=0;
std::atomic_flag graphics_settings_guard=ATOMIC_FLAG_INIT;
std::atomic_flag aa_render_guard=ATOMIC_FLAG_INIT;
std::atomic<bool> graphics_loaded{false};
std::atomic<unsigned> graphics_request{0},graphics_aa_request{0},graphics_latched{255},sampler_calls{0},sampler_overrides{0};
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
 if(!observer_active.load(std::memory_order_acquire)||graphics_settings_guard.test_and_set(std::memory_order_acquire))return;
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
 graphics_aa_request.store(unsigned(graphics_settings.anti_aliasing),std::memory_order_release);
 graphics_loaded.store(true,std::memory_order_release);
 // Bootstrap is the only module-side publisher. Expose the block only after
 // that publisher has stopped; the overlay is then the sole settings writer.
 __atomic_store_n(&graphics_control.status,1,__ATOMIC_RELEASE);
}

struct InputPoll {Re6NpadState state{};std::uint64_t tick=0;std::uint32_t id=0,style=0;};
static_assert(sizeof(InputPoll)==56);
struct InputSlot {
 std::atomic_flag writer=ATOMIC_FLAG_INIT;
 std::atomic<std::uint64_t> sequence{0},words[7]{};
 void publish(const InputPoll& input){
  if(writer.test_and_set(std::memory_order_acquire))return;
  const auto before=sequence.fetch_add(1,std::memory_order_acq_rel);
  std::uint64_t payload[7];std::memcpy(payload,&input,sizeof(payload));
  for(unsigned i=0;i<7;++i)words[i].store(payload[i],std::memory_order_relaxed);
  sequence.store(before+2,std::memory_order_release);writer.clear(std::memory_order_release);
 }
 bool read(InputPoll& output)const{
  const auto before=sequence.load(std::memory_order_acquire);if(!before||(before&1))return false;
  std::uint64_t payload[7];for(unsigned i=0;i<7;++i)payload[i]=words[i].load(std::memory_order_relaxed);
  std::atomic_thread_fence(std::memory_order_acquire);
  if(sequence.load(std::memory_order_acquire)!=before)return false;
  std::memcpy(&output,payload,sizeof(output));return true;
 }
} input_slots[6];
void observed_input(const Re6NpadState* state,std::uint32_t id,unsigned style){
 if(!observer_active.load(std::memory_order_acquire)||!state||(id!=0&&id!=0x20))return;
 const unsigned index=(id==0x20?3:0)+(style==1?0:style==2?1:2);
 input_slots[index].publish({*state,svcGetSystemTick(),id,style});
}
bool latest_input(std::uint64_t tick,InputPoll& result){
 bool found=false;
 for(const auto& slot:input_slots){
  InputPoll input;
  if(!slot.read(input)||!(input.state.attributes&1)||!input.tick||tick<input.tick||tick-input.tick>re6::profile::input_max_age)continue;
  if(!found||input.tick>result.tick){result=input;found=true;}
 }
 return found;
}
bool readable(std::uintptr_t address,std::size_t size){
 if(!address||address>UINTPTR_MAX-size)return false;
 const auto end=address+size;
 while(address<end){
  MemoryInfo region{};std::uint32_t page=0;
  if(R_FAILED(svcQueryMemory(&region,&page,address))||!(region.perm&1)||address<region.addr||address-region.addr>=region.size)return false;
  const auto remaining=region.size-(address-region.addr);const auto needed=end-address;
  address+=remaining<needed?remaining:needed;
 }
 return true;
}
template<class T> bool read_at(std::uintptr_t address,T& out){
 if(!readable(address,sizeof(T)))return false;
 std::memcpy(&out,reinterpret_cast<const void*>(address),sizeof(T));return true;
}
// RE6 native matrices use row vectors. The journal uses canonical row-major
// storage for column-vector multiplication, matching RE4's 3x4 view convention.
template<std::size_t N> void canonical(float (&out)[N],const float (&native)[16]){
 static_assert(N==12||N==16);
 for(unsigned row=0;row<N/4;++row)for(unsigned col=0;col<4;++col)out[row*4+col]=native[col*4+row];
}
void capture_view(void* pointer){
 if(!observer_active.load(std::memory_order_acquire)||!pointer||observer_capture.test_and_set(std::memory_order_acquire))return;
 struct Unlock {~Unlock(){observer_capture.clear(std::memory_order_release);}} unlock;
 const auto manager=reinterpret_cast<std::uintptr_t>(pointer);
 std::uintptr_t singleton=0;
 if(!read_at(observer_main+re6::profile::camera_singleton,singleton)||singleton!=manager)return;
 re6::Record record{};record.frame=++observer_frame;record.tick=svcGetSystemTick();
 InputPoll input;
 if(latest_input(record.tick,input)){
  record.validity|=re6::InputValid;record.sample_id=input.state.sample;record.buttons=input.state.buttons;
  record.input_tick=input.tick;record.controller_id=input.id;record.controller_style=input.style;
  record.sticks[0]=input.state.lx;record.sticks[1]=input.state.ly;record.sticks[2]=input.state.rx;record.sticks[3]=input.state.ry;
 }
 // The protocol observes the primary viewport. screenPlNo is authoritative;
 // neither actor slot zero nor actor+5A38 is guessed to mean the human player.
 std::uintptr_t camera=0;std::uint8_t enabled=0;
 if(read_at(manager+0x48,camera)&&camera&&read_at(manager+0x60,enabled)&&enabled){
  record.camera=camera;float matrix[16];
  if(read_at(manager+0xe0,matrix)){canonical(record.view,matrix);record.validity|=re6::CameraValid;}
  if(read_at(manager+0x120,matrix)){canonical(record.projection,matrix);record.validity|=re6::ProjectionValid;}
  std::uint32_t slot=8;std::uintptr_t players=0,actor=0,actor_camera=0;
  std::uint8_t active=0;std::uint32_t flags=0;
  if(read_at(manager+0xd7c,slot)&&slot<8&&read_at(observer_main+re6::profile::player_singleton,players)&&players
    &&read_at(players+0x30+slot*8,actor)&&actor&&read_at(actor+0xc,active)&&active
    &&read_at(actor+0x14,flags)&&((flags&7u)-1u)<2u&&read_at(actor+0x3a28,actor_camera)){
   bool associated=actor_camera==camera;
   if(!associated){
    // Native1603A50 selects this view override only when both bytes are set.
    std::uintptr_t override=0;std::uint8_t global=0,primary=0;
    associated=read_at(observer_main+re6::profile::view_override_singleton,override)&&override
     &&read_at(override+0x91,global)&&global&&read_at(override+0x5660,primary)&&primary;
   }
   if(associated&&read_at(actor+0xd0,matrix)){
    record.actor=actor;canonical(record.player,matrix);record.validity|=re6::PlayerValid;
   }
  }
 }
 observer_ring.capture(record);
}
HOOK_DEFINE_TRAMPOLINE(Re6FullPad){static void Callback(Re6NpadState* state,const std::uint32_t& id){Orig(state,id);observed_input(state,id,1);}};
HOOK_DEFINE_TRAMPOLINE(Re6HandheldPad){static void Callback(Re6NpadState* state,const std::uint32_t& id){Orig(state,id);observed_input(state,id,2);}};
HOOK_DEFINE_TRAMPOLINE(Re6DualPad){static void Callback(Re6NpadState* state,const std::uint32_t& id){Orig(state,id);observed_input(state,id,4);}};
HOOK_DEFINE_TRAMPOLINE(Re6CameraUpdate){static void Callback(void* manager){
 poll_graphics_settings();Orig(manager);capture_view(manager);
 graphics_control.frames=observer_frame;
 graphics_control.graphics_filter_latched=graphics_latched.load(std::memory_order_acquire);
 graphics_control.sampler_calls=sampler_calls.load(std::memory_order_relaxed);
 graphics_control.sampler_overrides=sampler_overrides.load(std::memory_order_relaxed);
}};
HOOK_DEFINE_TRAMPOLINE(Re6SamplerCreate){static void Callback(void* object,const std::uint8_t* descriptor){
 if(!observer_active.load(std::memory_order_acquire)){Orig(object,descriptor);return;}
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
HOOK_DEFINE_TRAMPOLINE(Re6AaRender){static void Callback(void* context,void* scene,std::uint32_t pass,const char* label){
 if(!observer_active.load(std::memory_order_acquire)){Orig(context,scene,pass,label);return;}
 poll_graphics_settings();
 const auto requested=graphics_aa_request.load(std::memory_order_acquire);
 if(!requested||requested>4||aa_render_guard.test_and_set(std::memory_order_acquire)){
  Orig(context,scene,pass,label);return;
 }
 struct Unlock{~Unlock(){aa_render_guard.clear(std::memory_order_release);}} unlock;
 const auto address=reinterpret_cast<std::uintptr_t>(scene);
 std::uintptr_t renderer=0,renderer_type=0,primary=0,type=0;
 std::uint32_t native=0;
 MemoryInfo region{};std::uint32_t page=0;
 const bool supported=address<=UINTPTR_MAX-0x5E8&&read_at(observer_main+0x2A5FD90,renderer)&&renderer&&renderer<=UINTPTR_MAX-0x1099770
   &&read_at(renderer,renderer_type)&&(renderer_type==observer_main+0x28C05C8||renderer_type==observer_main+0x25B66A8)
   &&read_at(renderer+0x1099768,primary)&&primary==address
   &&read_at(address,type)&&type==observer_main+0x28B1378
   &&read_at(address+0x5E4,native)&&native<=3
   &&R_SUCCEEDED(svcQueryMemory(&region,&page,address+0x5E4))&&(region.perm&3)==3
   &&address+0x5E4>=region.addr&&address+0x5E4-region.addr<=region.size&&region.size-(address+0x5E4-region.addr)>=4;
 if(!supported||(!native&&requested>1)){Orig(context,scene,pass,label);return;}
 const auto applied=requested-1;
 auto* field=reinterpret_cast<std::uint32_t*>(address+0x5E4);
 *field=applied;
 Orig(context,scene,pass,label); // Native mode selection is consumed synchronously into draw commands.
 std::uintptr_t after_type=0;std::uint32_t after=0;
 if(read_at(address,after_type)&&after_type==type&&read_at(address+0x5E4,after)&&after==applied
   &&R_SUCCEEDED(svcQueryMemory(&region,&page,address+0x5E4))&&(region.perm&3)==3
   &&address+0x5E4>=region.addr&&address+0x5E4-region.addr<=region.size&&region.size-(address+0x5E4-region.addr)>=4)*field=native;
}};
}
extern "C" void exl_main(void*,void*){
 u64 title=0;
 if(observer_active.load(std::memory_order_acquire)||R_FAILED(svcGetInfo(&title,InfoType_ProgramId,CUR_PROCESS_HANDLE,0))||title!=re6::profile::title_id)return;
 const auto& main=exl::util::GetMainModuleInfo();
 if(!re6::profile::matches(reinterpret_cast<const std::uint8_t*>(main.m_Total.m_Start),main.m_Total.m_Size)
   ||!exl::util::HasModule(exl::util::ModuleIndex::Sdk))return;
 const auto& sdk=exl::util::GetModuleInfo(exl::util::ModuleIndex::Sdk);
 if(!re6::profile::sdk_matches(reinterpret_cast<const std::uint8_t*>(sdk.m_Total.m_Start),sdk.m_Total.m_Size))return;
 observer_main=main.m_Total.m_Start;
 const auto initial=re6::initial_shared_control();
 auto* shared_bytes=reinterpret_cast<volatile std::uint8_t*>(&graphics_control);
 for(unsigned i=0;i<sizeof(initial);++i)shared_bytes[i]=reinterpret_cast<const std::uint8_t*>(&initial)[i];
 graphics_control.capabilities=re6::kCapabilityFiltering;
 const auto session=svcGetSystemTick()^reinterpret_cast<std::uintptr_t>(&observer_ring.storage);
 if(!observer_ring.initialize(session?session:1,re6::profile::build_id))return;
 exl::hook::Initialize();
 Re6FullPad::InstallAtFuncPtr(reinterpret_cast<PadFunction>(sdk.m_Total.m_Start+re6::profile::sdk_full));
 Re6HandheldPad::InstallAtFuncPtr(reinterpret_cast<PadFunction>(sdk.m_Total.m_Start+re6::profile::sdk_handheld));
 Re6DualPad::InstallAtFuncPtr(reinterpret_cast<PadFunction>(sdk.m_Total.m_Start+re6::profile::sdk_dual));
 Re6CameraUpdate::InstallAtOffset(re6::profile::camera_update);
 Re6SamplerCreate::InstallAtOffset(0x1673C20);
 Re6AaRender::InstallAtOffset(0x1234840);
 observer_active.store(true,std::memory_order_release);
}
extern "C" NORETURN void exl_exception_entry(){EXL_ABORT("RE6 observer exception");}
