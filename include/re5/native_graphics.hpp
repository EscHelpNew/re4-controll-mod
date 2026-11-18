#pragma once
#include "re5/profile.hpp"
#include "re5/graphics.hpp"
#include "re5/memory.hpp"
namespace re5 {
namespace native_graphics_detail {
inline int camera_slot(const MemoryReader& memory,std::uintptr_t main,std::uintptr_t camera,std::uintptr_t& manager){
 if(!camera||!memory.get(main,re5::profile::offset<0x26A8550>(),manager)||!manager)return -1;
 for(unsigned slot=0;slot<2;++slot){std::uintptr_t current=0;if(memory.get(manager,0x48+slot*0x200,current)&&current==camera)return int(slot);}
 return -1;
}
inline bool scaled_far(float near,float far,float scale,float& out){
 if(!std::isfinite(near)||!std::isfinite(far)||!std::isfinite(scale)||near<=0||far<=near||scale<.5f||scale>2||scale==1)return false;
 const double value=double(far)*scale;
 if(!std::isfinite(value)||value>std::numeric_limits<float>::max()||value<=double(near)+.001)return false;
 out=float(value);return true;
}
}
inline bool scale_projection_far(const MemoryReader& memory,std::uintptr_t main,std::uintptr_t camera,float near,float far,float scale,float& out){
 std::uintptr_t manager=0;float current_near=0,current_far=0;
 return native_graphics_detail::camera_slot(memory,main,camera,manager)>=0
  &&memory.get(camera,0x44,current_far)&&memory.get(camera,0x48,current_near)
  &&current_far==far&&current_near==near&&native_graphics_detail::scaled_far(near,far,scale,out);
}
inline bool scale_culling_far(const MemoryReader& memory,std::uintptr_t main,std::uintptr_t camera,std::uintptr_t viewport,float far,float scale,float& out){
 std::uintptr_t manager=0;const int slot=native_graphics_detail::camera_slot(memory,main,camera,manager);
 if(slot<0||viewport!=manager+0x40+unsigned(slot)*0x200)return false;
 float near=0,current=0;
 return memory.get(camera,0x44,current)&&memory.get(camera,0x48,near)&&current==far
  &&native_graphics_detail::scaled_far(near,far,scale,out);
}
inline bool override_post_aa(const MemoryReader& memory,std::uintptr_t main,std::uintptr_t target,unsigned current,AntiAliasing requested,unsigned& out){
 if(requested==AntiAliasing::Native||unsigned(requested)>2||current>4)return false;
 std::uintptr_t renderer=0,owned=0;unsigned msaa=99,native=99;
 if(!memory.get(main,re5::profile::offset<0x26A96A0>(),renderer)||!memory.get(renderer,0x106F698,owned)||owned!=target
  ||!memory.range(target,0x648)||!memory.get(target,0x610,msaa)||msaa
  ||!memory.get(target,0x614,native)||native!=current)return false;
 const auto next=native_aa_mode(requested,current);if(next==current)return false;
 out=next;return true;
}
inline bool disable_stage_shadow(const MemoryReader& memory,std::uintptr_t main,std::uintptr_t unit,std::uintptr_t method,unsigned view,unsigned unit_index,re4::ShadowMode mode){
 if(mode!=re4::ShadowMode::Off||view>1||unit_index>1||method!=main+re5::profile::offset<0x98030>())return false;
 std::uintptr_t manager=0,owned=0,vtable=0;
 return memory.get(main,re5::profile::offset<0x1CF89F0>(),manager)&&memory.get(manager,0x160+unit_index*8,owned)&&owned==unit
  &&memory.range(unit,0x250)&&memory.get(unit,0,vtable)&&vtable==main+re5::profile::offset<0x1AD20D8>();
}
}
