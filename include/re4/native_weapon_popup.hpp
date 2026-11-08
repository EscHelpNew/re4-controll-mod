#pragma once
#include "re4/native_inventory_text.hpp"
#include "re4/weapon_popup_viewport.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace re4 {
struct NativeWeaponPopupName {std::array<std::uint16_t,64> glyphs{};unsigned count=0;};
struct NativeWeaponPopupLine {
 std::array<NativeInventoryGlyph,64> glyphs{};
 unsigned count=0;
 float left=0,width=0,top=420,height=28;
};
inline bool native_weapon_popup_name(const std::uint16_t* codes,unsigned count,NativeWeaponPopupName& out){
 if(!codes||count<5||count>69||codes[0]||codes[count-3]!=14||codes[count-2]!=8||codes[count-1]!=1)return false;
 unsigned end=count-3;while(end>1&&codes[end-1]==0x80)--end;
 NativeWeaponPopupName value;bool ink=false;
 for(unsigned i=1;i<end;++i){
  if(codes[i]<0x80||codes[i]>=0x1080||value.count==value.glyphs.size())return false;
  const auto glyph=std::uint16_t(codes[i]-0x80);value.glyphs[value.count++]=glyph;ink|=glyph!=0;
 }
 if(!ink)return false;
 out=value;return true;
}
// Native control14 returns from a submessage; with no parentB0 pointer it
// terminates this entry (2B1E58..80 -> 2B1FB4). Later packed entries are not ink.
inline bool native_weapon_popup_alias_name(const std::uint16_t* codes,unsigned count,NativeWeaponPopupName& out){
 if(!codes||count<3||count>66||codes[0]||(codes[count-1]!=14&&codes[count-1]!=1))return false;
 std::array<std::uint16_t,68> normalized{};std::copy_n(codes,count-1,normalized.begin());
 normalized[count-1]=14;normalized[count]=8;normalized[count+1]=1;
 return native_weapon_popup_name(normalized.data(),count+2,out);
}
inline bool native_weapon_popup_layout(const NativeWeaponPopupName& name,
 const std::array<std::array<std::int8_t,2>,64>& metrics,unsigned cell_width,unsigned space,
 const NativeInventoryGlyph& style,const WeaponPopupViewport& viewport,float opacity,NativeWeaponPopupLine& out){
 if(!name.count||name.count>64||!cell_width||cell_width>128||!style.font
     ||!std::isfinite(opacity)||opacity<=0||opacity>1)return false;
 std::array<unsigned,64> advances{};unsigned total=0;
 for(unsigned i=0;i<name.count;++i){
  const int span=name.glyphs[i]?int(metrics[i][1])-int(metrics[i][0]):int(space);
  if(span<=0||span>int(cell_width))return false;
  advances[i]=unsigned(span)*18/cell_width;if(!advances[i])return false;total+=advances[i];
 }
 if(!total)return false;
 const float scale=std::min(1.f,300.f/total);
 NativeWeaponPopupLine value;value.width=total*scale;value.left=388-value.width;value.height=28*scale;
 float cursor=value.left;const unsigned alpha=unsigned(std::lround((style.rgba&255)*opacity));
 if(!alpha)return false;
 for(unsigned i=0;i<name.count;++i){
  const float advance=advances[i]*scale;
  if(name.glyphs[i]){
   const auto first=viewport.position(cursor,value.top),last=viewport.position(cursor+advance,value.top+value.height);
   const float y=first.y*(384.f/448),bottom=last.y*(384.f/448);
   if(!std::isfinite(first.x)||!std::isfinite(last.x)||!std::isfinite(y)||!std::isfinite(bottom)
       ||first.x< -32768||first.x>32767||last.x< -32768||last.x>32767||y< -32768||y>32767||bottom< -32768||bottom>32767)return false;
   const long x=std::lround(first.x),right=std::lround(last.x),top=std::lround(y),end=std::lround(bottom);
   if(right<=x||right-x>255||end<=top||end-top>255)return false;
   auto packet=style;packet.x=std::uint16_t(std::int16_t(x));packet.y=std::uint16_t(std::int16_t(top));
   packet.glyph=name.glyphs[i];packet.width=std::uint8_t(right-x);packet.height=std::uint8_t(end-top);
   packet.rgba=(style.rgba&0xFFFFFF00)|alpha;
   value.glyphs[value.count++]=packet;
  }
  cursor+=advance;
 }
 out=value;return value.count!=0;
}
}
