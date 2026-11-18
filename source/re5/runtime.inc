// Included inside module's private namespace. Only the validated RE5 image uses
// these adapters; global sPad/native option storage remains untouched.
struct PadSlot {
 std::atomic_flag lock=ATOMIC_FLAG_INIT;re5::PadState state;std::uint64_t tick=0;
 void publish(const re5::PadState& value){if(lock.test_and_set(std::memory_order_acquire))return;state=value;tick=svcGetSystemTick();lock.clear(std::memory_order_release);}
 bool read(re5::PadState& value){if(lock.test_and_set(std::memory_order_acquire))return false;
 const auto now=svcGetSystemTick();const bool valid=tick&&now>=tick&&now-tick<=1920000&&(state.attributes&1);if(valid)value=state;lock.clear(std::memory_order_release);return valid;}
} pad_slots[9][3];
unsigned pad_index(unsigned id){return id==32?8:id;}
unsigned style_index(unsigned style){return style==1?0:style==2?1:2;}
bool actor_player(std::uintptr_t actor,re5::ResolvedPlayer& player){
 std::uintptr_t wrapper=0;return memory.get(actor,0x3EE8,wrapper)&&wrapper&&wrapper<=UINTPTR_MAX-0x350&&re5::resolve_player(memory,main_base,actor,wrapper+0x350,player);
}
bool actor_pad(const re5::ResolvedPlayer& player,re5::PadState& pad){return pad_slots[pad_index(player.sticks.controller)][style_index(player.sticks.style)].read(pad);}
void observed_pad(re5::PadState* pad,unsigned id,unsigned style){
 runtime_probe_recorder.record(re5::RuntimeCounter::PadCalls);
 if(!pad||!game_active.load(std::memory_order_acquire)||(id>7&&id!=32))return;
 pad_slots[pad_index(id)][style_index(style)].publish(*pad); // raw, before remap
 re5::Settings settings;if(!controls_snapshot.read(settings)||!(pad->attributes&1))return;
 std::uintptr_t players=0;if(!memory.get(main_base,re5::profile::offset<0x1CF8CC8>(),players))return;
 int owner=-1;
 for(unsigned i=0;i<4;++i){std::uintptr_t actor=0;re5::ResolvedPlayer player;
  if(memory.get(players,0x30+i*8,actor)&&actor_player(actor,player)&&player.sticks.controller==id&&player.sticks.style==style){if(owner>=0&&owner!=int(player.local_player))return;owner=int(player.local_player);}}
 if(owner>=0){const auto& s=settings.players[owner];pad->buttons=re4::remap_buttons(pad->buttons,s.button_map,s.remap_enabled);}
}
HOOK_DEFINE_TRAMPOLINE(Re5FullPad){static void Callback(re5::PadState* pad,const std::uint32_t& id){Orig(pad,id);observed_pad(pad,id,1);}};
HOOK_DEFINE_TRAMPOLINE(Re5HandheldPad){static void Callback(re5::PadState* pad,const std::uint32_t& id){Orig(pad,id);observed_pad(pad,id,2);}};
HOOK_DEFINE_TRAMPOLINE(Re5DualPad){static void Callback(re5::PadState* pad,const std::uint32_t& id){Orig(pad,id);observed_pad(pad,id,4);}};
struct MotionReader {
 re5::ResolvedPad route{};std::uint32_t handle=0;bool running=false;
 bool read(const re5::ResolvedPad& source,re4::SensorState* states,int& count){
  using Handles=Result(*)(std::uint32_t*,int*,int,std::uint32_t,std::uint32_t);
  using Start=Result(*)(const std::uint32_t&);using Read=Result(*)(int*,re4::SensorState*,int,const std::uint32_t&);
  if(!running||route.controller!=source.controller||route.style!=source.style){
   running=false;route=source;std::uint32_t handles[2]{};int n=0;
   if(R_FAILED(reinterpret_cast<Handles>(sdk_base+0x22FAA0)(handles,&n,2,source.controller,source.style))||n<1||n>2)return false;
   // Prefer the right Joy-Con, matching the application's gyro preview.
   handle=handles[source.style==4&&n==2?1:0];
   if(R_FAILED(reinterpret_cast<Start>(sdk_base+0x229FA0)(handle)))return false;
   running=true;
  }
  count=0;const auto result=reinterpret_cast<Read>(sdk_base+0x22FC70)(&count,states,16,handle);
  if(R_FAILED(result)||count<0||count>16){running=false;count=0;return false;}return true;
 }
 // These handles may also be owned by native gestures. Do not Stop them here.
};
struct ControlFrame {
 re5::ResolvedPlayer player;re4::Settings settings;re5::Axes axes;re4::AimDelta gyro;
 re4::UnifiedDelta look;float native_heading=0,look_integral=0,overview_pitch=0;std::uint64_t look_epoch=0,look_release=0;
 std::uint64_t tick=0,generation=1,frame=0,ordinary_epoch=0;unsigned action=0;bool scope=false,valid=false,heading_valid=false,ordinary_proxy=false;
};
// The native getter returns its nontrivial vector through X8. A nontrivial
// C++ return type emits the same ABI; SDK assembly is verified independently.
struct alignas(16) NativeEuler {float value[4];~NativeEuler(){}};
static_assert(sizeof(NativeEuler)==16);
float native_actor_heading(std::uintptr_t actor){
 const auto value=reinterpret_cast<NativeEuler(*)(std::uintptr_t)>(main_base+re5::profile::offset<0xD28730>())(actor);
 return value.value[1];
}
void native_actor_heading(std::uintptr_t actor,float heading){reinterpret_cast<void(*)(std::uintptr_t,float)>(main_base+re5::profile::offset<0xD29000>())(actor,heading);}
void native_actor_local_matrix(std::uintptr_t actor){reinterpret_cast<void(*)(std::uintptr_t)>(main_base+re5::profile::offset<0xD29060>())(actor);}
struct ActorRotationApi {float (*get)(std::uintptr_t)=native_actor_heading;void (*set)(std::uintptr_t,float)=native_actor_heading;void (*rebuild)(std::uintptr_t)=native_actor_local_matrix;} actor_rotation_api;
bool ordinary_camera(std::uintptr_t actor,std::uintptr_t component,std::uintptr_t& camera);
bool actor_rotation_supported(std::uintptr_t actor);
bool anchored_action(unsigned action,const re4::Settings& settings){return action<=8||(action==0x1F&&settings.move_while_reloading);}
bool anchored_controls(const ControlFrame& frame){return frame.settings.analog_enabled&&frame.settings.unified_controls&&frame.settings.free_camera&&!frame.scope&&anchored_action(frame.action,frame.settings);}
bool actor_anchor_allowed(const ControlFrame& frame,std::uintptr_t& camera);
std::uint64_t ordinary_locomotion_input_begin(std::uintptr_t actor);
bool ordinary_locomotion_input_current(const ControlFrame& frame);
bool writable(void*,std::uintptr_t,std::size_t);
bool ordinary_locomotion_polar(ControlFrame& frame,float (&values)[4]);
std::atomic<std::uint64_t> anchored_releases[2]{};
std::atomic_flag anchored_locks[2]={ATOMIC_FLAG_INIT,ATOMIC_FLAG_INIT};
struct PlayerRuntime {re5::PlayerInput gyro;re4::UnifiedLook look;MotionReader sensor;ControlFrame frame;std::uint64_t next_frame=0,look_epoch=0,look_release=0,anchor_validated_tick=0;float look_integral=0,overview_pitch=0,heading_origin=0;bool anchored=false;};
// Mutable filter/sensor history belongs exclusively to the input_lock writer.
PlayerRuntime player_runtime[2];re5::FrameSnapshot<ControlFrame> published_frames[2];
std::atomic<std::uint64_t> runtime_thread{0};
std::atomic_flag input_lock=ATOMIC_FLAG_INIT;std::atomic<std::uint64_t> input_thread{0};ControlFrame input_scope;
std::uint64_t thread_id(){u64 id=0;return R_SUCCEEDED(svcGetThreadId(&id,CUR_THREAD_HANDLE))?id:0;}
ControlFrame* owned_scope(std::uintptr_t actor=0,std::uintptr_t command=0){
 const auto thread=input_thread.load(std::memory_order_acquire);if(!thread||thread!=thread_id())return nullptr;
 if((actor&&input_scope.player.actor!=actor)||(command&&input_scope.player.command!=command))return nullptr;
 re5::ResolvedPlayer player;if(!actor_player(input_scope.player.actor,player)||player.command!=input_scope.player.command||player.local_player!=input_scope.player.local_player||player.sticks.controller!=input_scope.player.sticks.controller||player.sticks.style!=input_scope.player.sticks.style||player.motion.controller!=input_scope.player.motion.controller||player.motion.style!=input_scope.player.motion.style)return nullptr;
 return &input_scope;
}
bool input_unfrozen(std::uintptr_t actor){std::uint8_t freeze=1;return memory.get(actor,0x3F0E,freeze)&&!(freeze&1);}
#include "camera_capture.inc"
HOOK_DEFINE_TRAMPOLINE(Re5LocalInput){static void Callback(void* command,void* actor){
 const auto ordinary_epoch=ordinary_locomotion_input_begin(reinterpret_cast<std::uintptr_t>(actor));
 const auto id=thread_id();
 const auto native_only=[&]{
  const auto owner=input_thread.load(std::memory_order_acquire);const bool suppress=id&&owner==id;
  if(suppress)input_thread.store(0,std::memory_order_release);
  Orig(command,actor);
  ordinary_locomotion_input_begin(reinterpret_cast<std::uintptr_t>(actor)); // Revoke late unproxied writes too.
  if(suppress)input_thread.store(owner,std::memory_order_release);
 };

 re5::ResolvedPlayer player;re5::Settings settings;re5::PadState pad;
 const auto pinned=runtime_thread.load(std::memory_order_acquire);
 runtime_probe_recorder.record(re5::RuntimeCounter::InputCalls,re5::RuntimeProbeThread::Input,id,pinned);
 if(!id){runtime_probe_recorder.record(re5::RuntimeCounter::InputRejectedThread);native_only();return;}
 if(!game_active.load(std::memory_order_acquire)||!settings_loaded.load(std::memory_order_acquire)
  ||!re5::resolve_player(memory,main_base,reinterpret_cast<std::uintptr_t>(actor),reinterpret_cast<std::uintptr_t>(command),player)
  ||!controls_snapshot.read(settings)){runtime_probe_recorder.record(re5::RuntimeCounter::InputRejectedRoute);native_only();return;}
 if(!actor_pad(player,pad)){runtime_probe_recorder.record(re5::RuntimeCounter::InputRejectedPad);native_only();return;}
 if(input_lock.test_and_set(std::memory_order_acquire)){runtime_probe_recorder.record(re5::RuntimeCounter::InputRejectedThread);native_only();return;}
 // Diagnostic only: native workers may migrate between accepted invocations.
 runtime_thread.store(id,std::memory_order_release);
 runtime_probe_recorder.record(re5::RuntimeCounter::InputAccepted);
 CameraInputCapture input_capture;
 struct Unlock{~Unlock(){input_thread.store(0,std::memory_order_release);input_lock.clear(std::memory_order_release);}} unlock;
 auto& runtime=player_runtime[player.local_player];const auto now=svcGetSystemTick();ControlFrame frame;
 frame.player=player;frame.settings=settings.players[player.local_player];frame.tick=now;frame.ordinary_epoch=ordinary_epoch;frame.frame=++runtime.next_frame;
 frame.generation=runtime.frame.generation;
 if(!runtime.frame.valid||runtime.frame.player.actor!=player.actor||runtime.frame.player.command!=player.command
  ||runtime.frame.player.motion.controller!=player.motion.controller||runtime.frame.player.motion.style!=player.motion.style
  ||runtime.frame.player.sticks.controller!=player.sticks.controller||runtime.frame.player.sticks.style!=player.sticks.style
  ||now<runtime.frame.tick||now-runtime.frame.tick>1920000){++frame.generation;runtime.gyro.reset();}
 std::uint8_t scope=0;memory.get(player.actor,0x1B40,frame.action);memory.get(player.actor,0x3AD8,scope);frame.scope=scope!=0;
 frame.axes=re5::shape(pad,frame.settings);frame.valid=true;input_capture.shaped(frame.axes);
 bool anchored=false;std::uintptr_t ordinary=0;
 frame.look_release=anchored_releases[player.local_player].load(std::memory_order_acquire);
 const bool released=runtime.look_release!=frame.look_release;
 if(!anchored_locks[player.local_player].test_and_set(std::memory_order_acquire)){
  if(actor_anchor_allowed(frame,ordinary)&&actor_rotation_api.get){
   frame.native_heading=actor_rotation_api.get(player.actor);frame.heading_valid=std::isfinite(frame.native_heading)&&std::fabs(frame.native_heading)<=6.283186f;anchored=frame.heading_valid;
  }
  runtime.anchor_validated_tick=anchored?now:0;
  anchored_locks[player.local_player].clear(std::memory_order_release);
 }else{
  // A busy pose worker is not a control-mode transition. Retain only a
  // recently validated owner; fallback samples never renew this lease.
  const auto& previous=runtime.frame;
  anchored=runtime.anchored&&previous.valid&&previous.heading_valid&&anchored_controls(frame)&&!released
   &&frame.generation==previous.generation&&runtime.look_epoch&&previous.look_epoch==runtime.look_epoch
   &&previous.look_release==frame.look_release&&runtime.look_release==frame.look_release
   &&previous.player.actor==player.actor&&previous.player.command==player.command&&previous.player.local_player==player.local_player
   &&previous.player.sticks.controller==player.sticks.controller&&previous.player.sticks.style==player.sticks.style
   &&previous.player.motion.controller==player.motion.controller&&previous.player.motion.style==player.motion.style
   &&runtime.anchor_validated_tick&&now>=runtime.anchor_validated_tick&&now-runtime.anchor_validated_tick<=1920000;
  if(anchored){frame.native_heading=runtime.heading_origin;frame.heading_valid=true;}
  else runtime.anchor_validated_tick=0;
 }
 if(frame.settings.re6_response||anchored){
  const bool continuous=runtime.frame.valid&&frame.generation==runtime.frame.generation&&now>=runtime.frame.tick&&now-runtime.frame.tick<=1920000;
  const float dt=continuous?float(now-runtime.frame.tick)/19200000.f:0;
  const auto& previous=runtime.frame.settings;
  if(!continuous||released||(anchored&&!runtime.anchored)||previous.camera_gain!=frame.settings.camera_gain||previous.right_deadzone!=frame.settings.right_deadzone
   ||previous.stick_curve!=frame.settings.stick_curve)runtime.look.reset();
  const bool aim=re5::active_aim(frame.action);
  if(!anchored&&frame.settings.free_camera&&!aim&&frame.axes.rx==0&&frame.axes.ry==0){
   runtime.look.reset();
  }else{
   const float gain=anchored?frame.settings.camera_gain:1.f;
   const auto filtered=runtime.look.update(player.actor,aim?2:1,now,true,frame.axes.rx*gain,frame.axes.ry*gain,frame.settings.re6_response);
   if(anchored)frame.look=filtered;
   const float yaw_rate=frame.settings.re6_response?(aim?57.3f:171.6f)*.0174532925199433f:2.09439510239f;
   const float pitch_rate=frame.settings.re6_response&&aim?53.f*.0174532925199433f:1.57079632679f;
   frame.axes.rx=dt>0?-filtered.yaw/(dt*yaw_rate):0;
   frame.axes.ry=dt>0?filtered.pitch/(dt*pitch_rate):0;
  }
 }else runtime.look.reset();
 // Prefix publication keeps accepted input samples that arrive between two
 // actor/world phases. Only the input writer owns these integrals.
 if(anchored&&frame.heading_valid){
  const bool new_owner=!runtime.frame.valid||runtime.frame.player.actor!=player.actor||runtime.frame.player.command!=player.command;
  if(!runtime.anchored||released||frame.generation!=runtime.frame.generation){
   ++runtime.look_epoch;runtime.look_integral=0;runtime.heading_origin=frame.native_heading;
   if(!runtime.anchored||released||new_owner)runtime.overview_pitch=0;
  }
  runtime.look_integral=std::remainder(runtime.look_integral+frame.look.yaw,6.28318530718f);
  runtime.overview_pitch+=re4::unified_stick_pitch_delta(runtime.overview_pitch,frame.look.pitch);
  frame.look_epoch=runtime.look_epoch;frame.look_integral=runtime.look_integral;frame.overview_pitch=runtime.overview_pitch;frame.native_heading=runtime.heading_origin;
  runtime.anchored=true;
 }else runtime.anchored=false;
 runtime.look_release=frame.look_release;
 const bool aiming=re5::active_aim(frame.action)&&input_unfrozen(player.actor);
 runtime.gyro.begin_frame(now,frame.settings,{player.actor,player.motion.controller,player.motion.style,frame.generation},aiming);
 if(frame.settings.gyro_enabled){re4::SensorState states[16]{};int count=0;
  if(runtime.sensor.read(player.motion,states,count))for(int i=count-1;i>=0;--i){const auto& sample=states[i];
   if(sample.attributes&1)runtime.gyro.push({sample.number,sample.interval_ns,-sample.angular_velocity[1]*6.28318530718f,-sample.angular_velocity[0]*6.28318530718f},player.motion.controller,player.motion.style);
  }
 }
 frame.gyro=runtime.gyro.finish_frame();input_scope=frame;input_thread.store(id,std::memory_order_release);
 Orig(command,actor);frame.ordinary_proxy=input_scope.ordinary_proxy;runtime.frame=frame;published_frames[player.local_player].publish(frame);input_capture.finish(frame,pad,runtime);
}};
HOOK_DEFINE_TRAMPOLINE(Re5Polar){static void Callback(void* object){Orig(object);auto* f=owned_scope();
 if(!f||reinterpret_cast<std::uintptr_t>(object)!=f->player.command+0x4F8||!f->settings.analog_enabled)return;
 if((f->ordinary_epoch&&!ordinary_locomotion_input_current(*f))||!writable(nullptr,f->player.command+0x880,16))return;
 float values[4];auto a=f->axes;re5::polar(a.lx*(f->settings.unified_controls?1.f:f->settings.turn_gain),a.ly,values[0],values[2]);re5::polar(a.rx,a.ry,values[1],values[3]);
 ordinary_locomotion_polar(*f,values);
 if(f->ordinary_epoch&&!ordinary_locomotion_input_current(*f))return;
 std::memcpy(reinterpret_cast<void*>(f->player.command+0x880),values,sizeof(values));
}};
HOOK_DEFINE_TRAMPOLINE(Re5NativeGyro){static void Callback(float* out,void* actor){Orig(out,actor);auto* f=owned_scope(reinterpret_cast<std::uintptr_t>(actor));
 if(out&&f&&f->settings.gyro_enabled)std::memset(out,0,16);
}};
unsigned native_scheme(std::uintptr_t actor){
 std::uintptr_t got=0,root=0,options=0;std::uint8_t slot=255;unsigned type=0;
 if(memory.get(main_base,re5::profile::offset<0x1CD9FC0>(),got)&&memory.get(got,0,root)&&memory.get(root,0x584E0,options)&&memory.get(actor,0x3F2C,slot)&&slot<4)memory.get(options,slot*0x1A0+0xB4,type);
 return type;
}
HOOK_DEFINE_INLINE(Re5AimAxes){static void Callback(exl::hook::InlineFloatCtx* ctx){auto* f=owned_scope(re4::saved_x(ctx,20),re4::saved_x(ctx,19));
 if(!f||!f->settings.analog_enabled||!input_unfrozen(f->player.actor))return;
 const auto& s=f->settings; // native options continue to select their stick
 const bool right=s.unified_controls||native_scheme(f->player.actor)==1||native_scheme(f->player.actor)==3;
 const float x=right?f->axes.rx:f->axes.lx,y=right?f->axes.ry:f->axes.ly;
 set_saved_s(ctx,0,x*(right?s.right_aim_gain_x:s.aim_gain_x)*(s.unified_controls&&!s.re6_response?s.camera_gain:1.f));
 set_saved_s(ctx,1,y*(right?s.right_aim_gain_y:s.aim_gain_y)*(s.unified_controls&&!s.re6_response?s.camera_gain:1.f));
}};
HOOK_DEFINE_INLINE(Re5AimDelta){static void Callback(exl::hook::InlineFloatCtx* ctx){auto* f=owned_scope(re4::saved_x(ctx,20),re4::saved_x(ctx,19));
 if(!f||!f->settings.gyro_enabled||!input_unfrozen(f->player.actor))return;
 float yaw=0;if(!memory.get(f->player.command,0x4C4,yaw)||!std::isfinite(yaw))return;
 yaw+=f->gyro.yaw*57.295779513f;const float pitch=saved_s(ctx,9)-f->gyro.pitch*57.295779513f;
 if(std::isfinite(yaw)&&std::isfinite(pitch)){std::memcpy(reinterpret_cast<void*>(f->player.command+0x4C4),&yaw,4);set_saved_s(ctx,9,pitch);f->gyro={};}
}};
HOOK_DEFINE_INLINE(Re5ScopeZoom){static void Callback(exl::hook::InlineFloatCtx* ctx){auto* f=owned_scope(re4::saved_x(ctx,20),re4::saved_x(ctx,19));
 if(!f||!f->scope||!f->settings.analog_enabled||!input_unfrozen(f->player.actor))return;
 const float delta=f->axes.ly*.2f*saved_s(ctx,0);if(std::isfinite(delta))set_saved_s(ctx,3,delta);
}};
void scheme_hook(exl::hook::InlineFloatCtx* ctx,unsigned reg){auto* f=owned_scope(re4::saved_x(ctx,19));if(f&&f->settings.unified_controls)re4::set_saved_x(ctx,reg,3);}
HOOK_DEFINE_INLINE(Re5Scheme8){static void Callback(exl::hook::InlineFloatCtx* c){scheme_hook(c,8);}};
HOOK_DEFINE_INLINE(Re5Scheme9){static void Callback(exl::hook::InlineFloatCtx* c){scheme_hook(c,9);}};
HOOK_DEFINE_INLINE(Re5Scheme10){static void Callback(exl::hook::InlineFloatCtx* c){scheme_hook(c,10);}};
HOOK_DEFINE_INLINE(Re5Scheme11){static void Callback(exl::hook::InlineFloatCtx* c){scheme_hook(c,11);}};

