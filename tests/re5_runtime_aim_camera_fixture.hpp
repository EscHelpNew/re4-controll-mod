#pragma once
namespace re5_aim_camera_test {
using namespace re5_combat_camera_test;
inline void close_ratios(const char* label,unsigned player,const re5::CameraPose& pose,Vec anchor,const std::array<double,3>& wanted){
 const auto actual=camera_projection27(pose,anchor);
 const bool pass=actual[2]>0&&std::fabs(actual[0]-wanted[0])<.00002&&std::fabs(actual[1]-wanted[1])<.00002;
 if(!pass)std::fprintf(stderr,"aim32 %s player%u wanted %.8f/%.8f actual %.8f/%.8f\n",label,player,wanted[0],wanted[1],actual[0],actual[1]);
 assert(pass);
}
inline void unchanged_optics(const re5::CameraPose& before,const re5::CameraPose& after){
 const auto old_direction=sub(before.target,before.eye),new_direction=sub(after.target,after.eye);
 for(unsigned i=0;i<3;++i)assert(std::fabs(old_direction[i]-new_direction[i])<.0002f);
 assert(before.up==after.up);
}
template<class Publish>void run(unsigned player,std::uintptr_t actor,std::uintptr_t camera,std::uintptr_t laser,Publish publish){
 const auto b=camera+0x350,component=camera+0xDB0;exl::hook::InlineFloatCtx registers{};
 auto component_call=[&]{re4::set_saved_x(&registers,19,component);camera_test::actor_phase(offset(0x24AA54),&registers);};
 auto desired=[&]{const auto pose=read(component,0x160,0x170,0x180);for(auto destination:{camera,b}){
  put(destination,0x160,pose.eye);put(destination,0x170,pose.up);put(destination,0x180,pose.target);put(destination,0x190,get<float>(component,0x190));}put(b,0x1F0,get<Matrix>(actor,0xD0));};
 auto follow=[&]{const auto before=calls;camera_test::follow_phase(offset(0x23DBB0),reinterpret_cast<void*>(b),1.f/30,test_Re5CameraFollow);assert(calls==before+1);
  const auto output=read(b,0x60,0x70,0x80);put(camera,0x60,output.eye);put(camera,0x70,output.up);put(camera,0x80,output.target);put(camera,0x50,get<float>(b,0x50));};
 auto rig=[&]{re4::set_saved_x(&registers,19,camera);camera_test::actor_phase(offset(0x2406CC),&registers);};
 auto finish=[&]{const auto before=read(camera,0x60,0x70,0x80);const auto lens=get<float>(camera,0x50),aspect=get<float>(camera,0x4C);rig();const auto after=read(camera,0x60,0x70,0x80);
  unchanged_optics(before,after);assert(get<float>(camera,0x50)==lens&&get<float>(camera,0x4C)==aspect);return after;};
 auto configure=[&](float distance,unsigned follow_mode){re5::Settings settings;assert(controls_snapshot.read(settings));settings.players[player]=re4::re6_profile();settings.players[player].third_person_distance=distance;settings.players[player].gyro_enabled=false;assert(controls_snapshot.publish(settings));
  put(actor,0x3AD8,std::uint8_t(0));put(camera,0x130,actor);put(camera,0x198,component);put(camera,0xDA0,component);put(camera,0x344,0.f);put(camera,0x2EC1,std::uint8_t(0));put(camera,0x4C,player?8.f/9:16.f/9);
  put(component,0x130,actor);put(component,0x197,std::uint8_t(0));put(component,0x630,0.f);
  put(b,0x230,.2f);put(b,0x234,.9f);put(b,0x238,.8f);put(b,0x23C,.7f);put(b,0x240,.6f);put(b,0x198,std::pow(.8f,30.f));put(b,0x194,.8f);
  put(b,0x1A0,std::uint8_t(1));put(b,0x1A1,std::uint8_t(0));put(b,0x1A2,std::uint8_t(0));put(b,0x1A3,std::uint8_t(follow_mode));put(b,0x270,.75f);put(b,0x274,std::uint8_t(0));};
 auto raw=[&](unsigned action,Vec root,float shift,unsigned native_mode=0,float countdown=0,bool reset=false,float lens=33.f){
  publish(root,0,action,reset);re4::set_saved_x(&registers,19,camera);Re5CameraPoseRestore::Callback(&registers);
  const auto pose=native_pose(root,action>8?.25f:0.f,action>8?.12f:0.f,shift,action>8?.03f:0.f);
  put(component,0x160,pose.eye);put(component,0x170,pose.up);put(component,0x180,pose.target);put(component,0x190,lens);put(component,0x195,std::uint8_t(native_mode));put(component,0x630,countdown);
  if(reset){put(b,0x60,pose.eye);put(b,0x70,pose.up);put(b,0x80,pose.target);put(b,0x50,lens);put(b,0x1B0,get<Matrix>(actor,0xD0));}return pose;};
 auto pipeline=[&]{component_call();desired();follow();return finish();};
 auto overview=[&](float distance,unsigned follow_mode){configure(distance,follow_mode);raw(1,{0,0,0},0,0,0,true,45);const auto pose=pipeline();return project(pose,{0,170,0},get<float>(camera,0x50),get<float>(camera,0x4C));};
 // An overview reference may never become the native shoulder reference.
 // Expectations come from the pre-edit component, before distance/follow/Rig.
 for(unsigned native_mode:{0u,1u})for(unsigned follow_mode:{0u,1u})for(float distance:{1.f,1.2f,2.f}){
  const auto retained_overview=overview(distance,follow_mode);Vec root{10,0,5};
  auto source=raw(0x1B,root,20,native_mode);auto wanted=camera_projection27(source,{10,170,5});
  const auto initial_firearm=wanted;const auto output=pipeline();close_ratios("native firearm entry must not inherit overview",player,output,{10,170,5},wanted);
  const auto overview_optical=camera_projection27(native_pose({0,0,0},0,0,0),{0,170,0});assert(std::fabs(wanted[0]-overview_optical[0])>.05);
  for(unsigned action:{0x1Cu,0x1Du,0x1Eu}){root=add(root,{3,0,2});raw(action,root,35+float(action),!native_mode,0,false,40);close_ratios("same-family firearm",player,pipeline(),add(root,{0,170,0}),wanted);}
  root=add(root,{3,0,2});source=raw(0x22,root,-15,native_mode,0,false,40);wanted=camera_projection27(source,add(root,{0,170,0}));close_ratios("new knife entry",player,pipeline(),add(root,{0,170,0}),wanted);
  for(unsigned action:{0x23u,0x24u,0x25u,0x26u,0x27u}){root=add(root,{3,0,2});raw(action,root,50+float(action),!native_mode,0,false,45);close_ratios("same-family knife strike",player,pipeline(),add(root,{0,170,0}),wanted);}
  root=add(root,{3,0,2});source=raw(0x1F,root,45,native_mode);wanted=camera_projection27(source,add(root,{0,170,0}));close_ratios("new reload entry",player,pipeline(),add(root,{0,170,0}),wanted);
  root=add(root,{3,0,2});source=raw(0x1B,root,30,native_mode);wanted=camera_projection27(source,add(root,{0,170,0}));assert(std::fabs(wanted[0]-initial_firearm[0])>.05);close_ratios("return firearm must reseed",player,pipeline(),add(root,{0,170,0}),wanted);
  root=add(root,{3,0,2});raw(1,root,0,0,0,false,45);const auto restored=pipeline();const auto screen=project(restored,add(root,{0,170,0}),get<float>(camera,0x50),get<float>(camera,0x4C));assert(std::fabs(screen[0]-retained_overview[0])<.00002&&std::fabs(screen[1]-retained_overview[1])<.00002);
  raw(0x2F,root,60,native_mode);pipeline();root=add(root,{3,0,2});source=raw(0x1B,root,55,native_mode);wanted=camera_projection27(source,add(root,{0,170,0}));close_ratios("unsupported-mode firearm re-entry",player,pipeline(),add(root,{0,170,0}),wanted);
 }
 // A committed family keeps its optical reference while native key sets
 // blend. Countdown630 delays a new seed, not an established aim/fire/strike.
 for(auto transition:{std::pair{0x1Bu,0x1Cu},std::pair{0x23u,0x24u}})for(unsigned native_mode:{0u,1u})for(unsigned follow_mode:{0u,1u}){
  overview(1.2f,follow_mode);const auto source=raw(transition.first,{10,0,5},20,native_mode,0,false,transition.first==0x1B?33.f:40.f);const auto wanted=camera_projection27(source,{10,170,5});pipeline();
  Vec root{10,0,5};
  for(float countdown:{.5f,.1f}){root=add(root,{3,0,2});const float lens=countdown==.5f?40.f:45.f;const auto native=raw(transition.second,root,countdown==.5f?40.f:65.f,native_mode,countdown,false,lens);
   component_call();unchanged_optics(native,read(component,0x160,0x170,0x180));assert(get<float>(component,0x190)==lens);desired();
   std::array<std::uint8_t,0x280> baseline{};std::memcpy(baseline.data(),reinterpret_cast<void*>(b),baseline.size());dispatch=false;re5_follow_test::native(reinterpret_cast<std::uintptr_t>(baseline.data()),1.f/30);dispatch=true;float native_lens=0;std::memcpy(&native_lens,baseline.data()+0x50,4);
   follow();assert(get<float>(b,0x50)==native_lens);close_ratios(transition.first==0x1B?"committed firearm survives630 blend":"committed knife survives630 blend",player,finish(),add(root,{0,170,0}),wanted);
  }
 }
 // An owned native handoff ends the combat entry whether it is observed
 // at the component, before Follow, or before the final Desired Rig callback.
 // Cached input can still describe the previous action and unscoped state.
 for(unsigned handoff=0;handoff<4;++handoff)for(unsigned phase=0;phase<3;++phase)for(unsigned native_mode:{0u,1u})for(unsigned follow_mode:{0u,1u}){
  overview(1.2f,follow_mode);const auto initial=raw(0x1B,{10,0,5},20,native_mode);const auto old_wanted=camera_projection27(initial,{10,170,5});pipeline();
  const auto source=raw(0x1C,{13,0,7},30,native_mode);assert(!player_runtime[player].frame.scope&&player_runtime[player].frame.action==0x1C);
  auto handoff_native=[&]{
   if(handoff==0)put(component,0x197,std::uint8_t(1));
   if(handoff==1)put(camera,0x2EC1,std::uint8_t(1));
   if(handoff==2)put(actor,0x3AD8,std::uint8_t(1));
   if(handoff==3)put(actor,0x1B40,0x23u);
  };
  if(phase==0)handoff_native();
  component_call();
  if(phase==0){const auto rejected=read(component,0x160,0x170,0x180);assert(!std::memcmp(&source,&rejected,sizeof(source)));}
  desired();if(phase==1)handoff_native();follow();if(phase==2)handoff_native();
  const auto before=read(camera,0x60,0x70,0x80);const auto output=finish();assert(!std::memcmp(&before,&output,sizeof(output)));
  put(component,0x197,std::uint8_t(0));put(camera,0x2EC1,std::uint8_t(0));put(actor,0x3AD8,std::uint8_t(0));
  const auto fresh=raw(0x1B,{16,0,9},50,native_mode);const auto wanted=camera_projection27(fresh,{16,170,9});assert(std::fabs(wanted[0]-old_wanted[0])>.1);
  const char* handoffs[]{"partner197","skipped2EC1","live scope","live cross-family"};const char* phases[]{"component","before Follow","before Rig"};char label[96]{};std::snprintf(label,sizeof(label),"%s handoff %s must reseed",handoffs[handoff],phases[phase]);close_ratios(label,player,pipeline(),{16,170,9},wanted);
  raw(0x1C,{19,0,11},75,native_mode);close_ratios("re-entry seed survives same-family fire",player,pipeline(),{19,170,11},wanted);
 }
 // An unrelated rejected component is not a native handoff for this camera.
 for(unsigned native_mode:{0u,1u}){
  overview(1.2f,1);const auto seed=raw(0x1B,{10,0,5},20,native_mode);const auto wanted=camera_projection27(seed,{10,170,5});pipeline();
  std::array<std::uint8_t,0x700> foreign{};const auto other=reinterpret_cast<std::uintptr_t>(foreign.data());game_test::memory_maps.push_back({other,foreign.size(),3});
  put(other,0x130,actor);put(other,0x160,seed.eye);put(other,0x170,seed.up);put(other,0x180,seed.target);put(other,0x190,33.f);put(other,0x195,std::uint8_t(native_mode));put(other,0x197,std::uint8_t(1));put(other,0x630,0.f);
  const auto real_before=read(component,0x160,0x170,0x180);const auto foreign_before=foreign;re4::set_saved_x(&registers,19,other);camera_test::actor_phase(offset(0x24AA54),&registers);const auto real_after=read(component,0x160,0x170,0x180);
  assert(foreign==foreign_before&&!std::memcmp(&real_before,&real_after,sizeof(real_before)));game_test::memory_maps.pop_back();
  raw(0x1C,{13,0,7},50,native_mode);close_ratios("foreign component cannot revoke active reference",player,pipeline(),{13,170,7},wanted);
 }
 // camera344==0 is insufficient while the independent component630 blends.
 // Such frames keep the whole unhooked native follow and final Desired.
 for(unsigned native_mode:{0u,1u})for(unsigned follow_mode:{0u,1u}){
  overview(1.2f,follow_mode);
  for(float countdown:{.5f,.1f}){const auto source=raw(0x1B,{10,0,5},countdown==.5f?10.f:15.f,native_mode,countdown);component_call();const auto after=read(component,0x160,0x170,0x180);assert(!std::memcmp(&source,&after,sizeof(source)));desired();
   std::array<std::uint8_t,0x280> baseline{};std::memcpy(baseline.data(),reinterpret_cast<void*>(b),baseline.size());dispatch=false;native(reinterpret_cast<std::uintptr_t>(baseline.data()),1.f/30);dispatch=true;follow();assert(!std::memcmp(baseline.data(),reinterpret_cast<void*>(b),baseline.size()));const auto before=read(camera,0x60,0x70,0x80);const auto output=finish();assert(!std::memcmp(&before,&output,sizeof(output)));}
  const auto source=raw(0x1B,{13,0,7},25,native_mode,0,false,33);const auto wanted=camera_projection27(source,{13,170,7});const auto output=pipeline();assert(std::fabs(get<float>(camera,0x50)-33.f)>.1f);close_ratios("630 zero uses raw pose despite lerping final FOV",player,output,{13,170,7},wanted);
  raw(0x1C,{16,0,9},60,!native_mode);close_ratios("settled seed survives fire",player,pipeline(),{16,170,9},wanted);
 }
 // Only a pending source needs the normalized195 provenance check. A changed
 // mode before follow or between follow and Rig must not commit that source.
 for(unsigned phase:{0u,1u})for(unsigned follow_mode:{0u,1u}){
  overview(1.2f,follow_mode);raw(0x1B,{10,0,5},20,0);component_call();desired();if(phase)follow();put(component,0x195,std::uint8_t(2));if(!phase)follow();
  const auto before=read(camera,0x60,0x70,0x80);const auto output=finish();assert(!std::memcmp(&before,&output,sizeof(output)));
  const auto fresh=raw(0x1B,{13,0,7},40,1);const auto wanted=camera_projection27(fresh,{13,170,7});close_ratios("rejected195 source cannot poison next entry",player,pipeline(),{13,170,7},wanted);
  raw(0x1C,{16,0,9},70,0);close_ratios("committed family ignores195 change",player,pipeline(),{16,170,9},wanted);
 }
 // Raw190 belongs to the pending source; final50 is an independent lens.
 for(unsigned phase:{0u,1u}){
  overview(1.2f,1);raw(0x1B,{10,0,5},20,1,0,false,33);component_call();desired();if(phase)follow();put(component,0x190,40.f);if(!phase)follow();
  const auto before=read(camera,0x60,0x70,0x80);const auto output=finish();assert(!std::memcmp(&before,&output,sizeof(output)));
  const auto fresh=raw(0x1B,{13,0,7},40,1,0,false,40);const auto wanted=camera_projection27(fresh,{13,170,7});close_ratios("rejected190 source cannot poison next entry",player,pipeline(),{13,170,7},wanted);
  raw(0x1C,{16,0,9},70,1,0,false,45);close_ratios("committed family accepts fresh native FOV",player,pipeline(),{16,170,9},wanted);
 }
 overview(1.2f,1);const auto nonzero=raw(0x1B,{10,0,5},20,2);const auto nonzero_wanted=camera_projection27(nonzero,{10,170,5});component_call();desired();put(component,0x195,std::uint8_t(1));follow();close_ratios("195 is normalized for pending source",player,finish(),{10,170,5},nonzero_wanted);
 // A fixed native optical pose with lens/aspect changes needs identical E/T.
 // This catches inventing lateral translation to retain a fixed combat NDC.
 for(unsigned native_mode:{0u,1u})for(unsigned follow_mode:{0u,1u}){
  re5::CameraPose first{};bool have_first=false;
  for(float lens:{33.f,40.f,45.f})for(float aspect:{16.f/9,8.f/9}){
   overview(1.2f,follow_mode);const auto seed=raw(0x1B,{10,0,5},20,native_mode);const auto wanted=camera_projection27(seed,{10,170,5});pipeline();
   const Vec root{16,0,9},anchor{16,170,9},hit{41,180,3009};raw(0x1C,root,60,!native_mode);component_call();desired();follow();
   const auto before=native_pose(root,.25f,.08f,65,.05f);put(camera,0x60,before.eye);put(camera,0x70,before.up);put(camera,0x80,before.target);put(camera,0x50,lens);put(camera,0x4C,aspect);
   const auto frame=player_runtime[player].frame;put(actor,0x3728,laser);put(laser,0,main_base+re5::profile::offset<0x1B735E0>());put(laser,0x374,std::uint32_t(frame.player.actor_slot));put(laser,0x274,std::uint8_t(1));put(laser,0x378,std::uint8_t(0));put(laser,0x280,anchor);put(laser,0x290,hit);
   re4::set_saved_x(&registers,19,actor);re4::set_saved_x(&registers,21,laser);Re5RayCapture::Callback(&registers);assert(rays[player].valid);
   std::array<std::uint8_t,0x4200> body{};std::memcpy(body.data(),reinterpret_cast<void*>(actor),body.size());const auto gait_before=gait_serial[player];
   const auto output=finish();close_ratios("lens/aspect-independent optical reference",player,output,anchor,wanted);
   if(!have_first){first=output;have_first=true;}else for(unsigned i=0;i<3;++i)assert(std::fabs(output.eye[i]-first.eye[i])<.0002f&&std::fabs(output.target[i]-first.target[i])<.0002f);
   const auto original_hit=project(before,hit,lens,aspect),actual_hit=project(output,hit,lens,aspect);assert(actual_hit[2]>0&&std::fabs(actual_hit[0])<1&&std::fabs(actual_hit[1])<1);
   const auto forward=unit(sub(before.target,before.eye)),right=unit(cross(forward,before.up)),up=cross(right,forward),delta=sub(output.eye,before.eye);const double tangent=std::tan(double(lens)*3.141592653589793/360);
   assert(std::fabs(dot(delta,forward))<.0002f);const double dx=-dot(delta,right)/(original_hit[2]*tangent*aspect),dy=-dot(delta,up)/(original_hit[2]*tangent);
   assert(std::fabs(actual_hit[0]-original_hit[0]-dx)<.000002&&std::fabs(actual_hit[1]-original_hit[1]-dy)<.000002&&std::fabs(dx)+std::fabs(dy)>.0001);
   assert(!std::memcmp(body.data(),reinterpret_cast<void*>(actor),body.size())&&gait_serial[player]==gait_before&&get<Vec>(laser,0x290)==hit&&std::memcmp(rays[player].endpoint,hit.data(),12)==0);
  }
 }
 // This projects a native world hit point; it does not model a head silhouette.
 std::printf("aim32 player%u raw native families/630/195/lens/target projection PASS\n",player);
}
}
