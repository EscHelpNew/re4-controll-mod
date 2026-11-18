#include "re5/runtime_input.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
int main(){
 re5::SettingsSnapshot snapshot;re5::Settings settings,out;out.players[1].gyro_speed=2;
 assert(!snapshot.read(out)&&out.players[1].gyro_speed==2);
 settings.players[0].gyro_enabled=true;settings.players[1].right_deadzone=.22f;
 assert(snapshot.publish(settings)&&snapshot.read(out));assert(out.players[0].gyro_enabled&&out.players[1].right_deadzone==.22f);
 settings.players[1].gyro_speed=NAN;assert(!snapshot.publish(settings)&&snapshot.read(out)&&std::isfinite(out.players[1].gyro_speed));
 re5::PadState p;p.lx=32767;p.ly=0;p.rx=16384;p.ry=-32767;auto s=re5::native_controls();s.left_deadzone=.2f;s.right_deadzone=.1f;
 const auto a=re5::shape(p,s);assert(a.lx==1&&a.ly==0&&a.rx>.44f&&a.rx<.45f&&a.ry==-1);
 float angle,mag;re5::polar(a.lx,a.ly,angle,mag);assert(std::fabs(angle+90)<.001f&&mag==1);re5::polar(0,0,angle,mag);assert(angle==0&&mag==0);
 assert(re5::active_aim(0x1E)&&re5::active_aim(0x27)&&!re5::active_aim(0x1F)&&!re5::active_aim(2));
 std::puts("RE5 coherent settings, independent axis shaping and native polar PASS");
}
