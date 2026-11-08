#include "re4/grenade_explosion_prediction.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

#define CHECK(c) do { if(!(c)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#c);std::exit(1);} } while(0)
using namespace re4;
bool near(float a,float b){return std::fabs(a-b)<.002f;}
struct Sample {ViewVec3 normal{};unsigned flags=0;bool water=false,correct=false;ViewVec3 point{};};
struct World {
    std::vector<Sample> samples;
    std::vector<ViewVec3> starts,velocities,candidates,origins,bursts;
    unsigned invalid_at=0;
    bool relocate_valid=true,relocate_bad=false;
    float relocate_x=0;
    GrenadeExplosionBackend backend(){return {this,
        [](void* p,ViewVec3 previous,ViewVec3 velocity,ViewVec3& candidate,GrenadeExplosionContact& contact){
            auto& w=*static_cast<World*>(p);
            w.starts.push_back(previous);w.velocities.push_back(velocity);w.candidates.push_back(candidate);
            const auto index=w.starts.size();
            if(index==w.invalid_at)return false;
            if(index<=w.samples.size()){
                const auto& s=w.samples[index-1];contact={s.normal,s.flags,s.water};
                if(s.correct)candidate=s.point;
            }
            return true;
        },
        [](void* p,ViewVec3 origin,ViewVec3& burst){
            auto& w=*static_cast<World*>(p);w.origins.push_back(origin);w.bursts.push_back(burst);
            burst.x+=w.relocate_x;
            if(w.relocate_bad)burst.z=std::numeric_limits<float>::quiet_NaN();
            return w.relocate_valid;
        }};}
};
int main(){
    GrenadeExplosion result;
    // Native fuse is inspected before movement: 45 moves, then entry46 bursts.
    World air;air.relocate_x=10;
    CHECK(predict_grenade_explosion({{1,100,3},{4,30,6},1,0},air.backend(),result));
    CHECK(result.outcome==GrenadeTermination::Burst&&result.steps==46&&result.time==45);
    CHECK(air.starts.size()==45&&air.origins.size()==1);
    CHECK(near(air.candidates[0].y,110)&&near(air.velocities[0].y,10));
    CHECK(near(result.position.x,191)&&near(result.position.y,-19250)&&near(result.position.z,273));
    CHECK(near(air.origins[0].x,181)&&near(air.origins[0].y,-19250));
    CHECK(near(air.bursts[0].x,181));
    // A fractional native dt overshoots the fuse: one extra zeroing movement.
    World overshoot;
    CHECK(predict_grenade_explosion({{0,0,0},{1,0,0},2,0},overshoot.backend(),result));
    CHECK(result.steps==25&&result.time==48&&overshoot.starts.size()==24);
    CHECK(near(result.position.x,48)&&near(result.position.y,-24000));
    World fractional;
    CHECK(predict_grenade_explosion({{0,0,0},{1,0,0},1.2f,0},fractional.backend(),result));
    CHECK(result.steps==40&&near(result.time,46.8f)&&fractional.starts.size()==39);
    CHECK(near(result.position.x,46.8f));
    World half;
    CHECK(predict_grenade_explosion({{0,0,0},{1,0,0},.5f,0},half.backend(),result));
    CHECK(result.steps==91&&result.time==45&&half.starts.size()==90);
    // Hand grenades continue past multiple floor contacts until their fuse.
    World bouncing;bouncing.samples={{{0,1,0}},{{0,1,0}}};
    CHECK(predict_grenade_explosion({{0,0,0},{10,0,0},1,0},bouncing.backend(),result));
    CHECK(bouncing.starts.size()==45&&result.time==45);
    CHECK(near(bouncing.velocities[1].x,5)&&near(bouncing.velocities[1].y,30));
    CHECK(near(bouncing.velocities[2].x,2.5f)&&near(bouncing.velocities[2].y,30));
    // Wall, ceiling, then floor. Burst clearance uses PRE-step matrix origin,
    // while its candidate is the sphere-corrected current center.
    for(unsigned kind:{1u,2u}){
        World impact;impact.relocate_x=100;
        impact.samples={{{2,0,0}},{{0,-3,0}},{{0,1,0},0,false,true,{8,25,9}}};
        CHECK(predict_grenade_explosion({{0,100,0},{10,50,8},1,kind},impact.backend(),result));
        CHECK(result.outcome==GrenadeTermination::Burst&&result.steps==3&&result.time==3);
        CHECK(near(impact.candidates[1].x,5)&&near(impact.candidates[1].y,125));
        CHECK(near(impact.candidates[2].x,2.5f)&&near(impact.candidates[2].y,107.5f));
        CHECK(near(impact.origins[0].x,5)&&near(impact.origins[0].y,125));
        CHECK(near(impact.bursts[0].x,8)&&near(impact.bursts[0].y,25));
        CHECK(near(result.position.x,108)&&near(result.position.y,25));
    }
    // Native floor thresholds use RAW normal.y, not normalized slope.
    World raw_floor;raw_floor.samples={{{100,1,0}}};
    CHECK(predict_grenade_explosion({{0,100,0},{1,0,0},1,1},raw_floor.backend(),result));
    CHECK(result.outcome==GrenadeTermination::Burst&&result.steps==1);
    // Material flag1 with raw Y>.8 rests a hand grenade without clearance;
    // the other ordinary grenade kinds terminate without a damaging burst.
    for(unsigned kind=0;kind<3;++kind){
        World material;material.samples={{{100,1,0},1,false,true,{7,25,9}}};
        CHECK(predict_grenade_explosion({{0,100,0},{0,0,0},1,kind},material.backend(),result));
        CHECK(material.starts.size()==1&&material.origins.empty());
        CHECK(result.position.x==7&&result.position.y==25&&result.position.z==9);
        CHECK(result.outcome==(kind?GrenadeTermination::Removed:GrenadeTermination::Burst));
        CHECK(result.steps==(kind?1u:45u)&&result.time==(kind?1.f:45.f));
    }
    // Water callback supplies the researched height+20 corrected center.
    for(unsigned kind=0;kind<3;++kind){
        World water;water.samples={{{},0,true,true,{3,120,5}}};
        CHECK(predict_grenade_explosion({{0,0,0},{0,0,0},1,kind},water.backend(),result));
        CHECK(water.starts.size()==1&&water.origins.empty());
        CHECK(result.position.x==3&&result.position.y==120&&result.position.z==5);
        CHECK(result.outcome==(kind?GrenadeTermination::Removed:GrenadeTermination::Burst));
        CHECK(result.steps==(kind?1u:45u));
    }
    const auto saved=result;
    auto unchanged=[&](){CHECK(result.position.x==saved.position.x&&result.position.y==saved.position.y
        &&result.position.z==saved.position.z&&result.time==saved.time&&result.steps==saved.steps&&result.outcome==saved.outcome);};
    World budget;CHECK(!predict_grenade_explosion({{0,0,0},{0,0,0},.001f,0},budget.backend(),result,~0u));
    CHECK(budget.starts.size()==256);unchanged();
    World missing_floor;CHECK(!predict_grenade_explosion({{0,0,0},{0,0,0},1,1},missing_floor.backend(),result));
    CHECK(missing_floor.starts.size()==96);unchanged();
    for(unsigned kind:{1u,2u}){
        World expires;
        CHECK(predict_grenade_explosion({{0,0,0},{1,0,0},2,kind},expires.backend(),result,256));
        CHECK(result.outcome==GrenadeTermination::Removed&&result.steps==151&&result.time==300);
        CHECK(expires.starts.size()==150&&expires.origins.empty());
    }
    result=saved;
    World entry_budget;CHECK(!predict_grenade_explosion({{0,0,0},{0,0,0},1,0},entry_budget.backend(),result,45));
    CHECK(entry_budget.starts.size()==45&&entry_budget.origins.empty());unchanged();
    World zero;CHECK(!predict_grenade_explosion({},zero.backend(),result,0));CHECK(zero.starts.empty());unchanged();
    // Native zero-speed reflection is undefined and must not invent a bounce.
    World stopped;stopped.samples={{{0,1,0}}};
    CHECK(!predict_grenade_explosion({{0,0,0},{0,20,0},1,1},stopped.backend(),result));unchanged();
    for(unsigned mode=0;mode<12;++mode){
        World invalid;GrenadeExplosionLaunch launch{{0,0,0},{0,0,0},1,0};
        if(mode==0)launch.dt=0;
        if(mode==1)launch.dt=-1;
        if(mode==2)launch.dt=std::numeric_limits<float>::infinity();
        if(mode==3)launch.dt=std::numeric_limits<float>::quiet_NaN();
        if(mode==4)launch.kind=3;
        if(mode==5)launch.position.x=1e20f;
        if(mode==6)launch.velocity.y=1e20f;
        if(mode==7)invalid.invalid_at=2;
        if(mode==8)invalid.samples={{{0,std::numeric_limits<float>::quiet_NaN(),0}}};
        if(mode==9)invalid.samples={{{},0,false,true,{0,1e20f,0}}};
        if(mode==10)invalid.relocate_valid=false;
        if(mode==11)invalid.relocate_bad=true;
        CHECK(!predict_grenade_explosion(launch,invalid.backend(),result));unchanged();
    }
    CHECK(!predict_grenade_explosion({}, {},result));unchanged();
    auto no_clearance=air.backend();no_clearance.relocate=nullptr;
    CHECK(!predict_grenade_explosion({},no_clearance,result));unchanged();
    std::puts("grenade explosion prediction tests: PASS");
}
