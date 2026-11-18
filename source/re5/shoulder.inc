// Qualified player+D8 common join: after native action/ground tilt and before
// neck/world-bone evaluation. Only this actor worker may replace body yaw.
struct AnchoredLook {
 std::uintptr_t actor=0,command=0,camera=0;std::uint64_t generation=0,frame=0,tick=0;
 float heading=0,pitch=0,height=0,last_integral=0;std::uint64_t look_epoch=0;bool active=false,anchor=false;std::array<float,16> expected_world{};re5::CameraPose arm;bool arm_valid=false;
} anchored_looks[2]; // Protected by its separate per-player anchored lock.
bool actor_rotation_supported(std::uintptr_t actor){
 std::uint16_t type=0,order=0,world_mode=0;std::uintptr_t parent=0;std::uint8_t attached=1,frozen=1,pending=1,blocked=1;std::array<float,4> q{};float scale[3]{};
 if(!memory.get(actor,8,type)||type!=0x201||!memory.get(actor,0x1A71,frozen)||frozen
  ||!memory.get(actor,0x1B4C,pending)||pending||!memory.get(actor,0x1AB0,blocked)||blocked||!memory.get(actor,0x48,parent)||parent
  ||!memory.get(actor,0x54,order)||order>5||!memory.get(actor,0x56,world_mode)||world_mode!=3||!memory.get(actor,0x2708,attached)||attached
  ||!memory.get(actor,0x70,q)||!memory.get(actor,0x80,scale))return false;
 float norm=0;for(float x:q){if(!std::isfinite(x))return false;norm+=x*x;}
 for(float x:scale)if(x!=1.f)return false;
 return std::fabs(norm-1.f)<.01f&&writable(nullptr,actor+0x70,16)&&writable(nullptr,actor+0x90,64);
}
bool actor_anchor_allowed(const ControlFrame& frame,std::uintptr_t& camera){
 unsigned action=0;
 return anchored_controls(frame)&&input_unfrozen(frame.player.actor)&&ordinary_camera(frame.player.actor,0,camera)
  &&actor_rotation_supported(frame.player.actor)&&memory.get(frame.player.actor,0x1B40,action)&&anchored_action(action,frame.settings);
}
void release_anchor(AnchoredLook& state,const ControlFrame& frame){
 // Invalidate a published accepted epoch even before its first body callback.
 // CAS makes repeated observations of that same frame a single invalidation;
 // native/default or already-rejected input does not create release activity.
 if(frame.heading_valid&&anchored_controls(frame)&&frame.look_epoch){
  auto expected=frame.look_release;
  anchored_releases[frame.player.local_player].compare_exchange_strong(expected,frame.look_release+1,std::memory_order_acq_rel,std::memory_order_acquire);
 }
 state={};
}
HOOK_DEFINE_INLINE(Re5ActorLook){static void Callback(exl::hook::InlineFloatCtx* ctx){
 const auto actor=re4::saved_x(ctx,19);ControlFrame frame;if(!fresh_frame(actor,frame))return;
 auto& lock=anchored_locks[frame.player.local_player];if(lock.test_and_set(std::memory_order_acquire))return;
 struct Unlock{std::atomic_flag& lock;~Unlock(){lock.clear(std::memory_order_release);}} unlock{lock};
 auto& state=anchored_looks[frame.player.local_player];std::uintptr_t camera=0;
 if(frame.look_release!=anchored_releases[frame.player.local_player].load(std::memory_order_acquire)){state={};return;}
 if(!frame.heading_valid||!actor_anchor_allowed(frame,camera)||!actor_rotation_api.get||!actor_rotation_api.set||!actor_rotation_api.rebuild){release_anchor(state,frame);return;}
 if(!std::isfinite(frame.look_integral)||!std::isfinite(frame.overview_pitch)||std::fabs(frame.overview_pitch)>.610866f||!frame.look_epoch){state={};return;}
 if(!state.active||state.actor!=actor||state.command!=frame.player.command||state.camera!=camera||state.generation!=frame.generation||state.look_epoch!=frame.look_epoch){
  state={};state.actor=actor;state.command=frame.player.command;state.camera=camera;state.generation=frame.generation;
  state.heading=std::remainder(frame.native_heading+frame.look_integral,6.28318530718f);state.last_integral=frame.look_integral;
  state.pitch=frame.overview_pitch;state.look_epoch=frame.look_epoch;state.active=true;
 }
 if(frame.frame<state.frame||frame.tick<state.tick){state={};return;}
 if(frame.frame>state.frame){
  if(state.tick&&frame.tick-state.tick>1920000){
   // A stopped actor phase drains a long backlog rather than catching up.
   const float present=actor_rotation_api.get(actor);if(!std::isfinite(present)){state={};return;}state.heading=present;
  }else state.heading=std::remainder(state.heading+std::remainder(frame.look_integral-state.last_integral,6.28318530718f),6.28318530718f);
  state.pitch=frame.overview_pitch;state.last_integral=frame.look_integral;state.frame=frame.frame;state.tick=frame.tick;
 }
 const float current=actor_rotation_api.get(actor);
 if(!std::isfinite(current)||std::fabs(current)>6.283186f){state={};return;}
 if(std::fabs(std::remainder(state.heading-current,6.28318530718f))>.000001f){
  actor_rotation_api.set(actor,state.heading);
  actor_rotation_api.rebuild(actor); // Native+D8 tail builds world transform/bones next.
  const float applied=actor_rotation_api.get(actor);
  if(!std::isfinite(applied)||std::fabs(std::remainder(applied-state.heading,6.28318530718f))>.00001f){state={};return;}
  runtime_probe_recorder.record(re5::RuntimeCounter::BodyHeadingApplied);
 }
 if(!memory.get(actor,0x90,state.expected_world)){state={};return;}
 for(float value:state.expected_world)if(!std::isfinite(value)){state={};return;}
}};
void camera_probe_reason(re5::CameraProbeDecision* observation,re5::CameraProbeReason reason){if(observation)observation->reason=std::uint32_t(reason);}
void camera_probe_body(re5::CameraProbeDecision* observation,const AnchoredLook& state){
 if(!observation)return;
 observation->body_frame=state.frame;observation->body_epoch=state.look_epoch;observation->body_generation=state.generation;
 observation->body_pitch=state.pitch;observation->heading=state.heading;observation->body_integral=state.last_integral;observation->height=state.height;
 observation->flags=(observation->flags&~std::uint32_t(re5::CameraBodyActive|re5::CameraBodyAnchor|re5::CameraArmValid))
  |(state.active?re5::CameraBodyActive:0u)|(state.anchor?re5::CameraBodyAnchor:0u)|(state.arm_valid?re5::CameraArmValid:0u);
 observation->expected_world=state.expected_world;observation->available|=re5::CameraHasBody|re5::CameraHasExpectedWorld;
 if(state.arm_valid){observation->arm=state.arm;observation->available|=re5::CameraHasArm;}
}
bool anchored_pose(const ControlFrame& frame,std::uintptr_t camera,const re5::CameraPose& native,re5::CameraPose& result,bool fresh_pitch=false,re5::CameraProbeDecision* observation=nullptr){
 auto& lock=anchored_locks[frame.player.local_player];if(lock.test_and_set(std::memory_order_acquire)){camera_probe_reason(observation,re5::CameraProbeReason::AnchorBusy);return false;}
 struct Unlock{std::atomic_flag& lock;~Unlock(){lock.clear(std::memory_order_release);}} unlock{lock};
 auto& state=anchored_looks[frame.player.local_player];std::uintptr_t current=0;camera_probe_body(observation,state);
 if(frame.look_release!=anchored_releases[frame.player.local_player].load(std::memory_order_acquire)){camera_probe_reason(observation,re5::CameraProbeReason::ReleaseMismatch);return false;}
 // Separate the original short-circuit clauses only to retain the exact
 // clause that failed. Native reads, release side effects and order are equal.
 if(!frame.heading_valid){camera_probe_reason(observation,re5::CameraProbeReason::HeadingMissing);release_anchor(state,frame);return false;}
 if(!actor_anchor_allowed(frame,current)){camera_probe_reason(observation,re5::CameraProbeReason::ActorBlocked);release_anchor(state,frame);return false;}
 if(current!=camera){camera_probe_reason(observation,re5::CameraProbeReason::BadOwner);release_anchor(state,frame);return false;}
 if(!state.active||state.actor!=frame.player.actor||state.command!=frame.player.command||state.camera!=camera
  ||state.generation!=frame.generation||state.frame>frame.frame
  ||(fresh_pitch&&(state.look_epoch!=frame.look_epoch||!std::isfinite(frame.overview_pitch)||std::fabs(frame.overview_pitch)>.610866f))){
  camera_probe_reason(observation,re5::CameraProbeReason::BodyIdentity);
  if(observation&&state.active&&state.actor==frame.player.actor&&state.command==frame.player.command&&state.camera==camera&&state.generation==frame.generation&&state.frame<=frame.frame)
   camera_probe_reason(observation,state.look_epoch!=frame.look_epoch?re5::CameraProbeReason::EpochMismatch:re5::CameraProbeReason::MathRejected);
  return false;
 }
 // Body yaw commits the exact basis. Native movement/ground resolution may
 // update translation afterward; both complete current reads must stay stable.
 std::array<float,16> world{},again{};
 if(!memory.get(frame.player.actor,0xD0,world)){camera_probe_reason(observation,re5::CameraProbeReason::WorldRead);return false;}
 if(observation){observation->world=world;observation->available|=re5::CameraHasWorld;}
 if(!memory.get(frame.player.actor,0xD0,again)){camera_probe_reason(observation,re5::CameraProbeReason::WorldRead);return false;}
 if(observation){observation->world_again=again;observation->available|=re5::CameraHasWorldAgain;}
 if(std::memcmp(world.data(),again.data(),sizeof(world))){camera_probe_reason(observation,re5::CameraProbeReason::WorldChanged);return false;}
 if(std::memcmp(world.data(),state.expected_world.data(),12*sizeof(float))||std::memcmp(world.data()+15,state.expected_world.data()+15,sizeof(float))){camera_probe_reason(observation,re5::CameraProbeReason::WorldNotCommitted);return false;}
 std::array<float,3> root{world[12],world[13],world[14]};const float forward_x=world[8],forward_z=world[10];
 if(!re5::camera_detail::finite(root)){camera_probe_reason(observation,re5::CameraProbeReason::SourceInvalid);return false;}
 if(observation){observation->forward_x=forward_x;observation->forward_z=forward_z;}
 float height=state.height;
 if(!state.anchor){float head[3];if(!head_anchor(frame.player.actor,head)){camera_probe_reason(observation,re5::CameraProbeReason::HeadRead);return false;}
  height=head[1]-root[1];if(observation)observation->height=height;
  if(!std::isfinite(height)||height<=0||height>10000){camera_probe_reason(observation,re5::CameraProbeReason::AnchorHeight);return false;}
 }
 auto anchor=root;anchor[1]+=height;
 if(!re5::camera_detail::finite(anchor)){camera_probe_reason(observation,re5::CameraProbeReason::SourceInvalid);return false;}
 // A rejected current root/anchor must not poison a retained height or arm.
 if(!state.anchor){state.height=height;state.anchor=true;}
 if(observation){observation->anchor=anchor;observation->available|=re5::CameraHasAnchor;}
 if(!state.arm_valid){
  state.arm=native;
  for(unsigned i=0;i<3;++i){state.arm.eye[i]-=anchor[i];state.arm.target[i]-=anchor[i];}
  state.arm_valid=true;
 }
 camera_probe_body(observation,state);
 auto arm=state.arm;for(unsigned i=0;i<3;++i){arm.eye[i]+=anchor[i];arm.target[i]+=anchor[i];}
 if(!re5::anchored_camera_pose(arm,anchor,forward_x,forward_z,fresh_pitch?frame.overview_pitch:state.pitch,frame.settings.third_person_distance,result)){camera_probe_reason(observation,re5::CameraProbeReason::MathRejected);return false;}
 if(frame.settings.first_person){float head[3];if(!head_anchor(frame.player.actor,head)){camera_probe_reason(observation,re5::CameraProbeReason::HeadRead);return false;}
  for(unsigned i=0;i<3;++i){const float delta=result.target[i]-result.eye[i];result.eye[i]=head[i];result.target[i]=head[i]+delta;}
 }
 return true;
}
