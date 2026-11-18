#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace re5_gait_test {
// Functional lower-channel metadata/order from the verified default Chris and
// Sheva weapon resources. Curves below are synthetic; no game asset is bundled.
inline constexpr unsigned ids[]{255,19,19,20,16,15,18,23,23,22};
inline constexpr unsigned channels[]{4,0,1,0,0,0,0,0,1,0};
inline constexpr unsigned solvers[]{0,0,0,42,42,0,0,0,0,0};
inline constexpr unsigned formats[]{4,14,4,15,15,7,11,14,4,15};
template<class T> inline void put(std::uintptr_t at,const T& value){std::memcpy(reinterpret_cast<void*>(at),&value,sizeof(value));}
template<class T> inline T get(std::uintptr_t at){T value{};std::memcpy(&value,reinterpret_cast<const void*>(at),sizeof(value));return value;}
inline std::uintptr_t address(std::vector<std::uint8_t>& data){return reinterpret_cast<std::uintptr_t>(data.data());}
inline unsigned model_index(unsigned player,unsigned logical){
 std::array<unsigned,147> map{};for(unsigned i=0;i<map.size();++i)map[i]=i;
 constexpr unsigned logical_ids[]{15,16,17,18,19,20,21,22,23,66,67,68,69};
 constexpr unsigned chris[]{111,112,113,114,115,117,119,120,121,123,122,116,118};
 constexpr unsigned sheva[]{1,2,3,4,5,8,9,10,11,7,13,6,12};
 for(unsigned i=0;i<std::size(logical_ids);++i){const unsigned desired=player?sheva[i]:chris[i];unsigned other=0;while(map[other]!=desired)++other;std::swap(map[logical_ids[i]],map[other]);}
 return map[logical];
}
struct Player {
 unsigned count;std::uintptr_t actor=0;
 std::vector<std::uint8_t> bones,map,action,weapon,model,bind,inverse,resource,table,clip,tracks,keys,parameters;
 explicit Player(unsigned n):count(n),bones(n*0xC0),map(256,255),action(0x40),weapon(0x40),model(0x100),bind(n*0x40),inverse(n*0x40),resource(0x80),table(8+116*8),clip(0x60),tracks(10*0x30),keys(10*0x40),parameters(10*32){}
 std::uintptr_t bone(unsigned player,unsigned logical){return address(bones)+model_index(player,logical)*0xC0;}
};
struct Assets {
 struct NativeActionFlags {std::uintptr_t actor;std::uint32_t action;bool owns_reload;};
 inline static std::array<NativeActionFlags,2> action_flags{};
 std::array<Player,2> players{Player(130),Player(147)};unsigned mirrors=0,samples=0,lookups=0;
 Assets(std::uintptr_t main,std::uintptr_t first,std::uintptr_t second){
  // Independently seeded native static item array; inventory records below
  // retain their real flags/valid-byte storage at2C, separate from categories.
  put(main+0x1AAEF28,main+0x1CFC510);
  put(main+0x1CFC510+15*0x50+0x2C,std::uint32_t(1));put(main+0x1CFC510+21*0x50+0x2C,std::uint32_t(1));
  put(main+0x1CFC510+1*0x50+0x2C,std::uint32_t(2));put(main+0x1CFC510+14*0x50+0x2C,std::uint32_t(2));
  for(unsigned id:{6u,7u,8u})put(main+0x1CFC510+id*0x50+0x2C,std::uint32_t(3));
  const std::uintptr_t actors[]{first,second};
  for(unsigned player=0;player<2;++player){auto& p=players[player];p.actor=actors[player];action_flags[player]={p.actor,UINT32_MAX,false};
   for(auto* data:{&p.bones,&p.map,&p.action,&p.weapon,&p.model,&p.bind,&p.inverse,&p.resource,&p.table,&p.clip,&p.tracks,&p.keys,&p.parameters})game_test::memory_maps.push_back({address(*data),data->size(),3});
   // Native ordinary combat action reset 0x7B9810 zeroes byte 0x1B10.
   put(p.actor+8,std::uint16_t(0x201));put(p.actor+0x1B10,std::uint8_t(0));put(p.actor+0x168E,std::uint8_t(1));put(p.actor+0x1AB0,std::uint8_t(0));put(p.actor+0x1B4C,std::uint8_t(0));put(p.actor+0x1A71,std::uint8_t(0));
   put(p.actor+0x1B60,address(p.action));put(address(p.action)+0x30,p.actor);put(p.actor+0x1EB8,std::int16_t(1000));put(p.actor+0x1EBA,std::int16_t(1000));put(p.actor+0x29B6,std::uint8_t(0));put(p.actor+0x29B8,std::int8_t(-1));
   put(p.actor+0x29C8,std::uint8_t(0));put(p.actor+0x2708,std::uint8_t(0));put(p.actor+0x2709,std::uint8_t(0xA5));put(p.actor+0x2700,std::uint8_t(player));put(p.actor+0x2701,std::uint8_t(player));put(p.actor+0x3AD8,std::uint8_t(0));
   put(p.actor+0x8C0,p.count);put(p.actor+0x8C8,address(p.bones));put(p.actor+0x8D0,address(p.map));put(p.actor+0x118,address(p.model));put(address(p.model)+0x80,address(p.bind));put(address(p.model)+0x88,address(p.inverse));put(p.actor+0x15D0,address(p.resource));
   for(unsigned i=0;i<3;++i){put(p.actor+0x80+i*4,1.f);put(p.actor+0x1570+i*4,1.f);}
   put(address(p.weapon)+8,std::uint32_t(0x10F));put(address(p.weapon)+0x2C,std::uint32_t(0x100));
   for(unsigned logical=0;logical<p.count;++logical){p.map[logical]=std::uint8_t(model_index(player,logical));const unsigned parent=logical==15?0:logical==16||logical==20?15:logical>=17&&logical<=23?logical-1:logical==0?255:0;
    const unsigned parent_index=parent==255?255:model_index(player,parent);const auto bone=p.bone(player,logical);
    put(bone+0xA0,std::uint32_t((logical<<24)|(parent_index<<8)));put(bone+0x70,re5::GaitValue{0,0,0,1});put(bone+0x90,re5::GaitValue{0,0,0,1});
    const auto index=model_index(player,logical);for(unsigned diagonal=0;diagonal<4;++diagonal){put(address(p.bind)+index*0x40+diagonal*20,1.f);put(address(p.inverse)+index*0x40+diagonal*20,1.f);}
   }
   for(unsigned logical=15;logical<=23;++logical){const unsigned mirror=logical==15?15:logical<20?logical+4:logical-4;put(p.bone(player,logical)+0xA4,std::uint8_t(model_index(player,mirror)));}
   put(address(p.resource)+0x70,address(p.table));put(address(p.table)+6,std::uint16_t(116));for(unsigned index:{1u,7u,12u,13u})put(address(p.table)+8+index*8,address(p.clip));
   put(address(p.clip),address(p.tracks));put(address(p.clip)+8,std::uint32_t(10));put(address(p.clip)+12,std::uint32_t(76));
   for(unsigned i=0;i<10;++i){const auto track=address(p.tracks)+i*0x30;const auto key=address(p.keys)+i*0x40;const auto parameter=address(p.parameters)+i*32;
    put(track,std::uint32_t((ids[i]<<24)|(solvers[i]<<16)|(channels[i]<<8)|formats[i]));put(track+4,1.f);
    const unsigned stride=formats[i]==4?8:formats[i]==14?6:formats[i]==15?5:4;const unsigned count=formats[i]==4?2:6;
    put(track+8,std::uint32_t(stride*count));put(track+0x10,key);put(track+0x28,parameter);for(unsigned j=0;j<4;++j)put(parameter+j*4,1.f);
    for(unsigned k=0;k<count;++k){const unsigned duration=k+1==count?0:formats[i]==4?75:15;
     if(formats[i]==4)put(key+k*stride+6,std::uint16_t(duration));else if(formats[i]==14)put(key+k*stride+4,std::uint16_t(duration<<12));else put(key+k*stride+(formats[i]==15?1:0),std::uint32_t(duration<<28));
    }
   }
   bind_lower_metadata(player);
  }
 }
 void bind_lower_metadata(unsigned player){auto& p=players[player];
  for(unsigned i=1;i<10;++i){const auto bone=p.bone(player,ids[i]);auto metadata=get<std::uint32_t>(bone+0xA0);
   if(solvers[i]!=2){metadata=(metadata&0xFF00FFFFu)|(solvers[i]<<16);put(bone+0xA0,metadata);}
   if(((metadata>>16)&255)==42)for(unsigned child=1;child<=2;++child){const auto at=p.bone(player,ids[i]+child)+0xA0;put(at,(get<std::uint32_t>(at)&0xFF00FFFFu)|((child==1?2u:19u)<<16));}
  }
 }
 static std::uintptr_t lookup(void* context,std::uintptr_t resource,unsigned index){auto& a=*static_cast<Assets*>(context);++a.lookups;const auto table=get<std::uintptr_t>(resource+0x70);return get<std::uintptr_t>(table+8+index*8);}
 static bool init(void*,void* cursor,std::uintptr_t track){std::memcpy(cursor,&track,sizeof(track));return true;}
 static bool sample(void* context,void* cursor,float frame,re5::GaitValue& out){auto& a=*static_cast<Assets*>(context);++a.samples;const auto track=get<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(cursor));const auto word=get<std::uint32_t>(track);const unsigned channel=(word>>8)&255,id=word>>24;
  if(channel==4){unsigned player=track>=address(a.players[1].tracks)&&track<address(a.players[1].tracks)+a.players[1].tracks.size()?1:0;out={0,0,frame*(player?140.4120483f:152.6217957f)/75,1};}
  else if(channel==0){const float angle=float(id)*.01f+frame*.001f;out={std::sin(angle),0,0,std::cos(angle)};}
  else out={float(id),frame*.01f,0,1};
  return true;
 }
 static std::uintptr_t weapon(void* context,std::uintptr_t actor){auto& a=*static_cast<Assets*>(context);for(auto& player:a.players)if(player.actor==actor)return address(player.weapon);return 0;}
 static bool mirror(void* context,std::uintptr_t,std::uintptr_t,re5::GaitValue* translation,re5::GaitValue* rotation){++static_cast<Assets*>(context)->mirrors;if(translation)translation->x=-translation->x;if(rotation)rotation->x=-rotation->x;return true;}
 static float frame_step(std::uintptr_t actor){
  // Test native action initialization before Proposal/Pose: reload 0x1F
  // sets 3F0C bit16 (971D18). Restore only the bit this fixture injected
  // on leaving reload; unrelated or pre-existing freeze flags stay native.
  // This boundary does not model native LocalInput initialization timing.
  for(auto& state:action_flags)if(state.actor==actor){
   const auto action=get<std::uint32_t>(actor+0x1B40);
   if(action!=state.action){
    const auto before=get<std::uint32_t>(actor+0x3F0C);auto flags=before;
    if(state.owns_reload)flags&=~0x10000u;
    state.owns_reload=action==0x1F&&!(flags&0x10000u);
    if(state.owns_reload)flags|=0x10000u;
    if(flags!=before)put(actor+0x3F0C,flags);
    state.action=action;
   }
   break;
  }
  return 1.f;
 }
 re5::GaitBackend backend(re5::MemoryReader memory,bool (*can_write)(void*,std::uintptr_t,std::size_t),bool (*write)(void*,std::uintptr_t,const void*,std::size_t)){
  return {memory,this,can_write,write,lookup,init,sample,weapon,mirror};
 }
};
}
