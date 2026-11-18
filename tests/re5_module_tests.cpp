#include "lib.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <type_traits>
#include "re5/runtime_input.hpp"
#include "re4/profiles.hpp"
// Dispatch the actual installed inline callback at the native actor phase.
// A missing hook naturally leaves native body yaw unchanged in the regression.
namespace camera_test {
struct Inline {std::size_t offset;void (*callback)(exl::hook::InlineFloatCtx*);};
inline std::vector<Inline> hooks;
struct Follow {std::size_t offset;void (*callback)(void*,float);};
inline std::vector<Follow> follows;
template<class Hook,auto Original> struct Registered : TestHook<Original> {
 static void InstallAtOffset(std::size_t offset){TestHook<Original>::InstallAtOffset(offset);hooks.push_back({offset,[](exl::hook::InlineFloatCtx* context){Hook::Callback(context);}});}
};
inline void actor_phase(std::size_t offset,exl::hook::InlineFloatCtx* context){for(const auto& hook:hooks)if(hook.offset==offset)hook.callback(context);}
template<class Hook,auto Original> struct RegisteredTrampoline : TestHook<Original> {
 static void InstallAtOffset(std::size_t offset){TestHook<Original>::InstallAtOffset(offset);
  if constexpr(std::is_invocable_v<decltype(Original),void*,float>)follows.push_back({offset,[](void* owner,float dt){Hook::Callback(owner,dt);}});
 }
};
inline void follow_phase(std::size_t offset,void* owner,float dt,void (*original)(void*,float)){
 for(const auto& hook:follows)if(hook.offset==offset){hook.callback(owner,dt);return;}
 original(owner,dt);
}
}
#undef HOOK_DEFINE_INLINE
#define HOOK_DEFINE_INLINE(Name) struct Name : camera_test::Registered<Name,&test_##Name>
#undef HOOK_DEFINE_TRAMPOLINE
#define HOOK_DEFINE_TRAMPOLINE(Name) struct Name : camera_test::RegisteredTrampoline<Name,&test_##Name>
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
void test_Re5CameraFollow(void*,float);
[[maybe_unused]] void test_Re5CameraFollowArm(exl::hook::InlineFloatCtx*){}
[[maybe_unused]] void test_Re5CameraFollowWorld(exl::hook::InlineFloatCtx*){}
void test_Re5ActorLook(exl::hook::InlineFloatCtx*){}
struct PadStatePlaceholder;
void test_Re5FullPad(re5::PadState*,const std::uint32_t&){}
void test_Re5HandheldPad(re5::PadState*,const std::uint32_t&){}
void test_Re5DualPad(re5::PadState*,const std::uint32_t&){}
inline void (*local_input_observer)(void*,void*)=nullptr;
void test_Re5LocalInput(void* command,void* actor){if(local_input_observer)local_input_observer(command,actor);}
void test_Re5Polar(void*){}
void test_Re5NativeGyro(float*,void*){}
void test_Re5RayCapture(exl::hook::InlineFloatCtx*){}
void test_Re5CockpitDraw(void*,void*){}
void test_Re5CameraUpdate(void*);
void test_Re5SamplerCreate(void*,const std::uint8_t*);
void test_Re5ProjectionFar(exl::hook::InlineFloatCtx*){}
void test_Re5CullingFar(exl::hook::InlineFloatCtx*){}
void test_Re5PostAA(exl::hook::InlineFloatCtx*){}
void test_Re5Shadow0(exl::hook::InlineFloatCtx*){}
void test_Re5Shadow1(exl::hook::InlineFloatCtx*){}
}
namespace camera_test {
inline unsigned tick_calls=0,world_read_calls=0,world_read_mode=0;
inline std::uintptr_t observed_world=0;
inline void (*query_observer)(std::uintptr_t,Result&)=nullptr;
}
Result camera_probe_query_memory(MemoryInfo* info,std::uint32_t* page,std::uintptr_t address){auto result=svcQueryMemory(info,page,address);if(camera_test::query_observer)camera_test::query_observer(address,result);return result;}
u64 camera_probe_system_tick(){++camera_test::tick_calls;return svcGetSystemTick();}
#define svcQueryMemory camera_probe_query_memory
#define svcGetSystemTick camera_probe_system_tick
#include "../source/re5/module.cpp"
#undef svcGetSystemTick
#undef svcQueryMemory
#include "re5_runtime_gait_fixture.hpp"
#undef assert
#define assert(c) do{if(!(c)){std::fprintf(stderr,"RE5 module line %d: %s\n",__LINE__,#c);std::exit(1);}}while(false)
namespace {
std::array<std::uint8_t,56> sampler_seen{};unsigned camera_calls=0;
void test_Re5CameraUpdate(void*){++camera_calls;}
void test_Re5SamplerCreate(void*,const std::uint8_t* descriptor){if(descriptor)std::memcpy(sampler_seen.data(),descriptor,sampler_seen.size());}
void publish_test_control(){assert(published_frames[0].publish(player_runtime[0].frame));assert(published_frames[1].publish(player_runtime[1].frame));}
template<class T>void put(std::vector<std::uint8_t>& bytes,std::size_t offset,T value){std::memcpy(bytes.data()+offset,&value,sizeof(value));}
template<class Sites>void seed(std::vector<std::uint8_t>& bytes,std::size_t offset,const re5::profile::BuildId& id,const Sites& sites){std::memcpy(bytes.data()+offset,id.data(),id.size());for(auto site:sites)put(bytes,site.offset,site.instruction);}
unsigned body_sets=0,body_rebuilds=0,heading_reads=0;
template<class T>T camera_probe_copy(const volatile T& source){T out;auto* dst=reinterpret_cast<unsigned char*>(&out);const auto* src=reinterpret_cast<const volatile unsigned char*>(&source);for(unsigned i=0;i<sizeof(T);++i)dst[i]=src[i];return out;}
float fake_heading(std::uintptr_t actor){++heading_reads;float y=0,w=1;std::memcpy(&y,reinterpret_cast<void*>(actor+0x74),4);std::memcpy(&w,reinterpret_cast<void*>(actor+0x7C),4);return 2*std::atan2(y,w);}
void fake_heading(std::uintptr_t actor,float heading){++body_sets;const float q[4]={0,std::sin(heading*.5f),0,std::cos(heading*.5f)};std::memcpy(reinterpret_cast<void*>(actor+0x70),q,16);}
void fake_local(std::uintptr_t actor){++body_rebuilds;const float heading=fake_heading(actor);const float c=std::cos(heading),s=std::sin(heading);float local[16]={c,0,-s,0,0,1,0,0,s,0,c,0,0,0,0,1};std::memcpy(local+12,reinterpret_cast<void*>(actor+0x60),12);std::memcpy(reinterpret_cast<void*>(actor+0x90),local,sizeof(local));}
void fake_world(std::uintptr_t actor){std::memcpy(reinterpret_cast<void*>(actor+0xD0),reinterpret_cast<void*>(actor+0x90),64);}
float scalar(const void* ctx,unsigned index){float x;std::memcpy(&x,static_cast<const std::uint8_t*>(ctx)+index*16,4);return x;}
void scalar(void* ctx,unsigned index,float x){std::memcpy(static_cast<std::uint8_t*>(ctx)+index*16,&x,4);}
}

