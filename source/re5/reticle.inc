std::atomic_flag reticle_locks[2]={ATOMIC_FLAG_INIT,ATOMIC_FLAG_INIT};
struct RayState {
 std::uintptr_t actor=0,laser=0;std::uint64_t generation=0,input_frame=0,tick=0,sequence=0,drawn[8]{};float endpoint[3]{};bool valid=false;
} rays[2];
HOOK_DEFINE_INLINE(Re5RayCapture){static void Callback(exl::hook::InlineFloatCtx* ctx){
 const auto actor=re4::saved_x(ctx,19),laser=re4::saved_x(ctx,21);ControlFrame f;
 if(!fresh_frame(actor,f))return;
 auto& lock=reticle_locks[f.player.local_player];if(lock.test_and_set(std::memory_order_acquire))return;
 struct Unlock{std::atomic_flag& flag;~Unlock(){flag.clear(std::memory_order_release);}} unlock{lock};
 auto& ray=rays[f.player.local_player];ray.valid=false;
 std::uintptr_t owned=0,vtable=0;std::uint32_t slot=99;std::uint8_t valid=0,scope=1;float origin[3],endpoint[3];
 if(f.scope||!memory.get(actor,0x3728,owned)||owned!=laser||!memory.get(laser,0,vtable)||vtable!=main_base+re5::profile::offset<0x1B735E0>()
  ||!memory.get(laser,0x374,slot)||slot!=f.player.actor_slot||!memory.get(laser,0x274,valid)||!valid
  ||!memory.get(laser,0x378,scope)||scope||!memory.get(laser,0x280,origin)||!memory.get(laser,0x290,endpoint))return;
 float distance=0;for(unsigned i=0;i<3;++i){if(!std::isfinite(origin[i])||!std::isfinite(endpoint[i]))return;const float d=endpoint[i]-origin[i];distance+=d*d;}
 if(!std::isfinite(distance)||distance<.000001f)return;
 ray.actor=actor;ray.laser=laser;ray.generation=f.generation;ray.input_frame=f.frame;ray.tick=svcGetSystemTick();++ray.sequence;
 std::memcpy(ray.endpoint,endpoint,sizeof(endpoint));ray.valid=true;
}};
struct NativeGui {
 alignas(16) std::uint8_t shell[0x2B0]{};std::uintptr_t polygons[4]{};bool initialized=false;
} native_gui[2];
bool hud_allowed(){
 std::uintptr_t slot=0,root=0,stage=0;std::uint32_t flags=0;std::uint8_t hide=0;std::int32_t index=-1;std::uint16_t stage_id=0;
 return memory.get(main_base,re5::profile::offset<0x1CD9FC0>(),slot)&&memory.get(slot,0,root)&&memory.get(root,0x584F9,hide)&&!(hide&0x80)
  &&memory.get(root,0x584F4,flags)&&!(flags&0x60000600)&&memory.get(main_base,re5::profile::offset<0x1CF7A40>(),stage)&&memory.get(stage,0x2C,index)&&index>=0&&index<64
  &&memory.get(stage,std::size_t(index)*0x5F40+0x2AEF8,stage_id)&&stage_id!=0x6D;
}
using GuiPair=void(*)(void*,void*);
struct GuiApi {
 void (*shell)(void*)=nullptr;std::uintptr_t (*polygon)()=nullptr;
 void (*mode)(std::uintptr_t,unsigned)=nullptr;void (*initialize)(void*,std::uintptr_t,unsigned)=nullptr;
 GuiPair projection=nullptr,pre=nullptr,draw=nullptr,post=nullptr;
 re5::Matrix (*view)(std::uintptr_t)=nullptr;re5::Matrix (*perspective)(std::uintptr_t)=nullptr;
} gui_api;
void draw_marker(NativeGui& gui,std::uintptr_t command,re5::Viewport viewport,const std::array<re5::MarkerRect,4>& rectangles,unsigned count){
 if(!gui.initialized){gui_api.shell(gui.shell);gui.initialized=true;}
 for(unsigned i=0;i<count;++i)if(!gui.polygons[i]){gui.polygons[i]=gui_api.polygon();if(!gui.polygons[i])return;}
 alignas(16) std::uint8_t draw[0x300]{};
 gui_api.mode(command,0x18);
 gui_api.initialize(draw,command,0);
 gui_api.projection(gui.shell,draw);
 float ox=0,oy=0,sx=0,sy=0;std::memcpy(&ox,draw+0xD8,4);std::memcpy(&oy,draw+0xDC,4);std::memcpy(&sx,draw+0xE0,4);std::memcpy(&sy,draw+0xE4,4);
 if(!std::isfinite(ox)||!std::isfinite(oy)||!std::isfinite(sx)||!std::isfinite(sy)||sx<=0||sy<=0)return;
 for(unsigned i=0;i<count;++i){const auto poly=gui.polygons[i];std::uintptr_t vtable=0;
  if(!memory.get(poly,0,vtable)||vtable!=main_base+re5::profile::offset<0x1CC0DA8>()||!writable(nullptr,poly+0x10,0xE8))return;
  const auto& r=rectangles[i];re5::Matrix matrix;matrix.m[0][0]=1/sx;matrix.m[1][1]=-1/sy;matrix.m[2][2]=matrix.m[3][3]=1;
  matrix.m[3][0]=(r.x-viewport.left-ox)/sx;matrix.m[3][1]=(r.y-viewport.top-oy)/sy;
  std::memcpy(reinterpret_cast<void*>(poly+0x10),&matrix,sizeof(matrix));
  // Each rectangle owns its own native three-frame vertex buffer. Reusing one
  // polygon for four differently shaped quads would overwrite queued vertices.
  for(unsigned offset:{0xC4u,0xC8u,0xCCu,0xD0u}){const std::uint32_t color=0x80FFFFFF;std::memcpy(reinterpret_cast<void*>(poly+offset),&color,4);}
  std::memcpy(reinterpret_cast<void*>(poly+0xD4),&r.width,4);std::memcpy(reinterpret_cast<void*>(poly+0xD8),&r.height,4);
  std::uint32_t dirty=0;std::memcpy(&dirty,reinterpret_cast<void*>(poly+0xE4),4);dirty|=0x07000000;std::memcpy(reinterpret_cast<void*>(poly+0xE4),&dirty,4);
  gui_api.pre(reinterpret_cast<void*>(poly),draw);
  gui_api.draw(reinterpret_cast<void*>(poly),draw);
  gui_api.post(reinterpret_cast<void*>(poly),draw);
 }
}
HOOK_DEFINE_TRAMPOLINE(Re5CockpitDraw){static void Callback(void* cockpit_pointer,void* command_pointer){
 Orig(cockpit_pointer,command_pointer);
 if(!game_active.load(std::memory_order_acquire)||!hud_allowed())return;
 const auto cockpit=reinterpret_cast<std::uintptr_t>(cockpit_pointer),command=reinterpret_cast<std::uintptr_t>(command_pointer);
 std::uintptr_t vtable=0,actor=0,owned=0,manager=0,camera=0,actor_camera=0,camera_owner=0;std::uint8_t visible=0,viewport_index=255;std::uint64_t tag=0;
 if(!memory.get(cockpit,0,vtable)||vtable!=main_base+re5::profile::offset<0x1B32820>()||!memory.get(cockpit,0xEB,visible)||!visible
  ||!memory.get(cockpit,0x1D0,actor)||!memory.get(actor,0x3660,owned)||owned!=cockpit
  ||!memory.get(command,0x210,tag)||(tag>>48)!=1||!memory.get(command,0x214,viewport_index)||viewport_index>7
  ||!memory.get(main_base,re5::profile::offset<0x26A8550>(),manager)||!memory.get(manager,0x48+viewport_index*0x200,camera)
  ||!memory.get(actor,0x1A80,actor_camera)||actor_camera!=camera||!memory.get(camera,0x130,camera_owner)||camera_owner!=actor)return;
 ControlFrame f;if(!fresh_frame(actor,f)||f.settings.reticle==re4::ReticleStyle::Off||f.scope||!re5::active_aim(f.action))return;
 auto& lock=reticle_locks[f.player.local_player];if(lock.test_and_set(std::memory_order_acquire))return;
 struct Unlock{std::atomic_flag& flag;~Unlock(){flag.clear(std::memory_order_release);}} unlock{lock};
 auto& ray=rays[f.player.local_player];const auto now=svcGetSystemTick();
 if(!ray.valid||ray.actor!=actor||ray.generation!=f.generation||ray.input_frame!=f.frame||now<ray.tick||now-ray.tick>1920000||ray.drawn[viewport_index]==ray.sequence)return;
 re5::Viewport viewport;if(!memory.get(command,0x144,viewport.left)||!memory.get(command,0x148,viewport.top)||!memory.get(command,0x14C,viewport.right)||!memory.get(command,0x150,viewport.bottom))return;
 const auto view=gui_api.view(camera),projection=gui_api.perspective(camera);
 re5::ScreenPoint point;if(!re5::project(view,projection,ray.endpoint,viewport,point))return;
 std::array<re5::MarkerRect,4> rectangles;const auto count=re5::marker_rectangles(point,viewport,unsigned(f.settings.reticle),unsigned(f.settings.reticle_size),rectangles);if(!count)return;
 ray.drawn[viewport_index]=ray.sequence;draw_marker(native_gui[f.player.local_player],command,viewport,rectangles,count);
}};
void install_reticle(){
 gui_api={reinterpret_cast<void(*)(void*)>(main_base+re5::profile::offset<0xD9DB00>()),reinterpret_cast<std::uintptr_t(*)()>(main_base+re5::profile::offset<0xEB6C90>()),
  reinterpret_cast<void(*)(std::uintptr_t,unsigned)>(main_base+re5::profile::offset<0xD06FF0>()),reinterpret_cast<void(*)(void*,std::uintptr_t,unsigned)>(main_base+re5::profile::offset<0xFD7EA0>()),
  reinterpret_cast<GuiPair>(main_base+re5::profile::offset<0xD9E5D0>()),reinterpret_cast<GuiPair>(main_base+re5::profile::offset<0xEB82E0>()),reinterpret_cast<GuiPair>(main_base+re5::profile::offset<0xEB8470>()),reinterpret_cast<GuiPair>(main_base+re5::profile::offset<0xEB8840>()),
  reinterpret_cast<re5::Matrix(*)(std::uintptr_t)>(main_base+re5::profile::offset<0x23AE60>()),reinterpret_cast<re5::Matrix(*)(std::uintptr_t)>(main_base+re5::profile::offset<0x23AE80>())};
 Re5RayCapture::InstallAtOffset(re5::profile::offset<0x7A220C>());Re5CockpitDraw::InstallAtOffset(re5::profile::offset<0x4D2FD0>());}
