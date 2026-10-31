#include "re4/graphics_preview.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#undef assert
#define assert(c) do{if(!(c)){std::fprintf(stderr,"graphics preview line %d: %s\n",__LINE__,#c);std::exit(1);}}while(false)
int main(){
 using namespace graphics_preview;using re4::GameTarget;
 assert(topic_for(GameTarget::RE4,0)==Topic::Filtering);
 assert(topic_for(GameTarget::RE4,1)==Topic::AntiAliasing);
 assert(topic_for(GameTarget::RE4,2)==Topic::Distance);
 assert(topic_for(GameTarget::RE4,3)==Topic::Shadows);
 assert(topic_for(GameTarget::RE4,4)==Topic::Overview&&topic_for(GameTarget::RE4,5)==Topic::Overview);
 assert(topic_for(GameTarget::RE6,0)==Topic::Filtering&&topic_for(GameTarget::RE6,1)==Topic::AntiAliasing);
 assert(topic_for(GameTarget::RE6,2)==Topic::Overview&&topic_for(GameTarget::RE6,3)==Topic::Overview);
 for(auto game:{GameTarget::RE4,GameTarget::RE6})for(auto topic:{Topic::Overview,Topic::Filtering,Topic::AntiAliasing,Topic::Distance,Topic::Shadows}){
  const auto r=focus(game,topic);const bool supported=topic!=Topic::Overview&&(game==GameTarget::RE4||topic==Topic::Filtering||topic==Topic::AntiAliasing);
  assert(bool(r.width&&r.height)==supported);assert(r.x+r.width<=320&&r.y+r.height<=180);
  const auto scaled=scaled_focus(game,topic,344,194);assert(scaled.x+scaled.width<=344&&scaled.y+scaled.height<=194);
  assert(std::strlen(caption(topic,false))>5&&std::strcmp(caption(topic,false),caption(topic,true)));
 }
 assert(focus(static_cast<GameTarget>(99),Topic::Filtering).width==0);
 const std::uint16_t pixels[]={0xf800,0x07e0,0x001f,0xffff};
 Image image{pixels,4,2,2};std::array<std::uint8_t,12> row{};row.fill(77);
 assert(rgba_row(image,0,row.data(),row.size()));
 assert(row[0]==255&&row[1]==0&&row[2]==0&&row[3]==255&&row[4]==0&&row[5]==255&&row[6]==0&&row[7]==255&&row[8]==77);
 assert(rgba_row(image,1,row.data(),row.size())&&row[0]==0&&row[1]==0&&row[2]==255&&row[4]==255&&row[5]==255&&row[6]==255);
 assert(!rgba_row(image,2,row.data(),row.size())&&!rgba_row(image,0,row.data(),7)&&!rgba_row(image,0,nullptr,12));
 unsigned calls=0;assert(draw_rows(image,20,40,[&](int x,int y,int width,int height,const std::uint8_t* p){
  assert(x==20&&y==40+int(calls)&&width==2&&height==1&&p[3]==255);++calls;
 }));assert(calls==2);
 for(Image invalid:{Image{},Image{pixels,3,2,2},Image{pixels,4,321,2},Image{pixels,4,2,181},Image{nullptr,4,2,2}}){
  assert(!rgba_row(invalid,0,row.data(),row.size()));
  assert(!draw_rows(invalid,0,0,[&](int,int,int,int,const std::uint8_t*){++calls;}));
 }assert(calls==2);
 std::puts("graphics preview: topic isolation, bounded focus, RGB565 scanlines and invalid guards PASS");
}