namespace {
std::array<std::array<std::uint8_t,0x130>,4> polygon_storage{};unsigned polygons_created=0,gui_phase=0,gui_quads=0;
void shell_init(void* shell){std::memset(shell,0,0x2B0);}
std::uintptr_t polygon_create(){assert(polygons_created<4);auto& poly=polygon_storage[polygons_created++];const auto table=main_base+0x1CC0DA8;std::memcpy(poly.data(),&table,8);std::uint32_t blend=0x10;std::memcpy(poly.data()+0xF8,&blend,4);game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(poly.data()),poly.size(),3});return reinterpret_cast<std::uintptr_t>(poly.data());}
void gui_mode(std::uintptr_t,unsigned mode){assert(mode==0x18);}
void gui_init(void* draw,std::uintptr_t command,unsigned flags){assert(flags==0);std::memcpy(static_cast<std::uint8_t*>(draw)+8,&command,8);}
void gui_projection(void*,void* draw){const float scale=1;std::memcpy(static_cast<std::uint8_t*>(draw)+0xE0,&scale,4);std::memcpy(static_cast<std::uint8_t*>(draw)+0xE4,&scale,4);}
void gui_pre(void*,void*){assert(gui_phase++%3==0);}
void gui_draw(void* poly,void*){assert(gui_phase++%3==1);std::uint32_t blend=0,color=0;std::memcpy(&blend,static_cast<std::uint8_t*>(poly)+0xF8,4);std::memcpy(&color,static_cast<std::uint8_t*>(poly)+0xC4,4);assert(blend==0x10&&color==0x80FFFFFF);++gui_quads;}
void gui_post(void*,void*){assert(gui_phase++%3==2);}
re5::Matrix gui_matrix(std::uintptr_t){re5::Matrix value;for(unsigned i=0;i<4;++i)value.m[i][i]=1;return value;}
}
std::array<double,3> camera_projection27(const re5::CameraPose& pose,const std::array<float,3>& point){
 auto dot=[](const std::array<double,3>& a,const std::array<double,3>& b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];};
 auto cross=[](const std::array<double,3>& a,const std::array<double,3>& b){return std::array<double,3>{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};};
 std::array<double,3> f{},up{},v{};for(unsigned i=0;i<3;++i){f[i]=double(pose.target[i])-pose.eye[i];up[i]=pose.up[i];v[i]=double(point[i])-pose.eye[i];}
 const auto length=std::sqrt(dot(f,f));for(auto& x:f)x/=length;auto right=cross(f,up);const auto width=std::sqrt(dot(right,right));for(auto& x:right)x/=width;up=cross(right,f);
 const auto depth=dot(v,f);return {dot(v,right)/depth,dot(v,up)/depth,depth};
}
void review_check(bool,const char*);
#include "re5_runtime_follow_fixture.hpp"
#include "re5_runtime_combat_camera_fixture.hpp"
#include "re5_runtime_locomotion_fixture.hpp"
namespace {void test_Re5CameraFollow(void* owner,float dt){re5_follow_test::native(reinterpret_cast<std::uintptr_t>(owner),dt);}}
unsigned review_failures=0;
void review_check(bool pass,const char* message){if(!pass){std::fprintf(stderr,"FINAL REVIEW: %s\n",message);++review_failures;}}
int main(){
 std::vector<std::uint8_t> main(re5::profile::main_size),sdk(re5::profile::sdk_size),manager(0x450),cam0(0x100),cam1(0x100),renderer(0x106F6A0),target(0x648),light(0x180),shadow0(0x250),shadow1(0x250);
#ifdef RE5_TEST_LEGACY
 constexpr auto tested_build=re5::profile::Build::Legacy;
 seed(main,re5::profile::legacy_build_id_offset,re5::profile::legacy_build_id,re5::profile::legacy_main_sites);
#else
 constexpr auto tested_build=re5::profile::Build::Latest;
 seed(main,re5::profile::build_id_offset,re5::profile::build_id,re5::profile::main_sites);
#endif
 seed(sdk,re5::profile::sdk_build_id_offset,re5::profile::sdk_build_id,re5::profile::sdk_sites);
 game_test::module.m_Total={reinterpret_cast<std::uintptr_t>(main.data()),main.size()};
 exl::util::sdk_module.m_Total={reinterpret_cast<std::uintptr_t>(sdk.data()),sdk.size()};
 game_test::program=0;exl_main(nullptr,nullptr);assert(game_test::installed==0);
 game_test::program=re5::profile::title_id;exl::util::sdk_present=false;exl_main(nullptr,nullptr);assert(game_test::installed==0);exl::util::sdk_present=true;
 const auto tested_view=re5::profile::view(tested_build);
 main[tested_view.sites[0].offset]^=1;exl_main(nullptr,nullptr);assert(game_test::installed==0);main[tested_view.sites[0].offset]^=1;
 sdk[re5::profile::sdk_sites[0].offset]^=1;exl_main(nullptr,nullptr);assert(game_test::installed==0);sdk[re5::profile::sdk_sites[0].offset]^=1;
 exl_main(nullptr,nullptr);assert(game_test::initialized==1&&shared_control.status==0);
 assert(re5::profile::selected_build==tested_build);
 assert(image_identity.status==1&&image_identity.abi==1&&image_identity.reserved==0);
 assert(image_identity.main_base==main_base&&image_identity.main_size==main.size()&&image_identity.guard_count==tested_view.count);
 for(unsigned i=0;i<16;++i)assert(image_identity.marker[i]==re5::kImageIdentityMarker[i]);
 for(unsigned i=0;i<32;++i)assert(image_identity.build_id[i]==(*tested_view.id)[i]&&image_identity.sdk_build_id[i]==re5::profile::sdk_build_id[i]);
#ifdef RE5_TEST_LEGACY
 for(auto offset:{0x2404DCu,0x167330u,0x10D8AD0u,0x23ACB4u,0x1043664u,0xCFE9D8u,0x1A07F0u,0x1A0808u,0x7CA2E0u,0x7DE868u,0x24A908u,0x4D2DF0u})assert(std::find(game_test::installed_offsets.begin(),game_test::installed_offsets.end(),offset)!=game_test::installed_offsets.end());
 assert(std::find(game_test::installed_offsets.begin(),game_test::installed_offsets.end(),0x7CA660u)==game_test::installed_offsets.end());
#else
 for(auto offset:{0x167510u,0x10D8C30u,0x23AE94u,0x10437C4u,0xCFEB38u,0x1A09D0u,0x1A09E8u})assert(std::find(game_test::installed_offsets.begin(),game_test::installed_offsets.end(),offset)!=game_test::installed_offsets.end());
#endif
 for(auto* region:{&main,&sdk,&manager,&cam0,&cam1,&renderer,&target,&light,&shadow0,&shadow1})game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(region->data()),region->size(),3});
 const auto base=reinterpret_cast<std::uintptr_t>(main.data());
 put(main,0x26A8550,reinterpret_cast<std::uintptr_t>(manager.data()));put(manager,0x48,reinterpret_cast<std::uintptr_t>(cam0.data()));put(manager,0x248,reinterpret_cast<std::uintptr_t>(cam1.data()));
 for(auto* c:{&cam0,&cam1}){put(*c,0x44,1000.f);put(*c,0x48,1.f);}
 put(main,0x26A96A0,reinterpret_cast<std::uintptr_t>(renderer.data()));put(renderer,0x106F698,reinterpret_cast<std::uintptr_t>(target.data()));put(target,0x610,std::uint32_t(0));put(target,0x614,std::uint32_t(4));
 put(main,0x1CF89F0,reinterpret_cast<std::uintptr_t>(light.data()));put(light,0x160,reinterpret_cast<std::uintptr_t>(shadow0.data()));put(light,0x168,reinterpret_cast<std::uintptr_t>(shadow1.data()));put(shadow0,0,base+0x1AD20D8);put(shadow1,0,base+0x1AD20D8);
 re5::Settings wanted;wanted.graphics={re4::TextureFilter::Aniso16,re4::ShadowMode::Off,re5::AntiAliasing::Off,1.75f};
 game_test::settings_primary.resize(re5::kSettingsBytes);assert(re5::encode_settings(wanted,game_test::settings_primary.data(),game_test::settings_primary.size()));game_test::mount_result=0;
 const auto before0=cam0,before1=cam1,before_target=target;
 Re5CameraUpdate::Callback(manager.data());assert(camera_calls==1&&shared_control.status==1&&shared_control.settings_source==1);
 std::array<std::uint8_t,56> descriptor{};std::uint32_t flags=7;std::memcpy(descriptor.data(),&flags,4);Re5SamplerCreate::Callback(nullptr,descriptor.data());
 std::memcpy(&flags,sampler_seen.data(),4);assert((flags&255)==8&&((flags>>17)&31)==16&&shared_control.sampler_overrides==1);
 exl::hook::InlineFloatCtx context{};re4::set_saved_x(&context,0,reinterpret_cast<std::uintptr_t>(cam0.data()));scalar(&context,0,1);scalar(&context,1,1000);Re5ProjectionFar::Callback(&context);assert(scalar(&context,1)==1750);
 re4::set_saved_x(&context,8,reinterpret_cast<std::uintptr_t>(cam1.data()));re4::set_saved_x(&context,19,reinterpret_cast<std::uintptr_t>(manager.data())+0x240);scalar(&context,2,1000);Re5CullingFar::Callback(&context);assert(scalar(&context,2)==1750);
 re4::set_saved_x(&context,20,reinterpret_cast<std::uintptr_t>(target.data()));re4::set_saved_x(&context,23,4);Re5PostAA::Callback(&context);assert(re4::saved_x(&context,23)==0);
 re4::set_saved_x(&context,0,reinterpret_cast<std::uintptr_t>(shadow1.data()));re4::set_saved_x(&context,8,base+0x98030);re4::set_saved_x(&context,19,1);re4::set_saved_x(&context,1,1);Re5Shadow1::Callback(&context);assert(re4::saved_x(&context,1)==0);
 assert(cam0==before0&&cam1==before1&&target==before_target); // Native owners are never rewritten.
 auto seq=shared_control.sequence;wanted.graphics.filter=re4::TextureFilter::Native;wanted.graphics.draw_distance=1;
 assert(re5::publish_settings(shared_control,wanted,seq));Re5CameraUpdate::Callback(manager.data());assert(shared_control.applied_sequence==seq&&shared_control.settings_source==4);
 Re5SamplerCreate::Callback(nullptr,descriptor.data());std::memcpy(&flags,sampler_seen.data(),4);assert(((flags>>17)&31)==16); // Sampler policy is creation-time latched.
 scalar(&context,1,1000);re4::set_saved_x(&context,0,reinterpret_cast<std::uintptr_t>(cam0.data()));scalar(&context,0,1);Re5ProjectionFar::Callback(&context);assert(scalar(&context,1)==1000);

 // Exercise real input callbacks with player1 in actor slot2 and a separately
 // assigned physical controller. Player2 uses its own logical settings.
 std::vector<std::uint8_t> actors(0x8400),wrappers(0x2000),players(0x100),pads(0x9000);
 for(auto* region:{&actors,&wrappers,&players,&pads})game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(region->data()),region->size(),3});
 const auto actor=reinterpret_cast<std::uintptr_t>(actors.data()),second=actor+0x4200,wrapper=reinterpret_cast<std::uintptr_t>(wrappers.data()),wrapper2=wrapper+0x1000;
 put(main,0x1CF8CC8,reinterpret_cast<std::uintptr_t>(players.data()));put(main,0x26A9288,reinterpret_cast<std::uintptr_t>(pads.data()));
 put(players,0x40,actor);put(players,0x38,second);
 put(actors,0x3F2C,std::uint8_t(2));put(actors,0x3F49,std::uint8_t(0));put(actors,0x3EE8,wrapper);put(wrappers,0x48,actor);
 put(actors,0x4200+0x3F2C,std::uint8_t(1));put(actors,0x4200+0x3F49,std::uint8_t(1));put(actors,0x4200+0x3EE8,wrapper2);put(wrappers,0x1048,second);
 put(pads,0x8C98,std::uint32_t(3));put(pads,0x8C94,std::uint32_t(3));
 for(unsigned i:{1u,3u}){put(pads,i*0xEE0+0x670,i);put(pads,i*0xEE0+0x674,std::uint32_t(1));}
 wanted.players[0].analog_enabled=true;wanted.players[0].unified_controls=true;wanted.players[0].right_deadzone=0;wanted.players[0].remap_enabled=true;wanted.players[0].button_map[0]=1;
 wanted.players[1].analog_enabled=true;wanted.players[1].right_aim_gain_x=.25f;assert(controls_snapshot.publish(wanted));
 // Real attached Joy-Con SDK observer -> native local-input callback. The
 // native pad type0x10 must normalize to SDK handheld2, not reject the player.
 put(pads,8*0xEE0+0x670,std::uint32_t(32));put(pads,8*0xEE0+0x674,std::uint32_t(0x10));
 put(pads,0x8C98,std::uint32_t(8));put(pads,0x8C94,std::uint32_t(8));
 game_test::tick=100000000;
 re5::PadState handheld;handheld.attributes=1;handheld.rx=16384;handheld.ly=16384;handheld.buttons=1;const std::uint32_t handheld_id=32;
 Re5HandheldPad::Callback(&handheld,handheld_id);assert(handheld.buttons==2);
 Re5LocalInput::Callback(reinterpret_cast<void*>(wrapper+0x350),reinterpret_cast<void*>(actor));
 assert(player_runtime[0].frame.valid&&player_runtime[0].frame.player.sticks.controller==32
  &&player_runtime[0].frame.player.sticks.style==2&&player_runtime[0].frame.player.motion.style==2&&player_runtime[0].frame.axes.rx>.49f);
 put(pads,0x8C98,std::uint32_t(3));put(pads,0x8C94,std::uint32_t(3));
 re5::PadState raw;raw.attributes=1;raw.rx=16384;raw.ly=16384;raw.buttons=1;observed_pad(&raw,3,1);assert(raw.buttons==2);
 re5::PadState raw2;raw2.attributes=1;raw2.rx=16384;observed_pad(&raw2,1,1);assert(raw2.buttons==0);
 Re5LocalInput::Callback(reinterpret_cast<void*>(wrapper+0x350),reinterpret_cast<void*>(actor));assert(player_runtime[0].frame.valid&&player_runtime[0].frame.player.actor_slot==2&&player_runtime[0].frame.axes.rx>.49f);
 Re5LocalInput::Callback(reinterpret_cast<void*>(wrapper2+0x350),reinterpret_cast<void*>(second));assert(player_runtime[1].frame.valid&&player_runtime[1].frame.settings.right_aim_gain_x==.25f);
 // Input worker A publishes a frame; camera worker B consumes the owned local
 // player without requiring native input and camera callbacks to share a worker.
 game_test::thread_id=2;ControlFrame cross_thread;
 assert(fresh_frame(actor,cross_thread)&&cross_thread.player.actor==actor&&cross_thread.axes.rx>.49f);
 assert(fresh_frame(second,cross_thread)&&cross_thread.player.local_player==1&&cross_thread.settings.right_aim_gain_x==.25f);
 // Sequential native input invocations may migrate as well. No first worker
 // identity authorizes later callbacks; the input try-lock owns writer state.
 const auto before_migration=player_runtime[0].frame.frame;
 Re5LocalInput::Callback(reinterpret_cast<void*>(wrapper+0x350),reinterpret_cast<void*>(actor));
 assert(player_runtime[0].frame.frame==before_migration+1&&runtime_thread.load()==2);
 game_test::thread_id=3;assert(fresh_frame(actor,cross_thread));
 game_test::thread_id=0;Re5LocalInput::Callback(reinterpret_cast<void*>(wrapper+0x350),reinterpret_cast<void*>(actor));
 assert(player_runtime[0].frame.frame==before_migration+1&&runtime_thread.load()==2);
 game_test::thread_id=3;input_lock.test_and_set();
 Re5LocalInput::Callback(reinterpret_cast<void*>(wrapper+0x350),reinterpret_cast<void*>(actor));input_lock.clear();
 assert(player_runtime[0].frame.frame==before_migration+1&&runtime_thread.load()==2);
 // Mailbox contention fails closed for only that player, without spinning.
 published_frames[0].lock.test_and_set();assert(!fresh_frame(actor,cross_thread));
 assert(fresh_frame(second,cross_thread)&&cross_thread.player.local_player==1);published_frames[0].lock.clear();
 const auto committed=player_runtime[0].frame;
 auto rejected=[&](const ControlFrame& frame){assert(published_frames[0].publish(frame));assert(!fresh_frame(actor,cross_thread));};
 auto invalid=committed;invalid.player.actor=second;rejected(invalid);
 invalid=committed;++invalid.player.command;rejected(invalid);
 invalid=committed;invalid.player.local_player=1;rejected(invalid);
 invalid=committed;++invalid.player.sticks.controller;rejected(invalid);
 invalid=committed;invalid.player.sticks.style=2;rejected(invalid);
 invalid=committed;++invalid.player.motion.controller;rejected(invalid);
 invalid=committed;invalid.player.motion.style=2;rejected(invalid);
 invalid=committed;invalid.valid=false;rejected(invalid);
 invalid=committed;invalid.tick=game_test::tick+1;rejected(invalid);
 invalid=committed;invalid.tick=game_test::tick-1920000;assert(published_frames[0].publish(invalid)&&fresh_frame(actor,cross_thread));
 invalid.tick=game_test::tick-1920001;rejected(invalid);
 publish_test_control();assert(fresh_frame(actor,cross_thread));
 settings_loaded.store(false);assert(!fresh_frame(actor,cross_thread));settings_loaded.store(true);
 game_active.store(false);assert(!fresh_frame(actor,cross_thread));game_active.store(true);
 game_test::thread_id=1;
 // Inline hooks consume an owned per-thread scope, and must preserve registers
 // for another actor, network actor, frozen input or invocation outside scope.
 input_scope=player_runtime[0].frame;input_thread.store(1);
 input_scope.axes.rx=.9f;game_test::thread_id=2;
 assert(!owned_scope(actor,wrapper+0x350));assert(fresh_frame(actor,cross_thread)&&cross_thread.axes.rx<.6f);
 re4::set_saved_x(&context,19,wrapper+0x350);re4::set_saved_x(&context,20,actor);scalar(&context,0,-9);
 Re5AimAxes::Callback(&context);assert(scalar(&context,0)==-9);
 game_test::thread_id=1;assert(owned_scope(actor,wrapper+0x350)&&fresh_frame(actor,cross_thread)&&cross_thread.axes.rx==.9f);
 assert(!owned_scope(second,wrapper+0x350)&&!owned_scope(actor,wrapper2+0x350));
 input_scope=player_runtime[0].frame;re4::set_saved_x(&context,19,actor);re4::set_saved_x(&context,8,0);Re5Scheme8::Callback(&context);assert(re4::saved_x(&context,8)==3);
 re4::set_saved_x(&context,19,wrapper+0x350);re4::set_saved_x(&context,20,actor);scalar(&context,0,-9);scalar(&context,1,-8);Re5AimAxes::Callback(&context);assert(scalar(&context,0)>.49f&&scalar(&context,1)==0);
 input_scope.scope=true;scalar(&context,0,1);Re5ScopeZoom::Callback(&context);assert(scalar(&context,3)>.09f&&scalar(&context,3)<.11f);
 put(actors,0x3F0E,std::uint8_t(1));scalar(&context,0,-9);Re5AimAxes::Callback(&context);assert(scalar(&context,0)==-9);put(actors,0x3F0E,std::uint8_t(0));
 input_scope.settings.gyro_enabled=true;input_scope.gyro={.1f,.2f};put(wrappers,0x350+0x4C4,10.f);scalar(&context,9,20);Re5AimDelta::Callback(&context);float yaw;std::memcpy(&yaw,wrappers.data()+0x350+0x4C4,4);assert(yaw>15.7f&&yaw<15.8f&&scalar(&context,9)>8.5f&&scalar(&context,9)<8.6f);assert(input_scope.gyro.yaw==0);
 // Modern's aim X/Y are independent of walking camera sensitivity.
 const auto before_modern_aim=input_scope;input_scope.settings=re4::re6_profile();
 input_scope.settings.camera_gain=0;input_scope.settings.right_aim_gain_x=.75f;input_scope.settings.right_aim_gain_y=.5f;input_scope.axes.rx=.4f;input_scope.axes.ry=.6f;
 Re5AimAxes::Callback(&context);
 review_check(std::fabs(scalar(&context,0)-.3f)<.000001f&&std::fabs(scalar(&context,1)-.3f)<.000001f,"R4 Modern aiming is incorrectly scaled by walking-camera sensitivity");
 input_scope.settings.re6_response=false;Re5AimAxes::Callback(&context);assert(scalar(&context,0)==0&&scalar(&context,1)==0);
 input_scope=before_modern_aim;
 // Real polar cache: classic turn gain must never scale unified movement.
  // This isolated scoped-shaping fixture has no native input invocation nonce.
  input_scope.ordinary_epoch=0;
 input_scope.settings.analog_enabled=true;input_scope.settings.unified_controls=true;
 for(const auto axes:{re5::Axes{1,0,0,0},re5::Axes{.6f,.8f,0,0}}){
  input_scope.axes=axes;input_scope.settings.turn_gain=1;Re5Polar::Callback(reinterpret_cast<void*>(wrapper+0x350+0x4F8));
  float full[4];std::memcpy(full,wrappers.data()+0x350+0x880,sizeof(full));
  input_scope.settings.turn_gain=0;Re5Polar::Callback(reinterpret_cast<void*>(wrapper+0x350+0x4F8));
  float zero[4];std::memcpy(zero,wrappers.data()+0x350+0x880,sizeof(zero));
  review_check(full[0]==zero[0]&&full[2]==zero[2]&&zero[2]>.99f,"R4 unified left-stick polar cache changes with turn gain");
 }
 input_scope.settings.unified_controls=false;input_scope.axes={1,0,0,0};
 Re5Polar::Callback(reinterpret_cast<void*>(wrapper+0x350+0x4F8));
 float classic[4];std::memcpy(classic,wrappers.data()+0x350+0x880,sizeof(classic));assert(classic[2]==0);
 input_thread.store(0);scalar(&context,0,-9);Re5AimAxes::Callback(&context);assert(scalar(&context,0)==-9);
 put(actors,0x3F40,std::uint32_t(2));raw.buttons=1;observed_pad(&raw,3,1);assert(raw.buttons==1);

 put(actors,0x3F40,std::uint32_t(0));
 std::vector<std::uint8_t> camera(0x3000),neck(0x150),mapping(256,255),bones(3*0xC0),laser(0x3F0),cockpit(0x2BA0),command_context(0x3200),root(0x58500),stage(0x30000);
 for(auto* region:{&camera,&neck,&mapping,&bones,&laser,&cockpit,&command_context,&root,&stage})game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(region->data()),region->size(),3});
 const auto cam=reinterpret_cast<std::uintptr_t>(camera.data());put(actors,0x1A80,cam);put(camera,0,base+0x1AD2510);put(camera,0x130,actor);put(camera,0x198,cam+0xDB0);put(camera,0xDA0,cam+0xDB0);put(manager,0x48,cam);
 auto& control=player_runtime[0].frame;control.settings.analog_enabled=true;control.action=0x1C;control.settings.third_person_distance=1.75f;
 // Retained orbit/FP paths still use the existing persistent-pose cleanup.
 control.settings.free_camera=true;control.settings.unified_controls=false;control.axes.rx=control.axes.ry=0;
 // Holding the camera slot suppresses orbit updates and pose capture/writes,
 // including the pre-desired cleanup. Native camera workers never wait here.
 put(camera,0x68,10.f);re4::set_saved_x(&context,19,cam);publish_test_control();
 const auto camera_busy_before=camera;const auto orbit_busy_before=orbits[0];
 camera_poses[0].lock.test_and_set();Re5CameraRig::Callback(&context);Re5CameraPoseRestore::Callback(&context);
 assert(camera==camera_busy_before&&orbits[0].actor==orbit_busy_before.actor&&orbits[0].tick==orbit_busy_before.tick);
 assert(camera_poses[0].lock.test_and_set());camera_poses[0].lock.clear();
 // The gait backend can reenter either hook; both skip its shared adapter and
 // frame caches while it is owned, leaving the outer owner's lock intact.
 re4::set_saved_x(&context,19,actor);gait_lock.test_and_set();const auto serial_busy_before=gait_serial[0];
 Re5MovementProposal::Callback(&context);Re5MovementPose::Callback(&context);
 assert(gait_serial[0]==serial_busy_before&&gait_lock.test_and_set());gait_lock.clear();
 // The native camera on a different worker still receives the owned overlay.
 game_test::thread_id=2;re4::set_saved_x(&context,19,cam);Re5CameraRig::Callback(&context);game_test::thread_id=1;float eye_z;std::memcpy(&eye_z,camera.data()+0x68,4);assert(eye_z==17.5f);
 // Native/default and distance1 must not arm restore from floating-point
 // roundoff or steep-angle clamping, nor rewind later collision coordinates.
 re4::set_saved_x(&context,19,cam);Re5CameraPoseRestore::Callback(&context);
 const auto baseline_camera=camera;control.settings.third_person_distance=1;
 for(const auto eye:{std::array<float,3>{1,2,3},std::array<float,3>{0,100,.1f}}){
  for(unsigned i=0;i<3;++i){put(camera,0x60+i*4,eye[i]);put(camera,0x80+i*4,0.f);}
  const auto neutral=camera;publish_test_control();Re5CameraRig::Callback(&context);assert(camera==neutral&&!camera_poses[0].state.pending(cam));
  put(camera,0x64,7.f);put(camera,0x74,.5f);const auto collision=camera;Re5CameraPoseRestore::Callback(&context);assert(camera==collision);
 }
 camera=baseline_camera;control.settings.third_person_distance=1.75f;publish_test_control();Re5CameraRig::Callback(&context);
 // Native desired may leave the previous persistent pose untouched. Restore
 // before that call, even if the next overlay is rejected by stale input.
 for(unsigned repeat=0;repeat<4;++repeat){
  re4::set_saved_x(&context,19,cam);Re5CameraPoseRestore::Callback(&context);
  float restored;std::memcpy(&restored,camera.data()+0x68,4);assert(restored==10.f);
  publish_test_control();Re5CameraRig::Callback(&context);std::memcpy(&restored,camera.data()+0x68,4);assert(restored==17.5f);
 }
 control.tick=game_test::tick-1920001;Re5CameraPoseRestore::Callback(&context);publish_test_control();Re5CameraRig::Callback(&context);
 std::memcpy(&eye_z,camera.data()+0x68,4);assert(eye_z==10.f);control.tick=game_test::tick;
 control.settings.camera_return_speed=.5f;re4::set_saved_x(&context,19,cam+0xDB0);re4::set_saved_x(&context,20,actor);scalar(&context,0,.25f);publish_test_control();Re5CameraReturn::Callback(&context);assert(scalar(&context,0)==.5f);
 control.settings.unified_controls=false;control.settings.free_camera=true;control.action=0;control.axes.rx=.5f;control.axes.ry=0;publish_test_control();Re5CameraAxes::Callback(&context);assert(scalar(&context,9)==0&&scalar(&context,0)==0&&re4::saved_x(&context,8)==0);
 // Modern aiming keeps the existing native aimed-camera sensitivity path;
 // its separate body-heading publication is intentionally absent while aiming.
 {
  auto aimed=control;aimed.settings=re4::re6_profile();aimed.action=0x1C;aimed.heading_valid=false;
  aimed.axes.rx=.4f;aimed.axes.ry=.6f;aimed.settings.camera_gain=.8f;aimed.settings.right_aim_gain_x=.5f;aimed.settings.right_aim_gain_y=.75f;
  put(actors,0x1B40,std::uint32_t(0x1C));assert(published_frames[0].publish(aimed));
  re4::set_saved_x(&context,19,cam+0xDB0);re4::set_saved_x(&context,20,actor);scalar(&context,9,-9);scalar(&context,0,-8);Re5CameraAxes::Callback(&context);
  assert(std::fabs(scalar(&context,9)-5242.72f)<.01f&&std::fabs(scalar(&context,0)-11796.12f)<.01f);
  put(actors,0x1B40,std::uint32_t(control.action));publish_test_control();
 }
 // Exact runtime logical eye mapping, including a model whose physical eye
 // indices differ from logical56/57. No body/mesh/global transform is changed.
 put(actors,0x2570,reinterpret_cast<std::uintptr_t>(neck.data()));put(neck,0x30,actor);put(neck,0xD0,std::uint16_t(4));put(neck,0x100,std::uint16_t(56));put(neck,0x130,std::uint16_t(57));mapping[4]=0;mapping[56]=1;mapping[57]=2;
 put(actors,0x8D0,reinterpret_cast<std::uintptr_t>(mapping.data()));put(actors,0x8C8,reinterpret_cast<std::uintptr_t>(bones.data()));put(actors,0x8C0,std::uint32_t(3));
 for(unsigned i=0;i<3;++i){put(bones,0xC0+0x50+i*4,float(10*(i+1)));put(bones,0x180+0x50+i*4,float(12*(i+1)));}
 control.settings.first_person=true;control.settings.free_camera=false;control.action=0x1C;re4::set_saved_x(&context,19,cam);publish_test_control();Re5CameraRig::Callback(&context);
 float anchor[3];std::memcpy(anchor,camera.data()+0x60,12);assert(anchor[0]==11&&anchor[1]==22&&anchor[2]==33);
 // Ordinary first-person placement retains native viewing direction, including
 // a steep view: only an owned free orbit may rotate that direction.
 re4::set_saved_x(&context,19,cam);Re5CameraPoseRestore::Callback(&context);
 put(camera,0x60,0.f);put(camera,0x64,10.f);put(camera,0x68,0.f);
 for(unsigned i=0;i<3;++i)put(camera,0x80+i*4,0.f);
 publish_test_control();Re5CameraRig::Callback(&context);float steep_eye[3],steep_target[3];std::memcpy(steep_eye,camera.data()+0x60,12);std::memcpy(steep_target,camera.data()+0x80,12);
 review_check(steep_target[0]-steep_eye[0]==0&&steep_target[1]-steep_eye[1]==-10&&steep_target[2]-steep_eye[2]==0,"R1 ordinary first-person native direction changed");
 // Modern free-camera direction, across actual fresh input callbacks and both
 // camera modes. Native rig output is reseeded before its post-update hook.
 actor_rotation_api={fake_heading,fake_heading,fake_local};
 wanted.players[0]=re4::re6_profile();wanted.players[0].gyro_enabled=false;
 wanted.players[0].right_deadzone=0;wanted.players[0].camera_return_speed=1;
 auto component_camera=[&]{
  const auto component=cam+0xDB0;put(camera,0xDB0+0x130,actor);
  for(auto offsets:{std::pair{0x60u,0x160u},std::pair{0x70u,0x170u},std::pair{0x80u,0x180u}})std::memcpy(reinterpret_cast<void*>(component+offsets.second),camera.data()+offsets.first,12);
  re4::set_saved_x(&context,19,component);camera_test::actor_phase(tested_build==re5::profile::Build::Legacy?0x24A874:0x24AA54,&context);
  for(auto offsets:{std::pair{0x60u,0x160u},std::pair{0x70u,0x170u},std::pair{0x80u,0x180u}})std::memcpy(camera.data()+offsets.first,reinterpret_cast<void*>(component+offsets.second),12);
  re4::set_saved_x(&context,19,cam);
 };
 auto seed_camera=[&](bool apply=true){
  for(unsigned i=0;i<3;++i){put(camera,0x60+i*4,i==2?-10.f:0.f);put(camera,0x80+i*4,0.f);put(camera,0x70+i*4,i==1?1.f:0.f);}
  if(apply)component_camera();
 };
 auto direction=[&]{std::array<float,3> out{};for(unsigned i=0;i<3;++i){float e=0,t=0;std::memcpy(&e,camera.data()+0x60+i*4,4);std::memcpy(&t,camera.data()+0x80+i*4,4);out[i]=t-e;}return out;};
 auto actor_yaw=[&]{float y=0,w=1;std::memcpy(&y,actors.data()+0x74,4);std::memcpy(&w,actors.data()+0x7C,4);return 2*std::atan2(y,w);};
 put(actors,8,std::uint16_t(0x201));put(actors,0x54,std::uint16_t(4));put(actors,0x56,std::uint16_t(3));put(actors,0x7C,1.f);put(actors,0xD0,1.f);put(actors,0xF8,1.f);
 for(unsigned i=0;i<3;++i)put(actors,0x80+i*4,1.f);
 put(actors,0x50,std::uint32_t(0xFFFFFFFF));fake_local(actor);fake_world(actor);
 auto actor_phase=[&]{
  fake_local(actor); // The native+D0 phase has already rebuilt local pose.
  re4::set_saved_x(&context,19,actor);
  camera_test::actor_phase(re5::profile::selected_build==re5::profile::Build::Legacy?0x24DC00:0x24DDE0,&context);
  fake_world(actor);
 };
 auto camera_frame=[&](int x,int y){
  game_test::tick+=320000;raw.rx=x;raw.ry=y;raw.buttons=0;observed_pad(&raw,3,1);
  Re5LocalInput::Callback(reinterpret_cast<void*>(wrapper+0x350),reinterpret_cast<void*>(actor));
  actor_phase();re4::set_saved_x(&context,19,cam);Re5CameraPoseRestore::Callback(&context);seed_camera();Re5CameraRig::Callback(&context);return direction();
 };
 // Review regression: a native quick-turn before the first actor anchor.
 assert(controls_snapshot.publish(wanted));anchored_looks[0]={};player_runtime[0].anchored=false;player_runtime[0].look.reset();
 raw.rx=raw.ry=0;game_test::tick+=320000;observed_pad(&raw,3,1);
 Re5LocalInput::Callback(reinterpret_cast<void*>(wrapper+0x350),reinterpret_cast<void*>(actor));
 assert(player_runtime[0].anchored&&!anchored_looks[0].active);
 const float game_heading=std::remainder(actor_yaw()+1.f,6.28318530718f);
 put(actors,0x1B40,std::uint32_t(9));re4::set_saved_x(&context,19,actor);
 camera_test::actor_phase(re5::profile::selected_build==re5::profile::Build::Legacy?0x24DC00:0x24DDE0,&context);
 fake_heading(actor,game_heading);fake_local(actor);fake_world(actor);put(actors,0x1B40,std::uint32_t(0));
 game_test::tick+=320000;observed_pad(&raw,3,1);Re5LocalInput::Callback(reinterpret_cast<void*>(wrapper+0x350),reinterpret_cast<void*>(actor));
 re4::set_saved_x(&context,19,actor);camera_test::actor_phase(re5::profile::selected_build==re5::profile::Build::Legacy?0x24DC00:0x24DDE0,&context);
 assert(std::fabs(std::remainder(actor_yaw()-game_heading,6.28318530718f))<.000001f);
 for(bool first_person:{false,true}){
  wanted.players[0].first_person=first_person;assert(controls_snapshot.publish(wanted));orbits[0]={};anchored_looks[0]={};
  camera_frame(16384,16384);auto previous=direction();
  for(unsigned frame=0;frame<3;++frame){const float old_body=actor_yaw();auto d=camera_frame(16384,16384);
   assert(actor_yaw()<old_body-.000001f); // Right stick turns the owned body with the camera.
   const float camera_turn=std::remainder(std::atan2(d[0],d[2])-std::atan2(previous[0],previous[2]),6.28318530718f);
   review_check(std::fabs(camera_turn-std::remainder(actor_yaw()-old_body,6.28318530718f))<.00001f,first_person?"R1 first-person camera/body yaw differs":"R1 third-person camera/body yaw differs");
   review_check(d[1]>previous[1]+.0001f,first_person?"R1 first-person free look ignores upward Y":"R1 third-person upward stick looks down");previous=d;
  }
  // Modern retains RE4/RE6's measured brake, then holds its final heading.
  for(unsigned frame=0;frame<36;++frame)camera_frame(0,0);
  const auto held=direction();const float held_yaw=anchored_looks[0].heading,held_pitch=anchored_looks[0].pitch;
  for(unsigned frame=0;frame<6;++frame){auto d=camera_frame(0,0);
   review_check(std::fabs(d[0]-held[0])<.0001f&&std::fabs(d[1]-held[1])<.0001f&&std::fabs(anchored_looks[0].heading-held_yaw)<.000001f&&std::fabs(anchored_looks[0].pitch-held_pitch)<.000001f,"R5 neutral anchored camera keeps moving after measured brake");
  }
  re4::set_saved_x(&context,19,cam+0xDB0);re4::set_saved_x(&context,20,actor);scalar(&context,0,.25f);publish_test_control();Re5CameraReturn::Callback(&context);
  review_check(scalar(&context,0)==1,"R5 native return still applies in free-camera mode");
 }
 // A camera may run between the actor join and the native world/bone tail.
 wanted.players[0].first_person=false;assert(controls_snapshot.publish(wanted));
 camera_frame(0,0);camera_frame(0,0);
 game_test::tick+=320000;raw.rx=16384;raw.ry=16384;observed_pad(&raw,3,1);
 Re5LocalInput::Callback(reinterpret_cast<void*>(wrapper+0x350),reinterpret_cast<void*>(actor));
 re4::set_saved_x(&context,19,actor);
 camera_test::actor_phase(re5::profile::selected_build==re5::profile::Build::Legacy?0x24DC00:0x24DDE0,&context);
 const auto joined=anchored_looks[0];const auto sets_after_join=body_sets;
 // Duplicate world joins cannot integrate one committed input sample twice.
 camera_test::actor_phase(re5::profile::selected_build==re5::profile::Build::Legacy?0x24DC00:0x24DDE0,&context);
 assert(anchored_looks[0].heading==joined.heading&&anchored_looks[0].pitch==joined.pitch&&body_sets==sets_after_join);
 re4::set_saved_x(&context,19,cam);Re5CameraPoseRestore::Callback(&context);seed_camera();const auto uncommitted_camera=camera;re4::set_saved_x(&context,19,cam);Re5CameraRig::Callback(&context);
 assert(camera==uncommitted_camera);
 fake_world(actor);component_camera();Re5CameraRig::Callback(&context);assert(camera!=uncommitted_camera);
 // Qualification may change after the actor phase but before a camera worker.
 for(unsigned changed=0;changed<5;++changed){
  camera_frame(0,0);re4::set_saved_x(&context,19,cam);Re5CameraPoseRestore::Callback(&context);seed_camera(false);const auto before_camera=camera;
  if(changed==0)put(actors,0x3F0E,std::uint8_t(1));
  if(changed==1)put(actors,0x48,std::uintptr_t(0x1234));
  if(changed==2)put(actors,0x80,2.f);
  if(changed==3)put(actors,0x56,std::uint16_t(1));
  if(changed==4)put(actors,0x1B40,std::uint32_t(0x1C));
  component_camera();Re5CameraRig::Callback(&context);assert(!std::memcmp(camera.data()+0x60,before_camera.data()+0x60,0x30));
  put(actors,0x3F0E,std::uint8_t(0));put(actors,0x48,std::uintptr_t(0));put(actors,0x80,1.f);put(actors,0x56,std::uint16_t(3));put(actors,0x1B40,std::uint32_t(0));
 }
 // Fresh camera pitch does not consume or mutate the actor-owned heading.
 camera_frame(0,0);const auto pose_before_input=anchored_looks[0];raw.rx=raw.ry=16384;game_test::tick+=320000;observed_pad(&raw,3,1);
 Re5LocalInput::Callback(reinterpret_cast<void*>(wrapper+0x350),reinterpret_cast<void*>(actor));
 re4::set_saved_x(&context,19,cam);Re5CameraPoseRestore::Callback(&context);seed_camera();Re5CameraRig::Callback(&context);
 assert(anchored_looks[0].frame==pose_before_input.frame&&anchored_looks[0].heading==pose_before_input.heading&&anchored_looks[0].pitch==pose_before_input.pitch);
 // Native walking may replace local yaw before the next pose stage. Hold the
 // owned absolute heading rather than accumulating that unrelated left turn.
 const float desired=anchored_looks[0].heading;
 fake_heading(actor,desired+.25f);re4::set_saved_x(&context,19,actor);
 const auto committed_before_reassert=anchored_looks[0];
 camera_test::actor_phase(re5::profile::selected_build==re5::profile::Build::Legacy?0x24DC00:0x24DDE0,&context);
 assert(std::fabs(actor_yaw()-anchored_looks[0].heading)<.000001f&&anchored_looks[0].heading<desired);
 const auto after_reassert=anchored_looks[0];fake_heading(actor,desired+.4f);
 camera_test::actor_phase(re5::profile::selected_build==re5::profile::Build::Legacy?0x24DC00:0x24DDE0,&context);
 assert(anchored_looks[0].heading==after_reassert.heading&&anchored_looks[0].pitch==after_reassert.pitch&&std::fabs(actor_yaw()-after_reassert.heading)<.000001f);
 (void)committed_before_reassert;
 // Preserved first-person cleanup: contention skips restore and desired output.
 wanted.players[0].first_person=true;assert(controls_snapshot.publish(wanted));
 camera_frame(0,0);camera_frame(0,0);re4::set_saved_x(&context,19,cam);Re5CameraPoseRestore::Callback(&context);
 seed_camera();Re5CameraRig::Callback(&context);
 auto saved_restore=camera_poses[0].state;re5::CameraPose native_before;
 assert(saved_restore.take({cam,actor,player_runtime[0].frame.player.command,0},game_test::tick,native_before));
 assert(!camera_poses[0].lock.test_and_set());Re5CameraPoseRestore::Callback(&context);camera_poses[0].lock.clear();
 put(actors,0x60,3.f);fake_local(actor);actor_phase();
 re4::set_saved_x(&context,19,cam);Re5CameraRig::Callback(&context);Re5CameraPoseRestore::Callback(&context);
 re5::CameraPose restored;std::memcpy(restored.eye.data(),camera.data()+0x60,12);std::memcpy(restored.up.data(),camera.data()+0x70,12);std::memcpy(restored.target.data(),camera.data()+0x80,12);
 assert(!std::memcmp(&native_before,&restored,sizeof(restored)));
 wanted.players[0].first_person=false;assert(controls_snapshot.publish(wanted));
 put(actors,0x60,0.f);fake_local(actor);fake_world(actor);
 // Two input callbacks may precede one body/world phase. Consume their
 // cumulative integral rather than dropping the overwritten first sample.
 fake_world(actor);const auto before_two_inputs=anchored_looks[0];float expected_delta=0;
 for(unsigned input=0;input<2;++input){
  game_test::tick+=320000;observed_pad(&raw,3,1);Re5LocalInput::Callback(reinterpret_cast<void*>(wrapper+0x350),reinterpret_cast<void*>(actor));expected_delta+=player_runtime[0].frame.look.yaw;
 }
 re4::set_saved_x(&context,19,actor);camera_test::actor_phase(re5::profile::selected_build==re5::profile::Build::Legacy?0x24DC00:0x24DDE0,&context);
 assert(std::fabs(std::remainder(anchored_looks[0].heading-before_two_inputs.heading-expected_delta,6.28318530718f))<.000001f);
 fake_world(actor);
 // Settings/ownership/freeze/scope/native-parent guards perform no body writes.
 auto& guarded_control=player_runtime[0].frame;const auto saved_control=guarded_control;
 for(unsigned bad=0;bad<13;++bad){
  camera_frame(0,0);guarded_control=player_runtime[0].frame;
  const auto set_count_before=body_sets;
  if(bad==0)guarded_control.settings.free_camera=false;
  if(bad==1)guarded_control.settings.unified_controls=false;
  if(bad==2)guarded_control.scope=true;
  if(bad==3)put(actors,0x3F0E,std::uint8_t(1));
  if(bad==4)put(actors,0x48,std::uintptr_t(0x1234));
  if(bad==5)put(actors,0x3AD8,std::uint8_t(1));
  if(bad==6)guarded_control.heading_valid=false;
  if(bad==7)put(actors,0x56,std::uint16_t(0));
  if(bad==8)put(actors,0x80,2.f);
  if(bad==9)put(actors,0x1A71,std::uint8_t(1));
  if(bad==10)put(actors,0x1B4C,std::uint8_t(1));
  if(bad==11)put(actors,0x1AB0,std::uint8_t(1));
  if(bad==12)put(actors,0x1B40,std::uint32_t(9));
  assert(published_frames[0].publish(guarded_control));re4::set_saved_x(&context,19,actor);
  camera_test::actor_phase(re5::profile::selected_build==re5::profile::Build::Legacy?0x24DC00:0x24DDE0,&context);
  assert(body_sets==set_count_before);
  if(bad==4||bad==7||bad==9||bad==10||bad==11||bad==12){
   re4::set_saved_x(&context,19,cam+0xDB0);re4::set_saved_x(&context,20,actor);scalar(&context,9,.3f);scalar(&context,0,.4f);Re5CameraAxes::Callback(&context);
   assert(scalar(&context,9)==.3f&&scalar(&context,0)==.4f);scalar(&context,0,.25f);Re5CameraReturn::Callback(&context);assert(scalar(&context,0)==.25f);
  }
  put(actors,0x3F0E,std::uint8_t(0));put(actors,0x48,std::uintptr_t(0));put(actors,0x3AD8,std::uint8_t(0));put(actors,0x56,std::uint16_t(3));put(actors,0x80,1.f);
  put(actors,0x1A71,std::uint8_t(0));put(actors,0x1B4C,std::uint8_t(0));put(actors,0x1AB0,std::uint8_t(0));put(actors,0x1B40,std::uint32_t(0));
 }
 (void)saved_control;camera_frame(0,0);
 // A complete native quick-turn can fit between input callbacks. Releasing
 // the body stage invalidates that old epoch, including its queued integral.
 const float prior_turn=actor_yaw();put(actors,0x1B40,std::uint32_t(9));re4::set_saved_x(&context,19,actor);
 camera_test::actor_phase(re5::profile::selected_build==re5::profile::Build::Legacy?0x24DC00:0x24DDE0,&context);
 const float quick_turn=std::remainder(prior_turn+3.14159265359f,6.28318530718f);fake_heading(actor,quick_turn);put(actors,0x1B40,std::uint32_t(0));
 const auto quick_turn_sets=body_sets;
 camera_test::actor_phase(re5::profile::selected_build==re5::profile::Build::Legacy?0x24DC00:0x24DDE0,&context);
 assert(body_sets==quick_turn_sets&&std::fabs(std::remainder(actor_yaw()-quick_turn,6.28318530718f))<.000001f);
 camera_frame(0,0);assert(std::fabs(std::remainder(actor_yaw()-quick_turn,6.28318530718f))<.000001f);
 // Native desired pose can lag actor movement. A cached body-relative arm
 // follows the committed root without moving the character across the frame.
 camera_frame(0,0);
 float prior_eye[3],prior_target[3];std::memcpy(prior_eye,camera.data()+0x60,12);std::memcpy(prior_target,camera.data()+0x80,12);
 put(actors,0x60,3.f);put(actors,0x68,-2.f);actor_phase();re4::set_saved_x(&context,19,cam);Re5CameraPoseRestore::Callback(&context);seed_camera();re4::set_saved_x(&context,19,cam);Re5CameraRig::Callback(&context);
 float moved_eye[3],moved_target[3];std::memcpy(moved_eye,camera.data()+0x60,12);std::memcpy(moved_target,camera.data()+0x80,12);
 assert(std::fabs(moved_eye[0]-prior_eye[0]-3)<.00001f&&std::fabs(moved_eye[2]-prior_eye[2]+2)<.00001f);
 assert(std::fabs(moved_target[0]-prior_target[0]-3)<.00001f&&std::fabs(moved_target[2]-prior_target[2]+2)<.00001f);
 put(actors,0x60,0.f);put(actors,0x68,0.f);actor_phase();
 // Instrumentation is default-Off and uses the existing requested lease only.
 assert(!runtime_probe_recorder.active());
 const auto body_counter=unsigned(re5::RuntimeCounter::BodyHeadingApplied);
 assert(runtime_probe.counters[body_counter]==0);
 __atomic_store_n(&runtime_probe.request_sequence,1,__ATOMIC_RELEASE);runtime_probe_recorder.poll(game_test::tick);
 re4::set_saved_x(&context,19,actor);camera_test::actor_phase(re5::profile::selected_build==re5::profile::Build::Legacy?0x24DC00:0x24DDE0,&context);
 fake_heading(actor,anchored_looks[0].heading+.25f);
 camera_test::actor_phase(re5::profile::selected_build==re5::profile::Build::Legacy?0x24DC00:0x24DDE0,&context);
 assert(runtime_probe.counters[body_counter]>0);
 runtime_probe_recorder.poll(game_test::tick+re5::kRuntimeProbeTicks);assert(!runtime_probe_recorder.active());
 // Local player2 keeps a separate absolute heading and pitch, even when
 // its physical pad and native actor slot differ from player1.
 std::vector<std::uint8_t> camera2(0x3000);game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(camera2.data()),camera2.size(),3});
 const auto second_camera=reinterpret_cast<std::uintptr_t>(camera2.data());
 put(actors,0x4200+8,std::uint16_t(0x201));put(actors,0x4200+0x54,std::uint16_t(4));put(actors,0x4200+0x56,std::uint16_t(3));put(actors,0x4200+0x7C,1.f);
 for(unsigned i=0;i<3;++i)put(actors,0x4200+0x80+i*4,1.f);
 put(actors,0x4200+0x1A80,second_camera);put(camera2,0,base+0x1AD2510);put(camera2,0x130,second);put(camera2,0x198,second_camera+0xDB0);put(camera2,0xDA0,second_camera+0xDB0);put(manager,0x248,second_camera);
 fake_local(second);fake_world(second);wanted.players[1]=re4::re6_profile();wanted.players[1].gyro_enabled=false;wanted.players[1].right_deadzone=0;assert(controls_snapshot.publish(wanted));
 const auto first_player_heading=anchored_looks[0].heading;const auto first_player_pitch=anchored_looks[0].pitch;
 for(unsigned frame=0;frame<4;++frame){
  game_test::tick+=320000;raw2.rx=-16384;raw2.ry=-16384;observed_pad(&raw2,1,1);
  Re5LocalInput::Callback(reinterpret_cast<void*>(wrapper2+0x350),reinterpret_cast<void*>(second));
  re4::set_saved_x(&context,19,second);camera_test::actor_phase(re5::profile::selected_build==re5::profile::Build::Legacy?0x24DC00:0x24DDE0,&context);fake_world(second);
 }
 assert(anchored_looks[1].active&&anchored_looks[1].heading>0&&anchored_looks[1].pitch<0);
 assert(anchored_looks[0].heading==first_player_heading&&anchored_looks[0].pitch==first_player_pitch);
 // Gain0 drains an already active Modern look immediately. Walking uses the
 // camera gain and never borrows the independent weapon-aim X/Y gains.
 wanted.players[0].camera_gain=0;wanted.players[0].right_aim_gain_x=.25f;wanted.players[0].right_aim_gain_y=.75f;assert(controls_snapshot.publish(wanted));
 game_test::tick+=320000;observed_pad(&raw,3,1);Re5LocalInput::Callback(reinterpret_cast<void*>(wrapper+0x350),reinterpret_cast<void*>(actor));
 assert(player_runtime[0].frame.look.yaw==0&&player_runtime[0].frame.look.pitch==0);
 wanted.players[0].camera_gain=1;assert(controls_snapshot.publish(wanted));control.settings.camera_gain=1;control.settings.right_aim_gain_x=control.settings.right_aim_gain_y=1;
 // An input hook can call a camera hook before publishing runtime.frame.
 input_scope=control;input_scope.tick=game_test::tick;input_scope.axes.rx=.75f;input_scope.settings.free_camera=false;
 control.tick=game_test::tick-1920001;input_thread.store(1);
 re4::set_saved_x(&context,19,cam+0xDB0);scalar(&context,9,-9);publish_test_control();Re5CameraAxes::Callback(&context);
 review_check(scalar(&context,9)>.74f*32767,"R1 same-frame owned camera hook reads stale previous input");input_thread.store(0);control.tick=game_test::tick;
 control.settings.free_camera=false;control.settings.camera_return_speed=.5f;scalar(&context,0,.25f);publish_test_control();Re5CameraReturn::Callback(&context);assert(scalar(&context,0)==.5f);
 // Scope and component gates retain the native camera unchanged.
 put(actors,0x3AD8,std::uint8_t(1));const auto scoped_camera=camera;re4::set_saved_x(&context,19,cam);publish_test_control();Re5CameraRig::Callback(&context);assert(camera==scoped_camera);put(actors,0x3AD8,std::uint8_t(0));
 put(camera,0x198,cam+0xDB8);const auto foreign_component=camera;publish_test_control();Re5CameraRig::Callback(&context);assert(camera==foreign_component);put(camera,0x198,cam+0xDB0);
 mapping[56]=255;mapping[57]=255;put(bones,0x50,4.f);float head[3];assert(head_anchor(actor,head)&&head[0]==4);put(neck,0x30,second);assert(!head_anchor(actor,head));put(neck,0x30,actor);
 control.action=0x1C;
 // Capture the actual final native laser endpoint, then exercise native HUD
 // ownership, projection and complete per-polygon pre/draw/post lifecycle.
 const auto laser_address=reinterpret_cast<std::uintptr_t>(laser.data()),cockpit_address=reinterpret_cast<std::uintptr_t>(cockpit.data());
 put(actors,0x3728,laser_address);put(laser,0,base+0x1B735E0);put(laser,0x374,std::uint32_t(2));put(laser,0x274,std::uint8_t(1));put(laser,0x288,1.f);put(laser,0x298,.5f);
 re4::set_saved_x(&context,19,actor);re4::set_saved_x(&context,21,laser_address);publish_test_control();Re5RayCapture::Callback(&context);assert(rays[0].valid&&rays[0].endpoint[2]==.5f);
 const auto captured_ray=rays[0];reticle_locks[0].test_and_set();Re5RayCapture::Callback(&context);
 assert(rays[0].valid==captured_ray.valid&&rays[0].input_frame==captured_ray.input_frame&&rays[0].sequence==captured_ray.sequence);assert(reticle_locks[0].test_and_set());reticle_locks[0].clear();
 control.scope=true;publish_test_control();Re5RayCapture::Callback(&context);assert(!rays[0].valid);control.scope=false;publish_test_control();Re5RayCapture::Callback(&context);
 put(actors,0x3660,cockpit_address);put(cockpit,0,base+0x1B32820);put(cockpit,0xEB,std::uint8_t(1));put(cockpit,0x1D0,actor);
 const auto root_address=reinterpret_cast<std::uintptr_t>(root.data());std::uintptr_t root_pointer=root_address;game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(&root_pointer),8,3});
 put(main,0x1CD9FC0,reinterpret_cast<std::uintptr_t>(&root_pointer));put(main,0x1CF7A40,reinterpret_cast<std::uintptr_t>(stage.data()));
 put(command_context,0x210,std::uint64_t(1ULL<<48));put(command_context,0x14C,std::uint32_t(1280));put(command_context,0x150,std::uint32_t(720));
 gui_api={shell_init,polygon_create,gui_mode,gui_init,gui_projection,gui_pre,gui_draw,gui_post,gui_matrix,gui_matrix};control.settings.reticle=re4::ReticleStyle::Crosshair;
 publish_test_control();reticle_locks[0].test_and_set();Re5CockpitDraw::Callback(cockpit.data(),command_context.data());
 assert(gui_quads==0&&polygons_created==0&&reticle_locks[0].test_and_set());reticle_locks[0].clear();
 Re5CockpitDraw::Callback(cockpit.data(),command_context.data());assert(gui_quads==4&&polygons_created==4&&gui_phase==12);
 publish_test_control();Re5CockpitDraw::Callback(cockpit.data(),command_context.data());assert(gui_quads==4); // Once per captured ray/view.
 publish_test_control();Re5RayCapture::Callback(&context);put(root,0x584F9,std::uint8_t(0x80));publish_test_control();Re5CockpitDraw::Callback(cockpit.data(),command_context.data());assert(gui_quads==4);put(root,0x584F9,std::uint8_t(0));
 put(cockpit,0x1D0,second);publish_test_control();Re5CockpitDraw::Callback(cockpit.data(),command_context.data());assert(gui_quads==4);
 // Exercise the actual native proposal and pre-IK callbacks with a
 // real-order lower-channel binding (final ankle solver0, not temporary19).
 {
  re5_gait_test::Assets assets(base,actor,second);const auto native_gait=gait_runtime;
  gait_runtime={assets.backend(memory,writable,write_memory),re5_gait_test::Assets::frame_step};gait.reset();
  put(root,0x584E0,reinterpret_cast<std::uintptr_t>(command_context.data()));put(command_context,0x64,std::uint32_t(0));put(stage,0x2C,std::int32_t(0));put(stage,0x2AF00,std::uint32_t(0));
  for(auto& player:wanted.players){player=re4::re6_profile();player.gyro_enabled=false;player.right_deadzone=player.left_deadzone=0;}
  assert(controls_snapshot.publish(wanted));
  struct alignas(16) NativeFrame {exl::hook::InlineFloatCtx context;std::array<std::uint8_t,0x100> stack{};} native_frame;
  const auto native_frame_address=reinterpret_cast<std::uintptr_t>(&native_frame);game_test::memory_maps.push_back({native_frame_address,sizeof(native_frame),3});
  const auto proposal=native_frame_address+0x300+0x60;const re5::GaitValue position{10,20,30,99},quaternion{0,0,0,1};
  const std::uintptr_t local_actors[]{actor,second};
  for(unsigned player=0;player<2;++player){auto& data=assets.players[player];const auto current=local_actors[player];
   fake_heading(current,0);fake_local(current);fake_world(current);
   auto& view=player?camera2:camera;put(view,0x60,0.f);put(view,0x64,170.f);put(view,0x68,100.f);put(view,0x80,0.f);put(view,0x84,170.f);put(view,0x88,0.f);
   assert(((re5_gait_test::get<std::uint32_t>(data.bone(player,18)+0xA0)>>16)&255)==0);
   // Actual callbacks must use the static category, not inventory flags.
   for(unsigned category:{3u,4u}){
    const unsigned item=category==3?0x106:0x10F;re5_gait_test::put(re5_gait_test::address(data.weapon)+8,item);
    re5_gait_test::put(base+0x1CFC510+(item&255)*0x50+0x2C,category);re5_gait_test::put(current+0x1B40,std::uint32_t(0x1C));
    game_test::tick+=320000;auto& pad=player?raw2:raw;pad.lx=0;pad.ly=16384;pad.rx=pad.ry=0;observed_pad(&pad,player?1:3,1);
    Re5LocalInput::Callback(reinterpret_cast<void*>((player?wrapper2:wrapper)+0x350),reinterpret_cast<void*>(current));
    re5_gait_test::put(proposal,position);re5_gait_test::put(proposal+16,quaternion);re4::set_saved_x(&native_frame.context,19,current);
    const auto untouched=data.bones;Re5MovementProposal::Callback(&native_frame.context);Re5MovementPose::Callback(&native_frame.context);
    assert(!std::memcmp(reinterpret_cast<void*>(proposal),&position,sizeof(position))&&data.bones==untouched);
   }
   re5_gait_test::put(base+0x1CFC510+15*0x50+0x2C,std::uint32_t(1));
   for(unsigned action:{0x1Bu,0x1Cu,0x1Du,0x1Eu,0x1Fu,0x22u,0x23u,0x24u,0x25u,0x26u,0x27u}){
    re5_gait_test::put(re5_gait_test::address(data.weapon)+8,std::uint32_t(action>=0x22?(player?0x10E:0x101):0x10F));
    re5_gait_test::put(current+0x1B40,action);game_test::tick+=320000;
    auto& pad=player?raw2:raw;pad.lx=player?16384:0;pad.ly=player?0:16384;pad.rx=pad.ry=0;observed_pad(&pad,player?1:3,1);
    const auto command=(player?wrapper2:wrapper)+0x350;Re5LocalInput::Callback(reinterpret_cast<void*>(command),reinterpret_cast<void*>(current));
    re5_gait_test::put(proposal,position);re5_gait_test::put(proposal+16,quaternion);
    const auto actor_position=re5_gait_test::get<re5::GaitValue>(current+0x60);re4::set_saved_x(&native_frame.context,19,current);
    Re5MovementProposal::Callback(&native_frame.context);const auto moved=re5_gait_test::get<re5::GaitValue>(proposal);
    assert((moved.x!=position.x||moved.z!=position.z)&&moved.y==position.y&&moved.w==position.w);
    assert(!std::memcmp(reinterpret_cast<void*>(proposal+16),&quaternion,sizeof(quaternion)));
    const auto after_actor=re5_gait_test::get<re5::GaitValue>(current+0x60);assert(!std::memcmp(&actor_position,&after_actor,sizeof(actor_position)));
    // Native combat animation evaluates between collision proposal and
    // pre-IK. Replacing its fresh lower pose must not depend on prior gait.
    for(unsigned logical:{15u,16u,18u,19u,20u,22u,23u})re5_gait_test::put(data.bone(player,logical)+0x70,re5::GaitValue{0,0,0,1});
    for(unsigned logical:{19u,23u})re5_gait_test::put(data.bone(player,logical)+0x90,re5::GaitValue{0,0,0,1});
    const auto before=data.bones;
    Re5MovementPose::Callback(&native_frame.context);assert(data.bones!=before);
    const auto consumed=data.bones;Re5MovementPose::Callback(&native_frame.context);assert(data.bones==consumed);
    assert(!std::memcmp(data.bones.data(),before.data(),0xC0)); // Unowned torso/root stays native.
   }
    // The native actor can enter combat after an ordinary input frame was
    // sampled. Its old proxy marker cannot suppress the existing combat gait.
    for(unsigned live_action:{0x1Bu,0x1Fu,0x23u}){
     re5_gait_test::put(re5_gait_test::address(data.weapon)+8,std::uint32_t(live_action>=0x22?(player?0x10E:0x101):0x10F));
     re5_gait_test::put(current+0x1B40,live_action);re5_gait_test::put(current+0x3F0E,std::uint8_t(live_action==0x1F?1:0));game_test::tick+=320000;
     auto& pad=player?raw2:raw;pad.lx=player?16384:0;pad.ly=player?0:16384;pad.rx=pad.ry=0;observed_pad(&pad,player?1:3,1);
     Re5LocalInput::Callback(reinterpret_cast<void*>((player?wrapper2:wrapper)+0x350),reinterpret_cast<void*>(current));
     const auto committed=player_runtime[player].frame;auto sampled=committed;sampled.action=0;sampled.ordinary_proxy=true;assert(published_frames[player].publish(sampled));
     re5_gait_test::put(proposal,position);re5_gait_test::put(proposal+16,quaternion);re4::set_saved_x(&native_frame.context,19,current);gait.reset();
     Re5MovementProposal::Callback(&native_frame.context);const auto moved=re5_gait_test::get<re5::GaitValue>(proposal);
     review_check((moved.x!=position.x||moved.z!=position.z)&&moved.y==position.y&&moved.w==position.w,"ordinary proxy suppressed live firearm/reload/knife gait");
     review_check(!std::memcmp(reinterpret_cast<void*>(proposal+16),&quaternion,sizeof(quaternion)),"ordinary-to-combat transition changed native quaternion");
     assert(published_frames[player].publish(committed));
    }
  }
  assert(assets.samples>0&&assets.lookups>0&&assets.mirrors>0);gait_runtime=native_gait;gait.reset();
 }

 // Native component output is the sole distance owner in ordinary mode.
 // These are asymmetric final paired outputs for all six pitch-key cases;
 // literals are independent of the production distance expression.
 {
  struct Pair {std::array<float,3> eye,target,expected;};
  const Pair pairs[]={
   {{0,2,10},{0,2,0},{0,2,15}},{{-6,10,12},{2,4,-4},{-10,13,20}},
   {{8,-2,-6},{-4,6,2},{14,-6,-10}},{{4,20,30},{12,8,10},{0,26,40}},
   {{-20,7,11},{4,3,1},{-32,9,16}},{{3,18,-7},{-1,-2,5},{5,28,-13}}};
  for(unsigned player=0;player<2;++player){
   auto& view=player?camera2:camera;const auto current=player?second:actor,current_camera=player?second_camera:cam,component=current_camera+0xDB0;
   auto& frame=player_runtime[player].frame;auto& pad=player?raw2:raw;
   auto fresh=[&]{game_test::tick+=640000;frame.tick=game_test::tick;frame.valid=true;frame.scope=false;frame.action=0;
    frame.settings.free_camera=false;frame.settings.first_person=false;frame.settings.unified_controls=true;frame.settings.third_person_distance=1.5f;
    observed_pad(&pad,player?1:3,1);publish_test_control();};
   auto prepare=[&](const Pair& pair){
    for(auto& region:game_test::memory_maps)if(region.addr==current_camera)region.perm=3;
    re5_gait_test::put(component+0x130,current);re5_gait_test::put(current_camera+0x198,component);re5_gait_test::put(current_camera+0xDA0,component);re5_gait_test::put(current+0x3AD8,std::uint8_t(0));
    std::memcpy(reinterpret_cast<void*>(component+0x160),pair.eye.data(),12);std::memcpy(reinterpret_cast<void*>(component+0x180),pair.target.data(),12);
    re5_gait_test::put(component+0x170,std::array<float,3>{.2f,1.f,.3f});re5_gait_test::put(component+0x190,73.f);re5_gait_test::put(component+0x16C,std::uint32_t(0xAABBCCDD));
    // Fresh and previous transition-key caches must stay entirely native.
    std::memset(reinterpret_cast<void*>(component+0x3F0),0x5A,0x240);
   };
   auto component_phase=[&]{re4::set_saved_x(&context,19,component);camera_test::actor_phase(tested_build==re5::profile::Build::Legacy?0x24A874:0x24AA54,&context);};
   auto desired_publish=[&]{for(auto offsets:{std::pair{0x60u,0x160u},std::pair{0x70u,0x170u},std::pair{0x80u,0x180u}})
    std::memcpy(reinterpret_cast<void*>(current_camera+offsets.first),reinterpret_cast<void*>(component+offsets.second),12);};
   fresh();re4::set_saved_x(&context,19,current_camera);Re5CameraPoseRestore::Callback(&context);
   for(const auto& pair:pairs){fresh();prepare(pair);const auto before=view;component_phase();
    assert(!std::memcmp(reinterpret_cast<void*>(component+0x160),pair.expected.data(),12));
    for(std::size_t i=0;i<view.size();++i)if(i<0xDB0+0x160||i>=0xDB0+0x16C)assert(view[i]==before[i]);
    assert(!camera_poses[player].state.pending(current_camera));
   }
   for(unsigned bad=0;bad<15;++bad){fresh();prepare(pairs[0]);
    if(bad==0)frame.settings.third_person_distance=1;
    if(bad==1)frame.settings.third_person_distance=std::numeric_limits<float>::quiet_NaN();
    if(bad==2)frame.settings.third_person_distance=2.01f;
    if(bad==3)re5_gait_test::put(component+0x160,std::numeric_limits<float>::quiet_NaN());
    if(bad==4)re5_gait_test::put(component+0x180,std::numeric_limits<float>::infinity());
    if(bad==5){re5_gait_test::put(component+0x160,999999.f);re5_gait_test::put(component+0x180,-999999.f);}
    if(bad==6)std::memcpy(reinterpret_cast<void*>(component+0x160),pairs[0].target.data(),12);
    if(bad==7)re5_gait_test::put(component+0x130,player?actor:second);
    if(bad==8)re5_gait_test::put(current_camera+0x198,component+8);
    if(bad==9)re5_gait_test::put(current+0x3AD8,std::uint8_t(1));
    if(bad==10)frame.tick=game_test::tick-1920001;
    if(bad==11)frame.settings.first_person=true;
    if(bad==12){frame.settings.free_camera=true;frame.action=9;}
    if(bad==13){frame.settings.free_camera=true;frame.settings.unified_controls=false;}
    if(bad==14)for(auto& region:game_test::memory_maps)if(region.addr==current_camera)region.perm=1;
    publish_test_control();const auto before=view;component_phase();assert(view==before&&!camera_poses[player].state.pending(current_camera));
   }
   fresh();prepare(pairs[0]);component_phase();desired_publish();re4::set_saved_x(&context,19,current_camera);Re5CameraRig::Callback(&context);
   assert(re5_gait_test::get<float>(current_camera+0x68)==15.f&&!camera_poses[player].state.pending(current_camera));
   // Original native collision commits persistent camera60 after desired.
   re5_gait_test::put(current_camera+0x68,6.f);fresh();re4::set_saved_x(&context,19,current_camera);Re5CameraPoseRestore::Callback(&context);
   prepare(pairs[0]);component_phase();
   // Native243418..444 snapshots the previous POST-collision persistent pose.
   std::memcpy(reinterpret_cast<void*>(current_camera+0x210),reinterpret_cast<void*>(current_camera+0x60),12);
   assert(re5_gait_test::get<float>(current_camera+0x218)==6.f);desired_publish();re4::set_saved_x(&context,19,current_camera);Re5CameraRig::Callback(&context);
   assert(re5_gait_test::get<float>(current_camera+0x68)==15.f&&!camera_poses[player].state.pending(current_camera));
   // Native2EC1 skips desired/component production; no scaling may repeat,
   // even after the old cleanup's100ms lifetime and a fresh input frame.
   re5_gait_test::put(current_camera+0x2EC1,std::uint8_t(1));game_test::tick+=1920001;frame.tick=game_test::tick;observed_pad(&pad,player?1:3,1);publish_test_control();
   Re5CameraPoseRestore::Callback(&context);Re5CameraRig::Callback(&context);
   assert(re5_gait_test::get<float>(current_camera+0x68)==15.f&&!camera_poses[player].state.pending(current_camera));
   re5_gait_test::put(current_camera+0x2EC1,std::uint8_t(0));
  }
 }

 // Modern publishes the complete rig at the native component phase. A fresh
 // camera-only Y sample must not wait for another actor-world callback.
 for(unsigned player=0;player<2;++player){
  auto& view=player?camera2:camera;const auto current=player?second:actor,current_camera=player?second_camera:cam,component=current_camera+0xDB0,command=(player?wrapper2:wrapper)+0x350;
  auto& pad=player?raw2:raw;
  for(auto& region:game_test::memory_maps)if(region.addr==current_camera)region.perm=3;
  re5_gait_test::put(current+0x60,re5::GaitValue{0,0,0,1});re5_gait_test::put(current+0x48,std::uintptr_t(0));re5_gait_test::put(current+0x1B40,std::uint32_t(0));re5_gait_test::put(current+0x3F0E,std::uint8_t(0));
  re5_gait_test::put(current+0x2570,reinterpret_cast<std::uintptr_t>(neck.data()));put(neck,0x30,current);put(neck,0xD0,std::uint16_t(4));put(neck,0x100,std::uint16_t(56));put(neck,0x130,std::uint16_t(57));
  mapping[4]=0;mapping[56]=1;mapping[57]=2;re5_gait_test::put(current+0x8D0,reinterpret_cast<std::uintptr_t>(mapping.data()));re5_gait_test::put(current+0x8C8,reinterpret_cast<std::uintptr_t>(bones.data()));re5_gait_test::put(current+0x8C0,std::uint32_t(3));
  for(unsigned bone:{0u,1u,2u})for(unsigned axis=0;axis<3;++axis)put(bones,bone*0xC0+0x50+axis*4,axis==1?170.f:0.f);
  fake_heading(current,0);fake_local(current);fake_world(current);anchored_looks[player]={};player_runtime[player].anchored=false;player_runtime[player].look.reset();
  wanted.players[player]=re4::re6_profile();wanted.players[player].gyro_enabled=false;wanted.players[player].right_deadzone=0;wanted.players[player].third_person_distance=1.2f;assert(controls_snapshot.publish(wanted));
  auto input=[&](int y,bool commit,int x=0){game_test::tick+=320000;pad.rx=x;pad.ry=y;pad.lx=pad.ly=0;observed_pad(&pad,player?1:3,1);
   Re5LocalInput::Callback(reinterpret_cast<void*>(command),reinterpret_cast<void*>(current));
   if(commit){fake_local(current);re4::set_saved_x(&context,19,current);camera_test::actor_phase(tested_build==re5::profile::Build::Legacy?0x24DC00:0x24DDE0,&context);fake_world(current);}
  };
  auto seed=[&]{
   re5_gait_test::put(component+0x130,current);re5_gait_test::put(current_camera+0x198,component);re5_gait_test::put(current_camera+0xDA0,component);
   re5_gait_test::put(component+0x160,std::array<float,3>{0,170,-100});re5_gait_test::put(component+0x170,std::array<float,3>{0,1,0});re5_gait_test::put(component+0x180,std::array<float,3>{0,170,0});
   for(unsigned offset:{0x16Cu,0x17Cu,0x18Cu}){re5_gait_test::put(component+offset,std::uint32_t(0xABCDEF12));}
   re5_gait_test::put(component+0x190,61.f);
   std::memset(reinterpret_cast<void*>(component+0x3F0),0x69,0x240);
  };
  auto component_phase=[&]{re4::set_saved_x(&context,19,component);camera_test::actor_phase(tested_build==re5::profile::Build::Legacy?0x24A874:0x24AA54,&context);};
  auto desired=[&]{for(auto offsets:{std::pair{0x60u,0x160u},std::pair{0x70u,0x170u},std::pair{0x80u,0x180u}})std::memcpy(reinterpret_cast<void*>(current_camera+offsets.first),reinterpret_cast<void*>(component+offsets.second),12);};
  auto consume=[&]{seed();const auto before=view;std::array<std::uint8_t,0xB0> body{};std::memcpy(body.data(),reinterpret_cast<void*>(current+0x60),body.size());component_phase();
   for(std::size_t i=0;i<view.size();++i){const bool output=(i>=0xDB0+0x160&&i<0xDB0+0x16C)||(i>=0xDB0+0x170&&i<0xDB0+0x17C)||(i>=0xDB0+0x180&&i<0xDB0+0x18C);if(!output)assert(view[i]==before[i]);}
   assert(!std::memcmp(body.data(),reinterpret_cast<void*>(current+0x60),body.size()));desired();re4::set_saved_x(&context,19,current_camera);const auto native=view;Re5CameraRig::Callback(&context);assert(view==native&&!camera_poses[player].state.pending(current_camera));
  };
  input(0,true);re4::set_saved_x(&context,19,current_camera);Re5CameraPoseRestore::Callback(&context);consume();input(0,true);consume();
  // Actual native callbacks must retain diagnostic input and decisions only
  // under an explicit Monitor lease; the gameplay view remains unchanged.
  __atomic_store_n(&runtime_probe.applied_sequence,std::uint32_t(900+player),__ATOMIC_RELEASE);
  __atomic_store_n(&runtime_probe.active,1u,__ATOMIC_RELEASE);
  input(32767,true,16384);consume();
  const auto trace_input=camera_probe_copy(camera_probe.players[player].last_input);
  assert(trace_input.raw[3]==32767&&trace_input.frame==player_runtime[player].frame.frame);
  assert(camera_probe.players[player].last_success.reason==std::uint32_t(re5::CameraProbeReason::Applied));
  __atomic_store_n(&runtime_probe.active,0u,__ATOMIC_RELEASE);
  const auto body_before_pitch=anchored_looks[player];
  for(unsigned sample=0;sample<8;++sample){input(32767,false);consume();}
  assert(player_runtime[player].frame.overview_pitch>.1f&&anchored_looks[player].pitch==body_before_pitch.pitch);
  assert(re5_gait_test::get<float>(current_camera+0x84)-re5_gait_test::get<float>(current_camera+0x64)>.1f);
  // A legitimate try-lock miss must preserve the accepted X/Y integrals,
  // epoch and native arm, while never reading Euler through the held lock.
  const auto accepted=player_runtime[player].frame;const auto retained_arm=anchored_looks[player];const auto validated=player_runtime[player].anchor_validated_tick;
  const auto reads_before_busy=heading_reads;assert(validated&&!anchored_locks[player].test_and_set());
  input(32767,false,16384);assert(player_runtime[player].frame.heading_valid&&player_runtime[player].anchored);
  assert(player_runtime[player].frame.look_epoch==accepted.look_epoch&&player_runtime[player].frame.overview_pitch>accepted.overview_pitch&&player_runtime[player].frame.look_integral!=accepted.look_integral);
  seed();const auto before_busy=view;component_phase();re4::set_saved_x(&context,19,current_camera);Re5CameraRig::Callback(&context);
  assert(view==before_busy&&heading_reads==reads_before_busy&&player_runtime[player].anchor_validated_tick==validated);
  assert(anchored_looks[player].active&&anchored_looks[player].arm_valid&&anchored_looks[player].frame==retained_arm.frame&&!std::memcmp(&anchored_looks[player].arm,&retained_arm.arm,sizeof(retained_arm.arm)));
  anchored_locks[player].clear();consume();input(0,true);consume();
  assert(player_runtime[player].frame.look_epoch==accepted.look_epoch&&player_runtime[player].frame.overview_pitch>=accepted.overview_pitch);
  // Borrowing cannot create a new owner, cross a release/generation/epoch,
  // or enter a scripted action without a real validation.
  for(unsigned denied=0;denied<7;++denied){
   re5_gait_test::put(current+0x1B40,std::uint32_t(0));input(0,true);consume();auto& runtime=player_runtime[player];
   if(denied==0)runtime.frame.valid=false;
   if(denied==1)runtime.frame.player.command+=8;
   if(denied==2)++runtime.frame.player.motion.controller;
   if(denied==3)anchored_releases[player].fetch_add(1);
   if(denied==4)re5_gait_test::put(current+0x1B40,std::uint32_t(9));
   if(denied==5)runtime.frame.tick=game_test::tick+640000;
   if(denied==6)++runtime.frame.look_epoch;
   const auto reads=heading_reads;assert(!anchored_locks[player].test_and_set());input(0,false);
   assert(!runtime.frame.heading_valid&&!runtime.anchored&&!runtime.anchor_validated_tick&&heading_reads==reads);anchored_locks[player].clear();
  }
  re5_gait_test::put(current+0x1B40,std::uint32_t(0));input(0,true);consume();
  // An acquired native denial must invalidate immediately, without borrowing.
  re5_gait_test::put(current+0x1A71,std::uint8_t(1));input(0,false);
  assert(!player_runtime[player].anchored&&!player_runtime[player].anchor_validated_tick);re5_gait_test::put(current+0x1A71,std::uint8_t(0));
  input(0,true);consume();const auto lease_start=player_runtime[player].anchor_validated_tick,lease_epoch=player_runtime[player].frame.look_epoch;const auto lease_reads=heading_reads;
  assert(!anchored_locks[player].test_and_set());
  for(unsigned miss=0;miss<6;++miss){input(0,false);assert(player_runtime[player].anchored&&player_runtime[player].frame.look_epoch==lease_epoch&&player_runtime[player].anchor_validated_tick==lease_start);}
  input(0,false);assert(!player_runtime[player].anchored&&!player_runtime[player].frame.heading_valid&&!player_runtime[player].anchor_validated_tick&&heading_reads==lease_reads);anchored_locks[player].clear();
  for(unsigned bad=0;bad<14;++bad){
   re5_gait_test::put(current+0x48,std::uintptr_t(0));re5_gait_test::put(current+0x1B40,std::uint32_t(0));re5_gait_test::put(current+0x3AD8,std::uint8_t(0));
   for(auto& region:game_test::memory_maps)if(region.addr==current_camera)region.perm=3;
   input(0,true);seed();auto guarded=player_runtime[player].frame;
   if(bad==0)++guarded.look_epoch;
   if(bad==1)++guarded.generation;
   if(bad==2)++guarded.look_release;
   if(bad==3)re5_gait_test::put(current+0xD0,.01f); // Exact rotation acknowledgment remains mandatory.
   if(bad==4)re5_gait_test::put(current+0x3AD8,std::uint8_t(1));
   if(bad==5)re5_gait_test::put(current+0x1B40,std::uint32_t(9));
   if(bad==6)re5_gait_test::put(component+0x130,player?actor:second);
   if(bad==7)re5_gait_test::put(current_camera+0x198,component+8);
   if(bad==8)guarded.overview_pitch=std::numeric_limits<float>::quiet_NaN();
   if(bad==9)for(auto& region:game_test::memory_maps)if(region.addr==current_camera)region.perm=1;
   if(bad==10)guarded.action=9;
   if(bad==11)guarded.heading_valid=false;
   if(bad==12)re5_gait_test::put(component+0x170,std::numeric_limits<float>::quiet_NaN());
   if(bad==13)re5_gait_test::put(current+0x48,std::uintptr_t(0x1234));
   assert(published_frames[player].publish(guarded));const auto before=view;component_phase();assert(view==before&&!camera_poses[player].state.pending(current_camera));
   re5_gait_test::put(current+0x48,std::uintptr_t(0));re5_gait_test::put(current+0x1B40,std::uint32_t(0));re5_gait_test::put(current+0x3AD8,std::uint8_t(0));fake_world(current);
  }
  for(auto& region:game_test::memory_maps)if(region.addr==current_camera)region.perm=3;
  input(0,true);consume();re5_gait_test::put(current_camera+0x68,6.f);input(0,true);re4::set_saved_x(&context,19,current_camera);Re5CameraPoseRestore::Callback(&context);seed();component_phase();
  std::memcpy(reinterpret_cast<void*>(current_camera+0x210),reinterpret_cast<void*>(current_camera+0x60),12);assert(re5_gait_test::get<float>(current_camera+0x218)==6.f);
  desired();re4::set_saved_x(&context,19,current_camera);Re5CameraRig::Callback(&context);const auto before_skipped=view;
  game_test::tick+=1920001;input(0,true);re4::set_saved_x(&context,19,current_camera);Re5CameraPoseRestore::Callback(&context);Re5CameraRig::Callback(&context);
  assert(!std::memcmp(view.data()+0x60,before_skipped.data()+0x60,0x30)&&!camera_poses[player].state.pending(current_camera));
  // Native aim/knife/reload retains its orientation and receives distance
  // upstream, including the actual native reload controller bit0.
  for(unsigned action:{0x1Cu,0x23u,0x1Fu}){
   re5_gait_test::put(current+0x1B40,action);re5_gait_test::put(current+0x3F0E,std::uint8_t(action==0x1F?1:0));input(0,false);seed();const auto before=view;component_phase();
   assert(std::fabs(re5_gait_test::get<float>(component+0x168)+120.f)<.001f);
    assert(std::fabs(re5_gait_test::get<float>(component+0x188)+20.f)<.001f); // Same native look vector after common translation.
    for(std::size_t i=0;i<view.size();++i){const bool eye=i>=0xDB0+0x160&&i<0xDB0+0x16C,target=i>=0xDB0+0x180&&i<0xDB0+0x18C;if(!eye&&!target)assert(view[i]==before[i]);}
   desired();re4::set_saved_x(&context,19,current_camera);Re5CameraRig::Callback(&context);assert(!camera_poses[player].state.pending(current_camera));
  }
  re5_gait_test::put(current+0x3F0E,std::uint8_t(0));
  // Default-Off observation objects perform no extra memory/SVC reads or
  // payload publication, even when every optional capture method is called.
  const auto quiet=camera_probe_copy(camera_probe);const auto queries=game_test::memory_queries,ticks=camera_test::tick_calls;
  alignas(CameraDecisionCapture) std::array<unsigned char,sizeof(CameraDecisionCapture)> off_storage;off_storage.fill(0xA5);
  auto* disabled=::new(static_cast<void*>(off_storage.data())) CameraDecisionCapture(re5::CameraProbeStage::Component,current_camera,component);
  const auto* untouched=reinterpret_cast<const unsigned char*>(&disabled->value);for(unsigned byte=0;byte<sizeof(re5::CameraProbeDecision);++byte)assert(untouched[byte]==0xA5);
  disabled->~CameraDecisionCapture();
  {CameraDecisionCapture off(re5::CameraProbeStage::Component,current_camera,component);CameraInputCapture input_off;
   off.frame(player_runtime[player].frame);off.fov(component);off.native({},true);input_off.shaped({});input_off.finish(player_runtime[player].frame,pad,player_runtime[player]);}
  assert(game_test::memory_queries==queries&&camera_test::tick_calls==ticks);const auto still_quiet=camera_probe_copy(camera_probe);assert(!std::memcmp(&quiet,&still_quiet,sizeof(quiet)));
  re5_gait_test::put(current+0x1B40,std::uint32_t(0));wanted.players[player].third_person_distance=1.2f;assert(controls_snapshot.publish(wanted));
  __atomic_store_n(&runtime_probe.applied_sequence,std::uint32_t(1000+player),__ATOMIC_RELEASE);__atomic_store_n(&runtime_probe.active,1u,__ATOMIC_RELEASE);
  input(32767,true,16384);consume();
  auto observed=camera_probe_copy(camera_probe.players[player]);
  assert(observed.last_input.raw[2]==16384&&observed.last_input.raw[3]==32767&&observed.last_input.shaped[3]==1.f);
  assert(observed.last_input.filtered[3]==player_runtime[player].frame.axes.ry&&observed.last_input.pitch==player_runtime[player].frame.overview_pitch);
  assert(observed.last_input.validation_tick==player_runtime[player].anchor_validated_tick&&observed.last_nonzero_ry.frame==observed.last_input.frame);
  assert(observed.last_success.reason==std::uint32_t(re5::CameraProbeReason::Applied)&&observed.last_success.input_frame==observed.last_input.frame);
  assert(observed.last_success.available&re5::CameraHasArm);assert(observed.last_success.available&re5::CameraHasFov);
  assert(observed.last_post_desired.reason==std::uint32_t(re5::CameraProbeReason::PostDesired));
  assert(!std::memcmp(observed.last_post_desired.native.eye.data(),reinterpret_cast<void*>(current_camera+0x60),12));
  // An uncommitted rotation one-ULP mismatch retains the actual failing branch
  // and both byte-exact world reads, not a reconstructed post-hoc diagnosis.
  input(0,true);seed();const auto expected=anchored_looks[player].expected_world;
  re5_gait_test::put(current+0xD0,std::nextafter(expected[0],std::numeric_limits<float>::infinity()));const auto before_mismatch=view;component_phase();
  observed=camera_probe_copy(camera_probe.players[player]);const auto mismatch=observed.last_failure;
  assert(mismatch.reason==std::uint32_t(re5::CameraProbeReason::WorldNotCommitted)&&view==before_mismatch);
  assert(!std::memcmp(mismatch.expected_world.data(),expected.data(),64)&&!std::memcmp(mismatch.world.data(),mismatch.world_again.data(),64));
  assert(std::memcmp(mismatch.expected_world.data(),mismatch.world.data(),64)&&mismatch.input_frame==player_runtime[player].frame.frame);
  assert(!std::memcmp(mismatch.native.eye.data(),reinterpret_cast<void*>(component+0x160),12)&&mismatch.fov==61.f);
  fake_world(current);seed();camera_test::observed_world=current+0xD0;camera_test::world_read_calls=0;camera_test::world_read_mode=1;
  camera_test::query_observer=[](std::uintptr_t address,Result& result){if(address!=camera_test::observed_world)return;++camera_test::world_read_calls;
   if(camera_test::world_read_mode==1&&camera_test::world_read_calls==2){float x=0;std::memcpy(&x,reinterpret_cast<void*>(address+48),4);x=std::nextafter(x,std::numeric_limits<float>::infinity());std::memcpy(reinterpret_cast<void*>(address+48),&x,4);}
   if(camera_test::world_read_mode==2)result=1;};
  component_phase();camera_test::query_observer=nullptr;observed=camera_probe_copy(camera_probe.players[player]);
  assert(observed.last_failure.reason==std::uint32_t(re5::CameraProbeReason::WorldChanged)&&std::memcmp(observed.last_failure.world.data(),observed.last_failure.world_again.data(),64));
  fake_world(current);seed();camera_test::world_read_calls=0;camera_test::world_read_mode=2;
  camera_test::query_observer=[](std::uintptr_t address,Result& result){if(address==camera_test::observed_world){++camera_test::world_read_calls;result=1;}};
  component_phase();camera_test::query_observer=nullptr;observed=camera_probe_copy(camera_probe.players[player]);
  assert(observed.last_failure.reason==std::uint32_t(re5::CameraProbeReason::WorldRead)&&!(observed.last_failure.available&re5::CameraHasWorld));
  fake_world(current);input(0,true);seed();assert(!anchored_locks[player].test_and_set());component_phase();anchored_locks[player].clear();
  observed=camera_probe_copy(camera_probe.players[player]);assert(observed.last_failure.reason==std::uint32_t(re5::CameraProbeReason::AnchorBusy));
  // Exercise the actual exact-equality no-write path at a native signed-zero
  // identity, without changing its angle arithmetic or memcpy predicate.
  wanted.players[player].third_person_distance=1.f;assert(controls_snapshot.publish(wanted));
  fake_heading(current,0.f);fake_local(current);fake_world(current);anchored_looks[player]={};player_runtime[player].anchored=false;player_runtime[player].look.reset();player_runtime[player].overview_pitch=0;
  input(0,true);seed();component_phase();observed=camera_probe_copy(camera_probe.players[player]);
  assert(observed.last_component.reason==std::uint32_t(re5::CameraProbeReason::SamePose));
  assert(observed.reasons[std::uint32_t(re5::CameraProbeReason::Applied)]&&observed.reasons[std::uint32_t(re5::CameraProbeReason::WorldNotCommitted)]&&observed.reasons[std::uint32_t(re5::CameraProbeReason::SamePose)]);
  const auto retained_trace=camera_probe_copy(camera_probe);__atomic_store_n(&runtime_probe.active,0u,__ATOMIC_RELEASE);input(0,true);consume();
  const auto expired_trace=camera_probe_copy(camera_probe);assert(!std::memcmp(&retained_trace,&expired_trace,sizeof(retained_trace)));
 }
 // Hardware26 call40066/frame29243: the native ordinary optical direction
 // is +actorWorldZ. Replay literal IEEE754 words through the installed body
 // and component callbacks; the expectation does not call the rig math helper.
 for(unsigned player=0;player<2;++player){
  auto& view=player?camera2:camera;auto& pad=player?raw2:raw;
  const auto current=player?second:actor,current_camera=player?second_camera:cam,component=current_camera+0xDB0,command=(player?wrapper2:wrapper)+0x350;
  const std::array<std::uint32_t,16> world_bits{0x3e4b8d40,0,0x3f7ae429,0,0x80000000,0x3f800000,0,0,0xbf7ae429,0x80000000,0x3e4b8d40,0,0x4670a524,0x42cd0518,0xc6c59aa0,0x3f800000};
  std::array<float,16> world;std::memcpy(world.data(),world_bits.data(),64);
  const re5::CameraPose native{{15579.5419921875f,282.0780029296875f,-25370.283203125f},{-.14708387851715088f,.988673985004425f,.02983311004936695f},{15285.529296875f,236.53860473632812f,-25310.6484375f}};
  const std::array<float,3> anchor{15401.28515625f,263.13250732421875f,-25293.3125f};
  re5_gait_test::put(current+0x1B40,std::uint32_t(1));re5_gait_test::put(current+0x3F0E,std::uint8_t(0));re5_gait_test::put(current+0x48,std::uintptr_t(0));
  re5_gait_test::put(current+0x2570,reinterpret_cast<std::uintptr_t>(neck.data()));put(neck,0x30,current);mapping[4]=0;mapping[56]=1;mapping[57]=2;
  for(unsigned bone:{0u,1u,2u})for(unsigned axis=0;axis<3;++axis)put(bones,bone*0xC0+0x50+axis*4,anchor[axis]);
  fake_heading(current,-1.3706822395324707f);for(unsigned i=0;i<3;++i)re5_gait_test::put(current+0x60+4*i,world[12+i]);
  wanted.players[player]=re4::re6_profile();wanted.players[player].gyro_enabled=false;wanted.players[player].third_person_distance=1.2f;assert(controls_snapshot.publish(wanted));
  player_runtime[player].anchored=false;player_runtime[player].look.reset();anchored_looks[player]={};pad.lx=pad.ly=pad.rx=pad.ry=0;game_test::tick+=320000;observed_pad(&pad,player?1:3,1);
  Re5LocalInput::Callback(reinterpret_cast<void*>(command),reinterpret_cast<void*>(current));auto replay=player_runtime[player].frame;replay.overview_pitch=.004114696756005287f;player_runtime[player].frame=replay;assert(published_frames[player].publish(replay));
  re5_gait_test::put(current+0x90,world);re5_gait_test::put(current+0xD0,world);re4::set_saved_x(&context,19,current);camera_test::actor_phase(tested_build==re5::profile::Build::Legacy?0x24DC00:0x24DDE0,&context);
  assert(!std::memcmp(anchored_looks[player].expected_world.data(),world_bits.data(),64));
  re5_gait_test::put(component+0x130,current);re5_gait_test::put(current_camera+0x198,component);re5_gait_test::put(current_camera+0xDA0,component);
  re5_gait_test::put(component+0x160,native.eye);re5_gait_test::put(component+0x170,native.up);re5_gait_test::put(component+0x180,native.target);re5_gait_test::put(component+0x190,45.f);
  const auto before=view;std::array<std::uint8_t,0xB0> body{};std::memcpy(body.data(),reinterpret_cast<void*>(current+0x60),body.size());
  re4::set_saved_x(&context,19,component);camera_test::actor_phase(tested_build==re5::profile::Build::Legacy?0x24A874:0x24AA54,&context);
  re5::CameraPose result;std::memcpy(result.eye.data(),reinterpret_cast<void*>(component+0x160),12);std::memcpy(result.up.data(),reinterpret_cast<void*>(component+0x170),12);std::memcpy(result.target.data(),reinterpret_cast<void*>(component+0x180),12);
  auto optical=[&](const re5::CameraPose& pose){return std::array<double,3>{double(pose.target[0])-pose.eye[0],double(pose.target[1])-pose.eye[1],double(pose.target[2])-pose.eye[2]};};
  const auto original=optical(native),look=optical(result);const auto alignment=[&](const std::array<double,3>& v){return (v[0]*world[8]+v[2]*world[10])/(std::hypot(v[0],v[2])*std::hypot(world[8],world[10]));};
  assert(alignment(original)>.99999);review_check(alignment(look)>.99999,player?"hardware27 player2 native forward reversed":"hardware27 player1 native forward reversed");
  assert(std::fabs(std::atan2(look[1],std::hypot(look[0],look[2]))-replay.overview_pitch)<.00001);
  const auto original_screen=camera_projection27(native,anchor),result_screen=camera_projection27(result,anchor);
  assert(original_screen[2]>0&&result_screen[2]>0&&std::fabs(original_screen[0]-result_screen[0])<.00001&&std::fabs(original_screen[1]-result_screen[1])<.00001);
  assert(!std::memcmp(body.data(),reinterpret_cast<void*>(current+0x60),body.size()));
  for(std::size_t byte=0;byte<view.size();++byte){const bool output=(byte>=0xDB0+0x160&&byte<0xDB0+0x16C)||(byte>=0xDB0+0x170&&byte<0xDB0+0x17C)||(byte>=0xDB0+0x180&&byte<0xDB0+0x18C);if(!output)assert(view[byte]==before[byte]);}
  // Literal26 calls41199 and41961: same committed basis, but native collision
  // resolves current root by one ULP or a larger ground correction.
  struct RootCase{std::array<std::uint32_t,16> expected;std::array<std::uint32_t,3> current;re5::CameraPose native;float pitch;};
  const RootCase roots[]{
   {{0x3f28025d,0,0x3f4127b5,0,0x80000000,0x3f800000,0,0,0xbf4127b5,0x80000000,0x3f28025d,0,0x46704192,0x42cd0518,0xc6c56e81,0x3f800000},{0x46704192,0x42cd0518,0xc6c56e80},
    {{15493.498046875f,282.0780029296875f,-25426.125f},{-.11323579400777817f,.9886740446090698f,.09849470853805542f},{15267.14453125f,236.53860473632812f,-25229.23828125f}},-.39816999435424805f},
   {{0x3f280264,0,0x3f4127af,0,0x80000000,0x3f800000,0,0,0xbf4127af,0x80000000,0x3f280264,0,0x466a4857,0x42bf088f,0xc6c27ad2,0x3f800000},{0x466a48bd,0x42be9091,0xc6c27ae9},
    {{15105.771484375f,282.2252197265625f,-25043.529296875f},{-.14269864559173584f,.9819526076316833f,.12412121146917343f},{14881.0263671875f,224.85501098632812f,-24848.04296875f}},.02224458009004593f}
  };
  __atomic_store_n(&runtime_probe.applied_sequence,std::uint32_t(3000+player),__ATOMIC_RELEASE);__atomic_store_n(&runtime_probe.active,1u,__ATOMIC_RELEASE);
  for(const auto& fixture:roots){
   std::array<float,16> expected_world,current_world;std::memcpy(expected_world.data(),fixture.expected.data(),64);current_world=expected_world;std::memcpy(current_world.data()+12,fixture.current.data(),12);
   std::array<float,3> expected_anchor{expected_world[12],expected_world[13]+160.62255859375f,expected_world[14]};
   auto head=[&](float y){for(unsigned bone:{0u,1u,2u})for(unsigned axis=0;axis<3;++axis)put(bones,bone*0xC0+0x50+axis*4,axis==1?y:expected_anchor[axis]);};head(expected_anchor[1]);
   fake_heading(current,-.8549098968505859f);for(unsigned i=0;i<3;++i)re5_gait_test::put(current+0x60+4*i,expected_world[12+i]);player_runtime[player].anchored=false;player_runtime[player].look.reset();anchored_looks[player]={};
   game_test::tick+=320000;observed_pad(&pad,player?1:3,1);Re5LocalInput::Callback(reinterpret_cast<void*>(command),reinterpret_cast<void*>(current));replay=player_runtime[player].frame;replay.overview_pitch=fixture.pitch;player_runtime[player].frame=replay;assert(published_frames[player].publish(replay));
   re5_gait_test::put(current+0x90,expected_world);re5_gait_test::put(current+0xD0,expected_world);re4::set_saved_x(&context,19,current);camera_test::actor_phase(tested_build==re5::profile::Build::Legacy?0x24DC00:0x24DDE0,&context);assert(!std::memcmp(anchored_looks[player].expected_world.data(),fixture.expected.data(),64));
   auto component_step=[&]{re5_gait_test::put(component+0x160,fixture.native.eye);re5_gait_test::put(component+0x170,fixture.native.up);re5_gait_test::put(component+0x180,fixture.native.target);re5_gait_test::put(component+0x190,45.f);re4::set_saved_x(&context,19,component);camera_test::actor_phase(tested_build==re5::profile::Build::Legacy?0x24A874:0x24AA54,&context);return camera_probe_copy(camera_probe.players[player].last_component);};
   auto native_untouched=[&]{assert(!std::memcmp(reinterpret_cast<void*>(component+0x160),fixture.native.eye.data(),12)&&!std::memcmp(reinterpret_cast<void*>(component+0x170),fixture.native.up.data(),12)&&!std::memcmp(reinterpret_cast<void*>(component+0x180),fixture.native.target.data(),12));};
   const auto baseline=component_step();assert(baseline.reason==std::uint32_t(re5::CameraProbeReason::Applied));const auto cached=anchored_looks[player];
   re5_gait_test::put(current+0xD0,current_world);std::array<unsigned char,0xB0> body_before{};std::memcpy(body_before.data(),reinterpret_cast<void*>(current+0x60),body_before.size());const auto moved=component_step();
   const bool applied=moved.reason==std::uint32_t(re5::CameraProbeReason::Applied);review_check(applied,player?"hardware27 player2 native translation drops camera":"hardware27 player1 native translation drops camera");
   if(applied){
    assert(!std::memcmp(moved.expected_world.data(),fixture.expected.data(),64)&&!std::memcmp(moved.world.data(),current_world.data(),64)&&!std::memcmp(moved.world.data(),moved.world_again.data(),64));
    assert(moved.input_frame==moved.body_frame&&moved.look_epoch==moved.body_epoch&&moved.generation==moved.body_generation);
    for(unsigned axis=0;axis<3;++axis){const float delta=current_world[12+axis]-expected_world[12+axis];assert(moved.anchor[axis]==current_world[12+axis]+(axis==1?cached.height:0.f));assert(std::fabs(moved.result.eye[axis]-baseline.result.eye[axis]-delta)<.005f&&std::fabs(moved.result.target[axis]-baseline.result.target[axis]-delta)<.005f);}
    const auto old_screen=camera_projection27(baseline.result,baseline.anchor),new_screen=camera_projection27(moved.result,moved.anchor);assert(old_screen[2]>0&&new_screen[2]>0&&std::fabs(old_screen[0]-new_screen[0])<.00001&&std::fabs(old_screen[1]-new_screen[1])<.00001);
    const auto sight=optical(moved.result);assert((sight[0]*current_world[8]+sight[2]*current_world[10])/(std::hypot(sight[0],sight[2])*std::hypot(current_world[8],current_world[10]))>.99999);assert(std::fabs(std::atan2(sight[1],std::hypot(sight[0],sight[2]))-fixture.pitch)<.00001);
   }
   assert(!std::memcmp(body_before.data(),reinterpret_cast<void*>(current+0x60),body_before.size()));
   // Every orientation/homogeneous word remains exact, including padding.
   for(unsigned word=0;word<16;++word)if(word<12||word==15){auto invalid_world=current_world;invalid_world[word]=std::nextafter(invalid_world[word],std::numeric_limits<float>::infinity());re5_gait_test::put(current+0xD0,invalid_world);const auto denied=component_step();assert(denied.reason==std::uint32_t(re5::CameraProbeReason::WorldNotCommitted));native_untouched();}
   // Rejected current roots/anchors may not poison either a valid cache or a
   // newly seeded height/arm. Use all axes and both finite bounds.
   for(bool ready:{false,true})for(unsigned axis=0;axis<3;++axis)for(float bad:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),1000001.f,-1000001.f}){
    anchored_looks[player]=cached;if(!ready){anchored_looks[player].anchor=false;anchored_looks[player].arm_valid=false;anchored_looks[player].height=7;}
    const auto before_cache=anchored_looks[player];auto invalid_world=current_world;invalid_world[12+axis]=bad;re5_gait_test::put(current+0xD0,invalid_world);const auto denied=component_step();assert(denied.reason!=std::uint32_t(re5::CameraProbeReason::Applied)&&!std::memcmp(&before_cache,&anchored_looks[player],sizeof(before_cache)));native_untouched();
   }
   anchored_looks[player]=cached;anchored_looks[player].anchor=false;anchored_looks[player].arm_valid=false;anchored_looks[player].height=7;const auto before_anchor=anchored_looks[player];auto invalid_anchor=current_world;invalid_anchor[13]=999999.f;head(1000010.f);re5_gait_test::put(current+0xD0,invalid_anchor);const auto denied=component_step();assert(denied.reason!=std::uint32_t(re5::CameraProbeReason::Applied)&&!std::memcmp(&before_anchor,&anchored_looks[player],sizeof(before_anchor)));native_untouched();head(expected_anchor[1]);anchored_looks[player]=cached;re5_gait_test::put(current+0xD0,current_world);
  }
  __atomic_store_n(&runtime_probe.active,0u,__ATOMIC_RELEASE);
 }
 for(unsigned player=0;player<2;++player){
  const auto current=player?second:actor,current_camera=player?second_camera:cam,component=current_camera+0xDB0,command=(player?wrapper2:wrapper)+0x350;
  auto& pad=player?raw2:raw;
  re5_gait_test::put(current+0x48,std::uintptr_t(0));re5_gait_test::put(current+0x1B40,std::uint32_t(1));re5_gait_test::put(current+0x3F0E,std::uint8_t(0));re5_gait_test::put(current+0x3AD8,std::uint8_t(0));
  re5_gait_test::put(current_camera+0x130,current);re5_gait_test::put(current_camera+0x198,component);re5_gait_test::put(current_camera+0xDA0,component);re5_gait_test::put(component+0x130,current);
  re5_gait_test::put(current+0x2570,reinterpret_cast<std::uintptr_t>(neck.data()));put(neck,0x30,current);mapping[4]=0;mapping[56]=1;mapping[57]=2;
  wanted.players[player]=re4::re6_profile();wanted.players[player].gyro_enabled=false;wanted.players[player].right_deadzone=0;wanted.players[player].third_person_distance=1.2f;assert(controls_snapshot.publish(wanted));
  player_runtime[player].anchored=false;player_runtime[player].look.reset();anchored_looks[player]={};fake_heading(current,0);
  auto publish=[&](std::array<float,3> position,float heading,unsigned action=1,bool reset=false){
   if(reset){player_runtime[player].frame.valid=false;player_runtime[player].anchored=false;player_runtime[player].look.reset();anchored_looks[player]={};}
   re5_gait_test::put(current+0x1B40,action);re5_gait_test::put(current+0x3F0E,std::uint8_t(action==0x1F?1:0));
   re5_gait_test::put(current+0x60,position);fake_heading(current,heading);fake_local(current);fake_world(current);
   for(unsigned bone:{0u,1u,2u})for(unsigned axis=0;axis<3;++axis)put(bones,bone*0xC0+0x50+axis*4,position[axis]+(axis==1?170.f:0.f));
   game_test::tick+=320000;pad.lx=pad.ly=pad.rx=pad.ry=0;observed_pad(&pad,player?1:3,1);Re5LocalInput::Callback(reinterpret_cast<void*>(command),reinterpret_cast<void*>(current));
   player_runtime[player].frame.look_integral=heading;player_runtime[player].frame.native_heading=0;
   if(action>8)player_runtime[player].frame.heading_valid=false;
   assert(published_frames[player].publish(player_runtime[player].frame));
   re4::set_saved_x(&context,19,current);camera_test::actor_phase(tested_build==re5::profile::Build::Legacy?0x24DC00:0x24DDE0,&context);fake_world(current);
  };
  re5_follow_test::run(player,current,current_camera,publish);
  re5_locomotion_test::run(player,current,current_camera,(player?wrapper2:wrapper)+0x350,player?raw2:raw,wanted);
  re5_combat_camera_test::run(player,current,current_camera,reinterpret_cast<std::uintptr_t>(laser.data()),publish);
 }
 assert(review_failures==0);
 std::puts("RE5 actual module: guarded hooks, cold settings, native resource ownership and live graphics PASS");
}
