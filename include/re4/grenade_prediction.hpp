#pragma once
#include "re4/view.hpp"

namespace re4 {
struct GrenadeLaunch {ViewVec3 position,velocity;float dt=1,max_time=45;};
struct GrenadeLanding {ViewVec3 position,normal;float time=0;unsigned steps=0;};
struct GrenadePhysicsBackend {
    void* context=nullptr;
    bool (*sweep)(void*,ViewVec3,ViewVec3&,ViewVec3&)=nullptr;
};
// These limits reject corrupt native snapshots and bound every query batch.
inline bool grenade_world_point(ViewVec3 point){
    return view_finite(point)&&std::fabs(point.x)<=1e7f
        &&std::fabs(point.y)<=1e7f&&std::fabs(point.z)<=1e7f;
}
inline bool grenade_unit_normal(ViewVec3 input,ViewVec3& output){
    if(!view_finite(input))return false;
    const double length=std::hypot(double(input.x),double(input.y),double(input.z));
    if(length<=1e-6)return false;
    output={float(input.x/length),float(input.y/length),float(input.z/length)};
    return true;
}
// The callback receives private center positions. False invalidates the entire
// prediction; true and a zero normal means no impact. Nonzero normals accompany
// a corrected sphere center, never an actor pointer or synthetic ground height.
inline bool predict_grenade_landing(const GrenadeLaunch& launch,const GrenadePhysicsBackend& backend,
        GrenadeLanding& out,unsigned max_steps=96){
    if(!backend.sweep||!grenade_world_point(launch.position)||!view_finite(launch.velocity)
        ||std::fabs(launch.velocity.x)>1e5f||std::fabs(launch.velocity.y)>1e5f||std::fabs(launch.velocity.z)>1e5f
        ||!std::isfinite(launch.dt)||launch.dt<=0||launch.dt>2
        ||!std::isfinite(launch.max_time)||launch.max_time<=0||launch.max_time>300)return false;
    ViewVec3 position=launch.position,velocity=launch.velocity;
    const unsigned count=std::min(max_steps,96u);
    for(unsigned step=1;step<=count;++step){
        const double time=double(step)*launch.dt;
        if(time>launch.max_time)return false;
        velocity.y-=20*launch.dt;
        ViewVec3 candidate{position.x+velocity.x*launch.dt,
            position.y+velocity.y*launch.dt,position.z+velocity.z*launch.dt};
        if(!grenade_world_point(candidate))return false;
        ViewVec3 normal{};
        if(!backend.sweep(backend.context,position,candidate,normal)
            ||!grenade_world_point(candidate)||!view_finite(normal))return false;
        position=candidate;
        if(normal.x==0&&normal.y==0&&normal.z==0)continue;
        ViewVec3 unit;
        if(!grenade_unit_normal(normal,unit))return false;
        if(unit.y>.7f){
            const ViewVec3 contact{position.x-25*unit.x,position.y-25*unit.y,position.z-25*unit.z};
            if(!grenade_world_point(contact))return false;
            out={contact,unit,float(time),step};return true;
        }
        const double dot=double(velocity.x)*unit.x+double(velocity.y)*unit.y+double(velocity.z)*unit.z;
        velocity={float((velocity.x-2*dot*unit.x)*.5),float((velocity.y-2*dot*unit.y)*.5),
            float((velocity.z-2*dot*unit.z)*.5)};
        if(!view_finite(velocity))return false;
    }
    return false;
}
}
