#include "re5/native_graphics.hpp"
#include <map>
#include <vector>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <limits>
#undef assert
#define assert(c) do{if(!(c)){std::fprintf(stderr,"RE5 native graphics line %d: %s\n",__LINE__,#c);std::exit(1);}}while(false)
namespace {
struct Memory {
 std::map<std::uintptr_t,std::vector<std::uint8_t>> regions;
 void add(std::uintptr_t base,std::size_t size){regions.emplace(base,std::vector<std::uint8_t>(size));}
 static bool readable(void* ctx,std::uintptr_t base,std::size_t size){auto& self=*static_cast<Memory*>(ctx);for(const auto& [at,bytes]:self.regions)if(base>=at&&base-at<=bytes.size()&&size<=bytes.size()-(base-at))return true;return false;}
 static bool read(void* ctx,std::uintptr_t base,void* out,std::size_t size){auto& self=*static_cast<Memory*>(ctx);for(const auto& [at,bytes]:self.regions)if(base>=at&&base-at<=bytes.size()&&size<=bytes.size()-(base-at)){std::memcpy(out,bytes.data()+base-at,size);return true;}return false;}
 template<class T>void put(std::uintptr_t base,std::size_t off,T value){for(auto& [at,bytes]:regions)if(base+off>=at&&base+off-at+sizeof(T)<=bytes.size()){std::memcpy(bytes.data()+base+off-at,&value,sizeof(T));return;}assert(false);}
 re5::MemoryReader reader(){return {this,read,readable};}
};
}
int main(){
 constexpr std::uintptr_t main=0x10000000,manager=0x20000000,first=0x30000000,second=0x30001000,renderer=0x40000000,target=0x50000000,linker=0x60000000,u0=0x70000000,u1=0x70001000;
 Memory m;m.add(main+0x26A8550,8);m.add(manager,0x450);m.add(first,0x100);m.add(second,0x100);
 m.put(main,0x26A8550,manager);m.put(manager,0x48,first);m.put(manager,0x248,second);
 for(auto cam:{first,second}){m.put(cam,0x44,1000.f);m.put(cam,0x48,1.f);}
 const auto read=m.reader();float out=77;
 for(auto cam:{first,second}){
  assert(re5::scale_projection_far(read,main,cam,1,1000,1.75f,out)&&out==1750);
  assert(!re5::scale_projection_far(read,main,cam,1,1000,1,out)&&out==1750);
 }
 assert(re5::scale_culling_far(read,main,second,manager+0x240,1000,1.75f,out)&&out==1750);
 assert(!re5::scale_culling_far(read,main,second,manager+0x40,1000,1.75f,out)&&out==1750);
 assert(!re5::scale_projection_far(read,main,first+8,1,1000,1.75f,out));
 assert(!re5::scale_projection_far(read,main,first,1,999,1.75f,out));
 for(float invalid:{0.49f,2.01f,std::numeric_limits<float>::quiet_NaN()})assert(!re5::scale_projection_far(read,main,first,1,1000,invalid,out));
 assert(!re5::scale_projection_far(read,main,first,1000,1000,.5f,out));
 m.put(first,0x44,1.5f);assert(!re5::scale_projection_far(read,main,first,1,1.5f,.5f,out));m.put(first,0x44,1000.f);
 // Shared final target belongs to the native renderer, irrespective of player viewport.
 m.add(main+0x26A96A0,8);m.add(renderer+0x106F698,8);m.add(target,0x648);
 m.put(main,0x26A96A0,renderer);m.put(renderer,0x106F698,target);m.put(target,0x610,std::uint32_t(0));m.put(target,0x614,std::uint32_t(4));
 unsigned mode=99;
 assert(re5::override_post_aa(read,main,target,4,re5::AntiAliasing::Off,mode)&&mode==0);
 assert(!re5::override_post_aa(read,main,target,4,re5::AntiAliasing::Native,mode)&&mode==0);
 assert(!re5::override_post_aa(read,main,target+8,4,re5::AntiAliasing::Off,mode));
 m.put(target,0x610,std::uint32_t(2));assert(!re5::override_post_aa(read,main,target,4,re5::AntiAliasing::Off,mode));m.put(target,0x610,std::uint32_t(0));
 assert(!re5::override_post_aa(read,main,target,2,re5::AntiAliasing::Off,mode));
 m.put(target,0x614,std::uint32_t(0));assert(re5::override_post_aa(read,main,target,0,re5::AntiAliasing::Smaa,mode)&&mode==4);
 // Stage shadow hooks affect only their native units and local view callsites.
 m.add(main+0x1CF89F0,8);m.add(linker,0x180);m.add(u0,0x250);m.add(u1,0x250);
 m.put(main,0x1CF89F0,linker);m.put(linker,0x160,u0);m.put(linker,0x168,u1);
 m.put(u0,0,main+0x1AD20D8);m.put(u1,0,main+0x1AD20D8);
 for(unsigned view=0;view<2;++view){
  assert(re5::disable_stage_shadow(read,main,u0,main+0x98030,view,0,re4::ShadowMode::Off));
  assert(re5::disable_stage_shadow(read,main,u1,main+0x98030,view,1,re4::ShadowMode::Off));
 }
 assert(!re5::disable_stage_shadow(read,main,u0,main+0x98030,0,0,re4::ShadowMode::Native));
 assert(!re5::disable_stage_shadow(read,main,u1,main+0x98030,0,0,re4::ShadowMode::Off));
 assert(!re5::disable_stage_shadow(read,main,u0,main+0x98034,0,0,re4::ShadowMode::Off));
 assert(!re5::disable_stage_shadow(read,main,u0,main+0x98030,2,0,re4::ShadowMode::Off));
 m.put(u0,0,std::uintptr_t(0));assert(!re5::disable_stage_shadow(read,main,u0,main+0x98030,0,0,re4::ShadowMode::Off));
 std::puts("RE5 native graphics: two-view projection/frustum, primary post-AA and owned stage shadows PASS");
}
