#pragma once
namespace re5_follow_test {
using Vec=std::array<float,3>;using Matrix=std::array<float,16>;
inline unsigned calls=0,updates=0;inline bool dispatch=true;
template<class T>T get(std::uintptr_t p,unsigned offset){return re5_gait_test::get<T>(p+offset);}
template<class T>void put(std::uintptr_t p,unsigned offset,const T& value){re5_gait_test::put(p+offset,value);}
inline Vec add(Vec a,Vec b){for(unsigned i=0;i<3;++i)a[i]+=b[i];return a;}
inline Vec sub(Vec a,Vec b){for(unsigned i=0;i<3;++i)a[i]-=b[i];return a;}
inline Vec mix(Vec old,Vec desired,float alpha){for(unsigned i=0;i<3;++i)old[i]=old[i]*alpha+desired[i]*(1-alpha);return old;}
inline float dot(Vec a,Vec b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
inline Vec unit(Vec a){const float length=std::sqrt(dot(a,a));for(float& v:a)v/=length;return a;}
inline Vec cross(Vec a,Vec b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
inline Vec rotate(const Matrix& m,Vec a,bool inverse=false){Vec v{};for(unsigned i=0;i<3;++i)for(unsigned j=0;j<3;++j)v[i]+=(inverse?m[i*4+j]:m[j*4+i])*a[j];return v;}
inline Vec origin(const Matrix& m){return {m[12],m[13],m[14]};}
inline re5::CameraPose read(std::uintptr_t b,unsigned eye,unsigned up,unsigned target){return {get<Vec>(b,eye),get<Vec>(b,up),get<Vec>(b,target)};}
inline float roll(const re5::CameraPose& p){const auto f=unit(sub(p.target,p.eye)),r=unit(cross(f,{0,1,0})),u=cross(r,f);return std::atan2(dot(p.up,r),dot(p.up,u));}
inline Vec up_for(const re5::CameraPose& p,float angle){const auto f=unit(sub(p.target,p.eye)),r=unit(cross(f,{0,1,0})),u=cross(r,f);Vec result{};for(unsigned i=0;i<3;++i)result[i]=r[i]*std::sin(angle)+u[i]*std::cos(angle);return result;}
inline std::size_t offset(std::size_t latest){return latest-(re5::profile::selected_build==re5::profile::Build::Legacy?0x1E0:0);}
// Native23DBB0: threshold admission, actor-basis origin/local-arm blend or
// world blend, then separately reloaded194 roll/FOV and native cache updates.
// Synthetic unit-yaw matrices avoid importing game code/assets into the test.
inline void native(std::uintptr_t b,float dt){
 ++calls;const auto old=read(b,0x60,0x70,0x80),desired=read(b,0x160,0x170,0x180);
 const float retention=std::pow(get<float>(b,0x198),dt);put(b,0x194,retention);
 const float threshold=get<std::uint8_t>(b,0x274)?get<float>(b,0x270):(get<std::uint8_t>(b,0x1A0)?.1f:3.f);put(b,0x274,std::uint8_t(0));
 const bool different=std::sqrt(dot(sub(old.eye,desired.eye),sub(old.eye,desired.eye)))>threshold
  ||std::sqrt(dot(sub(old.target,desired.target),sub(old.target,desired.target)))>threshold
  ||std::sqrt(dot(sub(old.up,desired.up),sub(old.up,desired.up)))>threshold||std::fabs(get<float>(b,0x50)-get<float>(b,0x190))>threshold;
 auto previous=get<Matrix>(b,0x1B0);const auto current=get<Matrix>(b,0x1F0);
 if(different||get<std::uint8_t>(b,0x1A1)||get<std::uint8_t>(b,0x1A2)){
  ++updates;re5::CameraPose result=old;exl::hook::InlineFloatCtx registers{};re4::set_saved_x(&registers,19,b);
  if(get<std::uint8_t>(b,0x1A3)){
   const float alpha=.9800000190734863f*get<float>(b,0x240)+.019999980926513672f*get<float>(b,0x23C);
   scalar(&registers,3,alpha);if(dispatch)camera_test::actor_phase(offset(0x23DF7C),&registers);const float spatial=scalar(&registers,3);put(b,0x240,spatial);
   Vec followed{};for(unsigned i=0;i<3;++i)followed[i]=previous[12+i]*get<float>(b,0x230+4*i)+current[12+i]*(1-get<float>(b,0x230+4*i));
   const auto eye=mix(rotate(current,sub(old.eye,origin(previous)),true),rotate(current,sub(desired.eye,origin(current)),true),spatial);
   const auto target=mix(rotate(current,sub(old.target,origin(previous)),true),rotate(current,sub(desired.target,origin(current)),true),spatial);
   result.eye=add(followed,rotate(current,eye));result.target=add(followed,rotate(current,target));put(b,0x250,eye);put(b,0x25C,0.f);put(b,0x260,target);put(b,0x26C,0.f);
   previous=current;for(unsigned i=0;i<3;++i)previous[12+i]=followed[i];
  }else{
   scalar(&registers,5,retention);if(dispatch)camera_test::actor_phase(offset(0x23E398),&registers);const float spatial=scalar(&registers,5);
   result.eye=mix(old.eye,desired.eye,spatial);result.target=mix(old.target,desired.target,spatial);previous=current;put(b,0x240,.9f);
  }
  const float lens=get<float>(b,0x194),angle=roll(old)*lens+roll(desired)*(1-lens);result.up=up_for(result,angle);
  put(b,0x60,result.eye);put(b,0x70,result.up);put(b,0x80,result.target);put(b,0x50,get<float>(b,0x50)*lens+get<float>(b,0x190)*(1-lens));
 }else previous=current;
 put(b,0x1B0,previous);put(b,0x1A0,std::uint8_t(different));put(b,0x1A2,std::uint8_t(0));
}
template<class Publish>void run(unsigned player,std::uintptr_t actor,std::uintptr_t camera,Publish publish){
 const auto b=camera+0x350,component=camera+0xDB0;const auto baseline_calls=calls;exl::hook::InlineFloatCtx context{};
 const auto pose=[&](Vec root,float heading){const float c=std::cos(heading),s=std::sin(heading);auto point=[&](float z){return Vec{root[0]-35*c+z*s,root[1]+180,root[2]+35*s+z*c};};return re5::CameraPose{point(-120),{0,1,0},point(180)};};
 auto publish_component=[&](Vec root,float heading){publish(root,heading);const auto native_pose=pose(root,heading);
  put(component,0x160,native_pose.eye);put(component,0x170,native_pose.up);put(component,0x180,native_pose.target);put(component,0x190,55.f);
  re4::set_saved_x(&context,19,component);camera_test::actor_phase(offset(0x24AA54),&context);
  const auto desired=read(component,0x160,0x170,0x180);
  // Native2437CC..243800 publishes component -> camera raw -> follow desired.
  for(auto destination:{camera,b}){put(destination,0x160,desired.eye);put(destination,0x170,desired.up);put(destination,0x180,desired.target);put(destination,0x190,55.f);}
  put(b,0x1F0,get<Matrix>(actor,0xD0));return desired;
 };
 auto initialize=[&](unsigned mode){put(camera,0x344,0.f);put(camera,0x2EC1,std::uint8_t(0));
  put(b,0x230,.2f);put(b,0x234,.9f);put(b,0x238,.8f);put(b,0x23C,.7f);put(b,0x240,.6f);put(b,0x198,std::pow(.8f,30.f));put(b,0x194,.8f);
  put(b,0x1A0,std::uint8_t(1));put(b,0x1A1,std::uint8_t(0));put(b,0x1A2,std::uint8_t(0));put(b,0x1A3,std::uint8_t(mode));put(b,0x270,.75f);put(b,0x274,std::uint8_t(0));
  const auto initial=publish_component({0,0,0},0);put(b,0x60,initial.eye);put(b,0x70,initial.up);put(b,0x80,initial.target);put(b,0x50,45.f);put(b,0x1B0,get<Matrix>(actor,0xD0));
 };
 auto consume=[&](float dt=1.f/30){const auto before=calls;camera_test::follow_phase(offset(0x23DBB0),reinterpret_cast<void*>(b),dt,test_Re5CameraFollow);assert(calls==before+1);};
 auto native_baseline=[&](float dt=1.f/30){std::array<std::uint8_t,0x280> copy{};std::memcpy(copy.data(),reinterpret_cast<void*>(b),copy.size());dispatch=false;native(reinterpret_cast<std::uintptr_t>(copy.data()),dt);dispatch=true;return copy;};
 // Final persistent output, not merely component output, must hold the torso
 // projection under root travel, yaw, reversal and sub-threshold movement.
 for(unsigned mode:{0u,1u})for(float dt:{1.f/30,1.f/60}){
  initialize(mode);consume(dt);std::array<double,3> initial{};bool first=true;
  for(unsigned n=0;n<36;++n){const Vec root{float(n)*.07f,0,float(n)*.02f};const float heading=(n<18?float(n):float(36-n))*.025f;
   const auto desired=publish_component(root,heading);const auto baseline=native_baseline(dt);const auto before_updates=updates;consume(dt);const auto actual=read(b,0x60,0x70,0x80);
   const Vec anchor{root[0],170,root[2]};const auto expected=camera_projection27(desired,anchor),screen=camera_projection27(actual,anchor);
   if(first){initial=expected;first=false;}assert(expected[2]>0&&std::fabs(expected[0]-initial[0])<.00001);
   if(!(screen[2]>0&&std::fabs(screen[0]-expected[0])<.00001&&std::fabs(screen[1]-expected[1])<.00001))std::fprintf(stderr,"follow28 player%u mode%u frame%u expectedX %.8f actualX %.8f\n",player,mode,n,expected[0],screen[0]);
   assert(screen[2]>0&&std::fabs(screen[0]-expected[0])<.00001&&std::fabs(screen[1]-expected[1])<.00001);assert(updates==before_updates+1);
   assert(get<float>(b,0x230)==.2f&&get<float>(b,0x234)==.9f&&get<float>(b,0x238)==.8f&&get<float>(b,0x23C)==.7f);
   assert(get<float>(b,0x270)==.75f&&get<std::uint8_t>(b,0x274)==0);float lens=0,alpha=0;std::memcpy(&lens,baseline.data()+0x50,4);std::memcpy(&alpha,baseline.data()+0x240,4);
   assert(get<float>(b,0x50)==lens&&get<float>(b,0x240)==alpha);assert(mode||alpha==.9f);assert(get<std::uint8_t>(b,0x1A2)==0);
  }
 }
 // A tiny translation has no ordinary native admission; the owned one-shot
 // must admit it, while FOV and roll retain their independently loaded weight.
 initialize(1);consume();const auto tiny=publish_component({.01f,0,.01f},0);put(b,0x50,55.f);const auto before_tiny=updates;
 native_baseline();assert(updates==before_tiny);consume();assert(updates==before_tiny+1);
 assert(std::fabs(get<Vec>(b,0x60)[0]-tiny.eye[0])<.00001f);
 for(unsigned mode:{0u,1u})for(unsigned hints=0;hints<4;++hints){initialize(mode);publish_component({20,0,10},0);
  put(b,0x70,up_for(read(b,0x60,0x70,0x80),.25f));put(b,0x1A0,std::uint8_t(hints&1));put(b,0x1A1,std::uint8_t(hints&1));put(b,0x1A2,std::uint8_t((hints>>1)&1));
  const auto baseline=native_baseline();consume();Vec native_up{};float native_fov=0;std::memcpy(native_up.data(),baseline.data()+0x70,12);std::memcpy(&native_fov,baseline.data()+0x50,4);
  assert(get<Vec>(b,0x70)==native_up&&get<float>(b,0x50)==native_fov);assert(get<std::uint8_t>(b,0x1A1)==(hints&1)&&get<std::uint8_t>(b,0x1A2)==0);
 }
 // Identical publication still authorizes the pipeline, but must not create
 // a native update or overwrite240 with an uncaptured synthetic alpha.
 initialize(1);put(b,0x50,55.f);const auto before_identical=updates;const auto identical=native_baseline();consume();assert(updates==before_identical);
 assert(!std::memcmp(identical.data(),reinterpret_cast<void*>(b),identical.size()));
 publish_component({40,0,20},.4f);consume();const auto consumed=native_baseline();consume();assert(!std::memcmp(consumed.data(),reinterpret_cast<void*>(b),consumed.size()));
 // Follow leases belong to one camera invocation. A failed first validation
 // consumes that camera's lease; an unrelated camera cannot consume it.
 bool review_pass=true;auto review=[&](bool passed,const char* message){if(!passed){std::fprintf(stderr,"follow28 review player%u: %s\n",player,message);review_pass=false;}};
 initialize(1);publish_component({40,0,20},.4f);put(camera,0x130,std::uintptr_t(0));consume();put(camera,0x130,actor);
 const auto after_failure=native_baseline();consume();review(!std::memcmp(after_failure.data(),reinterpret_cast<void*>(b),after_failure.size()),"failed fresh frame retained a reusable camera lease");
 initialize(1);const auto real_desired=publish_component({40,0,20},.4f);
 {
  std::array<std::uint8_t,0x600> foreign{};const auto other=reinterpret_cast<std::uintptr_t>(foreign.data());
  game_test::memory_maps.push_back({other,foreign.size(),3});put(other,0x130,actor);std::memcpy(foreign.data()+0x350,reinterpret_cast<void*>(b),0x280);
  camera_test::follow_phase(offset(0x23DBB0),reinterpret_cast<void*>(other+0x350),1.f/30,test_Re5CameraFollow);game_test::memory_maps.pop_back();
 }
 consume();review(std::hypot(std::hypot(get<Vec>(b,0x60)[0]-real_desired.eye[0],get<Vec>(b,0x60)[1]-real_desired.eye[1]),get<Vec>(b,0x60)[2]-real_desired.eye[2])<.0001f,"foreign camera consumed the real camera lease");
 // Without any eye/target delta, a small native lens/roll change must retain
 // native admission, flags, banks and alpha instead of forcing a spatial pass.
 for(unsigned mode:{0u,1u})for(bool lens_only:{false,true}){initialize(mode);
  for(auto destination:{camera,component,b})put(destination,0x190,lens_only?45.05f:45.f);
  if(!lens_only)put(b,0x70,up_for(read(b,0x60,0x70,0x80),.02f));
  const auto baseline=native_baseline();consume();review(!std::memcmp(baseline.data(),reinterpret_cast<void*>(b),baseline.size()),lens_only?"FOV-only change forced native admission":"roll-only change forced native admission");
 }
 assert(review_pass);
 // Excluded invocations must produce the same complete native state as an
 // unhooked call, including coefficient evolution and a prior one-shot request.
 for(unsigned bad=0;bad<33;++bad){initialize(1);publish_component({40,0,20},.4f);auto frame=player_runtime[player].frame;const auto saved=frame;const auto saved_world=get<Matrix>(actor,0xD0);re5::Settings settings;assert(controls_snapshot.read(settings));float dt=1.f/30;
  if(bad==0)frame.settings.free_camera=false;
  if(bad==1)frame.settings.unified_controls=false;
  if(bad==2)frame.settings.first_person=true;
  if(bad==3)frame.scope=true;
  if(bad==4)frame.action=0x2F;
  if(bad==5)frame.tick=game_test::tick-1920001;
  if(bad==6)++frame.generation;
  if(bad==7)++frame.look_epoch;
  if(bad==8)++frame.frame;
  if(bad==9)put(camera,0x2EC1,std::uint8_t(1));
  if(bad==10)put(camera,0x344,1.f);
  if(bad==11)put(b,0x1A3,std::uint8_t(2));
  if(bad==12)put(b,0x274,std::uint8_t(1));
  if(bad==13)put(actor,0x3AD8,std::uint8_t(1));
  if(bad==14)put(camera,0x198,component+8);
  if(bad==15){auto changed=settings;changed.players[player].free_camera=false;assert(controls_snapshot.publish(changed));}
  if(bad==16)++frame.look_release;
  if(bad==17)put(camera,0x130,player_runtime[1-player].frame.player.actor);
  if(bad==18)put(camera,0x160,get<float>(camera,0x160)+1.f);
  if(bad==19)put(component,0x160,get<float>(component,0x160)+1.f);
  if(bad==20)put(b,0x160,get<float>(b,0x160)+1.f);
  if(bad==21)put(b,0x230,-.1f);
  if(bad==22)put(b,0x240,1.1f);
  if(bad==23)dt=0;
  if(bad==24)dt=.5f;
  if(bad==25)game_active.store(false);
  if(bad==26)put(camera,0x344,std::numeric_limits<float>::quiet_NaN());
  if(bad==27){auto changed=settings;changed.players[player].third_person_distance=1.5f;assert(controls_snapshot.publish(changed));}
  if(bad==28){auto changed=settings;changed.players[player].analog_enabled=false;assert(controls_snapshot.publish(changed));}
  if(bad==29){auto moved=saved_world;moved[12]+=1;put(actor,0xD0,moved);}
  if(bad==30){re4::set_saved_x(&context,19,camera);Re5CameraPoseRestore::Callback(&context);}
  if(bad==31){re4::set_saved_x(&context,19,component);put(component,0x130,std::uintptr_t(0));camera_test::actor_phase(offset(0x24AA54),&context);put(component,0x130,actor);}
  if(bad==32)frame.tick=game_test::tick+1;
  assert(published_frames[player].publish(frame));const auto baseline=native_baseline(dt);consume(dt);
  if(std::memcmp(baseline.data(),reinterpret_cast<void*>(b),baseline.size()))std::fprintf(stderr,"follow28 excluded player%u case%u changed native state\n",player,bad);
  assert(!std::memcmp(baseline.data(),reinterpret_cast<void*>(b),baseline.size()));
  player_runtime[player].frame=saved;assert(published_frames[player].publish(saved));assert(controls_snapshot.publish(settings));put(actor,0x3AD8,std::uint8_t(0));put(camera,0x198,component);put(camera,0x130,actor);put(actor,0xD0,saved_world);game_active.store(true);
 }
 // Collision is downstream: no later restore/rig callback can restore the
 // desired pre-wall eye over a native clipped persistent camera result.
 initialize(1);publish_component({20,0,10},.2f);consume();
 const auto followed=read(b,0x60,0x70,0x80);put(camera,0x60,followed.eye);put(camera,0x70,followed.up);put(camera,0x80,followed.target);put(camera,0x50,get<float>(b,0x50));put(camera,0x4C,16.f/9);
 re4::set_saved_x(&context,19,camera);camera_test::actor_phase(offset(0x2406CC),&context); // Desired completes before wall collision.
 const Vec clipped{7,8,9};put(camera,0x60,clipped);std::memcpy(reinterpret_cast<void*>(camera+0x210),reinterpret_cast<void*>(camera+0x60),0x30);
 Re5CameraPoseRestore::Callback(&context);camera_test::actor_phase(offset(0x2406CC),&context);assert(get<Vec>(camera,0x60)==clipped&&get<Vec>(camera,0x210)==clipped);
 assert(calls>baseline_calls);std::printf("follow28 player%u installed native follow/projection/ownership PASS\n",player);
}
}
