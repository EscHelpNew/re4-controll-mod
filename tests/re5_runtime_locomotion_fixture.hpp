#pragma once
// The break caught here is a native turn admission (zero displacement) for a
// lateral Modern stick, followed by no camera-relative collision proposal.
namespace { namespace re5_locomotion_test {
inline std::uintptr_t observing_actor=0,observing_command=0;
inline std::uint64_t native_buttons=0;
inline unsigned native_action=0,native_direction=0;
inline bool dispatch_polar=true;
inline float native_angle=0,native_magnitude=0;
inline re5::PadState native_pad;
inline void (*before_polar)(void*,void*)=nullptr,(*after_polar)(void*,void*)=nullptr;
inline bool retain_native_cache=false;
inline unsigned callback_depth=0;
inline re5::Settings live_settings;
inline bool race_fired=false;
inline std::uintptr_t race_actor=0,race_command=0;
inline unsigned race_player=0;
template<class T> inline T read(std::uintptr_t at){T out{};std::memcpy(&out,reinterpret_cast<void*>(at),sizeof(out));return out;}
template<class T> inline void write(std::uintptr_t at,const T& value){std::memcpy(reinterpret_cast<void*>(at),&value,sizeof(value));}
template<class Frame> inline bool proxied(const Frame& frame){if constexpr(requires{frame.ordinary_proxy;})return frame.ordinary_proxy;else return false;}
inline void native_input(void* command,void* actor){
 assert(reinterpret_cast<std::uintptr_t>(command)==observing_command&&reinterpret_cast<std::uintptr_t>(actor)==observing_actor);
 if(dispatch_polar){
  // The native original still polls/converts raw sticks when the outer mod
  // input callback rejects ownership. Model that side effect before the real
  // Polar hook; an empty native double would hide stale proxy authority.
  const float lx=float(native_pad.lx)/32767.f,ly=float(native_pad.ly)/32767.f,rx=float(native_pad.rx)/32767.f,ry=float(native_pad.ry)/32767.f;
  const float lm=std::min(std::hypot(lx,ly),1.f),rm=std::min(std::hypot(rx,ry),1.f);
  if(!retain_native_cache)write(observing_command+0x880,std::array<float,4>{lm>0?-std::atan2(lx,ly)*57.295779513f:0,rm>0?-std::atan2(rx,ry)*57.295779513f:0,lm<.3f?0:lm,rm<.3f?0:rm});
  if(before_polar)before_polar(command,actor);
  Re5Polar::Callback(reinterpret_cast<void*>(observing_command+0x4F8));
  if(after_polar)after_polar(command,actor);
 }
 const auto cache=read<std::array<float,4>>(observing_command+0x880);native_angle=cache[0];native_magnitude=cache[2];
 // Qualified native classifier7CB12C: magnitude>0.3 then forward/back/side
 // sectors. Native admission7CB760 uses forward direction and native button
 // bit4000;7DC320/40C schedules run05. Side directions can admit turn07/08.
 // These independent fixture thresholds separate the actual recorded vectors;
 // expected forward/back admission is not computed with production helpers.
 native_direction=native_magnitude<=.3f?0:std::fabs(native_angle)<45?1:std::fabs(native_angle)>135?2:native_angle>0?4:8;
 native_action=native_direction==1?((native_buttons&0x4000)?5:2):native_direction==2?6:native_direction==4?7:native_direction==8?8:0;
 write(observing_actor+0x1B40,native_action);
 // Native run/walk input copies the resulting angular command to3F4C.
 write(observing_actor+0x3F4C,native_angle);write(observing_actor+0x3F50,native_magnitude);
}
inline void publish_live_settings(void*,void*){assert(controls_snapshot.publish(live_settings));}
inline void nested_rejected_input(void* command,void* actor){
 if(callback_depth)return;
 ++callback_depth;const auto saved=native_pad;native_pad.lx=0;native_pad.ly=32767;native_pad.rx=native_pad.ry=0;
 observed_pad(&native_pad,race_player?1:3,1);Re5LocalInput::Callback(command,actor);native_pad=saved;--callback_depth;
}
inline void interleave_rejected_input(std::uintptr_t at,Result&){
 if(race_fired||at!=race_actor+0x1EE0)return;
 race_fired=true;const auto saved_thread=game_test::thread_id;const auto saved=native_pad;
 game_test::thread_id=saved_thread+1;native_pad.lx=0;native_pad.ly=32767;native_pad.rx=native_pad.ry=0;
 observed_pad(&native_pad,race_player?1:3,1);Re5LocalInput::Callback(reinterpret_cast<void*>(race_command),reinterpret_cast<void*>(race_actor));
 native_pad=saved;game_test::thread_id=saved_thread;
}
inline std::uintptr_t denied_begin=0,denied_end=0;
inline void deny_memory(std::uintptr_t at,Result& result){if(at>=denied_begin&&at<denied_end)result=1;}
inline void run(unsigned player,std::uintptr_t actor,std::uintptr_t camera,std::uintptr_t command,re5::PadState& pad,re5::Settings& settings){
 const auto prior_observer=local_input_observer;local_input_observer=native_input;observing_actor=actor;observing_command=command;
 struct alignas(16) NativeFrame {exl::hook::InlineFloatCtx context{};std::array<std::uint8_t,0x100> stack{};} native_frame;
 const auto frame_address=reinterpret_cast<std::uintptr_t>(&native_frame),proposal=frame_address+0x360;
 game_test::memory_maps.push_back({frame_address,sizeof(native_frame),3});
 const auto prior_query=camera_test::query_observer;const auto prior_gait=gait_runtime;const auto prior_action=read<std::uintptr_t>(actor+0x1B60);gait_runtime.frame_step=+[](std::uintptr_t){return 1.f;};
 struct Restore {decltype(local_input_observer) observer;decltype(camera_test::query_observer) query;GaitRuntimeApi gait_api;std::uintptr_t actor,action;~Restore(){local_input_observer=observer;camera_test::query_observer=query;gait_runtime=gait_api;write(actor+0x1B60,action);game_test::memory_maps.pop_back();}} restore{prior_observer,prior_query,prior_gait,actor,prior_action};
 auto setup=[&]{
  settings.players[player]=re4::re6_profile();auto& s=settings.players[player];s.gyro_enabled=false;s.remap_enabled=false;s.left_deadzone=s.right_deadzone=0;s.re6_response=false;
  assert(controls_snapshot.publish(settings));
  // Earlier combat-asset fixtures have destroyed their action object. No
  // ordinary case needs a combat overlay; keep the fallback's native action
  // owner absent rather than allowing a stale MemoryInfo entry to authorize
  // a dereference of that freed fixture allocation.
  write(actor+0x1B60,std::uintptr_t(0));
  write(actor+8,std::uint16_t(0x201));write(actor+0x48,std::uintptr_t(0));write(actor+0x54,std::uint16_t(0));write(actor+0x56,std::uint16_t(3));
  for(auto offset:{0x1A71u,0x1B4Cu,0x1AB0u,0x2708u,0x3F0Eu,0x3AD8u})write(actor+offset,std::uint8_t(0));
  for(unsigned axis=0;axis<3;++axis)write(actor+0x80+axis*4,1.f);
  write(camera,main_base+re5::profile::offset<0x1AD2510>());write(camera+0x130,actor);write(camera+0x198,camera+0xDB0);write(camera+0xDA0,camera+0xDB0);write(camera+0xDB0+0x130,actor);
  write(camera+0x60,std::array<float,3>{0,170,100});write(camera+0x80,std::array<float,3>{0,170,0});
  write(actor+0x60,std::array<float,3>{10,20,30});write(actor+0x1EE0,std::array<float,3>{10,20,30});
  fake_heading(actor,0);fake_local(actor);fake_world(actor);player_runtime[player].anchored=false;player_runtime[player].look.reset();anchored_looks[player]={};
  pad.attributes=1;pad.buttons=0xA501;pad.rx=pad.ry=0;dispatch_polar=true;native_buttons=0;before_polar=after_polar=nullptr;retain_native_cache=false;callback_depth=0;race_player=player;
  write(camera+0xDB0+0x197,std::uint8_t(0));write(camera+0x2EC1,std::uint8_t(0));write(camera+0x344,0.f);
 };
 auto input=[&](unsigned action,int lx,int ly,bool running){
  write(actor+0x1B40,action);pad.lx=lx;pad.ly=ly;native_pad=pad;native_buttons=running?0x4000:0;game_test::tick+=320000;const auto buttons=pad.buttons;
  observed_pad(&pad,player?1:3,1);Re5LocalInput::Callback(reinterpret_cast<void*>(command),reinterpret_cast<void*>(actor));assert(pad.buttons==buttons);
  return player_runtime[player].frame;
 };
 auto proposal_phase=[&](std::array<float,3> position){
  write(proposal,position);write(proposal+12,std::uint32_t(0xA5C3F701));write(proposal+16,std::array<float,4>{.1f,.2f,.3f,.4f});
  re4::set_saved_x(&native_frame.context,19,actor);Re5MovementProposal::Callback(&native_frame.context);return read<std::array<float,3>>(proposal);
 };
 setup();
 struct Case {int lx,ly;float x,z;bool back;};
 const Case cases[]{
  {-32739,-1338,-1.9983314f,.0816703f,false},{32731,1524,1.9978356f,-.0930220f,false},
  {-32750,1050,-1.9989729f,-.0640892f,false},{28251,16599,1.7243809f,-1.0131694f,false},
  {23166,23172,1.4140304f,-1.4143967f,false},{0,-32767,0,2,true},
  {-16384,-28378,-1.0000250f,1.7320364f,true}};
 for(unsigned action:{0u,1u,2u,3u,4u,5u,6u,7u,8u})for(bool running:{false,true})for(const auto& c:cases){
  const auto f=input(action,c.lx,c.ly,running);
  assert(native_direction==(c.back?2u:1u)); // RED28: pure side stays native turn.
  assert(native_action==(c.back?6u:running?5u:2u));assert(native_angle==(c.back?180.f:0.f));
  assert(proxied(f)&&f.axes.lx==float(c.lx)/32767.f&&f.axes.ly==float(c.ly)/32767.f);
  const auto cached=read<std::array<float,4>>(command+0x880);assert(cached[1]==0&&cached[3]==0);
  const auto before_actor=read<std::array<std::uint8_t,0xB0>>(actor+0x60);
  // Native animation supplies two horizontal units (backward has +Z). The
  // adapter must redirect that distance; it cannot add a synthetic walk step.
  auto moved=proposal_phase({10,21,c.back?32.f:28.f});assert(std::fabs(moved[0]-(10+c.x))<.0001f&&std::fabs(moved[2]-(30+c.z))<.0001f&&moved[1]==21);
  assert((read<std::uint32_t>(proposal+12)==0xA5C3F701&&read<std::array<float,4>>(proposal+16)==std::array<float,4>{.1f,.2f,.3f,.4f}));
  assert((read<std::array<std::uint8_t,0xB0>>(actor+0x60)==before_actor));
  Re5MovementProposal::Callback(&native_frame.context);assert((read<std::array<float,3>>(proposal)==moved)); // idempotent,60Hz proposals may share30Hz input
 }
 setup();input(7,-32767,0,true);auto zero=proposal_phase({10,21,30});assert((zero==std::array<float,3>{10,21,30}));
 // An absent actual Polar invocation must not publish a proxy marker or rotate
 // unrelated native motion, even if the previous input frame was proxied.
 dispatch_polar=false;const auto without_proxy=input(5,-32767,0,true);assert(!proxied(without_proxy));assert((proposal_phase({10,21,28})==std::array<float,3>{10,21,28}));
 // A rejected native input call still runs Orig and publishes no new frame.
 // Its unproxied native forward step must not inherit the prior left proxy.
 setup();const auto accepted=input(7,-32767,0,true);assert(proxied(accepted));
 input_lock.test_and_set();const auto rejected_input=input(5,0,32767,true);input_lock.clear();
 assert(rejected_input.frame==accepted.frame&&native_direction==1&&native_angle==0);
 assert((proposal_phase({10,21,28})==std::array<float,3>{10,21,28}));
 setup();input(7,-32767,0,true);camera_test::query_observer=deny_memory;denied_begin=proposal;denied_end=proposal+32;
 assert((proposal_phase({10,21,28})==std::array<float,3>{10,21,28}));camera_test::query_observer=prior_query;
 // Camera and body headings are independent: left from a camera looking-X
 // travels+Z, even while the body's forward animation currently points-X.
 setup();write(camera+0x60,std::array<float,3>{100,170,0});fake_heading(actor,1.57079632679f);fake_local(actor);fake_world(actor);
 input(7,-32767,0,true);assert((proposal_phase({8,21,30})==std::array<float,3>{10,21,32}));
 re4::set_saved_x(&native_frame.context,19,actor);Re5ActorLook::Callback(&native_frame.context);assert(std::fabs(fake_heading(actor)-1.57079632679f)<.00001f);
 input(5,32767,0,true);assert((proposal_phase({8,21,30})==std::array<float,3>{10,21,28}));
 // The same left-only turn request leaves right-owned body yaw unchanged;
 // adding rightX still reaches that existing body commit once.
 setup();input(7,-32767,0,true);Re5ActorLook::Callback(&native_frame.context);assert(std::fabs(fake_heading(actor))<.000001f);
 pad.rx=16384;const auto turning=input(5,-32767,0,true);assert(turning.look.yaw<0&&turning.look_integral<0);
 const auto native_cache=read<std::array<float,4>>(command+0x880);assert(native_cache[1]==0&&native_cache[3]==0);
 Re5ActorLook::Callback(&native_frame.context);const auto turned=fake_heading(actor);assert(turned<0&&std::fabs(turned-turning.look_integral)<.00001f);
 Re5ActorLook::Callback(&native_frame.context);assert(fake_heading(actor)==turned);
 // Reject before changing native action admission when input ownership,
 // ordinary camera, transforms, scope or native previous position is unsafe.
 for(unsigned bad=0;bad<18;++bad){
  setup();auto changed=settings;
  if(bad==0)changed.players[player].analog_enabled=false;
  if(bad==1)changed.players[player].unified_controls=false;
  if(bad==2)changed.players[player].free_camera=false;
  if(bad==3)changed.players[player].first_person=true;
  if(bad<4)assert(controls_snapshot.publish(changed));
  if(bad==4)write(actor+0x3AD8,std::uint8_t(1));
  if(bad==5)write(actor+0x3F0E,std::uint8_t(1));
  if(bad==6)write(actor+0x1B4C,std::uint8_t(1));
  if(bad==7)write(actor+0x48,actor);
  if(bad==8)write(actor+0x2708,std::uint8_t(1));
  if(bad==9)write(actor+8,std::uint16_t(0x202));
  if(bad==10)write(camera+0x198,camera+0xDB8);
  if(bad==11)write(camera+0x130,std::uintptr_t(0));
  if(bad==12)write(actor+0x1EE0,std::array<float,3>{std::numeric_limits<float>::quiet_NaN(),20,30});
  if(bad==13)write(actor+0x1EE0,std::array<float,3>{1001,20,30});
  if(bad==14)write(camera+0x80,std::array<float,3>{0,170,100});
  if(bad==15)write(actor+0x60,std::array<float,3>{10,std::numeric_limits<float>::infinity(),30});
  if(bad==16){camera_test::query_observer=deny_memory;denied_begin=command+0x880;denied_end=denied_begin+16;}
  write(command+0x880,std::array<float,4>{90,0,1,0});const auto denied=input(bad==17?9:7,-32767,0,true);
  assert(!proxied(denied));assert(read<float>(command+0x880)==90.f);camera_test::query_observer=prior_query;
 }
 // Publication is the authority: stale/foreign/invalid generations and state
 // changes after a valid proxy cannot redirect an unrelated native proposal.
 for(unsigned bad=0;bad<13;++bad){
  setup();auto f=input(7,-32767,0,true);
  if(bad==0)f.tick=game_test::tick-1920001;
  if(bad==1)++f.player.command;
  if(bad==2)++f.player.sticks.controller;
  if(bad==3)++f.player.motion.style;
  if(bad==4)f.generation=0;
  if(bad==5)f.frame=0;
  if(bad==6)f.heading_valid=false;
  if(bad==7)write(actor+0x1B40,std::uint32_t(0x1C));
  if(bad==8)write(actor+0x3AD8,std::uint8_t(1));
  if(bad==9)write(actor+0x3F0E,std::uint8_t(1));
  if(bad==10)write(actor+0x1EE0,std::array<float,3>{10,20,std::numeric_limits<float>::quiet_NaN()});
  if(bad==11)write(camera+0x80,std::array<float,3>{0,170,100});
  if(bad==12)f.axes.lx=std::numeric_limits<float>::quiet_NaN();
  assert(published_frames[player].publish(f));assert((proposal_phase({10,21,28})==std::array<float,3>{10,21,28}));
 }
 setup();input(7,-32767,0,true);assert((proposal_phase({10,21,-300})==std::array<float,3>{10,21,-300}));
 setup();input(7,-32767,0,true);const float nan=std::numeric_limits<float>::quiet_NaN();const auto invalid=proposal_phase({nan,21,28});assert(std::isnan(invalid[0])&&invalid[1]==21&&invalid[2]==28);
 // Final-review regressions collect every effect so one RED does not conceal
 // independent ownership and native-output failures later in this fixture.
 auto check=[&](bool ok,const char* name){if(!ok)std::fprintf(stderr,"locomotion29 review player%u: %s\n",player,name);review_check(ok,name);};
 setup();after_polar=nested_rejected_input;input(7,-32767,0,true);after_polar=nullptr;
 check((proposal_phase({10,21,28})==std::array<float,3>{10,21,28}),"ordinary nested rejected input reuses outer scope");
 // Use a fresh native actor identity before its first proxy authorization.
 // The same physical route and wrapper now authenticate this temporary actor.
 setup();{
  std::vector<std::uint8_t> fresh_actor(0x4200);std::memcpy(fresh_actor.data(),reinterpret_cast<void*>(actor),fresh_actor.size());
  const auto fresh=reinterpret_cast<std::uintptr_t>(fresh_actor.data());game_test::memory_maps.push_back({fresh,fresh_actor.size(),3});
  const auto players=read<std::uintptr_t>(main_base+re5::profile::offset<0x1CF8CC8>());const auto slot=read<std::uint8_t>(actor+0x3F2C);const auto member=players+0x30+slot*8;
  const auto saved_member=read<std::uintptr_t>(member),saved_wrapper=read<std::uintptr_t>(command-0x350+0x48);
  write(member,fresh);write(command-0x350+0x48,fresh);write(camera+0x130,fresh);write(camera+0xDB0+0x130,fresh);observing_actor=fresh;
  race_actor=fresh;race_command=command;race_fired=false;camera_test::query_observer=interleave_rejected_input;
  native_pad=pad;native_pad.lx=-32767;native_pad.ly=0;native_pad.rx=native_pad.ry=0;native_buttons=0x4000;game_test::tick+=320000;observed_pad(&native_pad,player?1:3,1);
  Re5LocalInput::Callback(reinterpret_cast<void*>(command),reinterpret_cast<void*>(fresh));camera_test::query_observer=prior_query;
  check(race_fired,"ordinary cross-worker rejected input interleaving did not execute");
  write(proposal,std::array<float,3>{10,21,28});re4::set_saved_x(&native_frame.context,19,fresh);Re5MovementProposal::Callback(&native_frame.context);
  check((read<std::array<float,3>>(proposal)==std::array<float,3>{10,21,28}),"ordinary first authorization overwrites rejected input revocation");
  write(member,saved_member);write(command-0x350+0x48,saved_wrapper);write(camera+0x130,actor);write(camera+0xDB0+0x130,actor);observing_actor=actor;game_test::memory_maps.pop_back();
 }
 for(unsigned mode=0;mode<4;++mode){
  setup();live_settings=settings;
  if(mode==0)live_settings.players[player].free_camera=false;
  if(mode==1)live_settings.players[player].unified_controls=false;
  if(mode==2)live_settings.players[player].analog_enabled=false;
  if(mode==3)live_settings.players[player].first_person=true;
  before_polar=publish_live_settings;input(7,-32767,0,true);before_polar=nullptr;
  check(read<float>(command+0x880)==90.f,"ordinary polar ignores changed live mode settings");
  setup();input(7,-32767,0,true);live_settings=settings;
  if(mode==0)live_settings.players[player].free_camera=false;
  if(mode==1)live_settings.players[player].unified_controls=false;
  if(mode==2)live_settings.players[player].analog_enabled=false;
  if(mode==3)live_settings.players[player].first_person=true;
  assert(controls_snapshot.publish(live_settings));check((proposal_phase({10,21,28})==std::array<float,3>{10,21,28}),"ordinary proposal ignores changed live mode settings");
 }
 for(unsigned gate=0;gate<3;++gate){
  auto exclude=[&]{if(gate==0)write(camera+0xDB0+0x197,std::uint8_t(1));if(gate==1)write(camera+0x2EC1,std::uint8_t(1));if(gate==2)write(camera+0x344,.5f);};
  setup();exclude();const auto excluded=input(7,-32767,0,true);
  check(!proxied(excluded)&&read<float>(command+0x880)==90.f,"ordinary polar changes native partner/skip/transition camera");
  setup();input(7,-32767,0,true);exclude();check((proposal_phase({10,21,28})==std::array<float,3>{10,21,28}),"ordinary proposal redirects native partner/skip/transition camera");
 }
 setup();input(2,-3277,0,true);check((proposal_phase({10,21,28})==std::array<float,3>{10,21,28}),"ordinary subthreshold stick redirects continuing native step");
 setup();const std::array<float,4> sentinel{33.f,44.f,.625f,.875f};write(command+0x880,sentinel);retain_native_cache=true;
 camera_test::query_observer=deny_memory;denied_begin=command+0x880;denied_end=denied_begin+16;input(7,-32767,0,true);camera_test::query_observer=prior_query;retain_native_cache=false;
 check((read<std::array<float,4>>(command+0x880)==sentinel),"ordinary rejected nonwritable polar cache still overwritten");
 setup();


}
}
}
