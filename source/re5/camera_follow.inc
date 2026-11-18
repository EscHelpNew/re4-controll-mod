// Native follow owns its pose banks and collision remains downstream. A
// successful component grants one same-frame spatial-follow lease, never a
// persistent overwrite of camera eye/history after native wall clipping.
struct CameraFollowSlot {
 std::atomic_flag lock=ATOMIC_FLAG_INIT;std::atomic<std::uint64_t> thread{0};
 ControlFrame frame;re5::CameraPose desired;std::uintptr_t camera=0,component=0,base=0;
 bool pending=false,arm_seen=false;std::uint32_t native_alpha=0;
} camera_follow_slots[2];
void invalidate_camera_follow(std::uintptr_t camera,std::uintptr_t component){
 for(auto& slot:camera_follow_slots){
  if(slot.lock.test_and_set(std::memory_order_acquire))continue;
  if((camera&&slot.camera==camera)||(component&&slot.component==component))slot.pending=false;
  slot.lock.clear(std::memory_order_release);
 }
}
void publish_camera_follow(const ControlFrame& frame,std::uintptr_t camera,std::uintptr_t component,const re5::CameraPose& desired){
 if(!frame.settings.analog_enabled||!frame.settings.unified_controls||!frame.settings.free_camera||frame.settings.first_person||frame.scope||frame.action>8)return;
 auto& slot=camera_follow_slots[frame.player.local_player];
 if(slot.lock.test_and_set(std::memory_order_acquire))return;
 slot.frame=frame;slot.desired=desired;slot.camera=camera;slot.component=component;slot.pending=true;
 slot.lock.clear(std::memory_order_release);
}
bool read_follow_pose(std::uintptr_t object,unsigned eye,unsigned up,unsigned target,re5::CameraPose& pose){
 return memory.get(object,eye,pose.eye)&&memory.get(object,up,pose.up)&&memory.get(object,target,pose.target)
  &&re5::camera_detail::finite(pose.eye)&&re5::camera_detail::finite(pose.up)&&re5::camera_detail::finite(pose.target);
}
bool camera_follow_owned(CameraFollowSlot& slot,std::uintptr_t camera,std::uintptr_t base,const ControlFrame& frame,float dt){
 const auto& accepted=slot.frame;
 if(slot.camera!=camera||!std::isfinite(dt)||dt<=0||dt>.25f
  ||!frame.heading_valid||frame.scope||frame.action>8||!frame.settings.analog_enabled||!frame.settings.unified_controls||!frame.settings.free_camera||frame.settings.first_person
  ||frame.player.actor!=accepted.player.actor||frame.player.command!=accepted.player.command||frame.player.local_player!=accepted.player.local_player
  ||frame.frame!=accepted.frame||frame.generation!=accepted.generation||frame.look_epoch!=accepted.look_epoch||frame.look_release!=accepted.look_release||frame.tick!=accepted.tick
  ||frame.look_integral!=accepted.look_integral||frame.overview_pitch!=accepted.overview_pitch||frame.settings.third_person_distance!=accepted.settings.third_person_distance)return false;
 re5::Settings settings;
 if(!controls_snapshot.read(settings))return false;
 const auto& current=settings.players[frame.player.local_player];
 if(!current.analog_enabled||!current.unified_controls||!current.free_camera||current.first_person||current.third_person_distance!=accepted.settings.third_person_distance)return false;
 std::uintptr_t owned=0;std::uint8_t skipped=1,mode=2,pending=1;unsigned action=9;float transition=1;
 if(!ordinary_camera(frame.player.actor,slot.component,owned)||owned!=camera||!input_unfrozen(frame.player.actor)
  ||!memory.get(frame.player.actor,0x1B40,action)||action>8
  ||!memory.get(camera,0x2EC1,skipped)||skipped||!memory.get(camera,0x344,transition)||!std::isfinite(transition)||transition>0
  ||!memory.get(base,0x1A3,mode)||mode>1||!memory.get(base,0x274,pending)||pending)return false;
 // Authenticate both native copies. A scripted/transition-adjusted goal keeps
 // its native follow; steady ordinary Desired copies these vectors exactly.
 for(const auto object:{slot.component,camera,base}){
  re5::CameraPose pose;if(!read_follow_pose(object,0x160,0x170,0x180,pose)||std::memcmp(&pose,&slot.desired,sizeof(pose)))return false;
 }
 re5::CameraPose checked;
 if(!anchored_pose(frame,camera,slot.desired,checked,true)||std::memcmp(&checked,&slot.desired,sizeof(checked)))return false;
 re5::CameraPose old;if(!read_follow_pose(base,0x60,0x70,0x80,old))return false;
 // Pure lens/roll changes retain native admission, flags and coefficient
 // evolution. Our zero-threshold request is only for actual spatial work.
 bool spatial=false;for(unsigned i=0;i<3;++i)if(old.eye[i]!=slot.desired.eye[i]||old.target[i]!=slot.desired.target[i])spatial=true;
 if(!spatial)return false;
 for(unsigned offset:{0x194u,0x230u,0x234u,0x238u,0x23Cu,0x240u}){
  float value=0;if(!memory.get(base,offset,value)||!std::isfinite(value)||value<0||value>1)return false;
 }
 return writable(nullptr,base+0x230,12)&&writable(nullptr,base+0x240,4)
  &&writable(nullptr,base+0x270,4)&&writable(nullptr,base+0x274,1);
}
CameraFollowSlot* active_camera_follow(exl::hook::InlineFloatCtx* ctx){
 if(!ctx)return nullptr;
 const auto caller=thread_id();if(!caller)return nullptr;
 const auto base=re4::saved_x(ctx,19);
 for(auto& slot:camera_follow_slots)if(slot.thread.load(std::memory_order_acquire)==caller&&slot.base==base)return &slot;
 return nullptr;
}
HOOK_DEFINE_INLINE(Re5CameraFollowArm){static void Callback(exl::hook::InlineFloatCtx* ctx){
 auto* slot=active_camera_follow(ctx);if(!slot)return;
 // S3 is native .98*priorAlpha+.02*configuredAlpha before its first spatial
 // use. Keep its exact computed bits; do not duplicate native float arithmetic.
 const float alpha=saved_s(ctx,3);if(!std::isfinite(alpha)||alpha<0||alpha>1)return;
 std::memcpy(&slot->native_alpha,&alpha,4);slot->arm_seen=true;set_saved_s(ctx,3,0);
}};
HOOK_DEFINE_INLINE(Re5CameraFollowWorld){static void Callback(exl::hook::InlineFloatCtx* ctx){
 if(active_camera_follow(ctx))set_saved_s(ctx,5,0); // Later roll/FOV reload native194 independently.
}};
HOOK_DEFINE_TRAMPOLINE(Re5CameraFollow){static void Callback(void* parameter,float dt){
 const auto base=reinterpret_cast<std::uintptr_t>(parameter);if(base<0x350){Orig(parameter,dt);return;}
 const auto camera=base-0x350;CameraFollowSlot* claimed=nullptr;
 // Consume only this camera's publication before any fallible route/freshness
 // read. A rejected first consumption cannot leave a lease for a later call.
 for(auto& candidate:camera_follow_slots){
  if(candidate.lock.test_and_set(std::memory_order_acquire))continue;
  if(candidate.pending&&candidate.camera==camera){candidate.pending=false;claimed=&candidate;break;}
  candidate.lock.clear(std::memory_order_release);
 }
 if(!claimed){Orig(parameter,dt);return;}
 auto& slot=*claimed;
 struct Unlock{CameraFollowSlot& slot;~Unlock(){slot.lock.clear(std::memory_order_release);}} unlock{slot};
 std::uintptr_t actor=0;ControlFrame frame;
 if(!memory.get(camera,0x130,actor)||!fresh_frame(actor,frame)){Orig(parameter,dt);return;}
 const auto caller=thread_id();
 if(!caller||!camera_follow_owned(slot,camera,base,frame,dt)){Orig(parameter,dt);return;}
 std::array<std::uint32_t,3> origin{};std::uint32_t threshold=0;std::uint8_t mode=0;
 if(!memory.get(base,0x230,origin)||!memory.get(base,0x270,threshold)||!memory.get(base,0x1A3,mode)){Orig(parameter,dt);return;}
 struct Restore {
  CameraFollowSlot& slot;std::uintptr_t base;std::array<std::uint32_t,3> origin;std::uint32_t threshold;bool local;
  ~Restore(){
   slot.thread.store(0,std::memory_order_release);
   if(local){std::memcpy(reinterpret_cast<void*>(base+0x230),origin.data(),12);
    if(slot.arm_seen)std::memcpy(reinterpret_cast<void*>(base+0x240),&slot.native_alpha,4);}
   std::memcpy(reinterpret_cast<void*>(base+0x270),&threshold,4);
   *reinterpret_cast<std::uint8_t*>(base+0x274)=0; // Native consumes our one-shot on every return.
   slot.base=0;slot.arm_seen=false;
  }
 } restore{slot,base,origin,threshold,mode==1};
 if(mode==1)std::memset(reinterpret_cast<void*>(base+0x230),0,12);
 std::memset(reinterpret_cast<void*>(base+0x270),0,4);*reinterpret_cast<std::uint8_t*>(base+0x274)=1;
 slot.base=base;slot.arm_seen=false;slot.thread.store(caller,std::memory_order_release);
 Orig(parameter,dt); // Exactly once: native banks, roll/FOV, resets and flags run normally.
}};
