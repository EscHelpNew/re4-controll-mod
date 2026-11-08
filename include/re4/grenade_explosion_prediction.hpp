#pragma once
#include "re4/grenade_prediction.hpp"
#include <cstdint>

namespace re4 {
enum class GrenadeTermination {Burst,Removed};
struct GrenadeExplosion {
    ViewVec3 position;float time=0;unsigned steps=0;
    GrenadeTermination outcome=GrenadeTermination::Burst;
};
struct GrenadeExplosionContact {ViewVec3 normal{};std::uint32_t material_flags=0;bool water=false;};
struct GrenadeExplosionLaunch {ViewVec3 position,velocity;float dt=1;unsigned kind=0;};
struct GrenadeExplosionBackend {
    void* context=nullptr;
    bool (*sweep)(void*,ViewVec3,ViewVec3,ViewVec3&,GrenadeExplosionContact&)=nullptr;
    bool (*relocate)(void*,ViewVec3,ViewVec3&)=nullptr;
};
// sweep implements private environment/water/sphere/material queries. Its
// velocity is AFTER gravity; candidate is the uncorrected next center. Water
// supplies its native height+20 correction and skips sphere/material handling.
// relocate implements native3980 with the PRE-step root matrix translation;
// every successful horizontal ray may overwrite the private burst candidate.
// False from either callback invalidates the entire prediction.
//
// steps counts native update entries. time counts dt spent moving/resting:
// fuse-on-entry termination adds an entry but no further movement or elapsed dt.
// Default96 matches the native adapter budget; explicit callers are capped256.
inline bool predict_grenade_explosion(const GrenadeExplosionLaunch& launch,const GrenadeExplosionBackend& backend,
        GrenadeExplosion& out,unsigned max_steps=96){
    if(!backend.sweep||launch.kind>2||!grenade_world_point(launch.position)||!view_finite(launch.velocity)
        ||std::fabs(launch.velocity.x)>1e5f||std::fabs(launch.velocity.y)>1e5f||std::fabs(launch.velocity.z)>1e5f
        ||!std::isfinite(launch.dt)||launch.dt<=0||launch.dt>2)return false;
    ViewVec3 position=launch.position,velocity=launch.velocity;
    float fuse=launch.kind?300.f:45.f;
    bool resting=false;
    double elapsed=0;
    auto finish=[&](GrenadeTermination outcome,unsigned step,bool clearance,ViewVec3 origin){
        ViewVec3 burst=position;
        if(clearance&&(!backend.relocate||!backend.relocate(backend.context,origin,burst)))return false;
        if(!grenade_world_point(burst))return false;
        out={burst,float(elapsed),step,outcome};return true;
    };
    const unsigned count=std::min(max_steps,256u);
    for(unsigned step=1;step<=count;++step){
        if(resting){
            fuse-=launch.dt;
            elapsed+=launch.dt;
            if(fuse<=0)return finish(launch.kind?GrenadeTermination::Removed:GrenadeTermination::Burst,step,false,{});
            continue;
        }
        // Do not clamp a subtraction overshoot: native first zeroes a negative
        // fuse and performs one more full movement before terminating on entry.
        if(fuse==0)return finish(launch.kind?GrenadeTermination::Removed:GrenadeTermination::Burst,step,launch.kind==0,position);
        if(fuse<0)fuse=0;
        else fuse-=launch.dt;
        const ViewVec3 origin=position;
        velocity.y-=20*launch.dt;
        ViewVec3 candidate{position.x+velocity.x*launch.dt,
            position.y+velocity.y*launch.dt,position.z+velocity.z*launch.dt};
        if(!grenade_world_point(candidate))return false;
        GrenadeExplosionContact contact{};
        if(!backend.sweep(backend.context,origin,velocity,candidate,contact)
            ||!grenade_world_point(candidate)||!view_finite(contact.normal))return false;
        position=candidate;elapsed+=launch.dt;
        if(contact.water||((contact.material_flags&1)&&contact.normal.y>.8f)){
            if(launch.kind)return finish(GrenadeTermination::Removed,step,false,{});
            resting=true;continue;
        }
        const auto normal=contact.normal;
        if(normal.x==0&&normal.y==0&&normal.z==0)continue;
        // Native reflection normalizes both its inputs and its reflected
        // direction, then scales by half incoming speed. Preserve raw normal.y
        // separately for the native floor/material thresholds above and below.
        const double speed=std::hypot(double(velocity.x),double(velocity.y),double(velocity.z));
        const double length=std::hypot(double(normal.x),double(normal.y),double(normal.z));
        if(!std::isfinite(speed)||speed==0||!std::isfinite(length)||length==0)return false;
        const double nx=normal.x/length,ny=normal.y/length,nz=normal.z/length;
        const double vx=velocity.x/speed,vy=velocity.y/speed,vz=velocity.z/speed;
        const double dot=vx*nx+vy*ny+vz*nz;
        const double rx=vx-2*dot*nx,ry=vy-2*dot*ny,rz=vz-2*dot*nz;
        const double reflected=std::hypot(rx,ry,rz);
        if(!std::isfinite(reflected)||reflected==0)return false;
        const double scale=speed*.5/reflected;
        if(!view_store_point(rx*scale,ry*scale,rz*scale,velocity))return false;
        if(normal.y>.7f){
            if(velocity.y<50)velocity.y=50;
            if(launch.kind)return finish(GrenadeTermination::Burst,step,true,origin);
        }
    }
    return false;
}
}
