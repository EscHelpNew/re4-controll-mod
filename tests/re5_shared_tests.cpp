#include "re5/shared.hpp"
#include <cstdio>
#include <cstdlib>
#undef assert
#define assert(c) do{if(!(c)){std::fprintf(stderr,"RE5 shared line %d: %s\n",__LINE__,#c);std::exit(1);}}while(false)
int main(){
 auto control=re5::initial_shared_control();control.capabilities=re5::kCapabilityFiltering|re5::kCapabilityGyro;
 re5::Settings wanted,out;wanted.players[0].gyro_speed=.25f;wanted.players[1].gyro_speed=2.5f;wanted.graphics.aa=re5::AntiAliasing::Smaa;
 std::uint32_t writer=0,reader=0;
 assert(re5::publish_settings(control,wanted,writer)&&writer==2);
 assert(re5::consume_settings(control,out,reader)&&reader==2&&control.applied_sequence==2);
 assert(out.players[0].gyro_speed==.25f&&out.players[1].gyro_speed==2.5f&&out.graphics.aa==re5::AntiAliasing::Smaa);
 assert(!re5::consume_settings(control,out,reader));
 control.sequence=3;assert(!re5::publish_settings(control,wanted,writer)&&!re5::consume_settings(control,out,reader));
 control.sequence=4;control.settings[10]^=1;assert(!re5::consume_settings(control,out,reader)&&reader==2&&out.players[1].gyro_speed==2.5f);control.settings[10]^=1;
 assert(re5::consume_settings(control,out,reader)&&reader==4);
 writer=4;control.marker[0]='X';assert(!re5::publish_settings(control,wanted,writer));control.marker[0]='R';
 control.capabilities=0x80000000u;assert(!re5::publish_settings(control,wanted,writer));control.capabilities=re5::kCapabilityFiltering;
 control.reserved=1;assert(!re5::publish_settings(control,wanted,writer));control.reserved=0;
 control.sequence=0xfffffffcu;writer=reader=0xfffffffcu;
 assert(re5::publish_settings(control,wanted,writer)&&writer==0xfffffffeu);
 assert(re5::consume_settings(control,out,reader));
 wanted.players[1].gyro_speed=3.5f;
 assert(re5::publish_settings(control,wanted,writer)&&writer==0);
 assert(re5::consume_settings(control,out,reader)&&out.players[1].gyro_speed==3.5f);
 control.sequence=0xfffffffeu;assert(!re5::consume_settings(control,out,reader));
 std::puts("RE5 live settings: two-player atomic snapshots, strict ABI, corruption rejection and wrap PASS");
}
