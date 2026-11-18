#include "re5/gait.hpp"
#include <map>
#include <vector>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <limits>
#undef assert
#define assert(c) do {if(!(c)){std::fprintf(stderr,"RE5 gait line %d: %s\n",__LINE__,#c);std::exit(1);}}while(false)
namespace {
struct Fixture {
 static constexpr std::uintptr_t main=0x10000000,players=0x20000000,pads=0x30000000,a=0x40000000,b=0x40010000,wa=0x50000000,wb=0x50010000,resource=0x60000000,clip=0x61000000,tracks=0x62000000,table=0x63000000,proposal=0x70000000,parameters=main+0x1CFC510;
 std::map<std::uintptr_t,std::vector<unsigned char>> regions;
 std::uintptr_t denied=0,fail_write=0;bool sample_bad=false,quat_bad=false;unsigned writes=0;bool blend_fixture=false;
 static constexpr std::uintptr_t alternate_clip=0x61100000,alternate_tracks=0x62100000;
 void add(std::uintptr_t at,std::size_t n){regions.emplace(at,std::vector<unsigned char>(n));}
 static bool range(void* c,std::uintptr_t at,std::size_t n){for(auto& [base,v]:static_cast<Fixture*>(c)->regions)if(at>=base&&at-base<=v.size()&&n<=v.size()-(at-base))return true;return false;}
 static bool read(void* c,std::uintptr_t at,void* out,std::size_t n){for(auto& [base,v]:static_cast<Fixture*>(c)->regions)if(at>=base&&at-base<=v.size()&&n<=v.size()-(at-base)){std::memcpy(out,v.data()+at-base,n);return true;}return false;}
 static bool writable(void* c,std::uintptr_t at,std::size_t n){auto& f=*static_cast<Fixture*>(c);return at!=f.denied&&range(c,at,n);}
 static bool write(void* c,std::uintptr_t at,const void* in,std::size_t n){auto& f=*static_cast<Fixture*>(c);++f.writes;if(at==f.fail_write){f.fail_write=0;return false;}for(auto& [base,v]:f.regions)if(at>=base&&at-base<=v.size()&&n<=v.size()-(at-base)){std::memcpy(v.data()+at-base,in,n);return true;}return false;}
 template<class T>void put(std::uintptr_t at,T value){assert(write(this,at,&value,sizeof(value)));}
 template<class T>T get(std::uintptr_t at){T v{};assert(read(this,at,&v,sizeof(v)));return v;}
 static std::uintptr_t lookup(void* c,std::uintptr_t r,unsigned index){auto& f=*static_cast<Fixture*>(c);return f.get<std::uintptr_t>(f.get<std::uintptr_t>(r+0x70)+8+index*8);}
 static bool init(void*,void* cursor,std::uintptr_t track){std::memcpy(cursor,&track,sizeof(track));return true;}
 static bool sample(void* c,void* cursor,float frame,re5::GaitValue& out){auto& f=*static_cast<Fixture*>(c);std::uintptr_t tr=0;std::memcpy(&tr,cursor,sizeof(tr));auto word=f.get<std::uint32_t>(tr);
  if(f.blend_fixture){const bool alternate=tr>=alternate_tracks&&tr<alternate_tracks+10*0x30;const auto type=(word>>8)&255;
   if(type==4&&alternate){if(frame<0||frame>6)return false;out={0,0,4*frame,1};return true;}
   if(type==0){out=alternate?re5::GaitValue{0,0,-.8f,-.6f}:re5::GaitValue{0,0,0,1};return true;}
   if(type==1){out={alternate?30+frame:10,0,0,1};return true;}
  }
  if(f.sample_bad){return false;}
  if(((word>>8)&255)==4){ // Independent native sampler boundary fixture, nonuniform root displacement.
  const float values[]={0,4,9,12};unsigned i=static_cast<unsigned>(frame);if(i>3)return false;out={0,0,i==3?values[3]:values[i]+(values[i+1]-values[i])*(frame-float(i)),1};
 }else if(((word>>8)&255)==1)out={float(word>>24),7,11,1};else if(f.quat_bad)out={0,0,0,0};else {const float x=float(word>>24)/100,y=frame/100;out={x,y,std::sqrt(.36f-x*x-y*y),.8f};}return true;}
 static bool mirror(void*,std::uintptr_t,std::uintptr_t,re5::GaitValue* translation,re5::GaitValue* quat){if(translation)translation->x=-translation->x;if(quat)quat->x=-quat->x;return true;}
 static std::uintptr_t weapon(void*,std::uintptr_t actor){return (actor==a?wa:wb)+0x1100;}
 re5::GaitBackend backend(){return {{this,read,range},this,writable,write,lookup,init,sample,weapon,mirror};}
 static unsigned model_index(std::uintptr_t actor,unsigned logical){
  // Hand-copied verified MOD metadata: Chris and Sheva have different layouts.
  std::array<unsigned,147> map{};for(unsigned i=0;i<147;++i)map[i]=i;
  constexpr unsigned ids[]={15,16,17,18,19,20,21,22,23,66,67,68,69};
  constexpr unsigned chris[]={111,112,113,114,115,117,119,120,121,123,122,116,118};
  constexpr unsigned sheva[]={1,2,3,4,5,8,9,10,11,7,13,6,12};
  for(unsigned i=0;i<13;++i){const unsigned desired=actor==a?chris[i]:sheva[i];unsigned other=0;while(map[other]!=desired)++other;std::swap(map[ids[i]],map[other]);}
  return map[logical];
 }
 std::uintptr_t bone(std::uintptr_t actor,unsigned logical){return (actor==a?0x64000000:0x65000000)+model_index(actor,logical)*0xC0;}
 void actor(std::uintptr_t actor,std::uintptr_t wrapper,unsigned slot,unsigned local){
  add(actor,0x4200);add(wrapper,0x900);const auto bones=bone(actor,0),map=bones+0x10000,action=wrapper+0x1000;add(bones,(actor==a?130:147)*0xC0);add(map,256);add(action,0x40);add(wrapper+0x1100,0x40);put(wrapper+0x1108,std::uint32_t(0x10F));put(wrapper+0x112C,std::uint32_t(0x100));
  put(players+0x30+slot*8,actor);put(actor+0x3F2C,std::uint8_t(slot));put(actor+0x3F49,std::uint8_t(local));put(actor+0x3EE8,wrapper);put(wrapper+0x48,actor);
  put(actor+8,std::uint16_t(0x201));put(actor+0x1B10,std::uint8_t(0));put(actor+0x168E,std::uint8_t(1));put(actor+0x1B40,std::uint32_t(0x1C));put(actor+0x1B60,action);put(action+0x30,actor);
  put(actor+0x29C8,std::uint8_t(1));put(actor+0x1EB8,std::int16_t(100));put(actor+0x1EBA,std::int16_t(100));
  put(actor+0x8C0,std::uint32_t(actor==a?130:147));put(actor+0x8C8,bones);put(actor+0x8D0,map);put(actor+0x15D0,resource);
  for(unsigned i=0;i<3;++i){put(actor+0x80+i*4,1.f);put(actor+0x1570+i*4,1.f);}
  for(unsigned id=0;id<(actor==a?130u:147u);++id){put(map+id,std::uint8_t(model_index(actor,id)));unsigned parent=id==15?0:id==16||id==20?15:id-1;unsigned kind=id==16||id==20?42:id==17||id==21?2:id==18||id==22?19:0;put(bone(actor,id)+0xA0,std::uint32_t((id<<24)|(kind<<16)|((parent<147?model_index(actor,parent):255)<<8)));put(bone(actor,id)+0x70,re5::GaitValue{0,0,0,1});put(bone(actor,id)+0x90,re5::GaitValue{0,0,0,1});}
 }
 Fixture(){
  add(main+0x1AAEF28,8);add(parameters,0x2D00);put(main+0x1AAEF28,parameters);
  for(unsigned id:{0x10Fu,0x115u})put(parameters+(id&255)*0x50+0x2C,std::uint32_t(1));
  for(unsigned id:{0x101u,0x10Eu})put(parameters+(id&255)*0x50+0x2C,std::uint32_t(2));
  for(unsigned id:{0x106u,0x107u,0x108u})put(parameters+(id&255)*0x50+0x2C,std::uint32_t(3));
  add(main+0x1CF8CC8,8);add(main+0x26A9288,8);add(players,0x100);add(pads,0x9000);add(resource,0x80);add(table,0x80);add(clip,0x60);add(tracks,0x30*10);add(proposal,0x40);
  put(main+0x1CF8CC8,players);put(main+0x26A9288,pads);put(pads+0x8C98,std::uint32_t(0));put(pads+0x8C94,std::uint32_t(0));for(unsigned i=0;i<2;++i){put(pads+i*0xEE0+0x670,std::uint32_t(i));put(pads+i*0xEE0+0x674,std::uint32_t(1));}
  put(resource+0x70,table);put(table+6,std::uint16_t(14));put(table+8+8,clip);put(table+8+7*8,clip);put(table+8+12*8,clip);put(table+8+13*8,clip);put(clip,tracks);put(clip+8,std::uint32_t(10));put(clip+12,std::uint32_t(4));
  const unsigned ids[]={255,15,16,18,19,20,22,23,19,23};for(unsigned i=0;i<10;++i){unsigned channel=i==0?4:i>=8?1:0;unsigned kind=ids[i]==16||ids[i]==20?42:0;put(tracks+i*0x30,std::uint32_t((ids[i]<<24)|(kind<<16)|(channel<<8)|1));put(tracks+i*0x30+4,1.f);}
  actor(a,wa,2,0);actor(b,wb,1,1);put(proposal,re5::GaitValue{10,20,30,99});writes=0;
 }
 void blend_clips(){
  blend_fixture=true;add(alternate_clip,0x60);add(alternate_tracks,10*0x30);regions[alternate_tracks]=regions[tracks];
  put(alternate_clip,alternate_tracks);put(alternate_clip+8,std::uint32_t(10));put(alternate_clip+12,std::uint32_t(7));
  put(table+8+13*8,alternate_clip);put(table+8+7*8,alternate_clip);
  // Endpoint resources need not store their channels in the same order.
  for(unsigned i=0;i<0x30;++i)std::swap(regions[alternate_tracks][0x30+i],regions[alternate_tracks][5*0x30+i]);
 }
 re5::GaitFrame frame(std::uintptr_t actor=a,std::uint64_t tick=1){re5::GaitFrame f;f.main=main;f.actor=actor;f.command=(actor==a?wa:wb)+0x350;f.frame=tick;f.generation=1;f.world_x=.6f;f.world_z=.8f;f.frame_step=1;f.settings.move_while_aiming=true;return f;}
};
bool near(float x,float y){return std::fabs(x-y)<.0001f;}
void proposal_uses_native_root_delta(){Fixture f;re5::GaitAdapter gait;auto frame=f.frame();assert(gait.prepare_proposal(f.backend(),frame,Fixture::proposal));auto p=f.get<re5::GaitValue>(Fixture::proposal);assert(near(p.x,12.4f)&&near(p.y,20)&&near(p.z,33.2f)&&p.w==99);}
void clip_wrap_uses_endpoint_difference(){Fixture f;re5::GaitAdapter gait;auto frame=f.frame();assert(gait.prepare_proposal(f.backend(),frame,Fixture::proposal));assert(gait.overlay_pose(f.backend(),frame));frame.frame=2;frame.frame_step=2;assert(gait.prepare_proposal(f.backend(),frame,Fixture::proposal));auto p=f.get<re5::GaitValue>(Fixture::proposal);assert(near(p.x,17.2f)&&near(p.z,39.6f));assert(gait.overlay_pose(f.backend(),frame));frame.frame=3;frame.frame_step=1;assert(gait.prepare_proposal(f.backend(),frame,Fixture::proposal));p=f.get<re5::GaitValue>(Fixture::proposal);assert(near(p.x,19.6f)&&near(p.z,42.8f));}
void overlay_preserves_upper_bones_metadata_and_helpers(){Fixture f;re5::GaitAdapter gait;auto frame=f.frame();auto before=f.regions[f.bone(Fixture::a,0)];assert(gait.prepare_proposal(f.backend(),frame,Fixture::proposal));assert(gait.overlay_pose(f.backend(),frame));assert(!gait.overlay_pose(f.backend(),frame));auto after=f.regions[f.bone(Fixture::a,0)];for(std::size_t i=0;i<after.size();++i){unsigned index=unsigned(i/0xC0),offset=unsigned(i%0xC0);bool owned=false;for(unsigned id:{15u,16u,18u,19u,20u,22u,23u})if(index==f.model_index(Fixture::a,id)&&offset>=0x70&&offset<0x80)owned=true;for(unsigned id:{19u,23u})if(index==f.model_index(Fixture::a,id)&&offset>=0x90&&offset<0xA0)owned=true;if(!owned)assert(before[i]==after[i]);}auto hip=f.get<re5::GaitValue>(f.bone(Fixture::a,16)+0x70);assert(hip.z>.5f&&near(hip.w,.8f));auto toe=f.get<re5::GaitValue>(f.bone(Fixture::a,19)+0x90);assert(toe.x==19&&toe.y==7&&toe.z==11);}
void invalid_source_never_mutates(){
 for(unsigned bad=0;bad<22;++bad){Fixture f;re5::GaitAdapter gait;auto frame=f.frame();switch(bad){
 case 0:f.put(Fixture::a+0x3AD8,std::uint32_t(1));break;case 1:f.put(Fixture::a+0x1B4C,std::uint8_t(1));break;
 case 2:f.put(Fixture::a+0x3F0E,std::uint8_t(1));break;case 3:f.put(Fixture::a+0x3F40,std::uint32_t(1));break;
 case 4:f.put(Fixture::players+0x40,Fixture::b);break;case 5:f.put(Fixture::wa+0x48,Fixture::b);break;
 case 6:f.put(Fixture::a+0x1B40,std::uint32_t(0x20));break;case 7:f.put(Fixture::a+0x1A71,std::uint8_t(1));break;
 case 8:f.put(Fixture::a+0x1B4C,std::uint8_t(1));break;case 9:f.put(Fixture::a+0x1AB0,std::uint32_t(1));break;
 case 10:f.put(Fixture::a+0x168E,std::uint8_t(0));break;case 11:f.put(f.bone(Fixture::a,17)+0xA0,std::uint32_t((17u<<24)|(1u<<16)|(16u<<8)));break;
 case 12:f.put(f.bone(Fixture::a,19)+0xA0,std::uint32_t((19u<<24)|(17u<<8)));break;case 13:frame.frame_step=std::numeric_limits<float>::quiet_NaN();break;
 case 14:frame.frame_step=9;break;case 15:frame.world_x=2;break;case 16:f.sample_bad=true;break;
 case 17:f.put(Fixture::tracks,std::uint32_t(0xFF000004));break;case 18:frame.settings.move_while_aiming=false;break;
 case 19:f.put(Fixture::a+0x1EB8,std::int16_t(25));break;case 20:f.put(Fixture::parameters+15*0x50+0x2C,std::uint32_t(3));break;
 case 21:f.put(Fixture::a+0x2708,std::uintptr_t(1));break;
 }auto before=f.regions;f.writes=0;assert(!gait.prepare_proposal(f.backend(),frame,Fixture::proposal));assert(f.regions==before&&f.writes==0);}
}
void rejects_unwritable_and_rolls_back_partial_pose(){Fixture f;re5::GaitAdapter gait;auto frame=f.frame();f.denied=f.bone(Fixture::a,23)+0x90;auto before=f.regions;assert(!gait.prepare_proposal(f.backend(),frame,Fixture::proposal));assert(f.regions==before);f.denied=0;assert(gait.prepare_proposal(f.backend(),frame,Fixture::proposal));before=f.regions;f.fail_write=f.bone(Fixture::a,19)+0x70;assert(!gait.overlay_pose(f.backend(),frame));assert(f.regions==before);}
void resource_change_and_scope_interrupt_cancel_pending_pose(){for(unsigned which=0;which<2;++which){Fixture f;re5::GaitAdapter gait;auto frame=f.frame();assert(gait.prepare_proposal(f.backend(),frame,Fixture::proposal));if(which)f.put(Fixture::a+0x3AD8,std::uint32_t(1));else f.put(Fixture::clip+12,std::uint32_t(5));auto before=f.regions;assert(!gait.overlay_pose(f.backend(),frame));assert(f.regions==before);}}
void both_actors_keep_independent_phases(){Fixture f;re5::GaitAdapter gait;auto first=f.frame(),second=f.frame(Fixture::b);assert(gait.prepare_proposal(f.backend(),first,Fixture::proposal));assert(gait.overlay_pose(f.backend(),first));first.frame=2;assert(gait.prepare_proposal(f.backend(),first,Fixture::proposal));assert(gait.overlay_pose(f.backend(),first));assert(gait.prepare_proposal(f.backend(),second,Fixture::proposal));assert(gait.overlay_pose(f.backend(),second));auto p=f.get<re5::GaitValue>(Fixture::proposal);assert(near(p.x,17.8f)&&near(p.z,40.4f));}
void missing_pose_resets_next_frame_phase(){Fixture f;re5::GaitAdapter gait;auto frame=f.frame();assert(gait.prepare_proposal(f.backend(),frame,Fixture::proposal));frame.frame=2;assert(gait.prepare_proposal(f.backend(),frame,Fixture::proposal));auto p=f.get<re5::GaitValue>(Fixture::proposal);assert(near(p.x,14.8f)&&near(p.z,36.4f));}
void all_directions_use_native_clip_and_ground_proposal(){
 for(auto direction:{re5::GaitDirection::Forward,re5::GaitDirection::Backward,re5::GaitDirection::Left,re5::GaitDirection::Right}){
  Fixture f;re5::GaitAdapter gait;auto frame=f.frame();frame.direction=direction;
  // Only the native selected clip is present: a wrong recipe cannot succeed.
  for(unsigned i:{1u,7u,12u,13u})f.put(Fixture::table+8+i*8,std::uintptr_t(0));
  unsigned index=direction==re5::GaitDirection::Forward?1:direction==re5::GaitDirection::Backward?7:direction==re5::GaitDirection::Left?12:13;
  f.put(Fixture::table+8+index*8,Fixture::clip);assert(gait.prepare_proposal(f.backend(),frame,Fixture::proposal));assert(gait.overlay_pose(f.backend(),frame));
 }
}
void mirrored_sheva_uses_source_pair_and_native_conversion(){
 Fixture f;re5::GaitAdapter gait;auto frame=f.frame(Fixture::b);frame.direction=re5::GaitDirection::Left;
 f.put(Fixture::b+0x2701,std::uint8_t(1));
 constexpr std::uintptr_t model=0x66000000,bind=0x67000000,inverse=0x68000000;
 f.add(model,0x100);f.add(bind,147*0x40);f.add(inverse,147*0x40);f.put(Fixture::b+0x118,model);f.put(model+0x80,bind);f.put(model+0x88,inverse);
 for(unsigned id=15;id<=23;++id)f.put(f.bone(Fixture::b,id)+0xA4,std::uint8_t(f.model_index(Fixture::b,id==15?15:id<20?id+4:id-4)));
 f.put(Fixture::table+8+12*8,std::uintptr_t(0)); // Hand1 native left uses D, not C.
 assert(gait.prepare_proposal(f.backend(),frame,Fixture::proposal));assert(gait.overlay_pose(f.backend(),frame));
 const auto hip=f.get<re5::GaitValue>(f.bone(Fixture::b,16)+0x70),toe=f.get<re5::GaitValue>(f.bone(Fixture::b,19)+0x90);
 assert(near(hip.x,-.20f)&&hip.z>.5f&&toe.x==-23);
}
void compressed_tracks_validate_duration_and_sentinel_before_sampling(){
 for(unsigned malformed=0;malformed<5;++malformed){Fixture f;re5::GaitAdapter gait;auto frame=f.frame();constexpr std::uintptr_t keys=0x69000000,quant=0x69001000;
  f.add(keys,16);f.add(quant,32);f.put(Fixture::tracks,std::uint32_t(0xFF000404));f.put(Fixture::tracks+8,std::uint32_t(16));f.put(Fixture::tracks+0x10,keys);f.put(Fixture::tracks+0x28,quant);f.put(keys+6,std::uint16_t(3));
  if(malformed==1){f.put(keys+6,std::uint16_t(0));}
  if(malformed==2){f.put(keys+14,std::uint16_t(1));}
  if(malformed==3){f.put(Fixture::tracks+8,std::uint32_t(15));}
  if(malformed==4){f.put(Fixture::tracks+0x28,std::uintptr_t(0));}
  auto before=f.regions;const bool applied=gait.prepare_proposal(f.backend(),frame,Fixture::proposal);assert(applied==(malformed==0));if(!applied)assert(f.regions==before);
 }
}
void controller_reassignment_resets_phase(){Fixture f;re5::GaitAdapter gait;auto frame=f.frame();assert(gait.prepare_proposal(f.backend(),frame,Fixture::proposal));assert(gait.overlay_pose(f.backend(),frame));f.put(Fixture::pads+0x8C98,std::uint32_t(1));frame.frame=2;assert(gait.prepare_proposal(f.backend(),frame,Fixture::proposal));auto p=f.get<re5::GaitValue>(Fixture::proposal);assert(near(p.x,14.8f)&&near(p.z,36.4f));}
void analog_magnitude_scales_gait_phase_with_root_motion(){Fixture f;re5::GaitAdapter gait;auto frame=f.frame();frame.world_x=.3f;frame.world_z=.4f;assert(gait.prepare_proposal(f.backend(),frame,Fixture::proposal));assert(gait.overlay_pose(f.backend(),frame));auto p=f.get<re5::GaitValue>(Fixture::proposal),hip=f.get<re5::GaitValue>(f.bone(Fixture::a,16)+0x70);assert(near(p.x,11.2f)&&near(p.z,31.6f));assert(near(hip.y,.005f));}
void action_switch_resets_phase(){Fixture f;re5::GaitAdapter gait;auto frame=f.frame();assert(gait.prepare_proposal(f.backend(),frame,Fixture::proposal));assert(gait.overlay_pose(f.backend(),frame));f.put(Fixture::a+0x1B40,std::uint32_t(0x1F));frame.settings.move_while_reloading=true;frame.frame=2;assert(gait.prepare_proposal(f.backend(),frame,Fixture::proposal));auto p=f.get<re5::GaitValue>(Fixture::proposal);assert(near(p.x,14.8f)&&near(p.z,36.4f));}
void pending_pose_rechecks_controller_ownership(){Fixture f;re5::GaitAdapter gait;auto frame=f.frame();assert(gait.prepare_proposal(f.backend(),frame,Fixture::proposal));f.put(Fixture::pads+0x8C98,std::uint32_t(1));auto before=f.regions;assert(!gait.overlay_pose(f.backend(),frame));assert(f.regions==before);}
void native_disabled_pose_channel_is_not_overwritten(){Fixture f;re5::GaitAdapter gait;auto frame=f.frame();auto meta=f.get<std::uint32_t>(f.bone(Fixture::a,16)+0xA0);f.put(f.bone(Fixture::a,16)+0xA0,meta|1);auto before=f.regions;assert(!gait.prepare_proposal(f.backend(),frame,Fixture::proposal));assert(f.regions==before);}
void native_scene_remaps_fail_closed(){
 for(std::uint32_t room:{0x2D00u,0x2D50u,0x2D63u,0x2D64u,0x2D6Au,0x2D72u,0x2D73u}){
  Fixture f;re5::GaitAdapter gait;auto frame=f.frame();constexpr std::uintptr_t slot=0x71000000,owner=0x72000000,context=0x73000000,stage=0x74000000;
  f.put(Fixture::a+0x29C8,std::uint8_t(0));f.add(Fixture::main+0x1CD9FC0,8);f.add(Fixture::main+0x1CF7A40,8);f.add(slot,8);f.add(owner+0x584E0,8);f.add(context,0x100);f.add(stage,0x2B000);
  f.put(Fixture::main+0x1CD9FC0,slot);f.put(slot,owner);f.put(owner+0x584E0,context);f.put(Fixture::main+0x1CF7A40,stage);f.put(stage+0x2AF00,room);
  auto before=f.regions;bool applied=gait.prepare_proposal(f.backend(),frame,Fixture::proposal);bool blocked=(room>=0x2D50&&room<=0x2D63)||(room>=0x2D6A&&room<=0x2D72);
  assert(applied!=blocked);if(blocked)assert(f.regions==before);
 }
}
void degenerate_sample_quaternion_cannot_move_actor(){Fixture f;re5::GaitAdapter gait;auto frame=f.frame();f.quat_bad=true;auto before=f.regions;assert(!gait.prepare_proposal(f.backend(),frame,Fixture::proposal));assert(f.regions==before);}
void changed_native_track_header_cancels_pending_pose(){Fixture f;re5::GaitAdapter gait;auto frame=f.frame();assert(gait.prepare_proposal(f.backend(),frame,Fixture::proposal));f.put(Fixture::tracks+0x10,std::uintptr_t(0x12340000));auto before=f.regions;assert(!gait.overlay_pose(f.backend(),frame));assert(f.regions==before);}
void unsupported_reciprocal_mirror_pair_never_writes(){
 Fixture f;re5::GaitAdapter gait;auto frame=f.frame(Fixture::b);f.put(Fixture::b+0x2701,std::uint8_t(1));
 constexpr std::uintptr_t model=0x66000000,bind=0x67000000,inverse=0x68000000;
 f.add(model,0x100);f.add(bind,147*0x40);f.add(inverse,147*0x40);f.put(Fixture::b+0x118,model);f.put(model+0x80,bind);f.put(model+0x88,inverse);
 for(unsigned id=15;id<=23;++id){const unsigned pair=id==16?19:id==19?16:id;f.put(f.bone(Fixture::b,id)+0xA4,std::uint8_t(f.model_index(Fixture::b,pair)));}
 auto before=f.regions;f.writes=0;assert(!gait.prepare_proposal(f.backend(),frame,Fixture::proposal));assert(f.writes==0&&f.regions==before);
}
void diagonal_blends_normalized_phase_pose_and_root_budgets(){
 Fixture f;f.blend_clips();re5::GaitAdapter gait;auto frame=f.frame();frame.body_x=.70710678f;frame.body_forward=.70710678f;
 assert(gait.prepare_proposal(f.backend(),frame,Fixture::proposal));assert(gait.overlay_pose(f.backend(),frame));
 const auto p=f.get<re5::GaitValue>(Fixture::proposal),q=f.get<re5::GaitValue>(f.bone(Fixture::a,16)+0x70),toe=f.get<re5::GaitValue>(f.bone(Fixture::a,19)+0x90);
 // Periods3 and6, normalized phase .25 => source frames .75 and1.5.
 // Root budgets3 and6 blend to4.5; hemisphere-correct normalized q=(0,0,.4472136,.8944272).
 assert(near(p.x,12.7f)&&near(p.z,33.6f));assert(near(q.z,.4472136f)&&near(q.w,.8944272f));assert(near(toe.x,20.75f));
}
void cardinal_direction_change_preserves_normalized_phase(){
 Fixture f;f.blend_clips();re5::GaitAdapter gait;auto frame=f.frame();assert(gait.prepare_proposal(f.backend(),frame,Fixture::proposal));assert(gait.overlay_pose(f.backend(),frame));
 frame.frame=2;frame.direction=re5::GaitDirection::Right;assert(gait.prepare_proposal(f.backend(),frame,Fixture::proposal));assert(gait.overlay_pose(f.backend(),frame));
 // Phase1/3 advances by1/6, so right endpoint samples frame3, rather than restarting at1.
 assert(near(f.get<re5::GaitValue>(f.bone(Fixture::a,19)+0x90).x,33));
 const auto p=f.get<re5::GaitValue>(Fixture::proposal);assert(near(p.x,14.8f)&&near(p.z,36.4f));
}
void blended_period_wrap_preserves_each_endpoint_cycle(){
 Fixture f;f.blend_clips();re5::GaitAdapter gait;auto frame=f.frame();frame.body_x=frame.body_forward=.70710678f;frame.frame_step=3;
 assert(gait.prepare_proposal(f.backend(),frame,Fixture::proposal));assert(gait.overlay_pose(f.backend(),frame));frame.frame=2;frame.frame_step=1;
 assert(gait.prepare_proposal(f.backend(),frame,Fixture::proposal));assert(gait.overlay_pose(f.backend(),frame));
 const auto p=f.get<re5::GaitValue>(Fixture::proposal);assert(near(p.x,20.8f)&&near(p.z,44.4f));
}
void both_blend_sources_revalidate_before_any_write(){
 for(unsigned failure=0;failure<2;++failure){Fixture f;f.blend_clips();re5::GaitAdapter gait;auto frame=f.frame();frame.body_x=frame.body_forward=.70710678f;
  if(failure==0){f.put(Fixture::alternate_tracks,std::uint32_t(0xFF000004));auto before=f.regions;f.writes=0;assert(!gait.prepare_proposal(f.backend(),frame,Fixture::proposal));assert(f.writes==0&&f.regions==before);}
  else{assert(gait.prepare_proposal(f.backend(),frame,Fixture::proposal));f.put(Fixture::alternate_tracks+0x10,std::uintptr_t(0x12340000));auto before=f.regions;f.writes=0;assert(!gait.overlay_pose(f.backend(),frame));assert(f.writes==0&&f.regions==before);}
 }
}
void invalid_body_components_fail_without_mutation(){
 for(unsigned invalid=0;invalid<3;++invalid){Fixture f;re5::GaitAdapter gait;auto frame=f.frame();frame.body_x=invalid==0?std::numeric_limits<float>::quiet_NaN():invalid==1?std::numeric_limits<float>::infinity():1.f;frame.body_forward=1.f;
  const auto before=f.regions;f.writes=0;assert(!gait.prepare_proposal(f.backend(),frame,Fixture::proposal));assert(f.writes==0&&f.regions==before);
 }
}
void blended_proposal_failure_restores_both_horizontal_coordinates(){Fixture f;f.blend_clips();re5::GaitAdapter gait;auto frame=f.frame();frame.body_x=frame.body_forward=.70710678f;f.fail_write=Fixture::proposal+8;auto before=f.regions;assert(!gait.prepare_proposal(f.backend(),frame,Fixture::proposal));assert(f.regions==before);}
void blend_source_change_resets_shared_phase(){
 Fixture f;f.blend_clips();re5::GaitAdapter gait;auto frame=f.frame();frame.body_x=frame.body_forward=.70710678f;
 assert(gait.prepare_proposal(f.backend(),frame,Fixture::proposal));assert(gait.overlay_pose(f.backend(),frame));
 // A harmless constant-track payload replacement still invalidates cached phase.
 f.put(Fixture::alternate_tracks+0x18,1.f);frame.frame=2;
 assert(gait.prepare_proposal(f.backend(),frame,Fixture::proposal));assert(gait.overlay_pose(f.backend(),frame));
 assert(near(f.get<re5::GaitValue>(f.bone(Fixture::a,19)+0x90).x,20.75f));
}
void diagonal_back_and_left_select_both_native_endpoints(){
 Fixture f;re5::GaitAdapter gait;auto frame=f.frame();frame.body_x=-.6f;frame.body_forward=-.8f;
 f.put(Fixture::table+8+1*8,std::uintptr_t(0));f.put(Fixture::table+8+13*8,std::uintptr_t(0));
 assert(gait.prepare_proposal(f.backend(),frame,Fixture::proposal));assert(gait.overlay_pose(f.backend(),frame));
}
void mirrored_diagonal_preserves_translation_ownership(){
 Fixture f;f.blend_clips();re5::GaitAdapter gait;auto frame=f.frame(Fixture::b);frame.body_x=-.70710678f;frame.body_forward=.70710678f;
 f.put(Fixture::b+0x2701,std::uint8_t(1));constexpr std::uintptr_t model=0x66000000,bind=0x67000000,inverse=0x68000000;
 f.add(model,0x100);f.add(bind,147*0x40);f.add(inverse,147*0x40);f.put(Fixture::b+0x118,model);f.put(model+0x80,bind);f.put(model+0x88,inverse);
 for(unsigned id=15;id<=23;++id)f.put(f.bone(Fixture::b,id)+0xA4,std::uint8_t(f.model_index(Fixture::b,id==15?15:id<20?id+4:id-4)));
 const auto hip_translation=f.get<re5::GaitValue>(f.bone(Fixture::b,16)+0x90);const auto hip_meta=f.get<std::uint64_t>(f.bone(Fixture::b,16)+0xA0);
 assert(gait.prepare_proposal(f.backend(),frame,Fixture::proposal));assert(gait.overlay_pose(f.backend(),frame));
 assert(near(f.get<re5::GaitValue>(f.bone(Fixture::b,19)+0x90).x,-20.75f));
 const auto untouched=f.get<re5::GaitValue>(f.bone(Fixture::b,16)+0x90);assert(std::memcmp(&untouched,&hip_translation,16)==0);assert(f.get<std::uint64_t>(f.bone(Fixture::b,16)+0xA0)==hip_meta);
}
void blended_players_have_independent_phase_and_analog_cadence(){
 Fixture f;f.blend_clips();re5::GaitAdapter gait;auto first=f.frame(),second=f.frame(Fixture::b);first.body_x=first.body_forward=second.body_x=second.body_forward=.70710678f;
 assert(gait.prepare_proposal(f.backend(),first,Fixture::proposal));assert(gait.overlay_pose(f.backend(),first));first.frame=2;
 assert(gait.prepare_proposal(f.backend(),first,Fixture::proposal));assert(gait.overlay_pose(f.backend(),first));
 second.world_x=.3f;second.world_z=.4f;assert(gait.prepare_proposal(f.backend(),second,Fixture::proposal));assert(gait.overlay_pose(f.backend(),second));
 assert(near(f.get<re5::GaitValue>(f.bone(Fixture::a,19)+0x90).x,21.5f));assert(near(f.get<re5::GaitValue>(f.bone(Fixture::b,19)+0x90).x,20.375f));
}
void native_byte_flag_ignores_adjacent_fields(){
 for(unsigned offset=1;offset<8;++offset){Fixture f;re5::GaitAdapter gait;auto frame=f.frame();
  f.put(Fixture::a+0x2708,std::uint8_t(0));f.put(Fixture::a+0x2708+offset,std::uint8_t(0xA5));
  assert(gait.prepare_proposal(f.backend(),frame,Fixture::proposal));assert(gait.overlay_pose(f.backend(),frame));
 }
 Fixture f;re5::GaitAdapter gait;auto frame=f.frame();f.put(Fixture::a+0x2708,std::uint8_t(1));
 auto before=f.regions;f.writes=0;assert(!gait.prepare_proposal(f.backend(),frame,Fixture::proposal));assert(f.regions==before&&f.writes==0);
}

}
void native_binder_order_accepts_explicit_ankle_zero(){
 // Actual lower-channel order from verified Chris/Sheva default weapon LMTs.
 // DD7E6C replaces the solver even with0 (except special solver2), while
 // earlier hip42 setup temporarily assigns child knee2/ankle19.
 constexpr unsigned ids[]={19,19,20,16,15,18,23,23,22};
 constexpr unsigned solvers[]={0,0,42,42,0,0,0,0,0};
 Fixture f;
 for(auto actor:{Fixture::a,Fixture::b}){
  for(unsigned i=0;i<9;++i){const auto at=f.bone(actor,ids[i])+0xA0;auto meta=f.get<std::uint32_t>(at);
   if(solvers[i]!=2){meta=(meta&0xFF00FFFFu)|(solvers[i]<<16);f.put(at,meta);}
   if(((meta>>16)&255)==42){for(unsigned child=1;child<=2;++child){const auto dest=f.bone(actor,ids[i]+child)+0xA0;auto current=f.get<std::uint32_t>(dest);f.put(dest,(current&0xFF00FFFFu)|((child==1?2u:19u)<<16));}}
  }
  assert(((f.get<std::uint32_t>(f.bone(actor,18)+0xA0)>>16)&255)==0);
  assert(((f.get<std::uint32_t>(f.bone(actor,22)+0xA0)>>16)&255)==0);
  re5::GaitAdapter gait;auto frame=f.frame(actor);frame.settings.move_while_aiming=frame.settings.move_while_knife=frame.settings.move_while_reloading=true;
  for(unsigned action:{0x1Bu,0x1Cu,0x1Du,0x1Eu,0x1Fu,0x22u,0x23u,0x24u,0x25u,0x26u,0x27u}){
   f.put(actor+0x1B40,action);assert(gait.prepare_proposal(f.backend(),frame,Fixture::proposal));assert(gait.overlay_pose(f.backend(),frame));++frame.frame;
  }
 }
}
void other_ankle_solver_kinds_still_fail_closed(){
 for(unsigned kind:{1u,2u,3u,18u,20u,42u,255u}){
  Fixture f;re5::GaitAdapter gait;auto frame=f.frame();const auto at=f.bone(Fixture::a,18)+0xA0;
  auto meta=f.get<std::uint32_t>(at);f.put(at,(meta&0xFF00FFFFu)|(kind<<16));const auto before=f.regions;f.writes=0;
  assert(!gait.prepare_proposal(f.backend(),frame,Fixture::proposal)&&f.writes==0&&f.regions==before);
 }
}
void static_item_category_controls_movement_without_inventory_flag_aliasing(){
 // Literal IDs/categories from the native initializer:106/107/108 category3,
 // 10F/115 category1 and knives101/10E category2. Inventory+2C is flags.
 for(unsigned id:{0x106u,0x107u,0x108u}){
  Fixture f;re5::GaitAdapter gait;f.put(Fixture::wa+0x1108,id);const auto before=f.regions;f.writes=0;
  assert(!gait.prepare_proposal(f.backend(),f.frame(),Fixture::proposal));assert(f.writes==0&&f.regions==before);
 }
 for(unsigned id:{0x10Fu,0x115u,0x101u,0x10Eu})for(unsigned flags:{3u,4u,0x100u}){
  Fixture f;re5::GaitAdapter gait;f.put(Fixture::wa+0x1108,id);f.put(Fixture::wa+0x112C,flags);
  assert(gait.prepare_proposal(f.backend(),f.frame(),Fixture::proposal));assert(gait.overlay_pose(f.backend(),f.frame()));
 }
 // Preserve category4 rejection even though this build has no initialized
 // group1 category4 item; a changed static parameter must still fail closed.
 Fixture f;re5::GaitAdapter gait;f.put(Fixture::parameters+15*0x50+0x2C,std::uint32_t(4));const auto before=f.regions;f.writes=0;
 assert(!gait.prepare_proposal(f.backend(),f.frame(),Fixture::proposal));assert(f.writes==0&&f.regions==before);
}
void missing_malformed_or_unsupported_item_parameters_never_write(){
 for(unsigned bad=0;bad<12;++bad){Fixture f;re5::GaitAdapter gait;
  switch(bad){
  case 0:f.regions.erase(Fixture::main+0x1AAEF28);break;
  case 1:f.put(Fixture::main+0x1AAEF28,std::uintptr_t(0));break;
  case 2:f.put(Fixture::main+0x1AAEF28,std::uintptr_t(0x12345678));break;
  case 3:f.put(Fixture::main+0x1AAEF28,UINTPTR_MAX-0x20);break;
  case 4:f.put(Fixture::main+0x1AAEF28,Fixture::parameters+0x50);break;
  case 5:f.regions[Fixture::parameters].resize(0x2CFF);break;
  case 6:f.regions.erase(Fixture::parameters);break;
  case 7:f.put(Fixture::wa+0x1108,std::uint32_t(1));break;
  case 8:f.put(Fixture::wa+0x1108,std::uint32_t(0x190));break;
  case 9:f.put(Fixture::wa+0x1108,std::uint32_t(0x1010F));break;
  case 10:f.put(Fixture::wa+0x1108,std::uint32_t(0x200));break;
  case 11:f.put(Fixture::wa+0x1108,std::uint32_t(0x10A));break;
  }
  const auto before=f.regions;f.writes=0;assert(!gait.prepare_proposal(f.backend(),f.frame(),Fixture::proposal));assert(f.writes==0&&f.regions==before);
 }
}
void static_category_changes_cancel_pending_lower_pose(){
 for(unsigned category:{3u,4u}){Fixture f;re5::GaitAdapter gait;auto frame=f.frame();assert(gait.prepare_proposal(f.backend(),frame,Fixture::proposal));
  f.put(Fixture::parameters+15*0x50+0x2C,category);const auto before=f.regions;f.writes=0;
  assert(!gait.overlay_pose(f.backend(),frame));assert(f.writes==0&&f.regions==before);
 }
}
void ordinary_native_evaluator_values_allow_all_combat_actions(){
 // Native reset 0x7B9810 writes 0x1B10=0; the zero branch 0x794E34 performs
 // ordinary motion setup then rejoins animation at 0x794E38. Initial value 1
 // bypasses that setup and reaches the same animation/pose stage.
 for(std::uint8_t evaluator:{std::uint8_t(0),std::uint8_t(1)})for(auto actor:{Fixture::a,Fixture::b}){
  Fixture f;re5::GaitAdapter gait;auto frame=f.frame(actor);
  f.put(actor+0x1B10,evaluator);
  frame.settings.move_while_aiming=frame.settings.move_while_knife=frame.settings.move_while_reloading=true;
  for(unsigned action:{0x1Bu,0x1Cu,0x1Du,0x1Eu,0x1Fu,0x22u,0x23u,0x24u,0x25u,0x26u,0x27u}){
   f.put(actor+0x1B40,action);
   const auto before=f.get<re5::GaitValue>(Fixture::proposal);
   assert(gait.prepare_proposal(f.backend(),frame,Fixture::proposal));
   const auto moved=f.get<re5::GaitValue>(Fixture::proposal);
   assert((moved.x!=before.x||moved.z!=before.z)&&moved.y==before.y&&moved.w==before.w);
   assert(gait.overlay_pose(f.backend(),frame));
   assert(f.get<std::uint8_t>(actor+0x1B10)==evaluator);++frame.frame;
  }
 }
}
void native_reload_action_flag_allows_only_enabled_reload(){
 // Reload init 0x971D18 sets DWORD 0x3F0C bit16, hence byte 0x3F0E bit0.
 // Hardware action 0x1F carries this flag while every other gait gate passes.
 for(auto actor:{Fixture::a,Fixture::b}){
  for(unsigned flags:{1u,3u,9u,0x81u,0xFFu}){
   Fixture f;re5::GaitAdapter gait;auto frame=f.frame(actor);frame.settings.move_while_reloading=true;
   f.put(actor+0x1B40,std::uint32_t(0x1F));f.put(actor+0x3F0E,std::uint8_t(flags));
   const auto before=f.get<re5::GaitValue>(Fixture::proposal);
   assert(gait.prepare_proposal(f.backend(),frame,Fixture::proposal));assert(gait.overlay_pose(f.backend(),frame));
   const auto moved=f.get<re5::GaitValue>(Fixture::proposal);
   assert((moved.x!=before.x||moved.z!=before.z)&&moved.y==before.y&&moved.w==before.w);
   assert(f.get<std::uint8_t>(actor+0x3F0E)==flags);
  }
  for(unsigned action:{0x1Bu,0x1Cu,0x1Du,0x1Eu,0x22u,0x23u,0x24u,0x25u,0x26u,0x27u}){
   Fixture f;re5::GaitAdapter gait;auto frame=f.frame(actor);
   frame.settings.move_while_aiming=frame.settings.move_while_knife=frame.settings.move_while_reloading=true;
   f.put(actor+0x1B40,action);f.put(actor+0x3F0E,std::uint8_t(1));const auto before=f.regions;f.writes=0;
   assert(!gait.prepare_proposal(f.backend(),frame,Fixture::proposal));assert(f.regions==before&&f.writes==0);
  }
  for(unsigned flags:{0u,1u,0xFFu}){
   Fixture f;re5::GaitAdapter gait;auto frame=f.frame(actor);frame.settings.move_while_reloading=false;
   f.put(actor+0x1B40,std::uint32_t(0x1F));f.put(actor+0x3F0E,std::uint8_t(flags));const auto before=f.regions;f.writes=0;
   assert(!gait.prepare_proposal(f.backend(),frame,Fixture::proposal));assert(f.regions==before&&f.writes==0);
  }
  for(unsigned flags:{0u,2u,8u,0x80u,0xFEu})for(unsigned action:{0x1Bu,0x1Cu,0x1Fu,0x22u,0x23u}){
   Fixture f;re5::GaitAdapter gait;auto frame=f.frame(actor);
   frame.settings.move_while_aiming=frame.settings.move_while_knife=frame.settings.move_while_reloading=true;
   f.put(actor+0x1B40,action);f.put(actor+0x3F0E,std::uint8_t(flags));
   assert(gait.prepare_proposal(f.backend(),frame,Fixture::proposal));assert(gait.overlay_pose(f.backend(),frame));
   assert(f.get<std::uint8_t>(actor+0x3F0E)==flags);
  }
 }
}
void reload_action_flag_retains_other_safety_gates(){
 for(unsigned bad=0;bad<10;++bad){Fixture f;re5::GaitAdapter gait;auto frame=f.frame();frame.settings.move_while_reloading=true;
  f.put(Fixture::a+0x1B40,std::uint32_t(0x1F));f.put(Fixture::a+0x3F0E,std::uint8_t(1));
  switch(bad){
  case 0:f.put(Fixture::a+0x1A71,std::uint8_t(1));break;
  case 1:f.put(Fixture::a+0x1B4C,std::uint8_t(1));break;
  case 2:f.put(Fixture::a+0x3AD8,std::uint8_t(1));break;
  case 3:f.put(Fixture::wa+0x1030,Fixture::b);break;
  case 4:f.put(Fixture::a+0x1EB8,std::int16_t(25));break;
  case 5:f.put(Fixture::a+0x2708,std::uint8_t(1));break;
  case 6:f.put(Fixture::a+0x1AB0,std::uint8_t(1));break;
  case 7:f.put(Fixture::a+0x15D0,std::uintptr_t(0));break;
  case 8:f.put(Fixture::parameters+15*0x50+0x2C,std::uint32_t(3));break;
  case 9:f.put(Fixture::a+0x1B60,std::uintptr_t(0));break;
  }
  const auto before=f.regions;f.writes=0;
  assert(!gait.prepare_proposal(f.backend(),frame,Fixture::proposal));assert(f.regions==before&&f.writes==0);
 }
}
int main(){native_reload_action_flag_allows_only_enabled_reload();reload_action_flag_retains_other_safety_gates();ordinary_native_evaluator_values_allow_all_combat_actions();static_item_category_controls_movement_without_inventory_flag_aliasing();missing_malformed_or_unsupported_item_parameters_never_write();static_category_changes_cancel_pending_lower_pose();native_binder_order_accepts_explicit_ankle_zero();other_ankle_solver_kinds_still_fail_closed();native_byte_flag_ignores_adjacent_fields();proposal_uses_native_root_delta();clip_wrap_uses_endpoint_difference();overlay_preserves_upper_bones_metadata_and_helpers();invalid_source_never_mutates();rejects_unwritable_and_rolls_back_partial_pose();resource_change_and_scope_interrupt_cancel_pending_pose();both_actors_keep_independent_phases();missing_pose_resets_next_frame_phase();all_directions_use_native_clip_and_ground_proposal();mirrored_sheva_uses_source_pair_and_native_conversion();compressed_tracks_validate_duration_and_sentinel_before_sampling();controller_reassignment_resets_phase();analog_magnitude_scales_gait_phase_with_root_motion();action_switch_resets_phase();pending_pose_rechecks_controller_ownership();native_disabled_pose_channel_is_not_overwritten();native_scene_remaps_fail_closed();degenerate_sample_quaternion_cannot_move_actor();changed_native_track_header_cancels_pending_pose();unsupported_reciprocal_mirror_pair_never_writes();diagonal_blends_normalized_phase_pose_and_root_budgets();cardinal_direction_change_preserves_normalized_phase();blended_period_wrap_preserves_each_endpoint_cycle();both_blend_sources_revalidate_before_any_write();invalid_body_components_fail_without_mutation();blended_proposal_failure_restores_both_horizontal_coordinates();blend_source_change_resets_shared_phase();diagonal_back_and_left_select_both_native_endpoints();mirrored_diagonal_preserves_translation_ownership();blended_players_have_independent_phase_and_analog_cadence();std::puts("RE5 gait: native reload flags, ordinary evaluator, binder replay and38 behavior groups PASS");}
