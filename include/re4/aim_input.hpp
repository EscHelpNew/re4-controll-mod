#pragma once
#include "input.hpp"
#include <cmath>
namespace re4 {
struct AimRoute {bool right=false;float full_scale=63;};
struct AimInput {std::int32_t x=0,y=0;};
inline AimInput shape_aim_input(float lx,float ly,float rx,float ry,const Settings& s,AimRoute route){
    if(!std::isfinite(route.full_scale) || route.full_scale<0 || route.full_scale>4096)return {};
    const float zone=route.right?s.right_deadzone:s.left_deadzone;
    const float x=shape_axis(route.right?rx:lx,zone,route.right?s.right_aim_gain_x:s.aim_gain_x,s.stick_curve);
    const float y=shape_axis(route.right?ry:ly,zone,route.right?s.right_aim_gain_y:s.aim_gain_y,s.stick_curve);
    return {std::int32_t(std::round(x*route.full_scale)),std::int32_t(std::round(y*route.full_scale))};
}
}
