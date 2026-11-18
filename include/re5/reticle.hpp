#pragma once
#include <array>
#include <cmath>
#include <cstdint>
namespace re5 {
struct alignas(16) Matrix {float m[4][4]{};};
struct Viewport {std::uint32_t left=0,top=0,right=0,bottom=0;};
struct ScreenPoint {float x=0,y=0;};
inline bool project(const Matrix& view,const Matrix& projection,const float (&point)[3],Viewport viewport,ScreenPoint& out){
 if(viewport.right<=viewport.left||viewport.bottom<=viewport.top||viewport.right>16384||viewport.bottom>16384)return false;
 double position[4]={point[0],point[1],point[2],1},camera[4]{},clip[4]{};
 for(double v:position)if(!std::isfinite(v))return false;
 for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j){if(!std::isfinite(view.m[i][j])||!std::isfinite(projection.m[i][j]))return false;camera[j]+=position[i]*view.m[i][j];}
 for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j)clip[j]+=camera[i]*projection.m[i][j];
 for(double v:clip)if(!std::isfinite(v))return false;
 if(clip[3]<=.00001||clip[0]<-clip[3]||clip[0]>clip[3]||clip[1]<-clip[3]||clip[1]>clip[3]||clip[2]<0||clip[2]>clip[3])return false;
 out={float(viewport.left+(clip[0]/clip[3]+1)*.5*(viewport.right-viewport.left)),float(viewport.top+(1-clip[1]/clip[3])*.5*(viewport.bottom-viewport.top))};return true;
}
struct MarkerRect {float x=0,y=0,width=0,height=0;};
inline unsigned marker_rectangles(ScreenPoint p,Viewport viewport,unsigned style,unsigned size,std::array<MarkerRect,4>& out){
 if((style!=1&&style!=2)||size>2||!std::isfinite(p.x)||!std::isfinite(p.y))return 0;
 std::array<MarkerRect,4> value{};unsigned count=1;
 if(style==1){float width=float(2+size);value[0]={p.x-width*.5f,p.y-width*.5f,width,width};}
 else{count=4;const float length=float(6+size*2),half=size?1.f:.5f,gap=float(2+size);
  value={MarkerRect{p.x-gap-length,p.y-half,length,2*half},MarkerRect{p.x+gap,p.y-half,length,2*half},MarkerRect{p.x-half,p.y-gap-length,2*half,length},MarkerRect{p.x-half,p.y+gap,2*half,length}};}
 for(const auto& r:value)if(r.width&&(!(r.x>=viewport.left&&r.y>=viewport.top&&r.x+r.width<=viewport.right&&r.y+r.height<=viewport.bottom)))return 0;
 out=value;return count;
}
}
