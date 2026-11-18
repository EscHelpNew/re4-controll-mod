// Native Desired owns banks/history and collision remains downstream. One
// accepted component authorizes one follow invocation and one final Desired
// composition pass; neither lease can survive the next pre-Desired boundary.
struct CameraFramingCompletion {unsigned player=2;std::uint64_t sequence=0;};
struct CameraFollowSlot {
 std::atomic_flag lock=ATOMIC_FLAG_INIT;std::atomic<std::uint64_t> thread{0};
 ControlFrame frame;re5::CameraPose desired;CameraFramingAnchor anchor;
 std::atomic<std::uintptr_t> completed_camera{0};std::atomic<std::uint64_t> completed_sequence{0};
 std::uint64_t completion_serial=0,last_completed=0;
 std::uintptr_t camera=0,component=0,base=0;
 bool pending=false,complete=false,anchor_valid=false,arm_seen=false;std::uint32_t native_alpha=0;
} camera_follow_slots[2];
void invalidate_camera_follow(std::uintptr_t camera,std::uintptr_t component){
 for(auto& slot:camera_follow_slots){
  if(slot.lock.test_and_set(std::memory_order_acquire))continue;
  if((camera&&slot.camera==camera)||(component&&slot.component==component)){slot.completed_sequence.store(0,std::memory_order_release);slot.pending=slot.complete=false;}
  slot.lock.clear(std::memory_order_release);
 }
}
void publish_camera_follow(const ControlFrame& frame,std::uintptr_t camera,std::uintptr_t component,
 const re5::CameraPose& desired,const CameraFramingAnchor* prepared=nullptr){
 if(frame.player.local_player>=2||!frame.settings.analog_enabled||!frame.settings.unified_controls||!frame.settings.free_camera
  ||frame.settings.first_person||frame.scope||(frame.action>8&&!framing_combat(frame.action)))return;
 if(frame.action>8&&!prepared)return;
 auto& slot=camera_follow_slots[frame.player.local_player];
 if(slot.lock.test_and_set(std::memory_order_acquire))return;
 slot.completed_sequence.store(0,std::memory_order_release);slot.completed_camera.store(camera,std::memory_order_release);
 slot.frame=frame;slot.desired=desired;slot.camera=camera;slot.component=component;slot.pending=true;slot.complete=false;
 // Reference availability never changes the already qualified28 overview
 // follow path. A missing lens merely delays its optional final composition.
 slot.anchor_valid=accept_camera_framing(frame,camera,component,prepared,slot.anchor);
 slot.lock.clear(std::memory_order_release);
}
bool read_follow_pose(std::uintptr_t object,unsigned eye,unsigned up,unsigned target,re5::CameraPose& pose){
 return memory.get(object,eye,pose.eye)&&memory.get(object,up,pose.up)&&memory.get(object,target,pose.target)
  &&re5::camera_detail::finite(pose.eye)&&re5::camera_detail::finite(pose.up)&&re5::camera_detail::finite(pose.target);
}
bool same_camera_follow_frame(const ControlFrame& frame,const ControlFrame& accepted){
 if(frame.player.actor!=accepted.player.actor||frame.player.command!=accepted.player.command||frame.player.local_player!=accepted.player.local_player
  ||frame.player.sticks.controller!=accepted.player.sticks.controller||frame.player.sticks.style!=accepted.player.sticks.style
  ||frame.player.motion.controller!=accepted.player.motion.controller||frame.player.motion.style!=accepted.player.motion.style
  ||frame.frame!=accepted.frame||frame.generation!=accepted.generation||frame.tick!=accepted.tick||frame.action!=accepted.action
  ||frame.settings.third_person_distance!=accepted.settings.third_person_distance)return false;
 if(frame.action<=8)return frame.heading_valid&&frame.look_epoch==accepted.look_epoch&&frame.look_release==accepted.look_release
  &&frame.look_integral==accepted.look_integral&&frame.overview_pitch==accepted.overview_pitch;
 return framing_combat(frame.action);
}
bool camera_follow_source(CameraFollowSlot& slot,std::uintptr_t camera,const ControlFrame& frame){
 if(slot.camera!=camera||!same_camera_follow_frame(frame,slot.frame)||!framing_mode(frame,camera,slot.component))return false;
 re5::Settings settings;if(!controls_snapshot.read(settings))return false;
 const auto& current=settings.players[frame.player.local_player];
 if(!current.analog_enabled||!current.unified_controls||!current.free_camera||current.first_person
  ||current.third_person_distance!=slot.frame.settings.third_person_distance
  ||(frame.action==0x1F&&current.move_while_reloading!=slot.frame.settings.move_while_reloading))return false;
 for(const auto object:{slot.component,camera,camera+0x350}){
  re5::CameraPose pose;if(!read_follow_pose(object,0x160,0x170,0x180,pose)||std::memcmp(&pose,&slot.desired,sizeof(pose)))return false;
 }
 if(frame.action<=8){
  re5::CameraPose checked;
  if(!anchored_pose(frame,camera,slot.desired,checked,true)||std::memcmp(&checked,&slot.desired,sizeof(checked)))return false;
 }else{
  if(!slot.anchor_valid||!framing_anchor_current(frame,camera,slot.component,slot.anchor))return false;
  re5::CameraPose checked;
  if(!re5::dilated_camera_pose(slot.anchor.native,slot.anchor.anchor,frame.settings.third_person_distance,checked)
   ||std::memcmp(&checked,&slot.desired,sizeof(checked)))return false;
 }
 return true;
}
bool camera_follow_owned(CameraFollowSlot& slot,std::uintptr_t camera,std::uintptr_t base,const ControlFrame& frame,float dt,bool* spatial_out=nullptr){
 if(!std::isfinite(dt)||dt<=0||dt>.25f||!camera_follow_source(slot,camera,frame))return false;
 std::uint8_t mode=2,pending=1;
 if(!memory.get(base,0x1A3,mode)||mode>1||!memory.get(base,0x274,pending)||pending)return false;
 re5::CameraPose old;if(!read_follow_pose(base,0x60,0x70,0x80,old))return false;
 bool spatial=false;for(unsigned i=0;i<3;++i)if(old.eye[i]!=slot.desired.eye[i]||old.target[i]!=slot.desired.target[i])spatial=true;
 if(spatial_out)*spatial_out=spatial;
 // A lens/roll-only invocation still completes its provenance. Its complete
 // native admission, flags and coefficient evolution remain untouched.
 if(!spatial)return true;
 for(unsigned offset:{0x194u,0x230u,0x234u,0x238u,0x23Cu,0x240u}){
  float value=0;if(!memory.get(base,offset,value)||!std::isfinite(value)||value<0||value>1)return false;
 }
 return writable(nullptr,base+0x230,12)&&writable(nullptr,base+0x240,4)
  &&writable(nullptr,base+0x270,4)&&writable(nullptr,base+0x274,1);
}
void complete_camera_follow(CameraFollowSlot& slot){
 if(!++slot.completion_serial)++slot.completion_serial;
 slot.last_completed=slot.completion_serial;slot.complete=true;
 slot.completed_camera.store(slot.camera,std::memory_order_release);
 slot.completed_sequence.store(slot.last_completed,std::memory_order_release);
}
CameraFramingCompletion claim_camera_framing(std::uintptr_t camera){
 for(unsigned i=0;i<2;++i){auto& slot=camera_follow_slots[i];
  if(slot.completed_camera.load(std::memory_order_acquire)!=camera)continue;
  auto sequence=slot.completed_sequence.load(std::memory_order_acquire);
  if(!sequence||slot.completed_camera.load(std::memory_order_acquire)!=camera)continue;
  if(slot.completed_sequence.compare_exchange_strong(sequence,0,std::memory_order_acq_rel,std::memory_order_acquire))return {i,sequence};
 }
 return {};
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
 const float alpha=saved_s(ctx,3);if(!std::isfinite(alpha)||alpha<0||alpha>1)return;
 std::memcpy(&slot->native_alpha,&alpha,4);slot->arm_seen=true;set_saved_s(ctx,3,0);
}};
HOOK_DEFINE_INLINE(Re5CameraFollowWorld){static void Callback(exl::hook::InlineFloatCtx* ctx){
 if(active_camera_follow(ctx))set_saved_s(ctx,5,0); // Roll/FOV reload original194.
}};
HOOK_DEFINE_TRAMPOLINE(Re5CameraFollow){static void Callback(void* parameter,float dt){
 const auto base=reinterpret_cast<std::uintptr_t>(parameter);if(base<0x350){Orig(parameter,dt);return;}
 const auto camera=base-0x350;CameraFollowSlot* claimed=nullptr;
 for(auto& candidate:camera_follow_slots){
  if(candidate.lock.test_and_set(std::memory_order_acquire))continue;
  if(candidate.pending&&candidate.camera==camera){candidate.pending=false;candidate.complete=false;claimed=&candidate;break;}
  candidate.lock.clear(std::memory_order_release);
 }
 if(!claimed){Orig(parameter,dt);return;}
 auto& slot=*claimed;
 struct Unlock{CameraFollowSlot& slot;~Unlock(){slot.lock.clear(std::memory_order_release);}} unlock{slot};
 std::uintptr_t actor=0;ControlFrame frame;
 if(!memory.get(camera,0x130,actor)||!fresh_frame(actor,frame)){Orig(parameter,dt);return;}
 const auto caller=thread_id();bool spatial=false;
 if(!caller||!camera_follow_owned(slot,camera,base,frame,dt,&spatial)){Orig(parameter,dt);return;}
 if(!spatial){Orig(parameter,dt);complete_camera_follow(slot);return;}
 std::array<std::uint32_t,3> origin{};std::uint32_t threshold=0;std::uint8_t mode=0;
 if(!memory.get(base,0x230,origin)||!memory.get(base,0x270,threshold)||!memory.get(base,0x1A3,mode)){Orig(parameter,dt);return;}
 {
  struct Restore {
   CameraFollowSlot& slot;std::uintptr_t base;std::array<std::uint32_t,3> origin;std::uint32_t threshold;bool local;
   ~Restore(){
    slot.thread.store(0,std::memory_order_release);
    if(local){std::memcpy(reinterpret_cast<void*>(base+0x230),origin.data(),12);
     if(slot.arm_seen)std::memcpy(reinterpret_cast<void*>(base+0x240),&slot.native_alpha,4);}
    std::memcpy(reinterpret_cast<void*>(base+0x270),&threshold,4);
    *reinterpret_cast<std::uint8_t*>(base+0x274)=0;
    slot.base=0;slot.arm_seen=false;
   }
  } restore{slot,base,origin,threshold,mode==1};
  if(mode==1)std::memset(reinterpret_cast<void*>(base+0x230),0,12);
  std::memset(reinterpret_cast<void*>(base+0x270),0,4);*reinterpret_cast<std::uint8_t*>(base+0x274)=1;
  slot.base=base;slot.arm_seen=false;slot.thread.store(caller,std::memory_order_release);
  Orig(parameter,dt);
 }
 complete_camera_follow(slot);
}};
bool finish_camera_framing(std::uintptr_t camera,const ControlFrame& frame,CameraFramingCompletion completion){
 if(completion.player>=2||!completion.sequence||frame.player.local_player!=completion.player)return false;
 auto& slot=camera_follow_slots[completion.player];
 if(slot.lock.test_and_set(std::memory_order_acquire))return false;
 struct Unlock{CameraFollowSlot& slot;~Unlock(){slot.lock.clear(std::memory_order_release);}} unlock{slot};
 if(!slot.complete||slot.camera!=camera||slot.last_completed!=completion.sequence)return false;
 slot.complete=false; // Consume before every fallible final-Desired read.
 if(!slot.anchor_valid||!camera_follow_source(slot,camera,frame)||!framing_anchor_current(frame,camera,slot.component,slot.anchor))return false;
 re5::CameraPose native,result;float fov=0,aspect=0;
 if(!read_follow_pose(camera,0x60,0x70,0x80,native)||!memory.get(camera,0x50,fov)||!memory.get(camera,0x4C,aspect))return false;
 auto& reference=camera_framing_refs[frame.player.local_player];
 if(reference.lock.test_and_set(std::memory_order_acquire))return false;
 struct ReferenceUnlock{CameraFramingReference& state;~ReferenceUnlock(){state.lock.clear(std::memory_order_release);}} reference_unlock{reference};
 if(!framing_identity(reference,frame,camera)||!reference.height_valid||reference.height!=slot.anchor.height)return false;
 if(!reference.valid){
  std::array<double,2> point;if(!re5::camera_anchor_projection(native,slot.anchor.anchor,fov,aspect,point))return false;
  reference.projection=point;reference.valid=true;return false;
 }
 if(!re5::composed_camera_pose(native,slot.anchor.anchor,fov,aspect,reference.projection,result)
  ||!std::memcmp(&native,&result,sizeof(native))||!writable(nullptr,camera+0x60,12)||!writable(nullptr,camera+0x80,12))return false;
 // Only the actual final Desired eye/target change. Native shake orientation,
 // roll, lens and post-collision history remain native-owned.
 std::memcpy(reinterpret_cast<void*>(camera+0x60),result.eye.data(),12);
 std::memcpy(reinterpret_cast<void*>(camera+0x80),result.target.data(),12);
 return true;
}
