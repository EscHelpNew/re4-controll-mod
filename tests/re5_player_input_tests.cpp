#include "re5/player_input.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#undef assert
#define assert(c) do{if(!(c)){std::fprintf(stderr,"RE5 input line %d: %s\n",__LINE__,#c);std::exit(1);}}while(false)
namespace {
bool near(float a,float b){return std::fabs(a-b)<0.000002f;}
re4::AimDelta frame(re5::PlayerInput& input,const re4::Settings& settings,re5::InputIdentity identity,std::uint64_t tick,std::uint64_t& sample,float yaw,float pitch,bool aiming=true){
 input.begin_frame(tick,settings,identity,aiming);
 for(unsigned i=0;i<4;++i)assert(input.push({++sample,5000000,yaw,pitch},identity.controller,identity.style));
 return input.finish_frame();
}
}
int main(){
 re5::PlayerInput first,second;re4::Settings a,b;
 a.gyro_enabled=b.gyro_enabled=true;a.gyro_deadzone_dps=b.gyro_deadzone_dps=0;
 a.gyro_speed=1;b.gyro_speed=0.5f;a.invert_x=a.invert_y=b.invert_x=b.invert_y=false;
 a.gyro_bias_x=0.25f;b.gyro_bias_x=-0.5f;
 const re5::InputIdentity one{0x1000,0,1,11},two{0x2000,1,4,11};
 std::uint64_t n1=0,n2=0,tick=1000000;
 for(unsigned i=0;i<14;++i){
  const auto x=frame(first,a,one,tick,n1,1.25f,2);
  const auto y=frame(second,b,two,tick,n2,-2.5f,4);
  if(i>=3){assert(near(x.yaw,.02f)&&near(x.pitch,.04f));assert(near(y.yaw,-.02f)&&near(y.pitch,.04f));}
  assert(first.finish_frame().yaw==0&&second.finish_frame().yaw==0);tick+=384000;
 }
 // Disabling/reassigning player2 cannot drain player1's filter or pending tail.
 auto replaced=two;replaced.actor=0x3000;
 assert(frame(second,b,replaced,tick,n2,10,10).yaw==0);
 assert(near(frame(first,a,one,tick,n1,1.25f,2).yaw,.02f));tick+=384000;
 replaced.controller=3;
 assert(frame(second,b,replaced,tick,n2,10,10).yaw==0);tick+=384000;
 replaced.style=1;
 assert(frame(second,b,replaced,tick,n2,10,10).yaw==0);tick+=384000;
 replaced.generation=12;
 assert(frame(second,b,replaced,tick,n2,10,10).yaw==0);tick+=384000;
 // A foreign pad cannot feed another player's history.
 second.begin_frame(tick,b,replaced,true);
 assert(!second.push({++n2,5000000,100,100},0,1));
 assert(!second.push({++n2,5000000,100,100},3,4));
 assert(second.finish_frame().yaw==0);
 assert(!second.push({++n2,5000000,100,100},3,1));tick+=384000;
 assert(frame(second,b,replaced,tick,n2,0,0,false).yaw==0);tick+=384000;
 // Rest for player2 is raw yaw -0.5, matching its configured sensor bias.
 assert(frame(second,b,replaced,tick,n2,-0.5f,0).yaw==0);tick+=384000;
 // Invalid/unassociated identities must never activate, even with valid samples.
 for(auto invalid:{re5::InputIdentity{},re5::InputIdentity{0x2000,9,1,11},re5::InputIdentity{0x2000,1,8,11}}){
  second.begin_frame(tick,b,invalid,true);
  assert(!second.push({++n2,5000000,100,100},invalid.controller,invalid.style));assert(second.finish_frame().yaw==0);tick+=384000;
 }
 // A new session with lower sample counters cannot replay the old session.
 n2=0;replaced.generation=13;
 assert(frame(second,b,replaced,tick,n2,0,0).yaw==0);tick+=384000;
 b.invert_x=true;
 assert(frame(second,b,replaced,tick,n2,0,0).yaw==0);tick+=384000;
 for(unsigned i=0;i<8;++i){const auto delta=frame(second,b,replaced,tick,n2,1.5f,0);if(i>2)assert(near(delta.yaw,-.02f));tick+=384000;}
 first.reset();assert(frame(first,a,one,tick,n1,0,0).yaw==0);
 std::puts("RE5 player input: two independent gyro histories, identity/session gates and inversion PASS");
}
