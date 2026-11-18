#pragma once
#include "re4/graphics.hpp"
#include <cstring>
namespace re5 {
// Own RE5 modes: the native whole-frame post-AA path starts as SMAA1X.
// These are settings IDs, not the engine's post-AA mode numbers.
enum class AntiAliasing : std::uint8_t { Native=0, Off=1, Smaa=2 };
struct GraphicsOptions {
 re4::TextureFilter filter=re4::TextureFilter::Native;
 re4::ShadowMode shadows=re4::ShadowMode::Native;
 AntiAliasing aa=AntiAliasing::Native;
 float draw_distance=1;
};
inline bool valid_graphics(const GraphicsOptions& g){return unsigned(g.filter)<=6&&unsigned(g.shadows)<=1&&unsigned(g.aa)<=2
 &&std::isfinite(g.draw_distance)&&g.draw_distance>=0.5f&&g.draw_distance<=2;}
inline bool same_graphics(const GraphicsOptions& a,const GraphicsOptions& b){return a.filter==b.filter&&a.shadows==b.shadows&&a.aa==b.aa&&a.draw_distance==b.draw_distance;}
inline unsigned native_aa_mode(AntiAliasing mode,unsigned original){return mode==AntiAliasing::Off?0:mode==AntiAliasing::Smaa?4:original;}

// Native nDraw::SamplerState constructor10D8C30 copies0x38 bytes synchronously.
// Filter7/8 are the only ordinary trilinear modes; comparison/point modes pass.
inline bool filter_sampler_descriptor(const std::uint8_t* source,std::uint8_t* out,std::size_t size,re4::TextureFilter filter){
 if(!source||!out||size<0x38)return false;
 std::uint8_t local[0x38];std::memcpy(local,source,sizeof(local));
 std::uint32_t word=0;std::memcpy(&word,local,4);
 const unsigned native=word&255u;
 if((native==7||native==8)&&filter!=re4::TextureFilter::Native&&unsigned(filter)<=unsigned(re4::TextureFilter::Aniso16)){
  const unsigned mode=filter==re4::TextureFilter::Bilinear?6:filter==re4::TextureFilter::Trilinear?7:8;
  const unsigned aniso=filter==re4::TextureFilter::Aniso2?2:filter==re4::TextureFilter::Aniso4?4:
   filter==re4::TextureFilter::Aniso8?9:filter==re4::TextureFilter::Aniso16?16:0;
  word=(word&~(255u|(31u<<17)))|mode|(aniso<<17);std::memcpy(local,&word,4);
 }
 const bool changed=std::memcmp(local,source,sizeof(local))!=0;
 std::memcpy(out,local,sizeof(local));return changed;
}
}
