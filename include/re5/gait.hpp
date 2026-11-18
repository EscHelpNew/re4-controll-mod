#pragma once
#include "re5/native_input.hpp"
#include "re4/settings.hpp"
#include <array>
#include <cmath>
#include <cstring>
namespace re5 {
struct GaitValue {float x=0,y=0,z=0,w=1;};
static_assert(sizeof(GaitValue)==16,"Native HFA4/vector storage");
struct GaitBackend {
 MemoryReader memory;
 void* context=nullptr;
 bool (*writable)(void*,std::uintptr_t,std::size_t)=nullptr;
 bool (*write)(void*,std::uintptr_t,const void*,std::size_t)=nullptr;
 std::uintptr_t (*lookup)(void*,std::uintptr_t,unsigned)=nullptr;
 bool (*init)(void*,void*,std::uintptr_t)=nullptr;
 bool (*sample)(void*,void*,float,GaitValue&)=nullptr;
 std::uintptr_t (*weapon)(void*,std::uintptr_t)=nullptr;
 bool (*mirror)(void*,std::uintptr_t,std::uintptr_t,GaitValue*,GaitValue*)=nullptr;
};
enum class GaitDirection { Forward,Backward,Left,Right };
struct GaitFrame {
 std::uintptr_t main=0,actor=0,command=0;
 std::uint64_t frame=0,generation=0;
 float world_x=0,world_z=0,frame_step=0;
 GaitDirection direction=GaitDirection::Forward;
 re4::Settings settings;
 // Optional body-relative ground components: +X right, +forward forward.
 // Both zero retains the legacy cardinal direction field.
 float body_x=0,body_forward=0;
};
// Calls run on the game thread: proposal at 7A6F80, overlay BEFORE DD3054.
// The backend must preserve callback destinations when reporting success. A
// failed write can be partial; rollback therefore includes that destination.
class GaitAdapter {
 struct Channel {std::uintptr_t track=0,destination=0;std::uint32_t word=0;GaitValue value;};
 struct State {
  bool live=false,pending=false;std::uintptr_t actor=0,command=0,resource=0,clip=0,tracks=0,bones=0,map=0,root=0;
  std::uint64_t generation=0,frame=0,fingerprint=1469598103934665603ULL,topology_fingerprint=0;unsigned index=0,count=0,frames=0,action=0;std::uintptr_t action_object=0;std::uint8_t hand=0;ResolvedPad sticks,motion;float phase=0;
  std::array<Channel,9> channels{};unsigned channels_count=0;
 };
 struct Session {
  State identity;
  std::array<State,4> endpoints{};
  std::array<bool,4> known{};
  std::array<float,4> weights{};
  std::array<Channel,9> output{};
  float world_x=0,world_z=0,frame_step=0;
 };
 std::array<Session,2> states_{};
 static bool finite(float v){return std::isfinite(v)&&std::fabs(v)<=1000000.f;}
 static bool value_ok(GaitValue v){return finite(v.x)&&finite(v.y)&&finite(v.z)&&finite(v.w);}
 static bool quaternion_ok(GaitValue v){const float n=v.x*v.x+v.y*v.y+v.z*v.z+v.w*v.w;return value_ok(v)&&std::fabs(n-1.f)<=.01f;}
 static bool read_bytes(const GaitBackend& b,std::uintptr_t at,void* out,std::size_t n){return b.memory.range(at,n)&&b.memory.read&&b.memory.read(b.memory.context,at,out,n);}
 static bool can_write(const GaitBackend& b,std::uintptr_t at,std::size_t n){return b.write&&b.writable&&b.writable(b.context,at,n);}
 static bool fingerprint(const GaitBackend& b,State& s,std::uintptr_t at,std::size_t bytes){
  std::array<std::uint8_t,48> data{};if(bytes>data.size()||!read_bytes(b,at,data.data(),bytes))return false;
  for(std::size_t i=0;i<bytes;++i){s.fingerprint^=data[i];s.fingerprint*=1099511628211ULL;}return true;
 }
 static bool normal_scene(const MemoryReader& m,const GaitFrame& f){
  std::uint8_t override_scene=0;if(!m.get(f.actor,0x29C8,override_scene))return false;
  if(override_scene)return true; // Native 7AD308 bypasses scene substitutions.
  std::uintptr_t slot=0,owner=0,context=0,stage=0;std::uint32_t mode=0,room=0;std::int32_t index=-1;
  if(!m.get(f.main,0x1CD9FC0,slot)||!m.get(slot,0,owner)||!m.get(owner,0x584E0,context)||!m.get(context,0x64,mode))return false;
  if(mode)return true; // Native 7AD328 bypasses scene substitutions.
  if(!m.get(f.main,0x1CF7A40,stage)||!m.get(stage,0x2C,index)||index<0||index>63
   ||!m.get(stage,std::size_t(index)*0x5F40+0x2AF00,room))return false;
  // Conservatively reject both proven remap ranges, including the range whose
  // native branch also checks the actor descriptor. No virtual call is needed.
  return !(room>=0x2D50&&room<=0x2D63)&&!(room>=0x2D6A&&room<=0x2D72);
 }
 static bool gates(const GaitBackend& b,const GaitFrame& f,ResolvedPlayer& player){
  // Native 0x1B10 selects ordinary motion setup; zero still evaluates animation
  // and pose. It does not indicate whether combat gait may be overlaid.
  // Native reload also sets 3F0E bit0; bypass that action lock only for
  // explicitly enabled reload movement, retaining every other safety gate.
  auto& m=b.memory;std::uint16_t type=0;std::uint8_t frozen=1,pending=1,freeze=1,ik=0,injury=1;std::int8_t special=1;
  std::int16_t hp=0,maxhp=0;std::uint8_t scope=1,blocked=1,native_block=1;std::uint32_t action=0;std::uintptr_t current=0,owner=0;
  if(!f.generation||!normal_scene(m,f)||!resolve_player(m,f.main,f.actor,f.command,player)
   ||!m.get(f.actor,0x1B40,action)
   ||!m.get(f.actor,8,type)||type!=0x201||!m.get(f.actor,0x1A71,frozen)||frozen
   ||!m.get(f.actor,0x1B4C,pending)||pending||!m.get(f.actor,0x3F0E,freeze)
   ||((freeze&1)&&!(action==0x1F&&f.settings.move_while_reloading))
   ||!m.get(f.actor,0x3AD8,scope)||scope
   ||!m.get(f.actor,0x1AB0,blocked)||blocked||!m.get(f.actor,0x168E,ik)||!ik
   ||!m.get(f.actor,0x2708,native_block)||native_block
   ||!m.get(f.actor,0x1B60,current)||!current||!m.get(current,0x30,owner)||owner!=f.actor
   ||!m.get(f.actor,0x1EB8,hp)||!m.get(f.actor,0x1EBA,maxhp)||maxhp<=0||hp<=1||hp<=maxhp/4
   ||!m.get(f.actor,0x29B6,injury)||injury||!m.get(f.actor,0x29B8,special)||special>0)return false;
  const bool allowed=action==0x1F?f.settings.move_while_reloading:
   action>=0x22&&action<=0x27?f.settings.move_while_knife:
   action>=0x1B&&action<=0x1E?f.settings.move_while_aiming:false;
  if(!allowed||!b.weapon)return false;
  const auto weapon=b.weapon(b.context,f.actor);std::uint32_t id=0,category=0;std::uintptr_t parameters=0;
  // Native weapon resources use group1. Its144 item records occupy the exact
  // static array1CFC510..1CFF210 in both builds (RELA slot1AAEF28). Category
  // belongs to that parameter row; inventory+2C contains flags/validity bytes.
  if(!weapon||!m.get(weapon,8,id)||(id>>8)!=1||(id&255)>=0x90||id==0x10A
   ||f.main>UINTPTR_MAX-0x1CFC510||!m.get(f.main,0x1AAEF28,parameters)||parameters!=f.main+0x1CFC510
   ||!m.range(parameters,0x2D00)||!m.get(parameters,std::size_t(id&255)*0x50+0x2C,category)||category==3||category==4)return false;
  for(unsigned i=0;i<3;++i){float a=0,r=0;if(!m.get(f.actor,0x80+i*4,a)||a!=1.f||!m.get(f.actor,0x1570+i*4,r)||r!=1.f)return false;}
  return finite(f.world_x)&&finite(f.world_z)&&f.world_x*f.world_x+f.world_z*f.world_z>0
   &&f.world_x*f.world_x+f.world_z*f.world_z<=1.00001f&&std::isfinite(f.frame_step)&&f.frame_step>0&&f.frame_step<=8;
 }
 static bool topology(const GaitBackend& b,const GaitFrame& f,State& s){
  std::uint32_t count=0;if(!b.memory.get(f.actor,0x8C0,count)||count<9||count>255
   ||!b.memory.get(f.actor,0x8C8,s.bones)||!b.memory.range(s.bones,count*0xC0)
   ||!b.memory.get(f.actor,0x8D0,s.map)||!b.memory.range(s.map,256))return false;
  if(!fingerprint(b,s,f.actor+0x118,8))return false;
  std::array<std::uint8_t,24> indices{};
  for(unsigned id: {0u,15u,16u,17u,18u,19u,20u,21u,22u,23u}){
   if(!b.memory.get(s.map,id,indices[id])||indices[id]>=count)return false;
   for(unsigned other=0;other<id;++other)if((other==0||other>=15)&&indices[other]==indices[id])return false;
  }
  constexpr unsigned parents[]={0,15,16,17,18,15,20,21,22};
  for(unsigned id=15;id<=23;++id){std::uint32_t meta=0;if(!b.memory.get(s.bones+indices[id]*0xC0,0xA0,meta)
   ||(meta>>24)!=id||((meta>>8)&255)!=indices[parents[id-15]])return false;
   if(!fingerprint(b,s,s.bones+indices[id]*0xC0+0xA0,8))return false;
   const unsigned kind=(meta>>16)&255,required=id==16||id==20?42:id==17||id==21?2:0;
   if(required&&kind!=required)return false;
   // Hip42 binding temporarily assigns ankle19. The later explicit ankle
   // quaternion track has solver0 and the native binder replaces19 with0.
   // Both native end states retain the same authenticated hip/knee chain.
   if((id==18||id==22)&&kind!=0&&kind!=19)return false;
  }
  if(s.hand){
   std::uintptr_t model=0,bind=0,inverse=0;
   if(!b.mirror||!b.memory.get(f.actor,0x118,model)||!b.memory.get(model,0x80,bind)||!b.memory.get(model,0x88,inverse)
    ||!b.memory.range(bind,count*0x40)||!b.memory.range(inverse,count*0x40))return false;
   for(unsigned id=15;id<=23;++id){std::uint8_t mirror=255,source=255;
    if(!b.memory.get(s.bones+indices[id]*0xC0,0xA4,mirror)||mirror>=count
     ||!b.memory.get(s.bones+mirror*0xC0,0xA3,source)
     ||source!=(id==15?15:id<20?id+4:id-4)||mirror!=indices[source])return false;
   }
  }
  return true;
 }
 static bool track_safe(const GaitBackend& b,std::uintptr_t track,unsigned frames,std::uint32_t word){
  unsigned format=word&255;float influence=0;
  if(!b.memory.get(track,4,influence)||influence!=1.f)return false;
  // Native constants have no key buffer. Every component still gets checked
  // after sampling; production walk assets normally use compressed formats.
  if(format==1||format==2)return b.memory.range(track,0x30);
  const unsigned stride=format==4?8:format==7||format==11?4:format==14?6:format==15?5:0;
  std::uint32_t bytes=0;std::uintptr_t keys=0,quant=0;
  if(!stride||!b.memory.get(track,8,bytes)||bytes<stride*2||bytes>0x100000||bytes%stride
   ||!b.memory.get(track,0x10,keys)||!b.memory.range(keys,bytes)
   ||!b.memory.get(track,0x28,quant)||!b.memory.range(quant,32))return false;
  for(unsigned i=0;i<8;++i){float parameter=0;if(!b.memory.get(quant,i*4,parameter)||!finite(parameter))return false;}
  unsigned duration=0,total=0;
  for(unsigned offset=0;offset<bytes;offset+=stride){
   if(format==4){std::uint16_t v=0;if(!b.memory.get(keys,offset+6,v))return false;duration=v;}
   else if(format==14){std::uint16_t v=0;if(!b.memory.get(keys,offset+4,v))return false;duration=v>>12;}
   else{std::uint32_t v=0;if(!b.memory.get(keys,offset+(format==15?1:0),v))return false;duration=v>>28;}
   if(offset+stride==bytes){if(duration)return false;}else if(!duration)return false;
   total+=duration;if(total>frames-1)return false;
  }
  return total==frames-1;
 }
 static bool sample(const GaitBackend& b,std::uintptr_t track,float frame,GaitValue& out){
  alignas(16) std::array<std::uint8_t,32> cursor{};
  return b.init&&b.sample&&b.init(b.context,cursor.data(),track)&&b.sample(b.context,cursor.data(),frame,out)&&value_ok(out);
 }
 static bool load(const GaitBackend& b,const GaitFrame& f,State& s,std::uintptr_t& root){
  if(!b.lookup||!b.memory.get(f.actor,0x15D0,s.resource)||!s.resource)return false;
  std::uintptr_t table=0;std::uint16_t count=0;
  if(!b.memory.get(s.resource,0x70,table)||!b.memory.get(table,6,count)||s.index>=count
   ||!b.memory.range(table+8,count*8))return false;
  std::uintptr_t expected=0;if(!b.memory.get(table,8+s.index*8,expected)||!expected)return false;
  s.clip=b.lookup(b.context,s.resource,s.index);if(s.clip!=expected
   ||!b.memory.range(s.clip,0x58)||!b.memory.get(s.clip,0,s.tracks)
   ||!b.memory.get(s.clip,8,s.count)||!s.count||s.count>512||!b.memory.get(s.clip,12,s.frames)
   ||s.frames<2||s.frames>65536||!b.memory.range(s.tracks,s.count*0x30)||!topology(b,f,s))return false;
  s.topology_fingerprint=s.fingerprint;
  std::array<bool,9> found{},destinations{};s.channels_count=0;root=0;
  constexpr unsigned ids[]={15,16,18,19,20,22,23,19,23};
  for(unsigned i=0;i<s.count;++i){const auto tr=s.tracks+i*0x30;std::uint32_t word=0;if(!b.memory.get(tr,0,word))return false;
   const unsigned id=word>>24,type=(word>>8)&255,solver=(word>>16)&255;
   if(type==4){if(id!=255||root||!track_safe(b,tr,s.frames,word)||!fingerprint(b,s,tr,0x30))return false;root=tr;continue;}
   for(unsigned j=0;j<9;++j)if(id==ids[j]&&type==(j<7?0u:1u)){
    if(found[j]||solver!=(id==16||id==20?42u:0u)||!track_safe(b,tr,s.frames,word)||!fingerprint(b,s,tr,0x30))return false;
    std::uint8_t mapped=255;if(!b.memory.get(s.map,id,mapped))return false;
    if(s.hand){ // Native mirrored evaluation samples the target's mirror source.
     // Invert the proven symmetric mirror pair to choose this source's target.
     std::uint8_t target=255;if(!b.memory.get(s.bones+mapped*0xC0,0xA4,target))return false;
     std::uint8_t reverse=255;if(!b.memory.get(s.bones+target*0xC0,0xA4,reverse)||reverse!=mapped)return false;
     mapped=target;
    }
    std::uint8_t target_id=255;
    if(!b.memory.get(s.bones+mapped*0xC0,0xA3,target_id))return false;
    unsigned destination_slot=9;for(unsigned k=0;k<9;++k)if(ids[k]==target_id&&type==(k<7?0u:1u))destination_slot=k;
    if(destination_slot==9||destinations[destination_slot])return false;
    std::uint8_t flags=0;if(!b.memory.get(s.bones+mapped*0xC0,0xA0,flags)||(flags&0x21))return false;
    found[j]=destinations[destination_slot]=true;++s.channels_count;
    s.channels[destination_slot]={tr,s.bones+mapped*0xC0+(type==0?0x70:0x90),word,{}};
   }
  }
  if(!root||s.channels_count!=9)return false;
  for(bool present:found)if(!present)return false;
  return true;
 }
 struct Change {std::uintptr_t at=0;std::size_t bytes=0;std::array<std::uint8_t,16> old{},next{};};
 static bool transaction(const GaitBackend& b,Change* changes,unsigned count){
  for(unsigned i=0;i<count;++i)if(!can_write(b,changes[i].at,changes[i].bytes)||!read_bytes(b,changes[i].at,changes[i].old.data(),changes[i].bytes))return false;
  for(unsigned i=0;i<count;++i)if(!b.write(b.context,changes[i].at,changes[i].next.data(),changes[i].bytes)){
   for(unsigned j=i+1;j>0;--j)b.write(b.context,changes[j-1].at,changes[j-1].old.data(),changes[j-1].bytes);
   return false;
  }
  return true;
 }
 static bool direction_weights(const GaitFrame& f,std::array<float,4>& weights){
  weights={};
  if(!std::isfinite(f.body_x)||!std::isfinite(f.body_forward)||std::hypot(f.body_x,f.body_forward)>1.00001f)return false;
  const float lateral=std::fabs(f.body_x),longitudinal=std::fabs(f.body_forward),sum=lateral+longitudinal;
  if(sum>0){
   weights[f.body_forward<0?1:0]=longitudinal/sum;
   weights[f.body_x<0?2:3]=lateral/sum;
   return true;
  }
  switch(f.direction){
   case GaitDirection::Forward:weights[0]=1;return true;
   case GaitDirection::Backward:weights[1]=1;return true;
   case GaitDirection::Left:weights[2]=1;return true;
   case GaitDirection::Right:weights[3]=1;return true;
  }
  return false;
 }
 static unsigned clip_index(unsigned endpoint,std::uint8_t hand){
  return endpoint==0?1:endpoint==1?7:endpoint==2?(hand?13:12):(hand?12:13);
 }
 static bool same_endpoint(const State& a,const State& b){
  if(a.fingerprint!=b.fingerprint||a.resource!=b.resource||a.clip!=b.clip||a.tracks!=b.tracks
   ||a.bones!=b.bones||a.map!=b.map||a.count!=b.count||a.frames!=b.frames||a.index!=b.index||a.root!=b.root)return false;
  for(unsigned i=0;i<9;++i)if(a.channels[i].track!=b.channels[i].track||a.channels[i].destination!=b.channels[i].destination||a.channels[i].word!=b.channels[i].word)return false;
  return true;
 }
 static bool sample_pose(const GaitBackend& b,const GaitFrame& f,State& endpoint,float normalized_phase){
  const float frame=normalized_phase*float(endpoint.frames-1);
  for(auto& channel:endpoint.channels){
   if(!can_write(b,channel.destination,16)||!sample(b,channel.track,frame,channel.value)
    ||(((channel.word>>8)&255)==0&&!quaternion_ok(channel.value)))return false;
  }
  if(endpoint.hand){
   for(unsigned i=0;i<7;++i){auto& channel=endpoint.channels[i];GaitValue* translation=nullptr;
    const auto bone=channel.destination-0x70;
    for(unsigned j=7;j<9;++j)if(endpoint.channels[j].destination==bone+0x90)translation=&endpoint.channels[j].value;
    if(!b.mirror(b.context,f.actor,bone,translation,&channel.value)||!quaternion_ok(channel.value)||(translation&&!value_ok(*translation)))return false;
   }
  }
  return true;
 }
 static bool root_budget(const GaitBackend& b,const State& endpoint,float before,float after,float wraps,float& distance){
  const float period=float(endpoint.frames-1);GaitValue begin,end,from,to;
  if(!sample(b,endpoint.root,0,begin)||!sample(b,endpoint.root,period,end)
   ||!sample(b,endpoint.root,before*period,from)||!sample(b,endpoint.root,after*period,to))return false;
  const float dx=to.x-from.x+wraps*(end.x-begin.x),dz=to.z-from.z+wraps*(end.z-begin.z);
  distance=std::hypot(dx,dz);return finite(distance)&&distance>0&&distance<=200;
 }
 static bool blend_pose(Session& next){
  unsigned first=0;while(first<4&&next.weights[first]<=0)++first;if(first==4)return false;
  for(unsigned channel=0;channel<9;++channel){
   next.output[channel]=next.endpoints[first].channels[channel];
   const auto reference=next.output[channel].value;GaitValue blended{0,0,0,0};
   for(unsigned endpoint=0;endpoint<4;++endpoint)if(next.weights[endpoint]>0){
    const auto& source=next.endpoints[endpoint].channels[channel];if(source.destination!=next.output[channel].destination)return false;
    auto value=source.value;const float dot=reference.x*value.x+reference.y*value.y+reference.z*value.z+reference.w*value.w;
    const float weight=next.weights[endpoint]*(channel<7&&dot<0?-1.f:1.f);
    blended.x+=weight*value.x;blended.y+=weight*value.y;blended.z+=weight*value.z;blended.w+=weight*value.w;
   }
   if(channel<7){
    const float norm=std::sqrt(blended.x*blended.x+blended.y*blended.y+blended.z*blended.z+blended.w*blended.w);
    if(!std::isfinite(norm)||norm<.00001f)return false;
    blended.x/=norm;blended.y/=norm;blended.z/=norm;blended.w/=norm;
    if(!quaternion_ok(blended))return false;
   }else if(!value_ok(blended))return false;
   next.output[channel].value=blended;
  }
  return true;
 }
 void invalidate(std::uintptr_t actor){for(auto& s:states_)if(s.identity.actor==actor)s={};}
public:
 void reset(){states_={};}
 bool prepare_proposal(const GaitBackend& b,const GaitFrame& f,std::uintptr_t proposal){
  ResolvedPlayer player;if(!gates(b,f,player)){invalidate(f.actor);return false;}
  auto& old=states_[player.local_player];const auto& prior=old.identity;
  if(prior.live&&prior.actor==f.actor&&prior.generation==f.generation&&f.frame<=prior.frame)return false;
  Session next;auto& identity=next.identity;
  if(!direction_weights(f,next.weights)||!b.memory.get(f.actor,0x1B40,identity.action)
   ||!b.memory.get(f.actor,0x1B60,identity.action_object)||!b.memory.get(f.actor,0x2701,identity.hand)||identity.hand>1){old={};return false;}
  identity.actor=f.actor;identity.command=f.command;identity.generation=f.generation;identity.frame=f.frame;
  identity.sticks=player.sticks;identity.motion=player.motion;
  bool continuation=prior.live&&!prior.pending&&prior.actor==identity.actor&&prior.command==identity.command&&prior.generation==identity.generation
   &&prior.action==identity.action&&prior.action_object==identity.action_object&&prior.hand==identity.hand
   &&prior.sticks.controller==identity.sticks.controller&&prior.sticks.style==identity.sticks.style
   &&prior.motion.controller==identity.motion.controller&&prior.motion.style==identity.motion.style&&f.frame==prior.frame+1;
  // Cached identities for inactive endpoints let a later return detect changed
  // resources, while normalized phase survives ordinary direction changes.
  if(continuation){next.endpoints=old.endpoints;next.known=old.known;}
  bool first=true;
  for(unsigned endpoint=0;endpoint<4;++endpoint)if(next.weights[endpoint]>0){
   State clip;clip.hand=identity.hand;clip.index=clip_index(endpoint,identity.hand);
   if(!load(b,f,clip,clip.root)){old={};return false;}
   if(first){identity.resource=clip.resource;identity.bones=clip.bones;identity.map=clip.map;identity.topology_fingerprint=clip.topology_fingerprint;first=false;}
   else if(identity.resource!=clip.resource||identity.bones!=clip.bones||identity.map!=clip.map||identity.topology_fingerprint!=clip.topology_fingerprint){old={};return false;}
   if(continuation&&next.known[endpoint]&&!same_endpoint(next.endpoints[endpoint],clip))continuation=false;
   next.endpoints[endpoint]=clip;next.known[endpoint]=true;
  }
  continuation=continuation&&prior.resource==identity.resource&&prior.bones==identity.bones&&prior.map==identity.map&&prior.topology_fingerprint==identity.topology_fingerprint;
  if(!continuation)for(unsigned endpoint=0;endpoint<4;++endpoint)if(next.weights[endpoint]<=0){next.known[endpoint]=false;next.endpoints[endpoint]={};}
  const float magnitude=std::hypot(f.world_x,f.world_z),before=continuation?prior.phase:0.f;
  float cycle_rate=0;
  for(unsigned endpoint=0;endpoint<4;++endpoint)if(next.weights[endpoint]>0)cycle_rate+=next.weights[endpoint]/float(next.endpoints[endpoint].frames-1);
  const float unwrapped=before+f.frame_step*magnitude*cycle_rate;
  identity.phase=std::fmod(unwrapped,1.f);const float wraps=std::floor(unwrapped);float distance=0;
  for(unsigned endpoint=0;endpoint<4;++endpoint)if(next.weights[endpoint]>0){
   float budget=0;
   if(!root_budget(b,next.endpoints[endpoint],before,identity.phase,wraps,budget)||!sample_pose(b,f,next.endpoints[endpoint],identity.phase)){old={};return false;}
   distance+=next.weights[endpoint]*budget;
  }
  if(!finite(distance)||distance<=0||distance>200||!blend_pose(next)){old={};return false;}
  float x=0,z=0,y=0;
  if(!b.memory.get(proposal,0,x)||!b.memory.get(proposal,4,y)||!b.memory.get(proposal,8,z)||!finite(x)||!finite(y)||!finite(z)){old={};return false;}
  x+=distance*(f.world_x/magnitude);z+=distance*(f.world_z/magnitude);if(!finite(x)||!finite(z)){old={};return false;}
  Change changes[2]{};changes[0].at=proposal;changes[1].at=proposal+8;
  for(auto& c:changes){c.bytes=4;}std::memcpy(changes[0].next.data(),&x,4);std::memcpy(changes[1].next.data(),&z,4);
  if(!transaction(b,changes,2)){old={};return false;}
  identity.live=identity.pending=true;next.world_x=f.world_x;next.world_z=f.world_z;next.frame_step=f.frame_step;old=next;return true;
 }
 bool overlay_pose(const GaitBackend& b,const GaitFrame& f){
  ResolvedPlayer player;if(!gates(b,f,player)){invalidate(f.actor);return false;}
  auto& session=states_[player.local_player];auto& s=session.identity;
  std::uint32_t action=0;std::uintptr_t current=0;std::uint8_t hand=255;std::array<float,4> weights{};
  if(!b.memory.get(f.actor,0x1B40,action)||!b.memory.get(f.actor,0x1B60,current)||!b.memory.get(f.actor,0x2701,hand)
   ||action!=s.action||current!=s.action_object||hand!=s.hand||!direction_weights(f,weights)
   ||player.sticks.controller!=s.sticks.controller||player.sticks.style!=s.sticks.style
   ||player.motion.controller!=s.motion.controller||player.motion.style!=s.motion.style){session={};return false;}
  if(!s.live||!s.pending||s.actor!=f.actor||s.command!=f.command||s.generation!=f.generation||s.frame!=f.frame)return false;
  if(weights!=session.weights||session.world_x!=f.world_x||session.world_z!=f.world_z||session.frame_step!=f.frame_step){session={};return false;}
  for(unsigned endpoint=0;endpoint<4;++endpoint)if(weights[endpoint]>0){
   State check;check.index=clip_index(endpoint,s.hand);check.hand=s.hand;
   if(!load(b,f,check,check.root)||!same_endpoint(check,session.endpoints[endpoint])){session={};return false;}
  }
  std::array<Change,9> changes{};
  for(unsigned i=0;i<9;++i){changes[i].at=session.output[i].destination;changes[i].bytes=16;std::memcpy(changes[i].next.data(),&session.output[i].value,16);}
  if(!transaction(b,changes.data(),9)){session={};return false;}s.pending=false;return true;
 }
};
}
