// RE6 native observer. Gameplay/controller values are only read. The four
// guarded trampoline patches are the only writes outside module-owned memory.
#include "lib.hpp"
#include "re6/profile.hpp"
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
HOOK_DEFINE_TRAMPOLINE(Re6CameraUpdate){static void Callback(void* manager){Orig(manager);capture_view(manager);}};
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
 const auto session=svcGetSystemTick()^reinterpret_cast<std::uintptr_t>(&observer_ring.storage);
 if(!observer_ring.initialize(session?session:1,re6::profile::build_id))return;
 exl::hook::Initialize();
 Re6FullPad::InstallAtFuncPtr(reinterpret_cast<PadFunction>(sdk.m_Total.m_Start+re6::profile::sdk_full));
 Re6HandheldPad::InstallAtFuncPtr(reinterpret_cast<PadFunction>(sdk.m_Total.m_Start+re6::profile::sdk_handheld));
 Re6DualPad::InstallAtFuncPtr(reinterpret_cast<PadFunction>(sdk.m_Total.m_Start+re6::profile::sdk_dual));
 Re6CameraUpdate::InstallAtOffset(re6::profile::camera_update);
 observer_active.store(true,std::memory_order_release);
}
extern "C" NORETURN void exl_exception_entry(){EXL_ABORT("RE6 observer exception");}
