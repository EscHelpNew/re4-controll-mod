#include "re4/grenade_prediction.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

#define CHECK(c) do { if(!(c)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#c);std::exit(1);} } while(0)
using namespace re4;
bool near(float a,float b){return std::fabs(a-b)<.001f;}
struct Sweep {
    std::vector<ViewVec3> previous,candidate;
    std::vector<ViewVec3> normals;
    unsigned floor_at=0,invalid_at=0;
    ViewVec3 corrected{7,25,11};
    bool bad_point=false,bad_normal=false;
    GrenadePhysicsBackend backend(){return {this,[](void* p,ViewVec3 from,ViewVec3& to,ViewVec3& normal){
        auto& s=*static_cast<Sweep*>(p);
        s.previous.push_back(from);s.candidate.push_back(to);
        const auto index=s.candidate.size();
        if(index==s.invalid_at)return false;
        if(index<=s.normals.size())normal=s.normals[index-1];
        if(index==s.floor_at){to=s.corrected;normal={0,2,0};}
        if(s.bad_point)to.x=std::numeric_limits<float>::infinity();
        if(s.bad_normal)normal.x=std::numeric_limits<float>::quiet_NaN();
        return true;
    }};}
};
int main(){
    GrenadeLanding landing{{91,92,93},{1,0,0},9,7};
    Sweep floor;floor.floor_at=2;
    CHECK(predict_grenade_landing({{1,100,3},{4,30,6},.5f,45},floor.backend(),landing));
    CHECK(floor.previous.size()==2);
    CHECK(near(floor.candidate[0].x,3)&&near(floor.candidate[0].y,110)&&near(floor.candidate[0].z,6));
    CHECK(near(floor.candidate[1].x,5)&&near(floor.candidate[1].y,115)&&near(floor.candidate[1].z,9));
    CHECK(near(landing.position.x,7)&&near(landing.position.y,0)&&near(landing.position.z,11));
    CHECK(near(landing.normal.y,1)&&near(landing.time,1)&&landing.steps==2);
    // A wall and then a ceiling reflect and halve velocity before the floor.
    Sweep bounces;bounces.normals={{2,0,0},{0,-3,0}};bounces.floor_at=3;
    CHECK(predict_grenade_landing({{0,100,0},{10,50,8},1,45},bounces.backend(),landing));
    CHECK(near(bounces.candidate[0].x,10)&&near(bounces.candidate[0].y,130));
    CHECK(near(bounces.candidate[1].x,5)&&near(bounces.candidate[1].y,125));
    CHECK(near(bounces.candidate[2].x,2.5f)&&near(bounces.candidate[2].y,107.5f));
    CHECK(landing.steps==3);
    // Normalize before classifying: native floor cutoff is strictly above .7.
    Sweep slope;slope.normals={{0,.7f,.71414286f}};slope.floor_at=2;
    CHECK(predict_grenade_landing({{0,100,0},{0,0,0},1,45},slope.backend(),landing));
    CHECK(landing.steps==2);
    Sweep normalized;normalized.normals={{0,8,6}};
    CHECK(predict_grenade_landing({{0,100,0},{0,0,0},1,45},normalized.backend(),landing));
    CHECK(near(landing.normal.y,.8f)&&near(landing.normal.z,.6f));
    CHECK(near(landing.position.y,60)&&near(landing.position.z,-15));
    const GrenadeLanding saved=landing;
    auto unchanged=[&](){CHECK(landing.position.x==saved.position.x&&landing.position.y==saved.position.y
        &&landing.position.z==saved.position.z&&landing.normal.x==saved.normal.x
        &&landing.normal.y==saved.normal.y&&landing.normal.z==saved.normal.z
        &&landing.time==saved.time&&landing.steps==saved.steps);};
    Sweep miss;CHECK(!predict_grenade_landing({{0,0,0},{0,0,0},1,45},miss.backend(),landing));
    CHECK(miss.candidate.size()==45);unchanged();
    Sweep fuse;fuse.floor_at=3;
    CHECK(!predict_grenade_landing({{0,0,0},{0,0,0},1,2.5f},fuse.backend(),landing));
    CHECK(fuse.candidate.size()==2);unchanged();
    Sweep steps;CHECK(!predict_grenade_landing({{0,0,0},{0,0,0},.001f,300},steps.backend(),landing,~0u));
    CHECK(steps.candidate.size()==96);unchanged();
    Sweep short_limit;CHECK(!predict_grenade_landing({{0,0,0},{0,0,0},1,45},short_limit.backend(),landing,2));
    CHECK(short_limit.candidate.size()==2);unchanged();
    for(float dt:{0.f,-1.f,2.01f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
        Sweep bad;CHECK(!predict_grenade_landing({{0,0,0},{0,0,0},dt,45},bad.backend(),landing));
        CHECK(bad.candidate.empty());unchanged();
    }
    for(float limit:{0.f,-1.f,301.f,std::numeric_limits<float>::infinity()}){
        Sweep bad;CHECK(!predict_grenade_landing({{0,0,0},{0,0,0},1,limit},bad.backend(),landing));
        CHECK(bad.candidate.empty());unchanged();
    }
    for(unsigned mode=0;mode<6;++mode){
        Sweep bad;GrenadeLaunch launch{{0,0,0},{0,0,0},1,45};
        if(mode==0)bad.invalid_at=1;
        if(mode==1)bad.bad_point=true;
        if(mode==2)bad.bad_normal=true;
        if(mode==3)launch.position.x=1e20f;
        if(mode==4)launch.velocity.y=1e20f;
        if(mode==5)launch.position.z=std::numeric_limits<float>::quiet_NaN();
        CHECK(!predict_grenade_landing(launch,bad.backend(),landing));unchanged();
    }
    CHECK(!predict_grenade_landing({}, {},landing));unchanged();
    Sweep zero;CHECK(!predict_grenade_landing({},zero.backend(),landing,0));CHECK(zero.candidate.empty());unchanged();
    std::puts("grenade prediction tests: PASS");
}
