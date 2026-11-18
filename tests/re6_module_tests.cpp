#include "native_journal_probe.hpp"
// Exercise guarded graphics callbacks without automatic input/frame capture.
#include "lib.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <limits>
namespace exl::util {
enum class ModuleIndex { Sdk };
inline game_test::Main sdk_module;
inline bool sdk_present=true;
inline bool HasModule(ModuleIndex){return sdk_present;}
inline const auto& GetModuleInfo(ModuleIndex){return sdk_module;}
}
namespace {
struct Re6NpadState;
// Unexpected pad hooks remain callable in the fixture, so installation count
// catches their registration as a behavioral failure.
[[maybe_unused]] void test_Re6FullPad(Re6NpadState*,const std::uint32_t&){}
[[maybe_unused]] void test_Re6HandheldPad(Re6NpadState*,const std::uint32_t&){}
[[maybe_unused]] void test_Re6DualPad(Re6NpadState*,const std::uint32_t&){}
void test_Re6CameraUpdate(void*);
void test_Re6SamplerCreate(void*,const std::uint8_t*);
}
#include "../source/re6/module.cpp"
#undef assert
#define assert(c) do {if(!(c)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#c);std::exit(1);}}while(0)
namespace {
unsigned camera_calls=0;
std::array<std::uint8_t,0x38> sampler_seen{};
void test_Re6SamplerCreate(void*,const std::uint8_t* descriptor){if(descriptor)std::memcpy(sampler_seen.data(),descriptor,sampler_seen.size());}
bool change_view=false;
void test_Re6CameraUpdate(void* manager){
 ++camera_calls;
 if(change_view&&manager){float value=123;std::memcpy(static_cast<std::uint8_t*>(manager)+0x110,&value,4);}
}
template<class T> void put(std::vector<std::uint8_t>& memory,std::size_t offset,const T& value){std::memcpy(memory.data()+offset,&value,sizeof(value));}
void seed(std::vector<std::uint8_t>& memory,const re6::BuildId& build,std::size_t offset,const re6::profile::Site* sites,std::size_t count){
 std::memcpy(memory.data()+offset,build.data(),build.size());for(std::size_t i=0;i<count;++i)put(memory,sites[i].offset,sites[i].instruction);
}
}
int main(){
 std::vector<std::uint8_t> main(re6::profile::main_size),sdk(re6::profile::sdk_size),manager(0xda0),players(0x100),actor(0x3a40),camera(0x200);
 seed(main,re6::profile::build_id,re6::profile::build_id_offset,re6::profile::main_sites,std::size(re6::profile::main_sites));
 seed(sdk,re6::profile::sdk_build_id,re6::profile::sdk_build_id_offset,re6::profile::sdk_sites,std::size(re6::profile::sdk_sites));
 assert(re6::profile::matches(main.data(),main.size()));assert(re6::profile::sdk_matches(sdk.data(),sdk.size()));
 assert(!re6::profile::matches(main.data(),re6::profile::build_id_offset+31));
 main[re6::profile::main_sites[0].offset]^=1;assert(!re6::profile::matches(main.data(),main.size()));main[re6::profile::main_sites[0].offset]^=1;
 game_test::module.m_Total={reinterpret_cast<std::uintptr_t>(main.data()),main.size()};
 exl::util::sdk_module.m_Total={reinterpret_cast<std::uintptr_t>(sdk.data()),sdk.size()};
 game_test::program=re6::profile::title_id;
 for(auto offset:{re6::profile::build_id_offset,re6::profile::main_sites[0].offset}){
  main[offset]^=1;exl_main(nullptr,nullptr);assert(game_test::installed==0&&game_test::initialized==0);main[offset]^=1;
 }
 exl::util::sdk_present=false;exl_main(nullptr,nullptr);assert(game_test::installed==0);exl::util::sdk_present=true;
 for(auto offset:{re6::profile::sdk_build_id_offset,re6::profile::sdk_sites[0].offset}){
  sdk[offset]^=1;exl_main(nullptr,nullptr);assert(game_test::installed==0&&game_test::initialized==0);sdk[offset]^=1;
 }
 game_test::program=0;exl_main(nullptr,nullptr);assert(game_test::installed==0);game_test::program=re6::profile::title_id;
 exl_main(nullptr,nullptr);assert(game_test::installed==2&&game_test::initialized==1);
 assert((game_test::installed_offsets==std::vector<std::size_t>{0x16038E0,0x1673C20}));
 exl_main(nullptr,nullptr);assert(game_test::installed==2&&game_test::initialized==1);
 assert(graphics_control.status==0); // Bootstrap owns the settings wire until ready.
 // Camera callbacks retain native updates and frame/status publication, with
 // no memory-map probes, matrix reads or journal storage/capture.
 const auto actor_before=actor,players_before=players;
 change_view=true;game_test::tick=1100;
 const auto reads_before=game_test::memory_queries;
 Re6CameraUpdate::Callback(manager.data());
 assert(camera_calls==1&&graphics_control.frames==1);
 float native_view=0;std::memcpy(&native_view,manager.data()+0x110,4);assert(native_view==123);
 Re6CameraUpdate::Callback(manager.data());assert(camera_calls==2&&graphics_control.frames==2);
 assert(game_test::memory_queries==reads_before&&actor==actor_before&&players==players_before);
 assert(native_journal_test::storage_instances==0&&native_journal_test::initializations==0&&native_journal_test::captures==0);
 // The actual constructor adapter must receive a private native descriptor,
 // preserving the game's original buffer and its nonfilter fields.
 auto control=re6::initial_shared_control();control.status=1;control.capabilities=re6::kCapabilityFiltering;
 std::memcpy(const_cast<re6::SharedControl*>(&graphics_control),&control,sizeof(control));
 re6::Settings settings;settings.texture_filtering=re6::TextureFilter::Af8x;std::uint32_t sequence=0;
 assert(re6::publish_settings(graphics_control,settings,sequence));
 std::array<std::uint8_t,0x38> descriptor{};descriptor[0]=7;descriptor[0x20]=91;
 const auto original=descriptor;Re6SamplerCreate::Callback(nullptr,descriptor.data());
 std::uint32_t flags=0;std::memcpy(&flags,sampler_seen.data(),4);
 assert((flags&0xff)==8&&((flags>>17)&31)==9&&sampler_seen[0x20]==91);
 assert(descriptor==original);
 settings.texture_filtering=re6::TextureFilter::Af16x;
 assert(re6::publish_settings(graphics_control,settings,sequence));Re6SamplerCreate::Callback(nullptr,descriptor.data());
 std::memcpy(&flags,sampler_seen.data(),4);assert(((flags>>17)&31)==9); // A live edit waits for the next process.
 // Manual AA modes must never install a render hook or edit game AA fields.
 std::vector<std::uint8_t> renderer(0x1099770),scene(0x600);
 put(main,0x2A5FD90,reinterpret_cast<std::uintptr_t>(renderer.data()));
 put(renderer,0x3CC,std::uint32_t(2));put(scene,0x5E4,std::uint32_t(2));
 const auto renderer_before=renderer,scene_before=scene;
 for(unsigned mode=0;mode<5;++mode){
  settings.anti_aliasing=re6::AntiAliasing(mode);
  assert(re6::publish_settings(graphics_control,settings,sequence));
  poll_graphics_settings();
  assert(graphics_settings.anti_aliasing==re6::AntiAliasing::Native);
  assert(renderer==renderer_before&&scene==scene_before&&game_test::installed==2);
 }
 // Cold launch loads the separate RE6 file before latching sampler policy.
 for(bool backup:{false,true}){
  control=re6::initial_shared_control();control.capabilities=re6::kCapabilityFiltering;
  std::memcpy(const_cast<re6::SharedControl*>(&graphics_control),&control,sizeof(control));
  graphics_settings={};graphics_sequence=0;graphics_loaded.store(false);graphics_latched.store(255);
  graphics_mounted=false;graphics_retry=0;graphics_attempts=0;game_test::mount_result=0;
  re6::Settings saved;saved.texture_filtering=re6::TextureFilter::Af4x;saved.anti_aliasing=re6::AntiAliasing::Fxaa3High;
  game_test::settings_primary.resize(re6::kSettingsBytes);assert(re6::encode_settings(saved,game_test::settings_primary.data(),re6::kSettingsBytes));
  game_test::settings_backup=backup?game_test::settings_primary:std::vector<std::uint8_t>{};
  if(backup)game_test::settings_primary[0]^=1;
  Re6SamplerCreate::Callback(nullptr,descriptor.data());std::memcpy(&flags,sampler_seen.data(),4);
  assert(((flags>>17)&31)==4&&graphics_loaded.load()&&graphics_control.settings_source==(backup?2u:1u));
  assert(graphics_control.status==1);
  assert(graphics_control.applied_sequence==graphics_control.sequence&&graphics_settings.anti_aliasing==re6::AntiAliasing::Native);
 }
 std::puts("RE6 guarded graphics callbacks without frame journals passed");
}
