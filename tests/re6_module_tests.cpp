// Exercise the production read-only adapter with bounded synthetic game memory.
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
void test_Re6FullPad(Re6NpadState*,const std::uint32_t&);
void test_Re6HandheldPad(Re6NpadState*,const std::uint32_t&);
void test_Re6DualPad(Re6NpadState*,const std::uint32_t&);
void test_Re6CameraUpdate(void*);
}
#include "../source/re6/module.cpp"
#undef assert
#define assert(c) do {if(!(c)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#c);std::exit(1);}}while(0)
namespace {
Re6NpadState supplied{};
unsigned pad_calls=0,camera_calls=0;
bool change_view=false;
void test_Re6FullPad(Re6NpadState* out,const std::uint32_t&){++pad_calls;if(out)*out=supplied;}
void test_Re6HandheldPad(Re6NpadState* out,const std::uint32_t& id){test_Re6FullPad(out,id);}
void test_Re6DualPad(Re6NpadState* out,const std::uint32_t& id){test_Re6FullPad(out,id);}
void test_Re6CameraUpdate(void* manager){
 ++camera_calls;
 if(change_view&&manager){float value=123;std::memcpy(static_cast<std::uint8_t*>(manager)+0x110,&value,4);}
}
bool direct_read(void*,std::uint64_t address,void* out,std::size_t size){std::memcpy(out,reinterpret_cast<const void*>(address),size);return true;}
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
 sdk[re6::profile::sdk_sites[0].offset]^=1;exl_main(nullptr,nullptr);assert(game_test::installed==0);sdk[re6::profile::sdk_sites[0].offset]^=1;
 game_test::program=0;exl_main(nullptr,nullptr);assert(game_test::installed==0);game_test::program=re6::profile::title_id;
 exl_main(nullptr,nullptr);assert(game_test::installed==4&&game_test::initialized==1);
 for(auto* region:{&main,&sdk,&manager,&players,&actor,&camera})game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(region->data()),region->size(),3});
 const auto actor_ptr=reinterpret_cast<std::uintptr_t>(actor.data()),camera_ptr=reinterpret_cast<std::uintptr_t>(camera.data());
 put(main,re6::profile::camera_singleton,reinterpret_cast<std::uintptr_t>(manager.data()));
 put(main,re6::profile::player_singleton,reinterpret_cast<std::uintptr_t>(players.data()));
 put(manager,0x48,camera_ptr);put(manager,0x60,std::uint8_t(1));put(manager,0xd7c,std::uint32_t(3));
 put(players,0x30+3*8,actor_ptr);put(actor,0xc,std::uint8_t(1));put(actor,0x14,std::uint32_t(1));put(actor,0x3a28,camera_ptr);
 for(unsigned i=0;i<16;++i){put(actor,0xd0+i*4,float(i+1));put(manager,0xe0+i*4,float(20+i));put(manager,0x120+i*4,float(40+i));}
 const auto actor_before=actor,players_before=players;
 supplied={77,0x123456789abcdef0ULL,-32768,32767,1234,-5678,1,0};
 game_test::tick=1000;Re6NpadState delivered{};Re6FullPad::Callback(&delivered,0);assert(!std::memcmp(&delivered,&supplied,sizeof(delivered))&&pad_calls==1);
 re6::RemoteReader reader{nullptr,direct_read};re6::Cursor cursor;
 const auto wire=reinterpret_cast<std::uintptr_t>(&observer_ring.storage);
 assert(cursor.attach(reader,wire,re6::profile::build_id)==re6::ReadStatus::Empty);
 game_test::tick=1100;change_view=true;Re6CameraUpdate::Callback(manager.data());assert(camera_calls==1);
 re6::Sample sample;assert(cursor.read_next(reader,sample)==re6::ReadStatus::Record);
 const auto& r=sample.record;
 assert(r.frame==1&&r.tick==1100&&r.input_tick==1000&&r.sample_id==77&&r.controller_id==0&&r.controller_style==1);
 assert(r.buttons==supplied.buttons&&r.sticks[0]==-32768&&r.sticks[3]==-5678);
 assert(r.validity==(re6::InputValid|re6::PlayerValid|re6::CameraValid|re6::ProjectionValid));
 assert(r.actor==actor_ptr&&r.camera==camera_ptr&&r.player[3]==13&&r.player[7]==14&&r.player[11]==15);
 assert(r.view[3]==123&&r.view[1]==24&&r.projection[1]==44&&r.projection[15]==55);
 assert(actor==actor_before&&players==players_before);
 // Nonprimary controllers cannot replace the observed primary sample.
 supplied.sample=88;supplied.lx=999;game_test::tick=1200;Re6DualPad::Callback(&delivered,1);
 game_test::tick=1300;Re6CameraUpdate::Callback(manager.data());assert(cursor.read_next(reader,sample)==re6::ReadStatus::Record);assert(sample.record.sample_id==77&&sample.record.sticks[0]==-32768);
 // Handheld uses its actual id/style; its state is never modified.
 game_test::tick=1400;Re6HandheldPad::Callback(&delivered,0x20);game_test::tick=1500;Re6CameraUpdate::Callback(manager.data());
 assert(cursor.read_next(reader,sample)==re6::ReadStatus::Record);assert(sample.record.sample_id==88&&sample.record.controller_id==0x20&&sample.record.controller_style==2&&sample.record.input_tick==1400);
 // A stale poll, disconnected pad, or backwards tick is not valid input.
 game_test::tick=1400+re6::profile::input_max_age+1;Re6CameraUpdate::Callback(manager.data());assert(cursor.read_next(reader,sample)==re6::ReadStatus::Record);assert(!(sample.record.validity&re6::InputValid));
 supplied.attributes=0;Re6FullPad::Callback(&delivered,0);Re6CameraUpdate::Callback(manager.data());assert(cursor.read_next(reader,sample)==re6::ReadStatus::Record);assert(!(sample.record.validity&re6::InputValid));
 // Invalid actor ownership does not manufacture an association with the camera.
 put(actor,0x3a28,std::uintptr_t(0));Re6CameraUpdate::Callback(manager.data());assert(cursor.read_next(reader,sample)==re6::ReadStatus::Record);assert(!(sample.record.validity&re6::PlayerValid));
 put(actor,0x3a28,camera_ptr);put(actor,0xc,std::uint8_t(0));Re6CameraUpdate::Callback(manager.data());assert(cursor.read_next(reader,sample)==re6::ReadStatus::Record);assert(!(sample.record.validity&re6::PlayerValid));
 // Non-finite matrices are reported through validity; no pose repair or game writes.
 put(actor,0xc,std::uint8_t(1));put(actor,0xd0,std::numeric_limits<float>::infinity());put(manager,0xe0,std::numeric_limits<float>::quiet_NaN());
 Re6CameraUpdate::Callback(manager.data());assert(cursor.read_next(reader,sample)==re6::ReadStatus::Record);assert(sample.record.validity&re6::PlayerNonFinite);assert(sample.record.validity&re6::CameraNonFinite);assert(!(sample.record.validity&re6::CameraValid));
 // Disabled view and wrong manager retain only the sample metadata, never stale matrices.
 put(manager,0x60,std::uint8_t(0));Re6CameraUpdate::Callback(manager.data());assert(cursor.read_next(reader,sample)==re6::ReadStatus::Record);assert(!(sample.record.validity&(re6::PlayerValid|re6::CameraValid|re6::ProjectionValid)));
 Re6CameraUpdate::Callback(nullptr);assert(cursor.read_next(reader,sample)==re6::ReadStatus::Empty);
 // A concurrent/reentrant update cannot turn the journal into a multi-producer ring.
 observer_capture.test_and_set(std::memory_order_acquire);Re6CameraUpdate::Callback(manager.data());
 assert(cursor.read_next(reader,sample)==re6::ReadStatus::Empty);observer_capture.clear(std::memory_order_release);
 std::puts("RE6 native observer tests passed");
}