bool writable(void*,std::uintptr_t at,std::size_t bytes){
 if(!readable(nullptr,at,bytes))return false;
 const auto end=at+bytes;
 while(at<end){MemoryInfo region{};std::uint32_t page=0;if(R_FAILED(svcQueryMemory(&region,&page,at))||(region.perm&3)!=3)return false;const auto n=region.size-(at-region.addr);at+=n<end-at?n:end-at;}return true;
}
bool write_memory(void*,std::uintptr_t at,const void* data,std::size_t bytes){if(!data||!writable(nullptr,at,bytes))return false;std::memcpy(reinterpret_cast<void*>(at),data,bytes);return true;}
std::uintptr_t gait_lookup(void*,std::uintptr_t resource,unsigned index){return reinterpret_cast<std::uintptr_t(*)(std::uintptr_t,unsigned)>(main_base+re5::profile::offset<0x100BB80>())(resource,index);}
bool gait_init(void*,void* cursor,std::uintptr_t track){reinterpret_cast<void(*)(void*,std::uintptr_t)>(main_base+re5::profile::offset<0xF673C0>())(cursor,track);return true;}
bool gait_sample(void*,void* cursor,float frame,re5::GaitValue& value){value=reinterpret_cast<re5::GaitValue(*)(void*,float)>(main_base+re5::profile::offset<0xF673E0>())(cursor,frame);return true;}
std::uintptr_t gait_weapon(void*,std::uintptr_t actor){return reinterpret_cast<std::uintptr_t(*)(std::uintptr_t,unsigned)>(main_base+re5::profile::offset<0x9274E0>())(actor+0x30E8,0xE);}
bool gait_mirror(void*,std::uintptr_t actor,std::uintptr_t bone,re5::GaitValue* translation,re5::GaitValue* rotation){reinterpret_cast<void(*)(std::uintptr_t,std::uintptr_t,re5::GaitValue*,re5::GaitValue*)>(main_base+re5::profile::offset<0xDD8980>())(actor,bone,translation,rotation);return true;}
float gait_frame_step(std::uintptr_t actor){return 2*reinterpret_cast<float(*)(std::uintptr_t)>(main_base+re5::profile::offset<0xC20F0>())(actor);}
struct GaitRuntimeApi {re5::GaitBackend backend;float (*frame_step)(std::uintptr_t);};
// Native service boundaries are explicit, as for actor rotation. The default
// backend and exact per-build helper addresses remain the live game path.
GaitRuntimeApi gait_runtime{{{nullptr,read_memory,readable},nullptr,writable,write_memory,gait_lookup,gait_init,gait_sample,gait_weapon,gait_mirror},gait_frame_step};
re5::GaitAdapter gait;re5::GaitFrame gait_frames[2];std::uint64_t gait_serial[2]{};
std::atomic_flag gait_lock=ATOMIC_FLAG_INIT;
bool fresh_frame(std::uintptr_t actor,ControlFrame& frame){
 const auto pinned=runtime_thread.load(std::memory_order_acquire),caller=thread_id();
 runtime_probe_recorder.record(re5::RuntimeCounter::FreshCalls,re5::RuntimeProbeThread::Camera,caller,pinned);
 re5::ResolvedPlayer player;if(!game_active.load(std::memory_order_acquire)||!settings_loaded.load(std::memory_order_acquire)||!actor_player(actor,player)){runtime_probe_recorder.record(re5::RuntimeCounter::FreshRejectedRoute);return false;}
 re5::PadState pad;if(!actor_pad(player,pad)){runtime_probe_recorder.record(re5::RuntimeCounter::FreshRejectedPad);return false;}
 // Only the owning input thread may inspect its in-progress scope. Other
 // native workers copy the complete committed frame under the mailbox lock.
 ControlFrame latest;const auto* scoped=owned_scope(actor);
 if(scoped)latest=*scoped;
 else if(!published_frames[player.local_player].read(latest)){runtime_probe_recorder.record(re5::RuntimeCounter::FreshRejectedAge);return false;}
 const auto now=svcGetSystemTick();
 if(player.sticks.controller!=latest.player.sticks.controller||player.sticks.style!=latest.player.sticks.style||player.motion.controller!=latest.player.motion.controller||player.motion.style!=latest.player.motion.style){runtime_probe_recorder.record(re5::RuntimeCounter::FreshRejectedRoute);return false;}
 if(!latest.valid){runtime_probe_recorder.record(re5::RuntimeCounter::FreshRejectedAge);return false;}
 if(latest.player.actor!=actor||latest.player.command!=player.command||latest.player.local_player!=player.local_player){runtime_probe_recorder.record(re5::RuntimeCounter::FreshRejectedRoute);return false;}
 if(now<latest.tick||now-latest.tick>1920000){runtime_probe_recorder.record(re5::RuntimeCounter::FreshRejectedAge);return false;}
 frame=latest;frame.player=player;return true;
}
bool world_direction(const ControlFrame& f,float& x,float& z,re5::GaitDirection& direction,float& body_x,float& body_forward){
 float rx=0,rz=0,fx=0,fz=0;
 if(!memory.get(f.player.actor,0xD0,rx)||!memory.get(f.player.actor,0xD8,rz)||!memory.get(f.player.actor,0xF0,fx)||!memory.get(f.player.actor,0xF8,fz))return false;
 fx=-fx;fz=-fz;float move_rx=rx,move_rz=rz,move_fx=fx,move_fz=fz;
 if(f.settings.unified_controls){std::uintptr_t camera=0;float eye[3],target[3];
  if(!memory.get(f.player.actor,0x1A80,camera)||!memory.get(camera,0x60,eye)||!memory.get(camera,0x80,target))return false;
  move_fx=target[0]-eye[0];move_fz=target[2]-eye[2];const float length=std::hypot(move_fx,move_fz);if(!std::isfinite(length)||length<.001f)return false;
  move_fx/=length;move_fz/=length;move_rx=-move_fz;move_rz=move_fx;
 }
 x=move_rx*f.axes.lx+move_fx*f.axes.ly;z=move_rz*f.axes.lx+move_fz*f.axes.ly;
 const float magnitude=std::hypot(x,z);if(!std::isfinite(magnitude)||magnitude<=.0001f)return false;
 if(magnitude>1){x/=magnitude;z/=magnitude;}
 const float side=x*rx+z*rz,forward=x*fx+z*fz;body_x=side;body_forward=forward;
 direction=std::fabs(forward)>=std::fabs(side)?(forward>=0?re5::GaitDirection::Forward:re5::GaitDirection::Backward):(side>=0?re5::GaitDirection::Right:re5::GaitDirection::Left);return true;
}
#include "locomotion.inc"
HOOK_DEFINE_INLINE(Re5MovementProposal){static void Callback(exl::hook::InlineFloatCtx* ctx){
 const auto actor=re4::saved_x(ctx,19);ControlFrame controls;if(!fresh_frame(actor,controls)||!controls.settings.analog_enabled)return;
 const auto proposal=reinterpret_cast<std::uintptr_t>(ctx)+0x300+0x60;
 unsigned live_action=UINT32_MAX;
 if(controls.ordinary_proxy&&memory.get(actor,0x1B40,live_action)&&live_action<=8){
  const bool moved=ordinary_locomotion_proposal(controls,proposal);
  runtime_probe_recorder.record(moved?re5::RuntimeCounter::MovementPrepared:re5::RuntimeCounter::MovementRejected);return;
 }
 if(gait_lock.test_and_set(std::memory_order_acquire))return;
 struct Unlock{~Unlock(){gait_lock.clear(std::memory_order_release);}} unlock;
 auto& f=gait_frames[controls.player.local_player];f={};f.main=main_base;f.actor=actor;f.command=controls.player.command;f.settings=controls.settings;
 f.generation=controls.generation;f.frame=++gait_serial[controls.player.local_player];
 if(!gait_runtime.frame_step)return;
 f.frame_step=gait_runtime.frame_step(actor);
 if(!world_direction(controls,f.world_x,f.world_z,f.direction,f.body_x,f.body_forward))return;
 const bool prepared=gait.prepare_proposal(gait_runtime.backend,f,proposal);
 runtime_probe_recorder.record(prepared?re5::RuntimeCounter::MovementPrepared:re5::RuntimeCounter::MovementRejected);
}};
HOOK_DEFINE_INLINE(Re5MovementPose){static void Callback(exl::hook::InlineFloatCtx* ctx){
 const auto actor=re4::saved_x(ctx,19);re5::ResolvedPlayer player;if(!actor_player(actor,player))return;
 if(gait_lock.test_and_set(std::memory_order_acquire))return;
 struct Unlock{~Unlock(){gait_lock.clear(std::memory_order_release);}} unlock;
 const auto& f=gait_frames[player.local_player];if(f.actor==actor)gait.overlay_pose(gait_runtime.backend,f);
}};

