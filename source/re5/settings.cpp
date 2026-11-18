#include "re5/settings.hpp"
#include <cstring>
namespace re5 {
namespace {
void put32(std::uint8_t* p,std::uint32_t value){for(unsigned i=0;i<4;++i)p[i]=std::uint8_t(value>>(i*8));}
std::uint32_t get32(const std::uint8_t* p){return std::uint32_t(p[0])|std::uint32_t(p[1])<<8|std::uint32_t(p[2])<<16|std::uint32_t(p[3])<<24;}
std::uint32_t crc32(const std::uint8_t* bytes){
 std::uint32_t crc=0xffffffffu;
 for(std::size_t i=0;i<kSettingsBytes;++i){
  crc^=i>=60&&i<64?0:bytes[i];
  for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^((crc&1)?0xedb88320u:0);
 }
 return crc^0xffffffffu;
}
}
bool valid_settings(const Settings& s){
 if(unsigned(s.language)>1||!valid_graphics(s.graphics))return false;
 for(const auto& player:s.players){
  if(!re4::valid_settings(player)||player.texture_filtering!=re4::TextureFilter::Native
   ||player.shadow_mode!=re4::ShadowMode::Native||player.anti_aliasing!=re4::AntiAliasing::Native
   ||player.draw_distance_scale!=1)return false;
 }
 return true;
}
bool encode_settings(const Settings& s,std::uint8_t* out,std::size_t size){
 if(!out||size!=kSettingsBytes||!valid_settings(s))return false;
 std::uint8_t bytes[kSettingsBytes]{};
 std::memcpy(bytes,"RE5CTRL1",8);put32(bytes+8,1);put32(bytes+12,kSettingsBytes);
 bytes[16]=unsigned(s.language);bytes[20]=unsigned(s.graphics.filter);
 bytes[21]=unsigned(s.graphics.shadows);bytes[22]=unsigned(s.graphics.aa);
 std::memcpy(bytes+28,&s.graphics.draw_distance,4);
 for(unsigned player=0;player<2;++player)
  if(!re4::encode_settings(s.players[player],bytes+64+player*re4::kSettingsBytes,re4::kSettingsBytes))return false;
 put32(bytes+60,crc32(bytes));std::memcpy(out,bytes,sizeof(bytes));return true;
}
bool decode_settings(const std::uint8_t* bytes,std::size_t size,Settings& out){
 if(!bytes||size!=kSettingsBytes||std::memcmp(bytes,"RE5CTRL1",8)||get32(bytes+8)!=1
  ||get32(bytes+12)!=kSettingsBytes||get32(bytes+60)!=crc32(bytes))return false;
 for(unsigned i=17;i<60;++i)
  if((i<20||i>22)&&(i<28||i>31)&&bytes[i])return false;
 Settings s;s.language=static_cast<re4::Language>(bytes[16]);
 s.graphics.filter=static_cast<re4::TextureFilter>(bytes[20]);
 s.graphics.shadows=static_cast<re4::ShadowMode>(bytes[21]);
 s.graphics.aa=static_cast<AntiAliasing>(bytes[22]);
 std::memcpy(&s.graphics.draw_distance,bytes+28,4);
 for(unsigned player=0;player<2;++player){
  const auto* p=bytes+64+player*re4::kSettingsBytes;
  if(get32(p+4)!=19||!re4::decode_settings(p,re4::kSettingsBytes,s.players[player]))return false;
 }
 if(!valid_settings(s))return false;
 out=s;return true;
}
}
