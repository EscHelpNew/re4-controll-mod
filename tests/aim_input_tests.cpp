#include "re4/aim_input.hpp"
#include <cassert>
#include <cstdio>
int main(){
    using namespace re4;Settings s;
    s.left_deadzone=s.right_deadzone=0;
    s.right_aim_gain_x=0.5f;s.right_aim_gain_y=0.25f;
    auto out=shape_aim_input(1,1,0.25f,-0.5f,s,{true,127});
    assert(out.x==16 && out.y==-16); // L full cannot override R quarter; independent axes.
    out=shape_aim_input(0.5f,-0.25f,1,1,s,{false,63});
    assert(out.x==32 && out.y==-16);
    s.stick_curve=StickCurve::Exponential;
    out=shape_aim_input(1,1,0.5f,0.5f,s,{true,127});assert(out.x==16 && out.y==8);
    s.right_deadzone=0.05f;
    assert(shape_aim_input(1,1,0.04f,0.04f,s,{true,127}).x==0);
    std::uint8_t bytes[kSettingsBytes];assert(encode_settings(s,bytes,sizeof(bytes)) && bytes[4]==12);
    Settings decoded;assert(decode_settings(bytes,sizeof(bytes),decoded));
    assert(decoded.right_aim_gain_x==0.5f && decoded.right_aim_gain_y==0.25f);
    s.right_aim_gain_x=1.01f;assert(!valid_settings(s));
    std::puts("aim input: chosen source, independent right axes, curve once and wirev6 PASS");
}
