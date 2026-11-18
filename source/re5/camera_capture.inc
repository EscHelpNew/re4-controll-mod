// Observation only: the existing Monitor lease gates every payload copy.
// These guards precede gameplay lock guards, so publication runs after unlock.
struct CameraDecisionCapture {
 bool enabled;unsigned player=2;std::uint32_t request=0;
 union {re5::CameraProbeDecision value;}; // No nested CameraPose initialization while Off.
 CameraDecisionCapture(re5::CameraProbeStage stage,std::uintptr_t camera,std::uintptr_t component):enabled(runtime_probe_recorder.active()){
  if(enabled){::new(static_cast<void*>(&value)) re5::CameraProbeDecision{};value.stage=std::uint32_t(stage);value.camera=camera;value.component=component;request=__atomic_load_n(&runtime_probe.applied_sequence,__ATOMIC_ACQUIRE);}
 }
 ~CameraDecisionCapture(){
  if(!enabled||!runtime_probe_recorder.active()||request!=__atomic_load_n(&runtime_probe.applied_sequence,__ATOMIC_ACQUIRE))return;
  if(!value.tick)value.tick=svcGetSystemTick();
  camera_probe_recorder.decision(true,request,player,value);
 }
 re5::CameraProbeDecision* observation(){return enabled?&value:nullptr;}
 void reason(re5::CameraProbeReason reason){if(enabled)value.reason=std::uint32_t(reason);}
 void actor(std::uintptr_t actor){if(enabled)value.actor=actor;}
 void frame(const ControlFrame& frame){if(!enabled)return;player=frame.player.local_player;value.actor=frame.player.actor;value.command=frame.player.command;
  value.input_frame=frame.frame;value.input_tick=frame.tick;value.generation=frame.generation;value.look_epoch=frame.look_epoch;value.look_release=frame.look_release;value.action=frame.action;
  value.input_pitch=frame.overview_pitch;value.native_heading=frame.native_heading;value.look_integral=frame.look_integral;value.distance=frame.settings.third_person_distance;
  value.flags=(frame.valid?re5::CameraFrameValid:0u)|(frame.heading_valid?re5::CameraHeadingValid:0u)|(frame.scope?re5::CameraScope:0u)
   |(frame.settings.free_camera&&frame.settings.unified_controls?re5::CameraModern:0u)|(frame.settings.first_person?re5::CameraFirstPerson:0u);value.available|=re5::CameraHasFrame;
 }
 void camera(std::uintptr_t camera){if(enabled)value.camera=camera;}
 void native(const re5::CameraPose& native,bool up){if(enabled){value.native=native;value.available|=re5::CameraHasNativeEyeTarget;if(up)value.available|=re5::CameraHasNativeUp;}}
 void result(const re5::CameraPose& result,bool up){if(enabled){value.result=result;value.available|=re5::CameraHasResultEyeTarget;if(up)value.available|=re5::CameraHasResultUp;}}
 void fov(std::uintptr_t component){if(enabled&&memory.get(component,0x190,value.fov))value.available|=re5::CameraHasFov;}
};
struct CameraInputCapture {
 bool enabled;unsigned player=2;std::uint32_t request=0;re5::CameraProbeInput value;
 CameraInputCapture():enabled(runtime_probe_recorder.active()){if(enabled){value={};request=__atomic_load_n(&runtime_probe.applied_sequence,__ATOMIC_ACQUIRE);}}
 ~CameraInputCapture(){if(enabled&&runtime_probe_recorder.active()&&request==__atomic_load_n(&runtime_probe.applied_sequence,__ATOMIC_ACQUIRE))camera_probe_recorder.input(true,request,player,value);}
 void shaped(const re5::Axes& axes){if(enabled)value.shaped={axes.lx,axes.ly,axes.rx,axes.ry};}
 void finish(const ControlFrame& frame,const re5::PadState& pad,const PlayerRuntime& runtime){if(!enabled)return;player=frame.player.local_player;
  value.tick=frame.tick;value.actor=frame.player.actor;value.command=frame.player.command;value.frame=frame.frame;value.generation=frame.generation;
  value.look_epoch=frame.look_epoch;value.look_release=frame.look_release;value.validation_tick=runtime.anchor_validated_tick;value.pad_sample=pad.sample;value.action=frame.action;
  value.flags=(frame.valid?re5::CameraFrameValid:0u)|(frame.heading_valid?re5::CameraHeadingValid:0u)|(frame.scope?re5::CameraScope:0u)
   |(frame.settings.free_camera&&frame.settings.unified_controls?re5::CameraModern:0u)|(frame.settings.first_person?re5::CameraFirstPerson:0u)|(runtime.anchored?re5::CameraInputAnchored:0u);
  value.route=(frame.player.sticks.controller&0xffffu)|(frame.player.sticks.style<<16);value.raw={pad.lx,pad.ly,pad.rx,pad.ry};
  value.filtered={frame.axes.lx,frame.axes.ly,frame.axes.rx,frame.axes.ry};value.pitch=frame.overview_pitch;value.native_heading=frame.native_heading;value.look_yaw=frame.look.yaw;value.look_pitch=frame.look.pitch;
 }
};
