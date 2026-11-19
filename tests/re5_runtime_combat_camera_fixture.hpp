#pragma once
namespace re5_combat_camera_test {
using namespace re5_follow_test;
inline std::array<double,3> project(const re5::CameraPose& pose,Vec anchor,float fov,float aspect){
 const auto optical=camera_projection27(pose,anchor);const double tangent=std::tan(double(fov)*3.141592653589793/360);
 return {optical[0]/(tangent*aspect),optical[1]/tangent,optical[2]};
}
inline re5::CameraPose native_pose(Vec root,float yaw,float pitch,float shift,float roll_angle=0){
 const Vec forward{std::sin(yaw)*std::cos(pitch),std::sin(pitch),std::cos(yaw)*std::cos(pitch)};
 const auto right=unit(cross(forward,{0,1,0})),vertical=cross(right,forward);Vec eye{},target{},up{};
 for(unsigned i=0;i<3;++i){eye[i]=root[i]+(i==1?170.f:0.f)+right[i]*(35+shift)+vertical[i]*(10+shift*.4f)-forward[i]*120;
  target[i]=eye[i]+forward[i]*300;up[i]=vertical[i]*std::cos(roll_angle)+right[i]*std::sin(roll_angle);}
 return {eye,up,target};
}
template<class Publish>void run(unsigned player,std::uintptr_t actor,std::uintptr_t camera,std::uintptr_t laser,Publish publish){
 const auto b=camera+0x350,component=camera+0xDB0;exl::hook::InlineFloatCtx registers{};
 auto component_call=[&]{re4::set_saved_x(&registers,19,component);camera_test::actor_phase(offset(0x24AA54),&registers);};
 auto desired=[&]{const auto pose=read(component,0x160,0x170,0x180);for(auto destination:{camera,b}){
  put(destination,0x160,pose.eye);put(destination,0x170,pose.up);put(destination,0x180,pose.target);put(destination,0x190,get<float>(component,0x190));}
  put(b,0x1F0,get<Matrix>(actor,0xD0));
 };
 auto follow=[&]{const auto before=calls;camera_test::follow_phase(offset(0x23DBB0),reinterpret_cast<void*>(b),1.f/30,test_Re5CameraFollow);assert(calls==before+1);
  const auto output=read(b,0x60,0x70,0x80);put(camera,0x60,output.eye);put(camera,0x70,output.up);put(camera,0x80,output.target);put(camera,0x50,get<float>(b,0x50));
 };
 auto rig=[&]{re4::set_saved_x(&registers,19,camera);camera_test::actor_phase(offset(0x2406CC),&registers);};
 const unsigned actions[]{1,0x1B,0x1C,0x1F,0x23,0x24,1};
 for(unsigned native_mode:{0u,1u})for(unsigned follow_mode:{0u,1u})for(float distance:{1.f,1.2f,2.f}){
  re5::Settings settings;assert(controls_snapshot.read(settings));settings.players[player]=re4::re6_profile();settings.players[player].third_person_distance=distance;settings.players[player].gyro_enabled=false;assert(controls_snapshot.publish(settings));
  put(camera,0x344,0.f);put(camera,0x2EC1,std::uint8_t(0));put(component,0x197,std::uint8_t(0));put(component,0x195,std::uint8_t(native_mode));put(component,0x630,0.f);put(camera,0x4C,player?8.f/9:16.f/9);
  put(b,0x230,.2f);put(b,0x234,.9f);put(b,0x238,.8f);put(b,0x23C,.7f);put(b,0x240,.6f);put(b,0x198,std::pow(.8f,30.f));put(b,0x194,.8f);
  put(b,0x1A0,std::uint8_t(1));put(b,0x1A1,std::uint8_t(0));put(b,0x1A2,std::uint8_t(0));put(b,0x1A3,std::uint8_t(follow_mode));put(b,0x270,.75f);put(b,0x274,std::uint8_t(0));
  std::array<double,3> wanted{},overview{};unsigned prior_family=0;
  for(unsigned step=0;step<std::size(actions);++step){const unsigned action=actions[step];const Vec root{float(step)*3,0,float(step)*1.5f};const bool combat=action>8;
   publish(root,0,action,step==0);re4::set_saved_x(&registers,19,camera);Re5CameraPoseRestore::Callback(&registers);
   const auto input=player_runtime[player].frame;assert(input.valid&&input.action==action);if(combat)assert(!input.heading_valid);
   put(component,0x195,std::uint8_t(combat?native_mode:0));put(b,0x1A3,std::uint8_t(combat?follow_mode:1));
   const auto pose=native_pose(root,combat?.25f:0.f,combat?.12f:0.f,combat?float(step)*6.f:0.f,combat?.03f:0.f);
   const unsigned family=action<=8?1:action==0x1F?4:action>=0x22?3:2;
   if(combat&&family!=prior_family)wanted=camera_projection27(pose,{root[0],170,root[2]});
   prior_family=family;
   put(component,0x160,pose.eye);put(component,0x170,pose.up);put(component,0x180,pose.target);put(component,0x190,step?50.f:45.f);
   if(step==0){put(b,0x60,pose.eye);put(b,0x70,pose.up);put(b,0x80,pose.target);put(b,0x50,45.f);put(b,0x1B0,get<Matrix>(actor,0xD0));}
   const Vec hit{root[0]+25,root[1]+180,root[2]+3000},ray_origin{root[0],root[1]+170,root[2]};
   put(actor,0x3728,laser);put(laser,0,main_base+re5::profile::offset<0x1B735E0>());put(laser,0x374,std::uint32_t(input.player.actor_slot));put(laser,0x274,std::uint8_t(1));put(laser,0x378,std::uint8_t(0));put(laser,0x280,ray_origin);put(laser,0x290,hit);
   re4::set_saved_x(&registers,19,actor);re4::set_saved_x(&registers,21,laser);Re5RayCapture::Callback(&registers);assert(rays[player].valid);
   std::array<std::uint8_t,0x4200> body{};std::memcpy(body.data(),reinterpret_cast<void*>(actor),body.size());const auto gait_before=gait_serial[player];
   const auto skeleton=get<std::uintptr_t>(actor,0x8C8);std::array<std::uint8_t,3*0xC0> posed_bones{};std::memcpy(posed_bones.data(),reinterpret_cast<void*>(skeleton),posed_bones.size());
   component_call();const auto component_pose=read(component,0x160,0x170,0x180);
   if(combat){const auto original_direction=sub(pose.target,pose.eye),component_direction=sub(component_pose.target,component_pose.eye);
    for(unsigned i=0;i<3;++i)assert(std::fabs(original_direction[i]-component_direction[i])<.0001f);
    assert(component_pose.up==pose.up);
   }
   desired();follow();
   // Native Desired may add optical recoil/quake after follow. The final hook
   // must preserve that orientation while correcting only common translation.
   if(combat){auto optical=read(camera,0x60,0x70,0x80);const auto direction=rotate(Matrix{std::cos(.01f),0,-std::sin(.01f),0,0,1,0,0,std::sin(.01f),0,std::cos(.01f),0,0,0,0,1},sub(optical.target,optical.eye));put(camera,0x80,add(optical.eye,direction));}
   const auto before=read(camera,0x60,0x70,0x80);const float fov=get<float>(camera,0x50),aspect=get<float>(camera,0x4C);rig();const auto output=read(camera,0x60,0x70,0x80);
   const auto old_direction=sub(before.target,before.eye),new_direction=sub(output.target,output.eye);
   for(unsigned i=0;i<3;++i)assert(std::fabs(old_direction[i]-new_direction[i])<.0001f);
   assert(output.up==before.up&&get<float>(camera,0x50)==fov&&get<float>(camera,0x4C)==aspect);
   const Vec anchor{root[0],170,root[2]};const auto screen=combat?camera_projection27(output,anchor):project(output,anchor,fov,aspect);if(step==0)overview=screen;if(!combat)wanted=overview;
   if(!(screen[2]>0&&std::fabs(screen[0]-wanted[0])<.00002&&std::fabs(screen[1]-wanted[1])<.00002))std::fprintf(stderr,"combat29 player%u mode%u follow%u distance%.1f action%02X wanted %.7f/%.7f actual %.7f/%.7f\n",player,native_mode,follow_mode,distance,action,wanted[0],wanted[1],screen[0],screen[1]);
   assert(screen[2]>0&&std::fabs(screen[0]-wanted[0])<.00002&&std::fabs(screen[1]-wanted[1])<.00002);
   assert(!std::memcmp(body.data(),reinterpret_cast<void*>(actor),body.size())&&!std::memcmp(posed_bones.data(),reinterpret_cast<void*>(skeleton),posed_bones.size())&&gait_serial[player]==gait_before&&get<Vec>(laser,0x290)==hit);
   re4::set_saved_x(&registers,19,actor);re4::set_saved_x(&registers,21,laser);Re5RayCapture::Callback(&registers);assert(std::memcmp(rays[player].endpoint,hit.data(),12)==0);
   // Native wall resolution and its next-frame history follow Desired. A
   // repeated final hook and cleanup must leave the clipped eye/history alone.
   const Vec clipped{output.eye[0]+3,output.eye[1]-2,output.eye[2]+1};put(camera,0x60,clipped);std::memcpy(reinterpret_cast<void*>(camera+0x210),reinterpret_cast<void*>(camera+0x60),0x30);rig();Re5CameraPoseRestore::Callback(&registers);rig();
   assert(get<Vec>(camera,0x60)==clipped&&get<Vec>(camera,0x210)==clipped);
  }
 }
 auto raw=[&](unsigned action,Vec root,float shift,bool reset,float fov=45.f){
  publish(root,0,action,reset);re4::set_saved_x(&registers,19,camera);Re5CameraPoseRestore::Callback(&registers);
  const auto pose=native_pose(root,action>8?.25f:0.f,action>8?.12f:0.f,shift);
  put(component,0x160,pose.eye);put(component,0x170,pose.up);put(component,0x180,pose.target);put(component,0x190,fov);
  if(reset){put(b,0x60,pose.eye);put(b,0x70,pose.up);put(b,0x80,pose.target);put(b,0x50,fov);put(b,0x1B0,get<Matrix>(actor,0xD0));}
 };
 auto start=[&](unsigned first_action=1){re5::Settings settings;assert(controls_snapshot.read(settings));settings.players[player]=re4::re6_profile();settings.players[player].third_person_distance=1.2f;settings.players[player].gyro_enabled=false;assert(controls_snapshot.publish(settings));
  put(camera,0x344,0.f);put(camera,0x2EC1,std::uint8_t(0));put(camera,0x198,component);put(component,0x197,std::uint8_t(0));put(component,0x195,std::uint8_t(0));put(component,0x630,0.f);put(component,0x130,actor);put(camera,0x130,actor);put(camera,0x4C,16.f/9);
  put(b,0x230,.2f);put(b,0x234,.9f);put(b,0x238,.8f);put(b,0x23C,.7f);put(b,0x240,.6f);put(b,0x198,std::pow(.8f,30.f));put(b,0x194,.8f);
  put(b,0x1A0,std::uint8_t(1));put(b,0x1A1,std::uint8_t(0));put(b,0x1A2,std::uint8_t(0));put(b,0x1A3,std::uint8_t(1));put(b,0x270,.75f);put(b,0x274,std::uint8_t(0));
  raw(first_action,{0,0,0},0,true);const auto source=read(component,0x160,0x170,0x180);component_call();desired();follow();rig();return first_action>8?camera_projection27(source,{0,170,0}):project(read(camera,0x60,0x70,0x80),{0,170,0},get<float>(camera,0x50),get<float>(camera,0x4C));
 };
 // Contention on an established reference cannot turn "busy" into a cache
 // miss and recapture a bent animation's head height before publication fails.
 start();raw(0x1B,{10,0,5},20,false);const auto head_bones=get<std::uintptr_t>(actor,0x8C8);
 for(unsigned i=0;i<3;++i)put(head_bones+i*0xC0,0x54,140.f);
 const auto before_busy=read(component,0x160,0x170,0x180);auto& reference=camera_framing_refs[player];assert(reference.height==170.f);assert(!reference.lock.test_and_set());component_call();reference.lock.clear();
 const auto after_busy=read(component,0x160,0x170,0x180);if(std::memcmp(&before_busy,&after_busy,sizeof(before_busy)))std::fprintf(stderr,"combat29 player%u busy height cache modified native component\n",player);
 assert(!std::memcmp(&before_busy,&after_busy,sizeof(before_busy))&&reference.height==170.f);
 // Starting directly in supported combat also creates one anatomical reference;
 // same-family animation changes must not recapture height or optical ratios.
 for(unsigned first_action:{0x1Bu,0x1Fu,0x23u}){const auto wanted=start(first_action);raw(first_action==0x1F?0x1F:first_action==0x23?0x24:0x1C,{10,0,5},25,false);
  const auto bones=get<std::uintptr_t>(actor,0x8C8);for(unsigned i=0;i<3;++i)put(bones+i*0xC0,0x54,190.f);
  component_call();desired();follow();rig();const auto screen=camera_projection27(read(camera,0x60,0x70,0x80),{10,170,5});
  assert(std::fabs(screen[0]-wanted[0])<.00002&&std::fabs(screen[1]-wanted[1])<.00002);
 }
 // Native action dispatch can advance aim to fire or knife stance to strike
 // after input publication. A same-family transition retains the reference.
 for(auto transition:{std::pair{0x1Bu,0x1Cu},std::pair{0x23u,0x24u}}){start();raw(transition.first,{10,0,5},20,false);const auto wanted=camera_projection27(read(component,0x160,0x170,0x180),{10,170,5});put(actor,0x1B40,transition.second);
  component_call();desired();follow();rig();const auto screen=camera_projection27(read(camera,0x60,0x70,0x80),{10,170,5});
  assert(std::fabs(screen[0]-wanted[0])<.00002&&std::fabs(screen[1]-wanted[1])<.00002);
 }
 // A lens-only native follow stays byte-for-byte native, yet its authenticated
 // final Desired must still constrain the retained body reference.
 for(unsigned mode:{0u,1u}){start();put(b,0x1A3,std::uint8_t(mode));raw(0x1B,{10,0,5},20,false,45.05f);const auto wanted=camera_projection27(read(component,0x160,0x170,0x180),{10,170,5});component_call();desired();
  const auto goal=read(b,0x160,0x170,0x180);put(b,0x60,goal.eye);put(b,0x70,goal.up);put(b,0x80,goal.target);put(b,0x50,45.f);
  std::array<std::uint8_t,0x280> baseline{};std::memcpy(baseline.data(),reinterpret_cast<void*>(b),baseline.size());dispatch=false;native(reinterpret_cast<std::uintptr_t>(baseline.data()),1.f/30);dispatch=true;
  follow();assert(!std::memcmp(baseline.data(),reinterpret_cast<void*>(b),baseline.size()));rig();const auto output=read(camera,0x60,0x70,0x80);
  const auto screen=camera_projection27(output,{10,170,5});assert(std::fabs(screen[0]-wanted[0])<.00002&&std::fabs(screen[1]-wanted[1])<.00002);
 }
 // Any unqualified producer/follow route leaves final native Desired alone.
 for(unsigned bad=0;bad<18;++bad){start();raw(bad==11?0x1F:0x1B,{10,0,5},20,false);auto frame=player_runtime[player].frame;const auto saved_frame=frame;re5::Settings settings;assert(controls_snapshot.read(settings));auto changed=settings;
  if(bad==0){frame.scope=true;put(actor,0x3AD8,std::uint8_t(1));}
  if(bad==1)changed.players[player].first_person=true;
  if(bad==2)changed.players[player].free_camera=false;
  if(bad==3)changed.players[player].unified_controls=false;
  if(bad==4)put(component,0x197,std::uint8_t(1));
  if(bad==5)put(camera,0x344,1.f);
  if(bad==6)put(camera,0x2EC1,std::uint8_t(1));
  if(bad==7)put(b,0x274,std::uint8_t(1));
  if(bad==8)put(camera,0x198,component+8);
  if(bad==9){frame.action=0x2F;put(actor,0x1B40,0x2Fu);}
  if(bad==10)put(actor,0x3F0E,std::uint8_t(1));
  if(bad==11)changed.players[player].move_while_reloading=false;
  if(bad==12)frame.tick=game_test::tick-1920001;
  if(bad==13)changed.players[player].analog_enabled=false;
  if(bad==14)put(camera,0x344,std::numeric_limits<float>::quiet_NaN());
  if(bad==15)game_active.store(false);
  if(bad==16)put(actor,0x1B40,0x2Fu);
  if(bad==17)put(actor,0x1B40,0x23u);
  assert(controls_snapshot.publish(changed));assert(published_frames[player].publish(frame));component_call();desired();follow();const auto before=read(camera,0x60,0x70,0x80);rig();const auto after=read(camera,0x60,0x70,0x80);
  if(std::memcmp(&before,&after,sizeof(before)))std::fprintf(stderr,"combat29 excluded player%u case%u changed native Desired\n",player,bad);
  assert(!std::memcmp(&before,&after,sizeof(before)));game_active.store(true);assert(controls_snapshot.publish(settings));player_runtime[player].frame=saved_frame;assert(published_frames[player].publish(saved_frame));put(actor,0x3AD8,std::uint8_t(0));put(actor,0x3F0E,std::uint8_t(0));
 }
 // The first actual final-Desired entry consumes completion even if an early
 // gate or a busy slot prevents framing. Collision cannot be corrected later.
 for(unsigned blocked=0;blocked<3;++blocked){start();raw(0x1B,{10,0,5},20,false);component_call();desired();follow();
  if(blocked==0)assert(!camera_poses[player].lock.test_and_set());
  if(blocked==1)put(camera,0x130,std::uintptr_t(0));
  if(blocked==2)assert(!camera_follow_slots[player].lock.test_and_set());
  rig();
  if(blocked==0)camera_poses[player].lock.clear();
  if(blocked==1)put(camera,0x130,actor);
  if(blocked==2)camera_follow_slots[player].lock.clear();
  const auto previous=get<Vec>(camera,0x60);const Vec clipped{previous[0]+3,previous[1]-2,previous[2]+1};put(camera,0x60,clipped);put(camera,0x210,clipped);rig();
  const char* messages[]{"busy camera pose let second Rig overwrite collision","failed fresh frame let second Rig overwrite collision","busy follow slot let second Rig overwrite collision"};
  review_check(get<Vec>(camera,0x60)==clipped&&get<Vec>(camera,0x210)==clipped,messages[blocked]);
 }
 // A settings edit arriving after input publication must stop all component
 // writes; taking the snapshot after the component would hide this defect.
 const char* edits[]{"latest analog-off changed native component","latest unified-off changed native component","latest free-off changed native component","latest first-person changed native component","latest distance edit changed native component","latest reload-off changed native component"};
 for(unsigned edit=0;edit<6;++edit){start();raw(edit==5?0x1F:0x1B,{10,0,5},20,false);re5::Settings settings;assert(controls_snapshot.read(settings));auto changed=settings;
  if(edit==0)changed.players[player].analog_enabled=false;
  if(edit==1)changed.players[player].unified_controls=false;
  if(edit==2)changed.players[player].free_camera=false;
  if(edit==3)changed.players[player].first_person=true;
  if(edit==4)changed.players[player].third_person_distance=1.5f;
  if(edit==5)changed.players[player].move_while_reloading=false;
  assert(controls_snapshot.publish(changed));const auto before=read(component,0x160,0x170,0x180);component_call();const auto after=read(component,0x160,0x170,0x180);
  review_check(!std::memcmp(&before,&after,sizeof(before)),edits[edit]);assert(controls_snapshot.publish(settings));
 }
 // Overview must qualify these native camera modes before changing the rig,
 // rather than discovering them only while publishing a follow lease.
 for(unsigned action=0;action<=8;++action)for(unsigned excluded=0;excluded<3;++excluded){start();raw(action,{10,0,5},20,false);
  if(excluded==0)put(component,0x197,std::uint8_t(1));
  if(excluded==1)put(camera,0x2EC1,std::uint8_t(1));
  if(excluded==2)put(camera,0x344,1.f);
  const auto before=read(component,0x160,0x170,0x180);component_call();const auto after=read(component,0x160,0x170,0x180);
  const char* messages[]{"overview partner camera component was modified","overview skipped camera component was modified","overview transition component was modified"};review_check(!std::memcmp(&before,&after,sizeof(before)),messages[excluded]);
 }
 std::printf("combat29 player%u native family projection/optics/ray/collision PASS\n",player);
}
}
