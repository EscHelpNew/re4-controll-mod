#include "re4/character_emitter.hpp"
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>
#include <cstdio>
namespace {
constexpr re4::ViewMatrix3x4 identity{{{1,0,0,0},{0,1,0,0},{0,0,1,0}}};
bool near(re4::ViewVec3 a,re4::ViewVec3 b,float epsilon=.025f){
    return std::fabs(a.x-b.x)<epsilon&&std::fabs(a.y-b.y)<epsilon&&std::fabs(a.z-b.z)<epsilon;
}
void verify_sites(){
    using M=re4::CharacterEmitterMath;using S=re4::CharacterEmitterSource;
    const std::uintptr_t body[]{0x31DF10,0x340190,0x340D00,0x3416F8,0x34212C,0x3483B0,
        0x34A4F8,0x34C110,0x34CBE0,0x34EEE0,0x350858,0x351234,0x352BB0};
    assert(re4::character_emitter_site(M::Point,0x2E8FB0).source==S::Laser);
    for(auto lr:body)assert(re4::character_emitter_site(M::Point,lr).source==S::BodyHand);
    for(auto lr:{0x1D6A4Cu,0x1F4A48u,0x1F68ACu,0x1E0194u})
        assert(re4::character_emitter_site(M::Point,lr).source==S::Held);
    for(auto lr:{0x1F4364u,0x1F4424u,0x1F4CF0u,0x1F5284u,0x1F59E0u,0x1D7F48u}){
        auto s=re4::character_emitter_site(M::Product,lr);assert(s.source==S::Held&&s.operand==1);
    }
    assert(re4::character_emitter_site(M::Product,0x1F4C8C).operand==0);
    assert(re4::character_emitter_site(M::Copy,0x1D7DA4).source==S::Held);
    assert(re4::character_emitter_site(M::Copy,0x1E81F0).source==S::Emitter);
    // Native skin, owned draw and already-derived particle matrices cannot
    // receive a second translation. A valid address in the wrong ABI is denied.
    for(auto m:{M::Point,M::Product,M::Copy})
        for(auto pc:{0u,0x4EC134u,0x4EC14Cu,0x4ED684u,0x4ED698u,0x1F4CA8u,0x1F4D00u,0x1F6A38u})
            assert(re4::character_emitter_site(m,pc).source==S::None);
    assert(re4::character_emitter_site(M::Copy,0x2E8FB0).source==S::None);
    assert(re4::character_emitter_site(M::Point,0x2E8FAC).source==S::None);
}
void verify_math(){
    using namespace re4;
    CharacterBindBone source[26]{},donor[26]{};ViewVec3 bind[26]{};
    for(unsigned i=0;i<26;++i){
        source[i].id=donor[i].id=std::uint8_t(i);
        source[i].parent=donor[i].parent=character_core_parents[i];
        source[i].position=i?ViewVec3{float(int(i%3)-1)*30,float(i%2)*25,8}:ViewVec3{0,1000,0};
        donor[i].position=i?ViewVec3{source[i].position.x*.73f,source[i].position.y*.61f,12}:ViewVec3{0,700,0};
    }
    CharacterRetargetMap map{};assert(character_retarget_map(source,26,donor,26,map));
    assert(character_bind_positions(source,26,bind));
    ViewMatrix3x4 world[26]{},inverse_bind[26]{},palette[26]{};
    for(unsigned i=0;i<26;++i){
        world[i]=inverse_bind[i]=palette[i]=identity;
        world[i].m[0][3]=bind[i].x;world[i].m[1][3]=bind[i].y;world[i].m[2][3]=bind[i].z;
        inverse_bind[i].m[0][3]=-bind[i].x;inverse_bind[i].m[1][3]=-bind[i].y;inverse_bind[i].m[2][3]=-bind[i].z;
        palette[i].m[1][3]=-1000;
    }
    CharacterRetargetPose reference{};assert(character_retarget_pose(map,palette,26,reference));
    CharacterEmitterPose neutral{};assert(character_emitter_pose(map,world,inverse_bind,neutral));
    for(unsigned i=0;i<3;++i)assert(near(neutral.delta[i],reference.joint_shift[i==0?10:i==1?16:17]));
    const ViewMatrix3x4 root{{{0,-2,.2f,100000},{1,0,0,-20000},{0,0,.5f,30000}}};
    ViewMatrix3x4 moved[26]{};
    for(unsigned i=0;i<26;++i)assert(character_emitter_detail::product(root,world[i],moved[i]));
    CharacterEmitterPose transformed{};assert(character_emitter_pose(map,moved,inverse_bind,transformed));
    auto linear=root;for(auto& row:linear.m)row[3]=0;
    for(unsigned i=0;i<3;++i){ViewVec3 expected{};assert(view_transform_point(linear,neutral.delta[i],expected));assert(near(transformed.delta[i],expected));}
    auto before_world=moved[2];const auto before_map=map;
    moved[2].m[1][3]+=40;CharacterEmitterPose animated{};
    assert(character_emitter_pose(map,moved,inverse_bind,animated));
    assert(!near(animated.delta[0],transformed.delta[0],.0001f)||!near(animated.delta[1],transformed.delta[1],.0001f));
    moved[2]=before_world;assert(!std::memcmp(&before_map,&map,sizeof(map)));
    const auto before=transformed;
    assert(!character_emitter_pose(map,nullptr,inverse_bind,transformed));assert(!std::memcmp(&before,&transformed,sizeof(before)));
    const auto saved=moved[0];moved[0]={};
    assert(!character_emitter_pose(map,moved,inverse_bind,transformed));assert(!std::memcmp(&before,&transformed,sizeof(before)));moved[0]=saved;
    moved[25].m[0][0]=std::numeric_limits<float>::quiet_NaN();
    assert(!character_emitter_pose(map,moved,inverse_bind,transformed));assert(!std::memcmp(&before,&transformed,sizeof(before)));
    assert(!character_emitter_pose(map,world,nullptr,transformed));
    assert(!character_emitter_pose(map,world,inverse_bind,*reinterpret_cast<CharacterEmitterPose*>(world)));
    ViewMatrix3x4 shifted{};
    assert(character_emitter_translate(root,{1,2,3},shifted));
    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)assert(shifted.m[r][c]==root.m[r][c]);
    assert(shifted.m[0][3]==100001&&shifted.m[1][3]==-19998&&shifted.m[2][3]==30003);
    const auto valid=shifted;
    assert(!character_emitter_translate(root,{std::numeric_limits<float>::infinity(),0,0},shifted));
    assert(!std::memcmp(&shifted,&valid,sizeof(valid)));
    assert(character_emitter_translate(shifted,{-1,-2,-3},shifted));assert(!std::memcmp(&shifted,&root,sizeof(root)));
}
}
int main(){verify_sites();verify_math();std::puts("current-pose emitter math and exact consumer policy PASS");}
