#include "re4/weapon_text.hpp"
#include <cassert>
#include <cstring>
#include <cmath>
struct Capture {unsigned prepares=0,begins=0,ends=0,positions=0,colors=0,expected=0,r=0,g=0,b=0;};
re4::ReticleDrawBackend backend(Capture& c){return {&c,
 +[](void* p){++static_cast<Capture*>(p)->prepares;},
 +[](void*,unsigned,unsigned,unsigned,unsigned){},
 +[](void*,unsigned,unsigned,unsigned,unsigned,unsigned){},
 +[](void* p,unsigned primitive,unsigned format,unsigned count){auto& c=*static_cast<Capture*>(p);assert(primitive==0x90&&format==0&&count%6==0&&count<=36000);++c.begins;c.expected=count;},
 +[](void* p,float,float,float){++static_cast<Capture*>(p)->positions;},
 +[](void* p,unsigned r,unsigned g,unsigned b,unsigned){auto& c=*static_cast<Capture*>(p);++c.colors;c.r=r;c.g=g;c.b=b;},
 +[](void* p,unsigned token){assert(token==0x34);++static_cast<Capture*>(p)->ends;}};}
int main(){
 // At16px capital height, match the reference Leon inscription's roughly80px
 // width at1280x720. Native512x448 coordinates stretch X more than Y.
 assert(re4::serif_text_width("Leon",16)>32&&re4::serif_text_width("Leon",16)<33);
 assert(std::strcmp(re4::weapon_name(37,false),"Red9")==0);
 assert(std::strcmp(re4::weapon_name(1,true),"Ручная граната")==0);
 assert(!re4::weapon_name(0xffff,false));
 float w=0;unsigned runs=0;
 auto visit=+[](void* p,float l,float t,float r,float b,unsigned a){assert(l<r&&t<b&&a&&a<=255);++*static_cast<unsigned*>(p);};
 assert(re4::serif_text_runs("Урон: 1.0",100,100,16,255,&runs,visit,&w));
 assert(runs>20&&runs<2000&&w>20&&w<150);
 unsigned before=runs;assert(!re4::serif_text_runs("Bad\xFF",100,100,16,255,&runs,visit));assert(runs==before);
 assert(!re4::serif_text_runs("A",0,0,16,255,&runs,visit));assert(runs==before);
 assert(!re4::serif_text_runs("A",100,100,NAN,255,&runs,visit));assert(runs==before);
 assert(!re4::serif_text_runs("A",100,100,16,0,&runs,visit));assert(runs==before);
 assert(!re4::serif_text_runs("A\nB",100,100,16,255,&runs,visit));assert(runs==before);
 assert(std::fabs(re4::serif_text_width("Урон: 1.0",16)-w)<.001f);
 assert(re4::serif_text_width("Bad\xFF",16)==0);
 Capture c;assert(re4::draw_serif_text(backend(c),"Урон: 1.0",100,100,16,255));
 assert(c.prepares==1&&c.begins==1&&c.ends==1&&c.positions==runs*6&&c.colors==c.positions&&c.expected==c.positions);
 Capture tint;assert(re4::draw_serif_text_color(backend(tint),"Атака: ",100,100,16,255,{238,221,204}));
 assert(tint.r==238&&tint.g==221&&tint.b==204);
 Capture bad;assert(!re4::draw_serif_text(backend(bad),"Bad\xFF",100,100,16,255));assert(!bad.prepares&&!bad.begins);
 for(unsigned id=0;id<0x110;++id)for(bool russian:{false,true})if(const auto* name=re4::weapon_name(id,russian)){
  const auto width=re4::serif_text_width(name,16);assert(width>0&&width<512);
  unsigned count=0;assert(re4::serif_text_runs(name,10,100,16,255,&count,visit));assert(count<6000);
 }
}
