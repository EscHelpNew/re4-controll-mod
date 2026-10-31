#pragma once
#include "re4/game_target.hpp"
#include <cstddef>
#include <cstdint>
#include <array>
namespace graphics_preview {
enum class Topic {Overview,Filtering,AntiAliasing,Distance,Shadows};
struct Rect{unsigned x=0,y=0,width=0,height=0;};
struct Image{const std::uint16_t* pixels=nullptr;std::size_t count=0;unsigned width=0,height=0;};
inline Topic topic_for(re4::GameTarget game,unsigned row){
 if(game==re4::GameTarget::RE4){constexpr Topic topics[]={Topic::Filtering,Topic::AntiAliasing,Topic::Distance,Topic::Shadows};
  if(row<4)return topics[row];
 }else if(game==re4::GameTarget::RE6&&row<2)return row?Topic::AntiAliasing:Topic::Filtering;
 return Topic::Overview;
}
inline Rect focus(re4::GameTarget game,Topic topic){
 if(game==re4::GameTarget::RE4)switch(topic){
  case Topic::Filtering:return {179,147,51,29};
  case Topic::AntiAliasing:return {241,77,72,44};
  case Topic::Distance:return {174,0,76,46};
  case Topic::Shadows:return {193,151,40,26};
  default:return {};
 }
 if(game==re4::GameTarget::RE6)switch(topic){
  case Topic::Filtering:return {284,137,34,31};
  case Topic::AntiAliasing:return {252,12,62,80};
  default:return {};
 }
 return {};
}
inline Rect scaled_focus(re4::GameTarget game,Topic topic,unsigned width,unsigned height){
 const auto r=focus(game,topic);if(!width||!height||!r.width||!r.height)return {};
 const auto x=unsigned(std::uint64_t(r.x)*width/320),y=unsigned(std::uint64_t(r.y)*height/180);
 const auto right=unsigned((std::uint64_t(r.x+r.width)*width+319)/320);
 const auto bottom=unsigned((std::uint64_t(r.y+r.height)*height+179)/180);
 return {x,y,right-x,bottom-y};
}
inline const char* caption(Topic topic,bool russian){
 switch(topic){
  case Topic::Filtering:return russian?"Пример: текстуры под углом":"Example: textures at an angle";
  case Topic::AntiAliasing:return russian?"Пример: контуры объектов":"Example: edges of objects";
  case Topic::Distance:return russian?"Пример: объекты вдали":"Example: distant objects";
  case Topic::Shadows:return russian?"Пример: тени на поверхностях":"Example: shadows on surfaces";
  default:return russian?"Пример игровой сцены":"Illustrative game scene";
 }
}
inline bool valid_image(Image image){return image.pixels&&image.width&&image.height&&image.width<=320&&image.height<=180
 &&image.count>=std::size_t(image.width)*image.height;}
inline bool rgba_row(Image image,unsigned row,std::uint8_t* out,std::size_t size){
 if(!valid_image(image)||row>=image.height||!out||size<std::size_t(image.width)*4)return false;
 for(unsigned x=0;x<image.width;++x){const auto p=image.pixels[row*image.width+x];
  out[x*4]=std::uint8_t(((p>>11)&31)*255/31);out[x*4+1]=std::uint8_t(((p>>5)&63)*255/63);
  out[x*4+2]=std::uint8_t((p&31)*255/31);out[x*4+3]=255;
 }return true;
}
template<class Draw> bool draw_rows(Image image,int x,int y,Draw draw){
 if(!valid_image(image))return false;
 std::array<std::uint8_t,320*4> row;
 for(unsigned i=0;i<image.height;++i){if(!rgba_row(image,i,row.data(),row.size()))return false;
  draw(x,y+int(i),int(image.width),1,row.data());
 }return true;
}
}
