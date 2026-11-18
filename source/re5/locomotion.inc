// Ordinary Modern movement keeps native action admission, animation and speed.
// The owned polar proxy makes native turn sectors produce a forward/back step;
// only that step's direction changes at the existing pre-collision proposal.
namespace ordinary_locomotion_detail {
constexpr float maximum_coordinate=1000000.f,maximum_step=256.f;
struct Authorization {std::atomic<std::uintptr_t> actor{0};std::atomic<std::uint64_t> frame{0},epoch{0};} authorized[2];

bool finite_position(const std::array<float,3>& p){for(float v:p)if(!std::isfinite(v)||std::fabs(v)>maximum_coordinate)return false;return true;}
bool allowed(const ControlFrame& frame){
 std::uintptr_t camera=0;unsigned action=UINT32_MAX;re5::ResolvedPlayer player;
 const auto now=svcGetSystemTick();
 if(frame.player.local_player>1||!frame.valid||!frame.frame||!frame.generation||!frame.heading_valid||frame.settings.first_person||frame.action>8
  ||now<frame.tick||now-frame.tick>1920000||!actor_anchor_allowed(frame,camera)
  ||!memory.get(frame.player.actor,0x1B40,action)||action>8
  ||!actor_player(frame.player.actor,player)||player.command!=frame.player.command||player.local_player!=frame.player.local_player
  ||player.sticks.controller!=frame.player.sticks.controller||player.sticks.style!=frame.player.sticks.style
  ||player.motion.controller!=frame.player.motion.controller||player.motion.style!=frame.player.motion.style)return false;
 re5::Settings settings;if(!controls_snapshot.read(settings))return false;
 const auto& current=settings.players[frame.player.local_player];
 if(!current.analog_enabled||!current.unified_controls||!current.free_camera||current.first_person
  ||current.third_person_distance!=frame.settings.third_person_distance)return false;
 std::uintptr_t component=0;std::uint8_t partner=1,skipped=1;float transition=1;
 if(!memory.get(camera,0xDA0,component)||!memory.get(component,0x197,partner)||partner
  ||!memory.get(camera,0x2EC1,skipped)||skipped||!memory.get(camera,0x344,transition)||!std::isfinite(transition)||transition>0)return false;
 for(float axis:{frame.axes.lx,frame.axes.ly,frame.axes.rx,frame.axes.ry})if(!std::isfinite(axis)||std::fabs(axis)>1.00001f)return false;
 return true;
}
bool direction(const ControlFrame& frame,float& x,float& z){
 re5::GaitDirection cardinal;float body_x=0,body_forward=0;
 if(!world_direction(frame,x,z,cardinal,body_x,body_forward)||!std::isfinite(body_x)||!std::isfinite(body_forward))return false;
 const float length=std::hypot(x,z);if(!std::isfinite(length)||length<.0001f||length>1.00001f)return false;
 x/=length;z/=length;return true;
}
bool previous(const ControlFrame& frame,std::array<float,3>& value){return memory.get(frame.player.actor,0x1EE0,value)&&finite_position(value);}
}
// Sequence every native input entry, including rejected Orig calls, without
// touching another worker's mutable frame. Entry and rejected-exit epochs
// prevent a late native write from retaining a prior proxy authorization.
std::uint64_t ordinary_locomotion_input_begin(std::uintptr_t actor){
 re5::ResolvedPlayer player;unsigned slot=2;
 if(actor_player(actor,player)&&player.local_player<2)slot=player.local_player;
 else for(unsigned i=0;i<2;++i)if(ordinary_locomotion_detail::authorized[i].actor.load(std::memory_order_acquire)==actor){slot=i;break;}
 if(slot>=2)return 0;
 auto& state=ordinary_locomotion_detail::authorized[slot];
 state.actor.store(actor,std::memory_order_release);state.frame.store(0,std::memory_order_release);
 auto epoch=state.epoch.fetch_add(1,std::memory_order_acq_rel)+1;
 if(!epoch)epoch=state.epoch.fetch_add(1,std::memory_order_acq_rel)+1;
 return epoch;
}
bool ordinary_locomotion_input_current(const ControlFrame& frame){
 if(frame.player.local_player>1||!frame.ordinary_epoch)return false;
 const auto& state=ordinary_locomotion_detail::authorized[frame.player.local_player];
 return state.actor.load(std::memory_order_acquire)==frame.player.actor&&state.epoch.load(std::memory_order_acquire)==frame.ordinary_epoch;
}
bool ordinary_locomotion_polar(ControlFrame& frame,float (&values)[4]){
 frame.ordinary_proxy=false;
 if(!ordinary_locomotion_input_current(frame)||!ordinary_locomotion_detail::allowed(frame)||!writable(nullptr,frame.player.command+0x880,sizeof(values)))return false;
 std::array<float,3> before{},current{};
 if(!ordinary_locomotion_detail::previous(frame,before)||!memory.get(frame.player.actor,0x60,current)||!ordinary_locomotion_detail::finite_position(current)
  ||std::hypot(current[0]-before[0],current[2]-before[2])>ordinary_locomotion_detail::maximum_step)return false;
 // Validate the direction while the original angular cache is still intact.
 // A neutral left stick still suppresses the duplicate native right turn.
 float x=0,z=0;if((frame.axes.lx!=0||frame.axes.ly!=0)&&!ordinary_locomotion_detail::direction(frame,x,z))return false;
 if(!std::isfinite(values[2])||values[2]<0||values[2]>1)return false;
 values[0]=frame.axes.ly<0&&std::fabs(frame.axes.ly)>std::fabs(frame.axes.lx)?180.f:0.f;
 values[1]=values[3]=0;
 auto& active=ordinary_locomotion_detail::authorized[frame.player.local_player];active.frame.store(0,std::memory_order_release);
 if(values[2]<=.3f)return true; // Native classifier admits no left direction at/below this threshold.
 if(!ordinary_locomotion_input_current(frame))return false;
 frame.ordinary_proxy=true;
 auto& authorization=ordinary_locomotion_detail::authorized[frame.player.local_player];
 authorization.actor.store(frame.player.actor,std::memory_order_release);authorization.frame.store(frame.frame,std::memory_order_release);return true;
}
bool ordinary_locomotion_proposal(const ControlFrame& frame,std::uintptr_t proposal){
 if(!frame.ordinary_proxy||!ordinary_locomotion_input_current(frame)||!ordinary_locomotion_detail::allowed(frame))return false;
 const auto& authorization=ordinary_locomotion_detail::authorized[frame.player.local_player];
 if(authorization.frame.load(std::memory_order_acquire)!=frame.frame||authorization.actor.load(std::memory_order_acquire)!=frame.player.actor)return false;
 std::array<float,3> before{},position{};float x=0,z=0;
 if(!ordinary_locomotion_detail::previous(frame,before)||!memory.get(proposal,0,position)||!ordinary_locomotion_detail::finite_position(position)
  ||!ordinary_locomotion_detail::direction(frame,x,z)||!writable(nullptr,proposal,sizeof(position)))return false;
 const float distance=std::hypot(position[0]-before[0],position[2]-before[2]);
 if(!std::isfinite(distance)||distance>ordinary_locomotion_detail::maximum_step)return false;
 const float moved_x=before[0]+distance*x,moved_z=before[2]+distance*z;
 if(!std::isfinite(moved_x)||!std::isfinite(moved_z)||std::fabs(moved_x)>ordinary_locomotion_detail::maximum_coordinate||std::fabs(moved_z)>ordinary_locomotion_detail::maximum_coordinate)return false;
 // Replacing the previous-relative vector is idempotent when native proposal
 // stages share an input frame. No consumed lease or additive stride is used.
 std::memcpy(reinterpret_cast<void*>(proposal),&moved_x,4);std::memcpy(reinterpret_cast<void*>(proposal+8),&moved_z,4);return true;
}
