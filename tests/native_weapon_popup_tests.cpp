#include "re4/native_weapon_popup.hpp"
#define main game_module_baseline_main
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#include "game_module_tests.cpp"
#pragma GCC diagnostic pop
#undef main
namespace {
using u8=std::uint8_t;using u16=std::uint16_t;using u32=std::uint32_t;
std::vector<re4::NativeInventoryGlyph> native_popup_packets;
std::vector<std::array<unsigned,4>> popup_blends;
unsigned alias_capacity_calls=0,alias_file_calls=0;
std::uint64_t alias_capacity=0,alias_file_bytes=0;
bool alias_file_ok=true;
struct NativePopupFixture:Fixture {
 std::vector<u8> core=std::vector<u8>(0x4000);
 std::array<std::array<std::int8_t,2>,512> metrics{};
 template<class T>void put(void* p,std::size_t at,T value){std::memcpy(static_cast<u8*>(p)+at,&value,sizeof(value));}
 NativePopupFixture(){
  view_mode();settings.dpad_weapon_switching=true;publish_view_settings();game_test::tick=100;
  native_popup_packets.clear();popup_blends.clear();weapon_popup_capture_native={};
  const auto font=image.data()+0x124FAB0,manager=image.data()+0x124FFB0,bank=core.data()+0x100;
  put(input_state.data(),0x60,core.data());put(core.data(),0x54,u32(0x100));put(core.data(),0x58,u32(0x3000));
  put(manager,0,std::int32_t(1));put(manager,0x20,bank);put(manager,0x44,u32(1));manager[0x53]=1;
  put(bank,4,u32(0x110));
  for(unsigned raw=0;raw<0x110;++raw)put(bank,8+raw*8,u64(0x1000));
  const std::array<u16,8> name{0,0xA7,0x80,0x1AC,0x80,14,8,1}; // Native Latin + native localized glyph300.
  std::memcpy(bank+0x1000,name.data(),sizeof(name));
  for(auto& metric:metrics)metric={0,32};
  font[0]=1;font[0xFC]=font[0xFD]=64;put(font,0xF8,u16(2048));put(font,0xFA,u16(1024));put(font,0xF0,metrics.data());
  put(image.data(),0xB00614,u32(0xE5D9CFF0));
  game_test::memory_maps={{main_base,image.size(),3},{reinterpret_cast<std::uintptr_t>(actor),sizeof(actor),3},
   {reinterpret_cast<std::uintptr_t>(input_state.data()),input_state.size(),3},
   {reinterpret_cast<std::uintptr_t>(core.data()),core.size(),3},{reinterpret_cast<std::uintptr_t>(metrics.data()),sizeof(metrics),3}};
  inventory_damage_glyph_native.draw=+[](const re4::NativeInventoryGlyph* p){
   assert((popup_blends.back()==std::array<unsigned,4>{1,1,0,0})); // Mist state restored before native font.
   native_popup_packets.push_back(*p);};
  native_gx_begin=+[](unsigned primitive,unsigned format,unsigned count){assert(primitive==0x90&&!format&&count==24*4*6);};
  native_gx_position=+[](float,float,float){};
  native_gx_color=+[](unsigned r,unsigned g,unsigned b,unsigned alpha){assert(r==155&&g==153&&b==149&&alpha<=46);};
  native_gx_end=+[](unsigned token){assert(token==0x34);};
  native_gx_blend=+[](unsigned a,unsigned b,unsigned c,unsigned d){popup_blends.push_back({a,b,c,d});};
  weapon_popup_commit(actor,35);
 }
 void draw(u64 elapsed=9'600'000){game_test::tick=100+elapsed;HudDraw::Callback(nullptr);}
 void japanese(std::vector<u8>& alternate){
  weapon_popup_native_reset();alias_capacity_calls=alias_file_calls=0;alias_file_ok=true;
  alias_file_bytes=alternate.size();alias_capacity=alternate.size();
  assert(!(reinterpret_cast<std::uintptr_t>(alternate.data())&15));
  put(image.data()+0x124FFB0,0,std::int32_t(0));put(image.data()+0x124FFB0,0x30,alternate.data());
  put(image.data()+0x124FFB0,0x4C,u32(0));image[0x124FFB0+0x55]=1;
  put(image.data(),0xB27538,image.data()+0x1240E20);put(image.data(),0x6EBDE0,u16(71));put(image.data(),0x6EBDE8,u16(72));
  std::memcpy(character_mount,"Contents",9);character_mount_ready.store(true);
  put(alternate.data(),4,u32(0x100));put(alternate.data()+0x100,4,u32(73));
  put(alternate.data()+0x100,8+71*8,u64(0x300));put(alternate.data()+0x100,8+72*8,u64(0x320));
  // Native RETURN14 ends this entry even when more packed messages follow.
  const std::array<u16,7> name0{0,0xA7,14,0,0x1AC,3,1};const std::array<u16,3> name1{0,0x1AC,1};
  std::memcpy(alternate.data()+0x400,name0.data(),sizeof(name0));std::memcpy(alternate.data()+0x420,name1.data(),sizeof(name1));
  game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(alternate.data()),alternate.size(),3});
  weapon_popup_name_native.allocation_size=+[](const void* allocator,const void* allocation)->std::size_t{
   ++alias_capacity_calls;assert(allocator==reinterpret_cast<const void*>(main_base+0x1240E20)&&!(reinterpret_cast<std::uintptr_t>(allocation)&15));return std::size_t(alias_capacity);};
  weapon_popup_name_native.file_size=+[](const char* path,std::uint64_t& bytes){
   ++alias_file_calls;assert(!std::strcmp(path,"Contents:/bio4/font/trc.mdt"));bytes=alias_file_bytes;return alias_file_ok;};
  weapon_popup_commit(actor,33);
 }
};
}
int main(){
 {
  NativePopupFixture f;std::vector<u8> alternate(0x800);f.japanese(alternate);f.draw();
  assert(native_popup_packets.size()==1&&native_popup_packets[0].glyph==39);
  assert(alias_capacity_calls==1&&alias_file_calls==1);
  native_popup_packets.clear();f.draw();assert(native_popup_packets.size()==1&&alias_capacity_calls==2&&alias_file_calls==1);
  game_test::tick=100;weapon_popup_commit(f.actor,64);native_popup_packets.clear();f.draw();
  assert(native_popup_packets.size()==1&&native_popup_packets[0].glyph==300);
 }
 {
  NativePopupFixture f;std::vector<u8> alternate(0x800);f.japanese(alternate);alias_file_ok=false;f.draw();
  assert(native_popup_packets.empty()&&alias_file_calls==1&&popup_blends.empty());
  alias_file_ok=true;f.draw();assert(native_popup_packets.size()==1&&alias_file_calls==2); // Failed size never cached.
 }
 {
  NativePopupFixture f;std::vector<u8> alternate(0x800);f.japanese(alternate);alias_file_bytes=0x900;f.draw();
  assert(native_popup_packets.empty()&&popup_blends.empty());
  alias_file_bytes=alternate.size();f.draw();assert(native_popup_packets.size()==1&&alias_file_calls==2);
 }
 {
  NativePopupFixture f;std::vector<u8> alternate(0x800);f.japanese(alternate);alias_file_bytes=0x400;f.draw();
  assert(native_popup_packets.empty()&&popup_blends.empty()); // Message lies in valid allocation padding, beyond exact file.
 }
 {
  NativePopupFixture f;std::vector<u8> alternate(0x800);f.japanese(alternate);alias_file_bytes=1;f.draw();
  assert(native_popup_packets.empty()&&popup_blends.empty()); // Short header cannot underflow nested-bank bounds.
 }
 {
  NativePopupFixture f;std::vector<u8> alternate(0x800);f.japanese(alternate);
  f.put(alternate.data(),0x402,u16(3));f.draw();assert(native_popup_packets.empty()&&popup_blends.empty()); // Reject unsupported inline control before RETURN.
 }
 {
  NativePopupFixture f;std::vector<u8> alternate(0x800);f.japanese(alternate);
  f.put(f.image.data()+0x124FFB0,0x30,alternate.data()+1);f.draw();
  assert(native_popup_packets.empty()&&alias_capacity_calls==0&&alias_file_calls==0);
 }
 {
  NativePopupFixture f;std::vector<u8> alternate(0x800);f.japanese(alternate);f.draw();assert(alias_file_calls==1);
  native_popup_packets.clear();alias_capacity=0x300;f.draw();assert(native_popup_packets.empty()); // Cached file bound never substitutes current allocation extent.
 }
 {
  NativePopupFixture f;std::vector<u8> alternate(0x800);f.japanese(alternate);f.draw();
  weapon_popup_native_reset();native_popup_packets.clear();f.draw();assert(native_popup_packets.size()==1&&alias_file_calls==2);
 }
 {
  NativePopupFixture f;std::vector<u8> alternate(0x800);f.japanese(alternate);
  const auto original=weapon_popup_name_native.file_size;
  weapon_popup_name_native.file_size=+[](const char*,std::uint64_t& bytes){
   ++alias_file_calls;bytes=alias_file_bytes;
   auto source=character_value<std::uintptr_t>(reinterpret_cast<void*>(main_base+0x124FFB0),0x30)+16;
   character_store(reinterpret_cast<void*>(main_base+0x124FFB0),0x30,source);return true;};
  f.draw();assert(native_popup_packets.empty()&&!weapon_popup_name_cache.bytes);
  weapon_popup_name_native.file_size=original;f.put(f.image.data()+0x124FFB0,0x30,alternate.data());
  f.draw();assert(native_popup_packets.size()==1&&alias_file_calls==2);
 }
 {
  NativePopupFixture f;std::vector<u8> alternate(0x800),replacement(0x800);f.japanese(alternate);f.draw();
  replacement=alternate;f.put(f.image.data()+0x124FFB0,0x30,replacement.data());
  game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(replacement.data()),replacement.size(),3});
  native_popup_packets.clear();f.draw();assert(native_popup_packets.size()==1&&alias_file_calls==2);
 }
 {
  NativePopupFixture f;f.draw();
  const auto files=alias_file_calls,capacities=alias_capacity_calls;
  assert(native_popup_packets.size()==2); // Full native item name; spaces advance but do not draw.
  assert(native_popup_packets[0].glyph==39&&native_popup_packets[1].glyph==300);
  for(const auto& p:native_popup_packets)assert(p.font==main_base+0x124FAB0&&p.rgba==0xE5D9CFF0&&p.height==24&&p.width==9);
  assert(native_popup_packets[0].x==366&&native_popup_packets[0].y==360&&native_popup_packets[1].x+native_popup_packets[1].width==388);
  assert((popup_blends.front()==std::array<unsigned,4>{1,4,1,0}));
  assert(popup_blends.size()==2&&(popup_blends.back()==std::array<unsigned,4>{1,1,0,0}));
  // Same peak opacity/light color: source-over vanishes on RGB155; additive lifts it by28.
  assert(std::lround(155*(46.f/255)+155*(1-46.f/255))==155);
  assert(std::lround(155*(46.f/255)+155)==183);
  native_popup_packets.clear();f.draw(4'800'000);assert((native_popup_packets[0].rgba&255)==120);
  native_popup_packets.clear();f.draw(52'800'000);assert((native_popup_packets[0].rgba&255)==120);
  native_popup_packets.clear();f.draw(57'600'000);assert(native_popup_packets.empty());
  assert(alias_file_calls==files&&alias_capacity_calls==capacities); // Ordinary EN/RU requires neither file IO nor allocator query.
 }
 {
  NativePopupFixture f;const auto software=reinterpret_cast<std::uintptr_t>(f.image.data()+0x20000);
  f.put(f.image.data(),0x1184150,software);const std::array<float,16> c{1,0,0,0,0,1.33333325f,0,0,0,0,1,0,0,0,0,1};
  std::memcpy(reinterpret_cast<void*>(software+0x440180),c.data(),sizeof(c));f.draw();
  assert(native_popup_packets.size()==2&&native_popup_packets[0].height==18);
  const auto& p=native_popup_packets[0];const float clip=c[5]*(1-2*float(p.y)/384);
  assert(std::fabs(clip-(1-2*420.f/448))<.004f);
 }
 {
  NativePopupFixture f;f.metrics[300]={0,0};f.draw();assert(native_popup_packets.empty());
 }
 {
  NativePopupFixture f;f.core[0x100+0x1000+2]=2;f.draw();assert(native_popup_packets.empty()); // Unknown MDT control.
 }
 {
  NativePopupFixture f;f.put(f.core.data()+0x100,8+35*8,u64(8));f.draw();
  assert(native_popup_packets.empty()&&popup_blends.empty()); // Offset into MDT table, no partial mist/name.
 }
 {
  NativePopupFixture f;f.image[0x124FAB0]=0;f.draw();assert(native_popup_packets.empty()&&popup_blends.empty());
 }
 {
  NativePopupFixture f;f.metrics[300]={0,65};f.draw();assert(native_popup_packets.empty()&&popup_blends.empty());
 }
 {
  NativePopupFixture f;const auto bank=f.core.data()+0x100;const auto nested=bank+0x100;
  f.put(f.image.data()+0x124FFB0,0,std::int32_t(3));f.put(f.image.data()+0x124FFB0,0x44,u32(0));f.image[0x124FFB0+0x53]=0;
  f.put(bank,4+4*5,u32(0x100));f.put(nested,4,u32(0x110));
  for(unsigned raw=0;raw<0x110;++raw)f.put(nested,8+raw*8,u64(0x1000));
  const std::array<u16,5> localized{0,0x1AC,14,8,1};std::memcpy(nested+0x1000,localized.data(),sizeof(localized));
  f.draw();assert(native_popup_packets.size()==1&&native_popup_packets[0].glyph==300); // Native multibank language3 index5.
 }
 {
  re4::NativeWeaponPopupName name;name.count=64;for(auto& glyph:name.glyphs)glyph=39;
  std::array<std::array<std::int8_t,2>,64> metrics{};for(auto& metric:metrics)metric={0,64};
  re4::NativeInventoryGlyph style;style.font=1;style.rgba=0xFFFFFFFF;
  re4::NativeWeaponPopupLine line;assert(re4::native_weapon_popup_layout(name,metrics,64,16,style,{},1,line));
  assert(line.count==64&&line.width==300&&line.left==88);
  for(const auto& packet:line.glyphs)assert(packet.x>=88&&packet.x+packet.width<=388&&packet.height<=24);
 }
 std::puts("native popup: exact case font/style, native localized codes, fade, compensation and rejection PASS");
}
