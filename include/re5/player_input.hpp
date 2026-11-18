#pragma once
#include "re4/gyro_preview.hpp"
namespace re5 {
struct InputIdentity {
 std::uintptr_t actor=0;
 std::uint32_t controller=0,style=0;
 std::uint64_t generation=0;
 bool valid()const{return actor&&generation&&(controller<8||controller==32)&&(style==1||style==2||style==4);}
 bool operator==(const InputIdentity& other)const{return actor==other.actor&&controller==other.controller&&style==other.style&&generation==other.generation;}
};
// One instance per local actor. Native code must resolve controller ownership;
// this class never assumes NpadId0 is the first player or NpadId1 the second.
class PlayerInput {
 re4::GyroPreview gyro_;
 InputIdentity identity_;
 bool associated_=false,open_=false;
public:
 void reset(){gyro_.reset();identity_={};associated_=open_=false;}
 void begin_frame(std::uint64_t tick,const re4::Settings& settings,InputIdentity identity,bool aiming){
  if(!identity.valid()){reset();return;}
  if(!associated_||!(identity==identity_))gyro_.reset();
  identity_=identity;associated_=open_=true;
  gyro_.begin_frame(tick,settings,aiming);
 }
 bool push(re4::GyroSample sample,std::uint32_t controller,std::uint32_t style){
  if(!open_||!associated_||controller!=identity_.controller||style!=identity_.style)return false;
  gyro_.push(sample,true);return true;
 }
 re4::AimDelta finish_frame(){
  if(!open_)return {};
  open_=false;return gyro_.finish_frame();
 }
};
}
