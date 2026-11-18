#pragma once
#include "re5/profile.hpp"
#include "re5/memory.hpp"
namespace re5 {
struct ResolvedPad {std::uint32_t controller=0,style=0;};
struct ResolvedPlayer {
 std::uintptr_t actor=0,command=0;
 unsigned actor_slot=0,local_player=0;
 ResolvedPad sticks,motion;
};
namespace native_input_detail {
inline bool pad(const MemoryReader& memory,std::uintptr_t pads,unsigned index,ResolvedPad& out){
 if(index>8)return false;
 std::uint32_t id=0,style=0;
 if(!memory.get(pads,index*0xEE0+0x670,id)||!memory.get(pads,index*0xEE0+0x674,style)||id!=(index==8?32:index))return false;
 // This field is the engine's style mask, not nn::hid/libnx's mask.
 // Engine full/dual/handheld bits1/2/16 map to SDK1/4/2. Single
 // Joy-Con bits4/8 do not provide this adapter's paired-stick controls.
 const unsigned selected=(style&1)?1:(style&16)?2:(style&2)?4:0;
 if(!selected)return false;
 out={id,selected};return true;
}
}
// Resolves the native local input arm. Actor slot, logical player selector,
// physical stick pad and physical motion pad are separate identities in RE5.
inline bool resolve_player(const MemoryReader& memory,std::uintptr_t main,std::uintptr_t actor,std::uintptr_t command,ResolvedPlayer& out){
 std::uint32_t owner=99,wrapper_owner=99;std::uint8_t slot=255,selector=255;
 std::uintptr_t players=0,member=0,wrapper=0,wrapped_actor=0,pads=0;
 if(!actor||!command||!memory.get(actor,0x3F40,owner)||owner
  ||!memory.get(actor,0x3F2C,slot)||slot>3||!memory.get(actor,0x3F49,selector)||selector>1
  ||!memory.get(main,re5::profile::offset<0x1CF8CC8>(),players)||!memory.get(players,0x30+slot*8,member)||member!=actor
  ||!memory.get(actor,0x3EE8,wrapper)||!wrapper||wrapper>UINTPTR_MAX-0x350||command!=wrapper+0x350
  ||!memory.get(wrapper,0x48,wrapped_actor)||wrapped_actor!=actor
  ||!memory.get(wrapper,0x34C,wrapper_owner)||wrapper_owner
  ||!memory.get(main,re5::profile::offset<0x26A9288>(),pads))return false;
 std::uint32_t stick_index=selector,motion_index=selector;
 if(!selector&&(!memory.get(pads,0x8C98,stick_index)||!memory.get(pads,0x8C94,motion_index)))return false;
 ResolvedPlayer candidate;candidate.actor=actor;candidate.command=command;
 candidate.actor_slot=slot;candidate.local_player=selector;
 if(!native_input_detail::pad(memory,pads,stick_index,candidate.sticks)||!native_input_detail::pad(memory,pads,motion_index,candidate.motion))return false;
 out=candidate;return true;
}
}
