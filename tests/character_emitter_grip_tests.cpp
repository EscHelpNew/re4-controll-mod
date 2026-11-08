#define main character_grip_fixture_main
#include "character_grip_tests.cpp"
#undef main
#include "re4/character_emitter.hpp"

int main(){
    Rig rig({4,-2,0});
    const auto fitted=rig.fit(),natural=rig.natural();
    assert(!near(fitted.joint_shift[10],natural.joint_shift[10]));
    const re4::ViewMatrix3x4 actor{{{0,-1,0,700},{1,0,0,-1200},{0,0,1,30}}};
    re4::ViewMatrix3x4 world[26]{},inverse[26]{};
    for(unsigned i=0;i<26;++i){
        auto local=identity;inverse[i]=identity;
        for(unsigned r=0;r<3;++r){
            const float p=r==0?rig.source[i].x:r==1?rig.source[i].y:rig.source[i].z;
            local.m[r][3]=p;inverse[i].m[r][3]=-p;
        }
        assert(re4::character_emitter_detail::product(actor,local,world[i]));
    }
    const auto original_world=world[10],original_inverse=inverse[10];
    re4::CharacterEmitterPose emitter{};
    assert(re4::character_emitter_pose(rig.map,world,inverse,emitter,true));
    auto linear=actor;for(auto& row:linear.m)row[3]=0;
    re4::ViewVec3 expected{};assert(re4::view_transform_point(linear,fitted.joint_shift[10],expected));
    assert(near(emitter.delta[0],expected,2e-4)&&near(emitter.delta[1],expected,2e-4));
    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c){
        assert(std::fabs(emitter.source10.m[r][c]-rig.palette[10].m[r][c])<2e-4);
        assert(std::fabs(emitter.donor10.m[r][c]-fitted.core[10].m[r][c])<2e-4);
    }
    assert(!std::memcmp(&original_world,&world[10],sizeof(original_world)));
    assert(!std::memcmp(&original_inverse,&inverse[10],sizeof(original_inverse)));
    re4::CharacterEmitterPose disabled{};
    assert(re4::character_emitter_pose(rig.map,world,inverse,disabled));
    assert(re4::view_transform_point(linear,natural.joint_shift[10],expected));
    assert(near(disabled.delta[0],expected,2e-4));
    assert(!near(disabled.delta[0],emitter.delta[0]));
    std::puts("fresh emitter FK uses the identical grip solution and world translation PASS");
}
