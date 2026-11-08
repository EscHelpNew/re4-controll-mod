#include "re4/character_blade_tip.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#define require(c) do{if(!(c)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#c);std::exit(1);}}while(0)
namespace {
constexpr re4::ViewMatrix3x4 identity{{{1,0,0,0},{0,1,0,0},{0,0,1,0}}};
bool near(re4::ViewVec3 a,re4::ViewVec3 b){return std::fabs(a.x-b.x)<.0001&&std::fabs(a.y-b.y)<.0001&&std::fabs(a.z-b.z)<.0001;}
}
int main(){
    using namespace re4;
    const ViewVec3 vertices[]{{0,1000,0},{5,1000,-30},{-20,990,150},{-80,980,400},{-20,990,150}};
    const auto original=vertices[3];CharacterBladeTip tip{};
    require(character_blade_tip_prepare(vertices,5,{0,1000,0},tip));
    require(tip.vertex==3&&near(tip.bind_point,original));
    const ViewMatrix3x4 skin{{{1,0,0,0},{0,1,0,-1000},{0,0,1,0}}};
    const ViewMatrix3x4 part{{{0,0,1,7},{0,1,0,-289},{-1,0,0,13}}};
    const ViewMatrix3x4 root{{{0,-1,0,50},{1,0,0,70},{0,0,1,-20}}};
    ViewVec3 out{};
    require(character_blade_tip_world(tip,skin,part,root,out));
    require(near(out,{359,477,73}));
    // A half-turn of the actual skinned blade precedes its owned part/root,
    // so the selected tip rotates as geometry instead of a vertical offset.
    auto turned=skin;turned.m[1][1]=-1;turned.m[2][2]=-1;turned.m[1][3]=1000;
    require(character_blade_tip_world(tip,turned,identity,identity,out));
    require(near(out,{-80,20,-400}));
    const auto unchanged=tip;
    require(!character_blade_tip_prepare(nullptr,5,{},tip));
    require(!character_blade_tip_prepare(vertices,0,{},tip));
    require(!character_blade_tip_prepare(vertices,65536,{},tip));
    require(!character_blade_tip_prepare(vertices,5,{NAN,0,0},tip));
    ViewVec3 invalid[2]{{0,0,10},{0,INFINITY,0}};
    require(!character_blade_tip_prepare(invalid,2,{},tip));
    const ViewVec3 zero[]{{0,0,0},{0,0,0}};
    require(!character_blade_tip_prepare(zero,2,{},tip));
    require(!std::memcmp(&tip,&unchanged,sizeof(tip)));
    auto bad=skin;bad.m[2][3]=std::numeric_limits<float>::quiet_NaN();const auto before=out;
    require(!character_blade_tip_world(tip,bad,part,root,out));require(near(out,before));
    require(!character_blade_tip_world(tip,skin,part,root,*reinterpret_cast<ViewVec3*>(&tip)));
    require(!character_blade_tip_prepare(vertices,5,{},*reinterpret_cast<CharacterBladeTip*>(const_cast<ViewVec3*>(vertices))));
    require(near(vertices[3],original));
    std::puts("owned blade tip geometry, full transform, no partial writes PASS");
}