struct CameraOrbit {std::uintptr_t actor=0;std::uint64_t generation=0,tick=0;float yaw=0,pitch=0;} orbits[2];
bool ordinary_camera(std::uintptr_t actor,std::uintptr_t component,std::uintptr_t& camera){
 std::uintptr_t owner=0,active=0,defaults=0,vtable=0,manager=0,view=0;
 std::uint8_t scoped=1;
 if(!memory.get(actor,0x1A80,camera)||!memory.get(camera,0,vtable)||vtable!=main_base+re5::profile::offset<0x1AD2510>()
  ||!memory.get(camera,0x130,owner)||owner!=actor||!memory.get(camera,0x198,active)
  ||!memory.get(camera,0xDA0,defaults)||!defaults||active!=defaults||(component&&active!=component)
  ||!memory.get(actor,0x3AD8,scoped)||scoped||!memory.get(main_base,re5::profile::offset<0x26A8550>(),manager))return false;
 for(unsigned i=0;i<2;++i)if(memory.get(manager,0x48+i*0x200,view)&&view==camera)return true;
 return false;
}
void invalidate_camera_follow(std::uintptr_t camera,std::uintptr_t component=0);
#include "camera_pose.inc"
HOOK_DEFINE_INLINE(Re5CameraAxes){static void Callback(exl::hook::InlineFloatCtx* ctx){
 const auto actor=re4::saved_x(ctx,20);ControlFrame f;std::uintptr_t camera=0;
 if(!fresh_frame(actor,f)||!f.settings.analog_enabled||!ordinary_camera(actor,re4::saved_x(ctx,19),camera)||!input_unfrozen(actor))return;
 if(f.settings.free_camera&&f.settings.unified_controls&&!re5::active_aim(f.action)){std::uintptr_t current=0;if(!f.heading_valid||!actor_anchor_allowed(f,current))return;}
 if(f.settings.free_camera&&!re5::active_aim(f.action)){set_saved_s(ctx,9,0);set_saved_s(ctx,0,0);re4::set_saved_x(ctx,8,0);}
 else{set_saved_s(ctx,9,f.axes.rx*32767*f.settings.camera_gain*f.settings.right_aim_gain_x);set_saved_s(ctx,0,f.axes.ry*32767*f.settings.camera_gain*f.settings.right_aim_gain_y);}
}};
HOOK_DEFINE_INLINE(Re5CameraReturn){static void Callback(exl::hook::InlineFloatCtx* ctx){
 const auto actor=re4::saved_x(ctx,20);ControlFrame f;std::uintptr_t camera=0;
 if(!fresh_frame(actor,f)||!ordinary_camera(actor,re4::saved_x(ctx,19),camera))return;
 if(f.settings.free_camera&&f.settings.unified_controls&&!re5::active_aim(f.action)){std::uintptr_t current=0;if(!f.heading_valid||!actor_anchor_allowed(f,current))return;}
 const float native=saved_s(ctx,0);if(std::isfinite(native)&&native>=0&&native<=1){
  const bool free=f.settings.free_camera&&!re5::active_aim(f.action)&&input_unfrozen(actor);
  set_saved_s(ctx,0,free?1.f:std::pow(native,f.settings.camera_return_speed));
 }
}};
bool head_anchor(std::uintptr_t actor,float (&point)[3]){
 std::uintptr_t neck=0,owner=0,map=0,bones=0;unsigned count=0;
 if(!memory.get(actor,0x2570,neck)||!memory.get(neck,0x30,owner)||owner!=actor
  ||!memory.get(actor,0x8D0,map)||!memory.get(actor,0x8C8,bones)||!memory.get(actor,0x8C0,count)||count>255)return false;
 auto bone=[&](unsigned offset,float (&out)[3]){std::uint16_t logical=0;std::uint8_t index=255;
  if(!memory.get(neck,offset,logical)||logical>255||!memory.get(map,logical,index)||index==255||index>=count||!memory.get(bones+index*0xC0,0x50,out))return false;
  for(float x:out)if(!std::isfinite(x))return false;
  return true;};
 float a[3],b[3];if(bone(0x100,a)&&bone(0x130,b)){for(unsigned i=0;i<3;++i)point[i]=(a[i]+b[i])*.5f;return true;}
 return bone(0xD0,point);
}
#include "shoulder.inc"
#include "camera_framing.inc"
#include "camera_follow.inc"
// Publish owned non-FP view changes with the fresh native component, before
// its desired/history/collision owner consumes them. Modern overview follows
// committed body heading; camera-only pitch comes from the fresh input frame.
HOOK_DEFINE_INLINE(Re5CameraDistance){static void Callback(exl::hook::InlineFloatCtx* ctx){
 const auto component=re4::saved_x(ctx,19);invalidate_camera_follow(0,component);std::uintptr_t actor=0,camera=0;ControlFrame frame;
 CameraDecisionCapture capture(re5::CameraProbeStage::Component,0,component);
 if(!memory.get(component,0x130,actor)){capture.reason(re5::CameraProbeReason::NoActor);return;}capture.actor(actor);
 if(!fresh_frame(actor,frame)){capture.reason(re5::CameraProbeReason::NoFreshFrame);return;}capture.frame(frame);
 if(frame.settings.first_person){capture.reason(re5::CameraProbeReason::FirstPerson);return;}
 const bool modern=frame.settings.free_camera&&frame.settings.unified_controls;
 if(frame.settings.free_camera&&!modern){capture.reason(re5::CameraProbeReason::LegacyFree);return;}
 const bool overview=modern&&frame.action<=8;
 const bool combat=modern&&framing_combat(frame.action);
 if(modern&&!overview&&!combat){capture.reason(re5::CameraProbeReason::NotMode);return;}
 const float distance=frame.settings.third_person_distance;
 if(!std::isfinite(distance)||distance<1.f||distance>2.f||(!overview&&!combat&&distance==1.f)){capture.reason(re5::CameraProbeReason::BadDistance);return;}
 if(!ordinary_camera(actor,component,camera)){capture.camera(camera);capture.reason(re5::CameraProbeReason::BadOwner);return;}capture.camera(camera);
 if(modern&&!framing_mode(frame,camera,component)){capture.reason(re5::CameraProbeReason::ActorBlocked);return;}
 re5::CameraPose native,result;CameraFramingAnchor prepared;const CameraFramingAnchor* framing=nullptr;
 if(!memory.get(component,0x160,native.eye)||!memory.get(component,0x180,native.target)){capture.reason(re5::CameraProbeReason::NativeRead);return;}capture.native(native,false);capture.fov(component);
 if(overview||combat){
  if(!memory.get(component,0x170,native.up)){capture.reason(re5::CameraProbeReason::NativeRead);return;}capture.native(native,true);
  if(!re5::camera_detail::finite(native.eye)||!re5::camera_detail::finite(native.target)||!re5::camera_detail::finite(native.up)){capture.reason(re5::CameraProbeReason::SourceInvalid);return;}
  if(overview){if(!anchored_pose(frame,camera,native,result,true,capture.observation()))return;}
  else{
   if(!combat_camera_pose(frame,camera,component,native,result,&prepared)){capture.reason(re5::CameraProbeReason::ActorBlocked);return;}
   framing=&prepared;
   if(capture.enabled){capture.value.world=prepared.world;capture.value.world_again=prepared.world_again;capture.value.anchor=prepared.anchor;capture.value.height=prepared.height;
    capture.value.available|=re5::CameraHasWorld|re5::CameraHasWorldAgain|re5::CameraHasAnchor;}
  }
 }else{
  result=native;
  for(unsigned i=0;i<3;++i){
   if(!std::isfinite(native.eye[i])||!std::isfinite(native.target[i])||std::fabs(native.eye[i])>1000000.f||std::fabs(native.target[i])>1000000.f){capture.reason(re5::CameraProbeReason::SourceInvalid);return;}
   result.eye[i]=native.target[i]+(native.eye[i]-native.target[i])*distance;
   if(!std::isfinite(result.eye[i])||std::fabs(result.eye[i])>1000000.f){capture.reason(re5::CameraProbeReason::MathRejected);return;}
  }
  if(std::hypot(std::hypot(native.eye[0]-native.target[0],native.eye[2]-native.target[2]),native.eye[1]-native.target[1])<.001f){capture.reason(re5::CameraProbeReason::MathRejected);return;}
 }
 capture.result(result,overview||combat);
 if(!std::memcmp(&native,&result,sizeof(native))){if(overview||combat)publish_camera_follow(frame,camera,component,result,framing);capture.reason(re5::CameraProbeReason::SamePose);return;}
 if(!writable(nullptr,component+0x160,12)||((overview||combat)&&(!writable(nullptr,component+0x170,12)||!writable(nullptr,component+0x180,12)))){capture.reason(re5::CameraProbeReason::NotWritable);return;}
 std::memcpy(reinterpret_cast<void*>(component+0x160),result.eye.data(),12);
 if(overview||combat){std::memcpy(reinterpret_cast<void*>(component+0x170),result.up.data(),12);std::memcpy(reinterpret_cast<void*>(component+0x180),result.target.data(),12);}
 if(overview||combat)publish_camera_follow(frame,camera,component,result,framing);
 runtime_probe_recorder.record(re5::RuntimeCounter::CameraApplied);capture.reason(re5::CameraProbeReason::Applied);
}};
HOOK_DEFINE_INLINE(Re5CameraRig){static void Callback(exl::hook::InlineFloatCtx* ctx){
 const auto camera=re4::saved_x(ctx,19);const auto framing_completion=claim_camera_framing(camera);std::uintptr_t actor=0,owned=0;
 CameraDecisionCapture capture(re5::CameraProbeStage::PostDesired,camera,0);
 if(!memory.get(camera,0x130,actor)){capture.reason(re5::CameraProbeReason::NoActor);return;}capture.actor(actor);
 ControlFrame f;
 if(!fresh_frame(actor,f)){capture.reason(re5::CameraProbeReason::NoFreshFrame);return;}capture.frame(f);
 if(!ordinary_camera(actor,0,owned)||owned!=camera){runtime_probe_recorder.record(re5::RuntimeCounter::CameraRejectedOwnership);capture.reason(re5::CameraProbeReason::BadOwner);return;}
 // Orbit, native-pose capture and writes share the restore hook's slot lock.
 auto& pose=camera_poses[f.player.local_player];if(pose.lock.test_and_set(std::memory_order_acquire)){capture.reason(re5::CameraProbeReason::CameraBusy);return;}
 struct PoseUnlock{std::atomic_flag& flag;~PoseUnlock(){flag.clear(std::memory_order_release);}} pose_unlock{pose.lock};
 float eye[3],target[3];if(!memory.get(camera,0x60,eye)||!memory.get(camera,0x80,target)){capture.reason(re5::CameraProbeReason::PostDesiredRead);return;}
 for(unsigned i=0;i<3;++i)if(!std::isfinite(eye[i])||!std::isfinite(target[i])){capture.reason(re5::CameraProbeReason::SourceInvalid);return;}
 re5::CameraPose native;std::memcpy(native.eye.data(),eye,12);std::memcpy(native.target.data(),target,12);
 if(!memory.get(camera,0x70,native.up)){capture.reason(re5::CameraProbeReason::PostDesiredRead);return;}capture.native(native,true);capture.reason(re5::CameraProbeReason::PostDesired);
 const auto now=svcGetSystemTick();if(capture.enabled)capture.value.tick=now;
 if(finish_camera_framing(camera,f,framing_completion)){
  runtime_probe_recorder.record(re5::RuntimeCounter::CameraApplied);
  if(capture.enabled){re5::CameraPose result;if(memory.get(camera,0x60,result.eye)&&memory.get(camera,0x70,result.up)&&memory.get(camera,0x80,result.target))capture.result(result,true);}
 }
 if(f.settings.first_person&&f.settings.free_camera&&f.settings.unified_controls&&!re5::active_aim(f.action)){
  re5::CameraPose result;
  if(!anchored_pose(f,camera,native,result))return;
  if(!std::memcmp(&native,&result,sizeof(native)))return;
  if(!writable(nullptr,camera+0x60,12)||!writable(nullptr,camera+0x70,12)||!writable(nullptr,camera+0x80,12))return;
  if(!pose.state.remember({camera,actor,f.player.command,f.player.local_player},native,now))return;
  std::memcpy(reinterpret_cast<void*>(camera+0x60),result.eye.data(),12);
  std::memcpy(reinterpret_cast<void*>(camera+0x70),result.up.data(),12);
  std::memcpy(reinterpret_cast<void*>(camera+0x80),result.target.data(),12);
  runtime_probe_recorder.record(re5::RuntimeCounter::CameraApplied);return;
 }
 auto& orbit=orbits[f.player.local_player];
 const bool continuous=orbit.actor==actor&&orbit.generation==f.generation&&now>=orbit.tick&&now-orbit.tick<=1920000;
 const float seconds=continuous?float(now-orbit.tick)/19200000.f:0;
 if(!continuous)orbit={actor,f.generation,now,0,0};
 orbit.tick=now;
 const bool component_owned=!f.settings.first_person&&(!f.settings.free_camera||f.settings.unified_controls);
 const bool free=!component_owned&&f.settings.free_camera&&!re5::active_aim(f.action)&&input_unfrozen(actor);
 if(free){
  orbit.yaw=std::remainder(orbit.yaw-f.axes.rx*f.settings.right_aim_gain_x*f.settings.camera_gain*2.4f*seconds,6.28318530718f);
  orbit.pitch=std::fmax(-1.2f,std::fmin(1.2f,orbit.pitch-f.axes.ry*f.settings.right_aim_gain_y*f.settings.camera_gain*1.8f*seconds));
 }else orbit.yaw=orbit.pitch=0;
 // This mode has already published distance in the fresh native component.
 // Never rewind its post-collision persistent pose or scale a skipped frame.
 if(component_owned)return;
 if(!f.settings.first_person&&f.settings.third_person_distance==1.f&&orbit.yaw==0&&orbit.pitch==0)return;
 float dx=eye[0]-target[0],dy=eye[1]-target[1],dz=eye[2]-target[2];const float length=std::hypot(std::hypot(dx,dz),dy);
 if(!std::isfinite(length)||length<.001f)return;
 // Positive raw Y means looking up: lower the eye-to-target offset. Apply
 // the same owned orientation before choosing either camera placement.
 const float yaw=std::atan2(dx,dz)+orbit.yaw,elevation=std::fmax(-1.45f,std::fmin(1.45f,std::atan2(dy,std::hypot(dx,dz))+orbit.pitch));
 float offset[3]={dx,dy,dz};
 if(orbit.yaw!=0||orbit.pitch!=0){offset[0]=std::sin(yaw)*std::cos(elevation)*length;offset[1]=std::sin(elevation)*length;offset[2]=std::cos(yaw)*std::cos(elevation)*length;}
 if(f.settings.first_person){float anchor[3];if(!head_anchor(actor,anchor))return;
  const float native_offset[3]={dx,dy,dz};
  for(unsigned i=0;i<3;++i){eye[i]=anchor[i];target[i]=anchor[i]-(free?offset[i]:native_offset[i]);}
 }else for(unsigned i=0;i<3;++i)eye[i]=target[i]+offset[i]*f.settings.third_person_distance;
 if(!std::memcmp(eye,native.eye.data(),12)&&!std::memcmp(target,native.target.data(),12))return;
 if(!pose.state.remember({camera,actor,f.player.command,f.player.local_player},native,now))return;
 if(writable(nullptr,camera+0x60,12)&&writable(nullptr,camera+0x80,12)){std::memcpy(reinterpret_cast<void*>(camera+0x60),eye,12);std::memcpy(reinterpret_cast<void*>(camera+0x80),target,12);runtime_probe_recorder.record(re5::RuntimeCounter::CameraApplied);}
}};
void install_controls(){
 Re5FullPad::InstallAtFuncPtr(reinterpret_cast<void(*)(re5::PadState*,const std::uint32_t&)>(sdk_base+re5::profile::sdk_full));
 Re5HandheldPad::InstallAtFuncPtr(reinterpret_cast<void(*)(re5::PadState*,const std::uint32_t&)>(sdk_base+re5::profile::sdk_handheld));
 Re5DualPad::InstallAtFuncPtr(reinterpret_cast<void(*)(re5::PadState*,const std::uint32_t&)>(sdk_base+re5::profile::sdk_dual));
 Re5LocalInput::InstallAtOffset(re5::profile::offset<0x7CA660>());Re5Polar::InstallAtOffset(re5::profile::offset<0x942150>());Re5NativeGyro::InstallAtOffset(re5::profile::offset<0x7DF940>());
 Re5AimAxes::InstallAtOffset(re5::profile::offset<0x7DEBE8>());Re5AimDelta::InstallAtOffset(re5::profile::offset<0x7DEF90>());Re5ScopeZoom::InstallAtOffset(re5::profile::offset<0x7DE638>());
 Re5MovementProposal::InstallAtOffset(re5::profile::offset<0x7A6F80>());Re5MovementPose::InstallAtOffset(re5::profile::offset<0xDD3054>());
 Re5CameraPoseRestore::InstallAtOffset(re5::profile::offset<0x2406BC>());
 Re5CameraDistance::InstallAtOffset(re5::profile::offset<0x24AA54>());
 Re5CameraFollow::InstallAtOffset(re5::profile::offset<0x23DBB0>());
 Re5CameraFollowArm::InstallAtOffset(re5::profile::offset<0x23DF7C>());Re5CameraFollowWorld::InstallAtOffset(re5::profile::offset<0x23E398>());
 Re5ActorLook::InstallAtOffset(re5::profile::offset<0x24DDE0>());
 Re5CameraAxes::InstallAtOffset(re5::profile::offset<0x24AAE8>());Re5CameraReturn::InstallAtOffset(re5::profile::offset<0x24A70C>());Re5CameraRig::InstallAtOffset(re5::profile::offset<0x2406CC>());
 Re5Scheme11::InstallAtOffset(re5::profile::offset<0x7caa90>());
 Re5Scheme9::InstallAtOffset(re5::profile::offset<0x7caae4>());
 Re5Scheme11::InstallAtOffset(re5::profile::offset<0x7cabb0>());
 Re5Scheme8::InstallAtOffset(re5::profile::offset<0x7cabec>());
 Re5Scheme8::InstallAtOffset(re5::profile::offset<0x7caf58>());
 Re5Scheme8::InstallAtOffset(re5::profile::offset<0x7cb528>());
 Re5Scheme8::InstallAtOffset(re5::profile::offset<0x7cba64>());
 Re5Scheme10::InstallAtOffset(re5::profile::offset<0x7cbb24>());
 Re5Scheme8::InstallAtOffset(re5::profile::offset<0x7cbbbc>());
 Re5Scheme8::InstallAtOffset(re5::profile::offset<0x7cbd54>());
 Re5Scheme10::InstallAtOffset(re5::profile::offset<0x7cbd78>());
 Re5Scheme8::InstallAtOffset(re5::profile::offset<0x7cbdf4>());
 Re5Scheme11::InstallAtOffset(re5::profile::offset<0x7cc0b8>());
 Re5Scheme11::InstallAtOffset(re5::profile::offset<0x7cc150>());
 Re5Scheme8::InstallAtOffset(re5::profile::offset<0x7cc1ac>());
 Re5Scheme8::InstallAtOffset(re5::profile::offset<0x7cc360>());
 Re5Scheme8::InstallAtOffset(re5::profile::offset<0x7cc854>());
 Re5Scheme8::InstallAtOffset(re5::profile::offset<0x7ccba0>());
 Re5Scheme11::InstallAtOffset(re5::profile::offset<0x7ccc34>());
 Re5Scheme10::InstallAtOffset(re5::profile::offset<0x7cd4a8>());
 Re5Scheme8::InstallAtOffset(re5::profile::offset<0x7cd4ec>());
 Re5Scheme8::InstallAtOffset(re5::profile::offset<0x7cd510>());
 Re5Scheme8::InstallAtOffset(re5::profile::offset<0x7cd618>());
}
