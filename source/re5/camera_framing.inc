// Overview keeps its own screen reference. Each combat-family entry retains
// a settled native optical reference, independent of lens and overview heading.
struct CameraFramingAnchor {
 std::array<float,16> world{},world_again{};std::array<float,3> anchor{};float height=0,native_fov=0,countdown=0;std::uint64_t native_epoch=0;std::uint8_t native_mode=0;bool settled=false;re5::CameraPose native{};
};
struct CameraFramingReference {
 std::atomic_flag lock=ATOMIC_FLAG_INIT;
 std::uintptr_t actor=0,command=0,camera=0;std::uint64_t generation=0,combat_epoch=0;
 float height=0;bool height_valid=false,valid=false,combat_valid=false;unsigned family=0;std::array<double,2> projection{},combat_optical{};
} camera_framing_refs[2];
std::atomic<std::uint64_t> framing_native_epochs[2]{};
bool framing_identity(const CameraFramingReference& state,const ControlFrame& frame,std::uintptr_t camera){
 return state.actor==frame.player.actor&&state.command==frame.player.command&&state.camera==camera&&state.generation==frame.generation;
}
bool framing_combat(unsigned action){return re5::active_aim(action)||action==0x1F;}
unsigned framing_family(unsigned action){return action<=8?1u:action>=0x1B&&action<=0x1E?2u:action>=0x22&&action<=0x27?3u:action==0x1F?4u:0u;}
// Revoke only the actor's genuine default component, including a native
// scope/script handoff. A different component cannot discard this reference.
void revoke_native_combat_framing(const ControlFrame& frame,std::uintptr_t component,bool confirmed_only=false){
 if(frame.player.local_player>=2||!frame.valid)return;
 std::uintptr_t camera=0,owner=0,defaults=0,vtable=0;
 if(!memory.get(frame.player.actor,0x1A80,camera)||!camera||!memory.get(camera,0,vtable)
  ||vtable!=main_base+re5::profile::offset<0x1AD2510>()||!memory.get(camera,0x130,owner)||owner!=frame.player.actor
  ||!memory.get(camera,0xDA0,defaults)||defaults!=component)return;
 if(confirmed_only){
  std::uint8_t value=0;unsigned action=0;float transition=0;std::uintptr_t active=0;
  const bool excluded=(memory.get(component,0x197,value)&&value)
   ||(memory.get(camera,0x2EC1,value)&&value)||(memory.get(frame.player.actor,0x3AD8,value)&&value)
   ||(memory.get(frame.player.actor,0x1B40,action)&&framing_family(action)!=framing_family(frame.action))
   ||(memory.get(camera,0x344,transition)&&std::isfinite(transition)&&transition>0)
   ||(memory.get(camera,0x198,active)&&active!=defaults);
  // A busy settings snapshot or unavailable read is not a native handoff.
  if(!excluded)return;
 }
 framing_native_epochs[frame.player.local_player].fetch_add(1,std::memory_order_acq_rel);
}
void revoke_native_combat_camera(const ControlFrame& frame,std::uintptr_t camera){
 std::uintptr_t current=0,component=0;
 if(memory.get(frame.player.actor,0x1A80,current)&&current==camera&&memory.get(camera,0xDA0,component))
  revoke_native_combat_framing(frame,component,true);
}
bool framing_combat_committed(const ControlFrame& frame,std::uintptr_t camera,bool& ready){
 ready=false;if(frame.player.local_player>=2)return false;
 auto& state=camera_framing_refs[frame.player.local_player];if(state.lock.test_and_set(std::memory_order_acquire))return false;
 ready=framing_identity(state,frame,camera)&&state.height_valid&&state.combat_valid&&state.family==framing_family(frame.action)
  &&state.combat_epoch==framing_native_epochs[frame.player.local_player].load(std::memory_order_acquire);
 state.lock.clear(std::memory_order_release);return true;
}
bool framing_observe_family(const ControlFrame& frame){
 if(frame.player.local_player>=2||!frame.valid)return false;
 auto& state=camera_framing_refs[frame.player.local_player];if(state.lock.test_and_set(std::memory_order_acquire))return false;
 const bool owned=state.actor==frame.player.actor&&state.command==frame.player.command&&state.generation==frame.generation;
 const bool enabled=frame.settings.analog_enabled&&frame.settings.unified_controls&&frame.settings.free_camera&&!frame.settings.first_person&&!frame.scope;
 const unsigned family=enabled?framing_family(frame.action):0;
 if(owned&&state.family!=family){state.family=family;state.combat_valid=false;framing_native_epochs[frame.player.local_player].fetch_add(1,std::memory_order_acq_rel);}
 state.lock.clear(std::memory_order_release);return true;
}
bool framing_input_allowed(const ControlFrame& frame){
 std::uint8_t freeze=1;if(!memory.get(frame.player.actor,0x3F0E,freeze))return false;
 return !(freeze&1)||(frame.action==0x1F&&frame.settings.move_while_reloading);
}
bool framing_settings_current(const ControlFrame& frame){
 if(frame.player.local_player>=2)return false;
 re5::Settings settings;if(!controls_snapshot.read(settings))return false;
 const auto& current=settings.players[frame.player.local_player];
 return current.analog_enabled&&current.unified_controls&&current.free_camera&&!current.first_person
  &&current.third_person_distance==frame.settings.third_person_distance
  &&(frame.action!=0x1F||current.move_while_reloading==frame.settings.move_while_reloading);
}
bool framing_mode(const ControlFrame& frame,std::uintptr_t camera,std::uintptr_t component){
 if(frame.player.local_player>=2||!frame.valid||!frame.settings.analog_enabled||!frame.settings.unified_controls||!frame.settings.free_camera
  ||frame.settings.first_person||frame.scope||(frame.action>8&&!framing_combat(frame.action))||!framing_input_allowed(frame)
  ||!actor_rotation_supported(frame.player.actor)||!framing_settings_current(frame)){
  revoke_native_combat_framing(frame,component,true);return false;
 }
 unsigned action=0;std::uintptr_t owned=0;std::uint8_t partner=1,skipped=1;float transition=1;
 const bool allowed=memory.get(frame.player.actor,0x1B40,action)&&framing_family(action)==framing_family(frame.action)&&ordinary_camera(frame.player.actor,component,owned)&&owned==camera
  &&memory.get(component,0x197,partner)&&!partner&&memory.get(camera,0x2EC1,skipped)&&!skipped
  &&memory.get(camera,0x344,transition)&&std::isfinite(transition)&&transition<=0;
 if(!allowed)revoke_native_combat_framing(frame,component,true);
 return allowed;
}
bool framing_world(const ControlFrame& frame,CameraFramingAnchor& out){
 if(!memory.get(frame.player.actor,0xD0,out.world)||!memory.get(frame.player.actor,0xD0,out.world_again)
  ||std::memcmp(out.world.data(),out.world_again.data(),sizeof(out.world)))return false;
 for(float x:out.world)if(!std::isfinite(x)||std::fabs(x)>1000000.f)return false;
 return out.world[15]==1;
}
bool framing_cached_height(const ControlFrame& frame,std::uintptr_t camera,float& height,bool& found){
 auto& state=camera_framing_refs[frame.player.local_player];if(state.lock.test_and_set(std::memory_order_acquire))return false;
 found=framing_identity(state,frame,camera)&&state.height_valid;if(found)height=state.height;
 state.lock.clear(std::memory_order_release);return true;
}
bool framing_anchor(const ControlFrame& frame,std::uintptr_t camera,std::uintptr_t component,CameraFramingAnchor& out){
 if(!framing_mode(frame,camera,component)||!framing_world(frame,out))return false;
 float height=0;bool found=false;
 if(!framing_cached_height(frame,camera,height,found))return false;
 if(!found){
  if(frame.action<=8){
   auto& lock=anchored_locks[frame.player.local_player];if(lock.test_and_set(std::memory_order_acquire))return false;
   const auto& body=anchored_looks[frame.player.local_player];
   const bool valid=body.active&&body.anchor&&body.actor==frame.player.actor&&body.command==frame.player.command&&body.camera==camera
    &&body.generation==frame.generation&&body.look_epoch==frame.look_epoch;
   if(valid)height=body.height;
   lock.clear(std::memory_order_release);if(!valid)return false;
  }else{float head[3];if(!head_anchor(frame.player.actor,head))return false;height=head[1]-out.world[13];}
 }
 if(!std::isfinite(height)||height<=0||height>10000)return false;
 out.height=height;out.anchor={out.world[12],out.world[13]+height,out.world[14]};
 return re5::camera_detail::finite(out.anchor);
}
bool framing_anchor_current(const ControlFrame& frame,std::uintptr_t camera,std::uintptr_t component,const CameraFramingAnchor& accepted){
 if(!framing_mode(frame,camera,component))return false;
 CameraFramingAnchor current;
 if(!framing_world(frame,current)||std::memcmp(current.world.data(),accepted.world.data(),sizeof(current.world))
  ||!std::isfinite(accepted.height)||accepted.height<=0||accepted.height>10000)return false;
 const std::array<float,3> anchor{current.world[12],current.world[13]+accepted.height,current.world[14]};
 if(framing_combat(frame.action)){
  std::uint8_t mode=0;float countdown=1,fov=0;
  if(accepted.native_epoch!=framing_native_epochs[frame.player.local_player].load(std::memory_order_acquire)
   ||!memory.get(component,0x195,mode)||std::uint8_t(mode!=0)!=accepted.native_mode
   ||!memory.get(component,0x630,countdown)||!std::isfinite(countdown)||countdown!=accepted.countdown
   ||!memory.get(component,0x190,fov)||fov!=accepted.native_fov)return false;
  if(!accepted.settled){bool ready=false;if(!framing_combat_committed(frame,camera,ready)||!ready)return false;}
 }
 return re5::camera_detail::finite(anchor)&&!std::memcmp(anchor.data(),accepted.anchor.data(),sizeof(anchor));
}
bool combat_camera_pose(const ControlFrame& frame,std::uintptr_t camera,std::uintptr_t component,
 const re5::CameraPose& native,re5::CameraPose& result,CameraFramingAnchor* prepared=nullptr){
 if(!framing_combat(frame.action))return false;
 CameraFramingAnchor anchor;
 if(!framing_anchor(frame,camera,component,anchor)||!re5::dilated_camera_pose(native,anchor.anchor,frame.settings.third_person_distance,result))return false;
 float countdown=1;std::uint8_t mode=0;
 if(!memory.get(component,0x630,countdown)||!std::isfinite(countdown)||countdown<0||!memory.get(component,0x195,mode)
  ||!memory.get(component,0x190,anchor.native_fov)||!std::isfinite(anchor.native_fov)||anchor.native_fov<=1||anchor.native_fov>=179)return false;
 anchor.native_epoch=framing_native_epochs[frame.player.local_player].load(std::memory_order_acquire);
 if(countdown!=0){bool ready=false;if(!framing_combat_committed(frame,camera,ready)||!ready)return false;}
 anchor.native_mode=std::uint8_t(mode!=0);anchor.countdown=countdown;anchor.settled=countdown==0;
 anchor.native=native;if(prepared)*prepared=anchor;return true;
}
bool accept_camera_framing(const ControlFrame& frame,std::uintptr_t camera,std::uintptr_t component,
 const CameraFramingAnchor* prepared,CameraFramingAnchor& accepted){
 if(prepared){if(!framing_anchor_current(frame,camera,component,*prepared))return false;accepted=*prepared;}
 else if(!framing_anchor(frame,camera,component,accepted))return false;
 auto& state=camera_framing_refs[frame.player.local_player];if(state.lock.test_and_set(std::memory_order_acquire))return false;
 if(!framing_identity(state,frame,camera)){
  state.actor=frame.player.actor;state.command=frame.player.command;state.camera=camera;state.generation=frame.generation;
  state.height_valid=state.valid=state.combat_valid=false;state.family=framing_family(frame.action);
 }
 if(state.height_valid&&state.height!=accepted.height){state.lock.clear(std::memory_order_release);return false;}
 state.height=accepted.height;state.height_valid=true;state.lock.clear(std::memory_order_release);return true;
}
