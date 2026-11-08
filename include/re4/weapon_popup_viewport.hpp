#pragma once
#include "re4/reticle_draw.hpp"
#include <array>
#include <cmath>

namespace re4 {
// Invert only the native viewport's independent clip-X/Y affine adjustment.
// All layout, coverage, timing and alpha continue to use approved512x448 units.
struct WeaponPopupViewport {
 double sx=1,sy=1,tx=0,ty=0;
 ViewHudPoint position(float x,float y)const{
  if(sx==1&&sy==1&&tx==0&&ty==0)return {x,y};
  return {float((((2*double(x)/512-1)-tx)/sx+1)*256),
          float((1-((1-2*double(y)/448)-ty)/sy)*224)};
 }
 static bool correction(const std::array<float,16>& c,WeaponPopupViewport& out){
  for(float value:c)if(!std::isfinite(value))return false;
  // Column-major C must leave Z/W untouched, without rotation/shear/projective terms.
  for(unsigned i=0;i<16;++i){
   if(i==0||i==5||i==12||i==13)continue;
   if(c[i]!=(i==10||i==15?1.f:0.f))return false;
  }
  if(c[0]<=0||c[5]<=0)return false;
  const WeaponPopupViewport candidate{c[0],c[5],c[12],c[13]};
  // Reject inverses that overflow or lose native float precision at HUD corners.
  for(float x:{0.f,512.f})for(float y:{0.f,448.f}){
   const auto p=candidate.position(x,y);
   if(!std::isfinite(p.x)||!std::isfinite(p.y)||std::fabs(p.x)>65536||std::fabs(p.y)>65536
       ||std::fabs(candidate.sx*(2*double(p.x)/512-1)+candidate.tx-(2*double(x)/512-1))>1e-5
       ||std::fabs(candidate.sy*(1-2*double(p.y)/448)+candidate.ty-(1-2*double(y)/448))>1e-5)return false;
  }
  out=candidate;return true;
 }
};
// This stack-owned adapter never escapes: native wrappers copy vertex/state
// arguments into their commands. Every callback forwards the original context.
struct WeaponPopupViewportBackend {
 ReticleDrawBackend source;
 WeaponPopupViewport viewport;
 static WeaponPopupViewportBackend& self(void* p){return *static_cast<WeaponPopupViewportBackend*>(p);}
 static void prepare(void* p){auto& s=self(p);s.source.prepare_hud(s.source.context);}
 static void blend(void* p,unsigned a,unsigned b,unsigned c,unsigned d){auto& s=self(p);s.source.blend(s.source.context,a,b,c,d);}
 static void format(void* p,unsigned a,unsigned b,unsigned c,unsigned d,unsigned e){auto& s=self(p);s.source.vertex_format(s.source.context,a,b,c,d,e);}
 static void begin(void* p,unsigned a,unsigned b,unsigned c){auto& s=self(p);s.source.begin(s.source.context,a,b,c);}
 static void position(void* p,float x,float y,float z){auto& s=self(p);const auto v=s.viewport.position(x,y);s.source.position(s.source.context,v.x,v.y,z);}
 static void color(void* p,unsigned r,unsigned g,unsigned b,unsigned a){auto& s=self(p);s.source.color(s.source.context,r,g,b,a);}
 static void end(void* p,unsigned token){auto& s=self(p);s.source.end(s.source.context,token);}
 ReticleDrawBackend backend(){return {this,source.prepare_hud?prepare:nullptr,source.blend?blend:nullptr,
  source.vertex_format?format:nullptr,source.begin?begin:nullptr,source.position?position:nullptr,
  source.color?color:nullptr,source.end?end:nullptr};}
};
}
