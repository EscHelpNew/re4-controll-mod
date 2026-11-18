#include "re5/settings.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#undef assert
#define assert(c) do { if(!(c)){std::fprintf(stderr,"RE5 settings line %d: %s\n",__LINE__,#c);std::exit(1);} } while(false)
namespace {
using Bytes=std::array<std::uint8_t,re5::kSettingsBytes>;
// Independent standard CRC32 oracle, including the player's encoded bytes.
void seal(Bytes& b){
 std::uint32_t c=0xffffffffu;
 for(std::size_t i=0;i<b.size();++i){
  const auto byte=i>=60&&i<64?0:b[i];c^=byte;
  for(unsigned bit=0;bit<8;++bit)c=(c>>1)^((c&1)?0xedb88320u:0);
 }
 c^=0xffffffffu;for(unsigned i=0;i<4;++i)b[60+i]=std::uint8_t(c>>(i*8));
}
void reject(Bytes bytes,std::size_t size=re5::kSettingsBytes){
 seal(bytes);re5::Settings out;out.players[1].gyro_speed=3.25f;
 assert(!re5::decode_settings(bytes.data(),size,out));
 assert(out.players[1].gyro_speed==3.25f&&out.language==re4::Language::English);
}
void wire(){
 re5::Settings s;
 assert(!s.players[0].analog_enabled&&!s.players[1].gyro_enabled&&!s.players[1].remap_enabled);
 assert(s.graphics.filter==re4::TextureFilter::Native&&s.graphics.draw_distance==1);
 s.language=re4::Language::Russian;
 s.players[0].gyro_enabled=true;s.players[0].gyro_speed=0.35f;s.players[0].gyro_bias_x=0.125f;
 s.players[0].left_deadzone=0.15f;s.players[0].invert_x=false;s.players[0].button_map[0]=1;
 s.players[1].gyro_enabled=true;s.players[1].gyro_speed=1.75f;s.players[1].gyro_bias_x=-0.25f;
 s.players[1].right_deadzone=0.3f;s.players[1].invert_y=false;s.players[1].button_map[0]=3;
 s.graphics={re4::TextureFilter::Aniso16,re4::ShadowMode::Off,re5::AntiAliasing::Smaa,1.75f};
 Bytes bytes{};assert(re5::encode_settings(s,bytes.data(),bytes.size()));
 assert(std::memcmp(bytes.data(),"RE5CTRL1",8)==0&&bytes[8]==1&&bytes[9]==0);
 assert(bytes[12]==0x40&&bytes[13]==1&&bytes[16]==1);
 assert(bytes[20]==6&&bytes[21]==1&&bytes[22]==2);
 assert(bytes[28]==0&&bytes[29]==0&&bytes[30]==0xe0&&bytes[31]==0x3f);
 assert(std::memcmp(bytes.data()+64,"R4CT",4)==0&&std::memcmp(bytes.data()+192,"R4CT",4)==0);
 auto oracle=bytes;seal(oracle);assert(oracle==bytes);
 re5::Settings out;assert(re5::decode_settings(bytes.data(),bytes.size(),out));
 assert(out.language==re4::Language::Russian&&out.players[0].gyro_speed==0.35f&&out.players[1].gyro_speed==1.75f);
 assert(out.players[0].gyro_bias_x==0.125f&&out.players[1].gyro_bias_x==-0.25f);
 assert(out.players[0].left_deadzone==0.15f&&out.players[1].right_deadzone==0.3f);
 assert(!out.players[0].invert_x&&!out.players[1].invert_y);
 assert(out.players[0].button_map[0]==1&&out.players[1].button_map[0]==3);
 assert(out.graphics.filter==re4::TextureFilter::Aniso16&&out.graphics.draw_distance==1.75f);
 // A player edit cannot overwrite the other player or shared graphics.
 out.players[0].gyro_speed=2.5f;assert(out.players[1].gyro_speed==1.75f&&out.graphics.draw_distance==1.75f);
 for(unsigned offset:{0u,8u,12u,16u,20u,21u,22u,23u,24u,32u,59u}){
  auto bad=bytes;bad[offset]=255;reject(bad);
 }
 auto bad=bytes;bad[192+16]^=0x10;reject(bad); // valid outer CRC, invalid nested settings.
 bad=bytes;bad[60]^=1;assert(!re5::decode_settings(bad.data(),bad.size(),out));
 assert(!re5::decode_settings(bytes.data(),bytes.size()-1,out));
 assert(!re5::decode_settings(bytes.data(),bytes.size()+1,out));
 assert(!re5::decode_settings(nullptr,bytes.size(),out));
 for(float value:{std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN(),0.49f,2.01f}){
  auto invalid=s;invalid.graphics.draw_distance=value;auto unchanged=bytes;
  assert(!re5::encode_settings(invalid,bytes.data(),bytes.size())&&bytes==unchanged);
 }
 for(unsigned player=0;player<2;++player){
  auto invalid=s;invalid.players[player].gyro_speed=-1;assert(!re5::valid_settings(invalid));
  invalid=s;invalid.players[player].texture_filtering=re4::TextureFilter::Aniso16;
  assert(!re5::valid_settings(invalid)); // shared graphics have one authoritative owner.
 }
 assert(!re5::encode_settings(s,nullptr,bytes.size()));
 assert(!re5::encode_settings(s,bytes.data(),bytes.size()-1));
}
void persistence(){
 namespace fs=std::filesystem;const fs::path dir="build/re5-settings-test";fs::create_directories(dir);
 const auto path=(dir/"settings.bin").string();
 for(const auto suffix:{"",".previous",".incoming"})fs::remove(path+suffix);
 re5::Settings first,second,out;first.players[1].gyro_speed=2.5f;
 second=first;second.players[0].left_deadzone=0.25f;second.graphics.draw_distance=1.75f;
 assert(re5::save_settings_file(path.c_str(),first));assert(fs::file_size(path)==320);
 assert(re5::save_settings_file(path.c_str(),second));
 assert(re5::load_settings_file(path.c_str(),out)&&out.players[0].left_deadzone==0.25f&&out.players[1].gyro_speed==2.5f);
 {std::ofstream f(path,std::ios::binary);f<<"broken";}
 assert(re5::load_settings_file(path.c_str(),out)&&out.players[0].left_deadzone==0.05f&&out.players[1].gyro_speed==2.5f);
 assert(re5::save_settings_file(path.c_str(),second));
 // A blocked incoming path must not destroy either valid settings file.
 fs::remove(path+".incoming");fs::create_directory(path+".incoming");
 assert(!re5::save_settings_file(path.c_str(),first));
 assert(re5::load_settings_file(path.c_str(),out)&&out.players[0].left_deadzone==0.25f);
 fs::remove(path+".incoming");
 auto invalid=first;invalid.players[1].gyro_gain_x=11;
 assert(!re5::save_settings_file(path.c_str(),invalid));
 assert(re5::load_settings_file(path.c_str(),out)&&out.players[0].left_deadzone==0.25f);
 assert(!re5::load_settings_file(nullptr,out)&&!re5::load_settings_file("",out));
 assert(!re5::save_settings_file(nullptr,first)&&!re5::save_settings_file("",first));
 for(const auto suffix:{"",".previous",".incoming"})fs::remove(path+suffix);
}
}
int main(){wire();persistence();std::puts("RE5 two-player settings: independent controls, CRC/bounds and real backup recovery PASS");}
