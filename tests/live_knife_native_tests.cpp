#include <switch.h>
Result knife_dispatch(Service*,unsigned,const void*,std::size_t,void*,std::size_t,TestDispatchParams);
#undef serviceDispatchIn
#define serviceDispatchIn(s,c,i,...) knife_dispatch(s,c,&i,sizeof(i),nullptr,0,TestDispatchParams{__VA_ARGS__})
#define main(...) game_link_baseline_main(__VA_ARGS__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#include "game_link_tests.cpp"
#pragma GCC diagnostic pop
#undef main
#include "../source/overlay/live_knife.inc"
#undef assert
#define assert(c) do {if(!(c)){std::fprintf(stderr,"line%d: %s\n",__LINE__,#c);std::exit(1);}}while(0)

namespace {
unsigned knife_reads=0,knife_bytes=0;
u64 race_address=0,race_target=0,race_value=0;
bool raced=false,race_word=false;u64 fail_after_address=0,fail_after_target=0;
template<class T>void replace_segment(u64 at,const T& value){
 for(auto& s:segments)if(s.address==at){assert(s.bytes.size()==sizeof(T));std::memcpy(s.bytes.data(),&value,sizeof(value));return;}
 assert(false);
}
template<class T>void change(u64 at,const T& value){
 for(auto& s:segments)if(at>=s.address&&at-s.address<=s.bytes.size()&&sizeof(value)<=s.bytes.size()-(at-s.address)){
  std::memcpy(s.bytes.data()+at-s.address,&value,sizeof(value));return;
 }
 assert(false);
}
constexpr u64 kp=0x800000,ks=0x900000,kc=0xA00000,kf=0xB00000,kpart=0xC00000;
constexpr u64 kh0=0xD00000,kh1=0xD10000,kroot=0xE00000,kbones=0xF00000,karchive=0x1100000,kbody=0x1200000;
template<std::size_t N,class T>void put(std::array<u8,N>& bytes,std::size_t offset,T value){std::memcpy(bytes.data()+offset,&value,sizeof(value));}
Metadata knife_fixture(){
 reset();knife_reads=knife_bytes=0;race_address=race_target=race_value=0;raced=race_word=false;fail_after_address=fail_after_target=0;
 main_base=0x7100000000;main_size=0x1300000;
 Metadata m{};m.pid=42;m.title=re4::profile::title_id;m.main={main_base,main_size};std::memcpy(m.build_id,re4::profile::build_id,32);
 segment(main_base+re4::profile::player_slot,kp);segment(main_base+0xB1AE08,ks);
 std::array<u8,0xA00> player{};put(player,0,main_base+0xB01230);put(player,8,u32(0x2006));
 put(player,0x108,kbones);player[0x114]=0;player[0x115]=11;player[0x118]=0;player[0x11A]=119;
 put(player,0x180,kpart);put(player,0x428,kbody);put(player,0x938,kc);put(player,0x950,kf);segment(kp,player);
 std::array<u8,0x20> state_pointers{};put(state_pointers,0,karchive);put(state_pointers,0x18,kbody);segment(ks+0x58,state_pointers);
 std::array<u8,0x10> state_mode{};state_mode[4]=48;state_mode[5]=1;state_mode[12]=0;segment(ks+0x5030,state_mode);segment(ks+0x2C,u32(0));
 std::array<u8,0x60> context{};put(context,0x38,kh0);put(context,0x40,kh1);segment(kc,context);
 std::array<u8,0x60> face{};put(face,0x38,kpart+0x1000);put(face,0x40,kpart+0x2000);put(face,0x58,kpart);segment(kf,face);
 std::array<u8,0x1A8> part{};put(part,0,main_base+0xB02088);put(part,0x148,u16(0x1234));put(part,8,u32(8));put(part,0x18,karchive+0x100);put(part,0x20,karchive+0x200);part[0xA7]=255;segment(kpart,part);
 std::array<u8,0x20> weapon{};put(weapon,0,u32(3));put(weapon,0x14,u32(0x200));put(weapon,0x18,u32(0x100));segment(karchive,weapon);
 std::array<u8,0x98> bin{};bin[0x29]=2;put(bin,0x58,u16(163));put(bin,0x5A,u16(164));put(bin,0x38,u32(0xA0000300));segment(karchive+0x100,bin);std::array<u8,16> tpl{};segment(karchive+0x200,tpl);
 std::array<u8,0x20> body{};put(body,0,u32(3));put(body,0x10,u32(0x200));put(body,0x14,u32(0x100));put(body,0x18,u32(0x300));segment(kbody,body);
 std::array<u8,0x408> held{};put(held,0,main_base+0xB0AAF8);put(held,8,u32(6));put(held,0x108,kroot);held[0x118]=0x24;held[0x11A]=2;put(held,0x180,kpart);put(held,0x3F8,kp);held[0x404]=8;segment(kh0,held);
 put(held,0,main_base+0xB0ABF8);held[0x118]=0x25;segment(kh1,held);
 std::array<u8,0x80> root{};put(root,0x78,kbones+10*0x1F0);float one=1;put(root,0x18,one);put(root,0x2C,one);put(root,0x40,one);segment(kroot,root);
 return m;
}
constexpr u64 khead=0x1300000,kpositions=0x1600004,knormals=0x1700004;
Metadata blade_shape_fixture(){
 auto m=knife_fixture();change(kp,u64(main_base+0xB09A60));change(kp+0x180,khead);
 std::array<u8,0x30> head{};put(head,0x18,kbody+0x300);put(head,0x28,kpart);segment(khead,head);
 for(auto& s:segments)if(s.address==kbody){s.bytes.assign(0x60,0);break;}
 change(kbody,u32(10));change(kbody+0x10,u32(0x300));change(kbody+0x14,u32(0x200));change(kbody+0x34,u32(0x100));
 change(kpart+8,u32(9));change(kpart+0x148,u16(0));change(kpart+0x18,kbody+0x100);change(kpart+0x20,kbody+0x200);
 std::array<u8,0x98> bin{};bin[0x29]=119;put(bin,0x58,u16(163));put(bin,0x5A,u16(163));put(bin,0x38,u32(0xA0300300));segment(kbody+0x100,bin);
 std::array<u8,0x10> tpl{};segment(kbody+0x200,tpl);
 change(ks+0x2C,u32(1));change(kpart+0xB8,kpositions);change(kpart+0xD0,knormals);
 segment(kpositions,std::array<float,3>{1,2,3});segment(knormals,std::array<float,3>{0,1,0});return m;
}
}
Result knife_dispatch(Service* service,unsigned command,const void* in,std::size_t in_size,void* out,std::size_t out_size,TestDispatchParams params){
 assert(command==65102);const auto* range=static_cast<const u64*>(in);++knife_reads;knife_bytes+=unsigned(range[1]);
 const auto result=test_dispatch(service,command,in,in_size,out,out_size,params);
 if(range[0]==fail_after_address)fail_read_address=fail_after_target;
 if(!raced&&range[0]==race_address){raced=true;if(race_word)change(race_target,u32(race_value));else change(race_target,race_value);}
 return result;
}
int main(){
 Service service{true};auto m=knife_fixture();re4::LiveKnifeObservation out;
 assert(read_live_knife(&service,m,out));
 assert(out.valid&&out.stable&&out.player_valid&&out.player_stable&&out.player_bones==119&&out.weapon_id==48);
 assert(out.asset_matches_state70&&out.body.valid&&out.body.chain_valid&&out.body.face58_member&&out.body.face58_index==0);
 assert(out.body.face58.visible&&out.body.face58.alpha==255&&!out.body.aliases_right_hand);
 assert(out.body.face58.fields_readable&&out.body.face58.vtable_in_main&&out.body.face58.vtable_offset==0xB02088);
 assert(out.body.face58.animation==0x1234&&out.body.face58.bin_vertices==163&&out.body.face58.bin_normals==164&&out.body.face58.bin_flags==0xA0000300&&out.body.face58.bin_header_3b==0xA0);
 assert(out.held[0].valid&&out.held[0].native_knife_class&&out.held[0].parent_is_player&&out.held[0].actor_visible&&out.held[0].control_hidden);
 assert(out.held[0].root_parent_is_grip&&out.held[0].part.weapon_bin&&out.held[0].part.weapon_tpl&&out.held[0].bin_bones_match);
 assert(!out.held[1].native_knife_class&&out.read_calls==knife_reads&&out.read_bytes==knife_bytes&&knife_reads<100&&knife_bytes<=16384&&write_count==0);
 std::printf("knife complete fixture: %u IPC, %u bytes\n",knife_reads,knife_bytes);
 m=blade_shape_fixture();assert(read_live_knife(&service,m,out)&&out.valid&&out.stable&&out.body.valid&&out.body.stable);
 assert(out.body.source_shape_matches&&out.body.face58.fields_readable&&out.body.face58.matrix_finite&&out.body.face58.animation==0);
 assert(out.body.buffers.slot_readable&&out.body.buffers.slot_valid&&out.body.buffers.stable&&out.body.buffers.slot==1);
 assert(out.body.buffers.positions_present&&out.body.buffers.positions_readable&&out.body.buffers.positions_finite);
 assert(out.body.buffers.normals_present&&out.body.buffers.normals_readable&&out.body.buffers.normals_finite);
 assert(out.read_calls==knife_reads&&out.read_bytes==knife_bytes&&knife_reads<=96&&knife_bytes<=16384&&write_count==0);
 std::printf("blade predicate + float-aligned samples: %u IPC, %u bytes\n",knife_reads,knife_bytes);
 for(u32 flags:{0xA0000300u,0xA0100300u,0xA0200300u,0xA0300301u}){
  m=blade_shape_fixture();change(kbody+0x138,flags);
  assert(read_live_knife(&service,m,out)&&out.valid&&out.stable&&out.body.valid&&!out.body.source_shape_matches);
 }
 for(unsigned bad=0;bad<4;++bad){
  m=blade_shape_fixture();
  if(bad==0)change(kpart+0x148,u16(1));
  if(bad==1)change(kbody+0x158,u16(162));
  if(bad==2)change(kpart,u64(main_base+0xB02080));
  if(bad==3)change(kpart+0x74,std::numeric_limits<float>::quiet_NaN());
  assert(read_live_knife(&service,m,out)&&out.valid&&out.stable&&out.body.valid&&!out.body.source_shape_matches);
 }
 m=blade_shape_fixture();fail_read_address=ks+0x2C;
 assert(read_live_knife(&service,m,out)&&out.valid&&out.stable&&out.body.valid&&!out.body.buffers.slot_readable);
 m=blade_shape_fixture();fail_read_address=kpositions;
 assert(read_live_knife(&service,m,out)&&out.valid&&out.stable&&out.body.source_shape_matches&&out.body.buffers.stable);
 assert(out.body.buffers.positions_present&&!out.body.buffers.positions_readable&&!out.body.buffers.positions_finite&&out.body.buffers.normals_finite);
 m=blade_shape_fixture();fail_after_address=kpositions;fail_after_target=ks+0x2C;
 assert(read_live_knife(&service,m,out)&&out.valid&&out.stable&&out.body.valid&&!out.body.buffers.stable);
 assert(!out.body.buffers.positions_present&&!out.body.buffers.positions_readable&&!out.body.buffers.normals_finite);
 m=blade_shape_fixture();change(kpositions,std::numeric_limits<float>::quiet_NaN());
 assert(read_live_knife(&service,m,out)&&out.stable&&out.body.buffers.positions_readable&&!out.body.buffers.positions_finite&&out.body.buffers.normals_finite);
 for(u64 pointer:{u64(0),kpositions+2,u64(0x800),~u64(0)-3}){
  m=blade_shape_fixture();change(kpart+0xB8,pointer);
  assert(read_live_knife(&service,m,out)&&out.valid&&out.stable&&out.body.buffers.stable);
  assert(out.body.buffers.positions_present==(pointer!=0)&&!out.body.buffers.positions_readable&&!out.body.buffers.positions_finite&&out.body.buffers.normals_finite);
 }
 m=blade_shape_fixture();change(ks+0x2C,u32(3));
 assert(read_live_knife(&service,m,out)&&out.stable&&out.body.buffers.slot_readable&&!out.body.buffers.slot_valid&&out.body.buffers.slot==3);
 assert(!out.body.buffers.positions_present&&!out.body.buffers.positions_readable&&!out.body.buffers.normals_present);
 m=blade_shape_fixture();race_address=kpositions;race_target=ks+0x2C;race_value=2;race_word=true;
 assert(read_live_knife(&service,m,out)&&out.valid&&out.stable&&out.body.valid&&!out.body.buffers.stable);
 assert(!out.body.buffers.positions_present&&!out.body.buffers.positions_readable&&!out.body.buffers.positions_finite&&!out.body.buffers.normals_present&&!out.body.buffers.normals_readable&&!out.body.buffers.normals_finite);
 m=knife_fixture();m.build_id[0]^=1;assert(!read_live_knife(&service,m,out)&&!out.valid&&knife_reads==0);
 m=knife_fixture();fail_read_address=kh0;assert(read_live_knife(&service,m,out)&&!out.held[0].valid&&out.held[1].valid);
 m=knife_fixture();std::array<u8,0x1A8> cycle{};put(cycle,0x28,kpart);replace_segment(kpart,cycle);
 assert(read_live_knife(&service,m,out)&&!out.body.chain_valid&&out.body.chain_cycle);
 m=knife_fixture();race_address=kh0;race_target=main_base+re4::profile::player_slot;race_value=kp+0x1000;
 assert(read_live_knife(&service,m,out)&&!out.stable&&!out.player_stable);
 m=knife_fixture();race_address=kh0;race_target=kc+0x38;race_value=kh1;
 assert(read_live_knife(&service,m,out)&&!out.stable&&out.player_stable&&!out.held[0].stable);
 m=knife_fixture();change(kc+0x40,kh0);change(kf+0x38,kpart);
 assert(read_live_knife(&service,m,out)&&out.stable&&out.held[0].aliases_other&&out.held[1].aliases_other&&out.body.aliases_right_hand);
 m=knife_fixture();change(kpart+0x18,kbody+0x100);change(kpart+0x20,kbody+0x200);
 {std::array<u8,0x98> bin{};bin[0x29]=119;segment(kbody+0x100,bin);std::array<u8,16> tpl{};segment(kbody+0x200,tpl);}
 assert(read_live_knife(&service,m,out)&&out.body.directory_valid&&out.body.face58.bin_entry==1&&out.body.face58.tpl_entry==0);
 assert(!out.held[0].part.weapon_bin&&!out.held[0].bin_bones_match);
 m=knife_fixture();change(kroot+0x18,std::numeric_limits<float>::quiet_NaN());
 assert(read_live_knife(&service,m,out)&&out.held[0].root_valid&&!out.held[0].root_matrix_valid);
 m=knife_fixture();change(kp+8,u32(6));change(kroot+0x78,kbones+10*0x200);
 for(unsigned i=0;i<10;++i)segment(kbones+i*0x200+0x108,kbones+(i+1)*0x200);
 assert(read_live_knife(&service,m,out)&&out.grip_valid&&out.held[0].root_parent_is_grip);
 change(kbones+5*0x200+0x108,kbones);
 assert(read_live_knife(&service,m,out)&&!out.grip_valid&&!out.held[0].root_parent_is_grip);
 m=knife_fixture();change(kp+0x938,kc+1);assert(read_live_knife(&service,m,out)&&!out.held[0].valid&&!out.held[1].valid);
 m=knife_fixture();m.main.base=~u64(0)-4095;assert(!read_live_knife(&service,m,out)&&knife_reads==0);
 m=knife_fixture();
 for(unsigned i=0;i<65;++i){
  const auto at=kpart+u64(i)*0x1000,next=i==64?u64(0):at+0x1000;
  if(!i)change(kpart+0x28,next);
  else {std::array<u8,0x30> link{};put(link,0x28,next);segment(at,link);}
 }
 assert(read_live_knife(&service,m,out)&&!out.body.chain_valid&&out.body.chain_truncated&&out.read_calls<100&&out.read_bytes<=16384);
 assert(out.read_calls<=96);std::printf("bounded long chain: %u IPC, %u bytes\n",out.read_calls,out.read_bytes);
 m=knife_fixture();replace_segment(main_base+re4::profile::player_slot,~u64(0)-7);
 assert(!read_live_knife(&service,m,out)&&!out.player_valid&&knife_reads<4);
 std::puts("native knife observer: fixed reads, both held handles, metadata, cycles, failed reads and races PASS");
}
