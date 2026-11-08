#pragma once
#include "re4/live_knife.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
namespace {
struct LiveKnifeReader {
 Service* service;const Metadata& metadata;re4::LiveKnifeObservation& out;
 static bool pointer(u64 value){return value>=0x1000&&!(value&7);}
 static bool add(u64 base,u64 offset,std::size_t size,u64& address){
  constexpr auto max=std::numeric_limits<u64>::max();
  if(!pointer(base)||offset>max-base||size>max-(base+offset))return false;
  address=base+offset;return true;
 }
 bool take(u64 address,void* bytes,std::size_t size){
  if(!address||address<0x1000||size>std::numeric_limits<u64>::max()-address)return false;
  if(out.read_calls>=96||size>16384-out.read_bytes){out.budget_exhausted=true;return false;}
  ++out.read_calls;out.read_bytes+=std::uint32_t(size);
  return R_SUCCEEDED(read_memory(service,address,bytes,size));
 }
 template<class T>bool field(u64 base,u64 offset,T& value){u64 address=0;return add(base,offset,sizeof(value),address)&&take(address,&value,sizeof(value));}
 template<std::size_t N>bool block(u64 address,std::array<u8,N>& bytes){return pointer(address)&&take(address,bytes.data(),bytes.size());}
 bool main_pointer(u64 offset,u64& value){
  u64 address=0;if(!main_address(metadata,offset,sizeof(value),address))return false;
  if(out.read_calls>=96||sizeof(value)>16384-out.read_bytes){out.budget_exhausted=true;return false;}
  ++out.read_calls;out.read_bytes+=sizeof(value);
  return read_main_pointer(service,metadata,offset,value);
 }
 template<class T,std::size_t N>static T at(const std::array<u8,N>& bytes,std::size_t offset){T value{};std::memcpy(&value,bytes.data()+offset,sizeof(value));return value;}
 bool vtable(u64 pointer_value,std::uint32_t& offset)const{
  if(!pointer(pointer_value)||pointer_value<metadata.main.base||pointer_value-metadata.main.base>=metadata.main.size
      ||pointer_value-metadata.main.base>UINT32_MAX)return false;
  offset=std::uint32_t(pointer_value-metadata.main.base);return true;
 }
 bool part(u64 pointer_value,u64 weapon_bin,u64 weapon_tpl,re4::LiveKnifePart& out_part,std::array<u8,0x1A8>& bytes){
  out_part.present=pointer_value!=0;if(!pointer_value){out_part.valid=true;return true;}
  if(!block(pointer_value,bytes))return false;
  out_part.fields_readable=true;out_part.vtable_in_main=vtable(at<u64>(bytes,0),out_part.vtable_offset);
  out_part.animation=at<u16>(bytes,0x148);out_part.matrix_finite=true;
  for(unsigned i=0;i<12;++i){const auto value=at<float>(bytes,0x74+i*4);if(!std::isfinite(value)||std::fabs(value)>1e9f)out_part.matrix_finite=false;}
  out_part.flags=at<u32>(bytes,8);out_part.visible=(out_part.flags&8)!=0;out_part.alpha=bytes[0xA7];
  const auto bin=at<u64>(bytes,0x18),tpl=at<u64>(bytes,0x20);
  out_part.next_present=at<u64>(bytes,0x28)!=0;
  out_part.weapon_bin=weapon_bin&&bin==weapon_bin;out_part.weapon_tpl=weapon_tpl&&tpl==weapon_tpl;
  std::array<u8,0x98> bin_bytes{};std::array<u8,16> tpl_bytes{};
  out_part.bin_readable=block(bin,bin_bytes);out_part.tpl_readable=block(tpl,tpl_bytes);
  if(out_part.bin_readable){
   out_part.bin_bones=bin_bytes[0x29];out_part.bin_vertices=at<u16>(bin_bytes,0x58);out_part.bin_normals=at<u16>(bin_bytes,0x5A);
   out_part.bin_flags=at<u32>(bin_bytes,0x38);out_part.bin_header_3b=bin_bytes[0x3B];
  }
  out_part.valid=out_part.bin_readable&&out_part.tpl_readable;return out_part.valid;
 }
 void buffers(u64 state,const std::array<u8,0x1A8>& part_bytes,bool source_stable,re4::LiveKnifeBladeBuffers& result){
  // Optional probes run after the original observation and reserve their full
  // worst-case budget first, so they cannot starve any existing subsection.
  if(out.read_calls>92||out.read_bytes>16384-32)return;
  u32 slot=0;if(!field(state,0x2C,slot))return;
  re4::LiveKnifeBladeBuffers next{};next.slot_readable=true;next.slot_valid=slot<3;next.slot=slot;
  const auto sample=[&](u64 address,bool& present,bool& readable,bool& finite){
   present=address!=0;
   // Native deformation arrays need float alignment, not pointer alignment.
   if(address<0x1000||(address&3)||address>std::numeric_limits<u64>::max()-12)return;
   std::array<float,3> values{};readable=take(address,values.data(),sizeof(values));
   if(readable){finite=true;for(float value:values)if(!std::isfinite(value))finite=false;}
  };
  if(next.slot_valid&&source_stable){
   sample(at<u64>(part_bytes,0xB0+slot*8),next.positions_present,next.positions_readable,next.positions_finite);
   sample(at<u64>(part_bytes,0xC8+slot*8),next.normals_present,next.normals_readable,next.normals_finite);
  }
  u32 after=0;next.stable=field(state,0x2C,after)&&after==slot&&source_stable;
  if(!next.stable){
   // Keep the first observed index but publish no samples from a changed slot
   // or unstable source. This is still best effort, never ordered skin proof.
   result.slot_readable=true;result.slot_valid=slot<3;result.slot=slot;return;
  }
  result=next;
 }
};
inline bool read_live_knife(Service* service,const Metadata& metadata,re4::LiveKnifeObservation& out){
 out={};if(!service||!serviceIsActive(service)||!metadata.pid||!supported(metadata))return false;
 LiveKnifeReader r{service,metadata,out};u64 player=0,state=0;
 if(!r.main_pointer(re4::profile::player_slot,player)||!r.main_pointer(0xB1AE08,state))return false;
 std::array<u8,0xA00> player_bytes{};
 if(!r.block(player,player_bytes))return false;
 using R=LiveKnifeReader;
 out.player_valid=true;out.player_flags=R::at<u32>(player_bytes,8);out.player_type=player_bytes[0x118];out.player_bones=player_bytes[0x11A];
 std::copy_n(player_bytes.data()+0x114,4,out.routine.data());
 out.vtable_in_main=r.vtable(R::at<u64>(player_bytes,0),out.player_vtable_offset);
 const auto bones=R::at<u64>(player_bytes,0x108),chain=R::at<u64>(player_bytes,0x180),asset=R::at<u64>(player_bytes,0x428);
 const auto context=R::at<u64>(player_bytes,0x938),face=R::at<u64>(player_bytes,0x950);
 std::array<u8,0x20> state_pointers{};std::array<u8,0x10> mode{};
 const bool pointers_ok=r.field(state,0x58,state_pointers),mode_ok=r.field(state,0x5030,mode);
 out.state_valid=pointers_ok&&mode_ok;out.valid=true;
 u64 weapon_bin=0,weapon_tpl=0,weapon_archive=0,body_archive=0;
 if(mode_ok){out.weapon_id=mode[4];out.weapon_sub=mode[5];out.native_character=mode[12];}
 if(pointers_ok){
  weapon_archive=R::at<u64>(state_pointers,0);body_archive=R::at<u64>(state_pointers,0x18);
  out.asset_matches_state70=R::pointer(asset)&&asset==body_archive;
  std::array<u8,0x20> directory{};
  if(r.block(weapon_archive,directory)){
   const auto bo=R::at<u32>(directory,0x18),to=R::at<u32>(directory,0x14);
   if(!bo||!R::add(weapon_archive,bo,0x98,weapon_bin))weapon_bin=0;
   if(!to||!R::add(weapon_archive,to,0x10,weapon_tpl))weapon_tpl=0;
  }
 }
 // Grip identity follows the native strided/linked representation, without
 // changing player animation or assuming current weapon5034 is the knife.
 u64 grip=bones;
 if(out.player_flags&0x2000)out.grip_valid=R::add(bones,10*0x1F0,1,grip)&&R::pointer(grip);
 else {
  std::array<u64,11> seen{};bool valid=R::pointer(grip);
  for(unsigned i=0;i<10&&valid;++i){
   seen[i]=grip;u64 next=0;if(!r.field(grip,0x108,next)||!R::pointer(next)){valid=false;break;}
   for(unsigned j=0;j<=i;++j)if(seen[j]==next)valid=false;
   grip=next;
  }
  out.grip_valid=valid;
 }
 std::array<u8,0x60> face_bytes{},context_bytes{};
 const bool context_ok=context&&r.block(context,context_bytes);
 const bool face_ok=face&&r.block(face,face_bytes);
 auto& body=out.body;body.face_present=face!=0;
 u64 face58=0;std::array<u8,0x1A8> face_part{};
 if(face_ok){
  face58=R::at<u64>(face_bytes,0x58);const auto right=R::at<u64>(face_bytes,0x38),left=R::at<u64>(face_bytes,0x40);
  body.right_hand_present=right!=0;body.left_hand_present=left!=0;
  body.aliases_right_hand=face58&&face58==right;body.aliases_left_hand=face58&&face58==left;
  r.part(face58,weapon_bin,weapon_tpl,body.face58,face_part);
 }
 // At most64 links, and the reader's aggregate IPC/byte budget also bounds
 // hostile chains. A truncated/cyclic traversal never claims non-membership.
 std::array<u64,64> seen{};u64 current=chain;body.chain_valid=true;
 while(current){
  if(body.chain_count==seen.size()){body.chain_truncated=true;body.chain_valid=false;break;}
  bool cycle=false;for(unsigned i=0;i<body.chain_count;++i)if(seen[i]==current)cycle=true;
  if(cycle){body.chain_cycle=true;body.chain_valid=false;break;}
  seen[body.chain_count]=current;
  std::array<u8,0x30> link{};
  if(current==face58&&body.face58.valid)std::copy_n(face_part.data(),link.size(),link.data());
  else if(!r.block(current,link)){body.chain_valid=false;break;}
  if(current==face58){body.face58_member=true;body.face58_index=std::int32_t(body.chain_count);}
  ++body.chain_count;current=R::at<u64>(link,0x28);
 }
 // Compare face geometry to the first64 entries of the actual source DAT.
 if(face58&&body.face58.valid&&R::pointer(body_archive)){
  std::array<u8,16> header{};
  if(r.block(body_archive,header)){
   const auto count=R::at<u32>(header,0);const auto bounded=std::min(count,64u);
   std::array<u32,64> offsets{};u64 table=0;
   if(count&&count<=4096&&R::add(body_archive,0x10,bounded*4,table)&&r.take(table,offsets.data(),bounded*4)){
    body.directory_valid=true;body.directory_complete=count<=64;
    const auto bin=R::at<u64>(face_part,0x18),tpl=R::at<u64>(face_part,0x20);
    for(unsigned i=0;i<bounded;++i){
     if(offsets[i]&&bin>=body_archive&&bin-body_archive==offsets[i])body.face58.bin_entry=std::int32_t(i);
     if(offsets[i]&&tpl>=body_archive&&tpl-body_archive==offsets[i])body.face58.tpl_entry=std::int32_t(i);
    }
   }
  }
 }
 const std::array<u64,2> handles{context_ok?R::at<u64>(context_bytes,0x38):0,context_ok?R::at<u64>(context_bytes,0x40):0};
 for(unsigned i=0;i<2;++i){
  auto& held=out.held[i];held.present=handles[i]!=0;held.aliases_other=handles[i]&&handles[i]==handles[1-i];
  if(!context_ok)continue;
  if(!handles[i]){held.valid=held.stable=true;continue;}
  if(i&&held.aliases_other){held=out.held[0];continue;}
  std::array<u8,0x408> bytes{};if(!r.block(handles[i],bytes))continue;
  held.valid=true;held.flags=R::at<u32>(bytes,8);held.type=bytes[0x118];held.bones=bytes[0x11A];held.control=bytes[0x404];
  held.actor_visible=(held.flags&6)==6;held.control_hidden=(held.control&8)!=0;
  const auto vtable=R::at<u64>(bytes,0);held.vtable_in_main=r.vtable(vtable,held.vtable_offset);
  u64 expected_vtable=0;held.native_knife_class=main_address(metadata,0xB0AAF8,8,expected_vtable)&&vtable==expected_vtable&&held.type==0x24;
  held.parent_is_player=R::at<u64>(bytes,0x3F8)==player;
  const auto root=R::at<u64>(bytes,0x108),part=R::at<u64>(bytes,0x180);held.root_present=root!=0;
  std::array<u8,0x1A8> part_bytes{};r.part(part,weapon_bin,weapon_tpl,held.part,part_bytes);
  held.bin_bones_match=held.part.bin_readable&&held.part.bin_bones==held.bones;
  std::array<u8,0x80> root_bytes{};
  if(r.block(root,root_bytes)){
   held.root_valid=true;held.root_parent_is_grip=out.grip_valid&&R::at<u64>(root_bytes,0x78)==grip;
   held.root_matrix_valid=true;std::array<float,12> matrix{};std::memcpy(matrix.data(),root_bytes.data()+0x18,sizeof(matrix));
   for(float value:matrix)if(!std::isfinite(value))held.root_matrix_valid=false;
   if(held.root_matrix_valid)held.root_matrix=matrix;
  }
  std::array<u8,0x408> after{};
  held.stable=r.block(handles[i],after)&&R::at<u64>(bytes,0)==R::at<u64>(after,0)
   &&R::at<u64>(bytes,0x108)==R::at<u64>(after,0x108)&&R::at<u64>(bytes,0x180)==R::at<u64>(after,0x180)
   &&R::at<u64>(bytes,0x3F8)==R::at<u64>(after,0x3F8)&&bytes[0x118]==after[0x118]&&bytes[0x11A]==after[0x11A];
 }
 std::array<u8,0xA00> player_after{};u64 player_slot=0,state_slot=0;
 const bool slot_ok=r.main_pointer(re4::profile::player_slot,player_slot)&&r.main_pointer(0xB1AE08,state_slot);
 out.player_stable=slot_ok&&player_slot==player&&state_slot==state&&r.block(player,player_after)
  &&R::at<u64>(player_after,0)==R::at<u64>(player_bytes,0)&&R::at<u64>(player_after,0x108)==bones
  &&R::at<u64>(player_after,0x180)==chain&&R::at<u64>(player_after,0x428)==asset
  &&R::at<u64>(player_after,0x938)==context&&R::at<u64>(player_after,0x950)==face
  &&player_after[0x118]==out.player_type&&player_after[0x11A]==out.player_bones
  &&!((R::at<u32>(player_after,8)^out.player_flags)&0x2000);
 std::array<u8,0x60> context_after{},face_after{};
 const bool handles_stable=context_ok&&r.block(context,context_after)
  &&R::at<u64>(context_after,0x38)==handles[0]&&R::at<u64>(context_after,0x40)==handles[1];
 for(auto& held:out.held)held.stable=held.stable&&handles_stable&&out.player_stable;
 body.stable=out.player_stable&&(!face||(face_ok&&r.block(face,face_after)
  &&R::at<u64>(face_after,0x58)==face58&&R::at<u64>(face_after,0x38)==R::at<u64>(face_bytes,0x38)
  &&R::at<u64>(face_after,0x40)==R::at<u64>(face_bytes,0x40)));
 body.valid=(!face||face_ok)&&(!face58||body.face58.valid)&&body.chain_valid;
 out.stable=out.player_stable&&body.stable&&handles_stable&&out.held[0].stable&&out.held[1].stable&&!out.budget_exhausted;
 const auto& shape=body.face58;
 body.source_shape_matches=body.valid&&body.stable&&out.player_stable&&out.state_valid
  &&out.vtable_in_main&&out.player_vtable_offset==0xB09A60&&out.player_type==0&&out.player_bones==119&&out.native_character==0
  &&body.face58_member&&shape.fields_readable&&shape.vtable_in_main&&shape.vtable_offset==0xB02088
  &&(shape.flags==1||shape.flags==9)&&!shape.animation&&shape.matrix_finite
  // Native2B4540 marks the loaded/relocated BIN with bits20 and21.
  &&shape.bin_bones==119&&shape.bin_vertices==163&&shape.bin_normals==163&&shape.bin_flags==0xA0300300
  &&shape.bin_entry==9&&shape.tpl_entry==1;
 r.buffers(state,face_part,body.stable&&shape.fields_readable,body.buffers);
 return true;
}
}
