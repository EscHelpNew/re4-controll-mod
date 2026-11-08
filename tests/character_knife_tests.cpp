#include "re4/character_knife.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define CHECK(c) do{if(!(c)){std::fprintf(stderr,"character knife line %d: %s\n",__LINE__,#c);std::exit(1);}}while(false)
using namespace re4;
namespace {
struct KnifeFixture {
    alignas(32) std::array<std::uint8_t,0x180> raw{};
    std::array<CharacterBindBone,26> bind{};
    template<class T> void put(std::size_t offset,T value){std::memcpy(raw.data()+offset,&value,sizeof(value));}
    CharacterBytes bytes(){return {raw.data(),raw.size()};}
    KnifeFixture(){
        for(unsigned i=0;i<26;++i){bind[i].id=std::uint8_t(i);bind[i].parent=character_core_parents[i];}
        bind[10].position={-2,0,0};
        put(0,std::uint64_t(0xA0));raw[0x29]=1;raw[0x28]=1;
        put(0x20,std::uint64_t(0xB0));raw[0xA0]=10;raw[0xA1]=9;
        raw[0xB0]=10;raw[0xB3]=1;raw[0xB4]=100;
        put(0x48,std::uint64_t(0xC0));put(0x50,std::uint64_t(0xF0));
        put(0x58,std::uint16_t(4));put(0x5A,std::uint16_t(1));
        put(0x70,std::uint64_t(0xFC));put(0x78,std::uint64_t(0x104));
        const ViewVec3 positions[]={{-14,-6,-8},{-10,-2,2},{-12,-6,4},{-14,-2,-8}};
        std::memcpy(raw.data()+0xC0,positions,sizeof(positions));put(0xF0,ViewVec3{0,0,1});
    }
};
bool near(float a,float b,float tolerance=1e-4f){return std::fabs(a-b)<=tolerance;}
bool near(ViewVec3 a,ViewVec3 b,float tolerance=1e-4f){return near(a.x,b.x,tolerance)&&near(a.y,b.y,tolerance)&&near(a.z,b.z,tolerance);}
ViewVec3 point(const ViewMatrix3x4& m,ViewVec3 v){
    return {m.m[0][0]*v.x+m.m[0][1]*v.y+m.m[0][2]*v.z+m.m[0][3],
            m.m[1][0]*v.x+m.m[1][1]*v.y+m.m[1][2]*v.z+m.m[1][3],
            m.m[2][0]*v.x+m.m[2][1]*v.y+m.m[2][2]*v.z+m.m[2][3]};
}
ViewMatrix3x4 multiply(const ViewMatrix3x4& a,const ViewMatrix3x4& b){
    ViewMatrix3x4 out{};
    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c){
        for(unsigned k=0;k<3;++k)out.m[r][c]+=a.m[r][k]*b.m[k][c];
        if(c==3)out.m[r][c]+=a.m[r][3];
    }
    return out;
}
float distance(ViewVec3 a,ViewVec3 b){return std::hypot(a.x-b.x,a.y-b.y,a.z-b.z);}
void proper_rotation(const ViewMatrix3x4& m){
    const auto& r=m.m;
    const double determinant=double(r[0][0])*(double(r[1][1])*r[2][2]-double(r[1][2])*r[2][1])
        -double(r[0][1])*(double(r[1][0])*r[2][2]-double(r[1][2])*r[2][0])
        +double(r[0][2])*(double(r[1][0])*r[2][1]-double(r[1][1])*r[2][0]);
    CHECK(std::fabs(determinant-1)<1e-6);
    for(unsigned a=0;a<3;++a)for(unsigned b=0;b<3;++b){
        double product=0;for(unsigned k=0;k<3;++k)product+=double(r[a][k])*r[b][k];
        CHECK(std::fabs(product-(a==b?1:0))<1e-6);
    }
    for(const auto& row:r)for(float v:row)CHECK(std::isfinite(v));
}
void posed_tests(){
    const CharacterKnifeOrientation orientation{{-2,-4,0},{-1,0,0}};
    const ViewMatrix3x4 posed{{{0,-1,0,10},{1,0,0,20},{0,0,1,30}}};ViewMatrix3x4 h{};
    CHECK(character_knife_half_turn(orientation,posed,h));proper_rotation(h);
    CHECK(near(point(h,{14,18,30}),{14,18,30}));
    CHECK(near(point(h,{14,8,22}),{14,8,38})); // Posed localZ, not globalX.
    CHECK(near(point(h,point(posed,{-12,-6,-8})),point(posed,{-12,-2,8})));
    CHECK(near(point(h,point(h,{19,-8,4})),{19,-8,4}));
    // A nonuniform/sheared skin matrix still produces a rigid rotation; it
    // cannot smuggle that scaling into the post-skin orientation correction.
    const ViewMatrix3x4 scaled{{{2,.5f,.2f,7},{.25f,3,.4f,8},{.5f,.2f,4,9}}};
    CHECK(character_knife_half_turn(orientation,scaled,h));proper_rotation(h);
    CHECK(near(point(h,{1,-4.5f,7.2f}),{1,-4.5f,7.2f}));
    CHECK(near(point(h,{9,-3.5f,9.2f}),{9,-3.5f,9.2f})); // Same grasp axis.
    CHECK(near(point(h,{2,-4.5f,3.2f}),{0,-4.5f,11.2f})); // Perpendicular reverses.
    for(const auto a:{ViewVec3{1,2,3},ViewVec3{-12,40,-200}})
        for(const auto b:{ViewVec3{4,5,6},ViewVec3{-4,2,1}})
            CHECK(near(distance(point(h,a),point(h,b)),distance(a,b),1e-3f));
    // Repositioning/turning the whole posed actor commutes with the correction.
    // This catches computing the flip in model/world origin coordinates.
    const ViewMatrix3x4 global{{{.6f,0,.8f,-50},{0,1,0,70},{-.8f,0,.6f,200}}};ViewMatrix3x4 moved_h{};
    CHECK(character_knife_half_turn(orientation,multiply(global,scaled),moved_h));proper_rotation(moved_h);
    for(const auto v:{ViewVec3{1,2,3},ViewVec3{-12,-6,-8},ViewVec3{-2,-4,0}})
        CHECK(near(point(moved_h,point(global,v)),point(global,point(h,v)),1e-3f));
    const ViewMatrix3x4 tiny{{{1e-18f,0,0,0},{0,1e-18f,0,0},{0,0,1e-18f,0}}};
    CHECK(character_knife_half_turn(orientation,tiny,h));proper_rotation(h);
    // A diagonal non-X grasp is derived from bind data, never hardcoded.
    KnifeFixture f;f.bind[9].position={4,5,6};f.bind[10].position={-2,2,-1};CharacterKnifeOrientation prepared;
    CHECK(character_knife_prepare(f.bytes(),f.bind.data(),26,prepared));
    CHECK(near(prepared.bind_pivot,{2,-4,5})&&near(prepared.bind_axis,{-2.f/3,2.f/3,-1.f/3}));
    CHECK(character_knife_half_turn(prepared,posed,h));proper_rotation(h);
    CHECK(near(point(h,point(posed,prepared.bind_pivot)),point(posed,prepared.bind_pivot)));
}
void invalid_prepare_tests(){
    for(unsigned fault=0;fault<25;++fault){
        KnifeFixture f;CharacterKnifeOrientation out{{7,8,9},{2,3,4}},before=out;
        auto bytes=f.bytes();const CharacterBindBone* bind=f.bind.data();unsigned count=26;
        if(fault==0)bytes.data=nullptr;
        if(fault==1)bytes.size=0x90;
        if(fault==2){++bytes.data;--bytes.size;}
        if(fault==3)bytes.size=character_archive_limit+1;
        if(fault==4)f.put(0x48,std::uint64_t(0xC1));
        if(fault==5)f.put(0x48,std::numeric_limits<std::uint64_t>::max());
        if(fault==6)f.put(0x58,std::uint16_t(0));
        if(fault==7)f.put(0x58,std::uint16_t(0xFFFF));
        if(fault==8)f.put(0xC0,ViewVec3{NAN,0,0});
        if(fault==9)f.put(0xF0,ViewVec3{0,INFINITY,0});
        if(fault==10)f.put(0xFC,std::uint16_t(1));
        if(fault==11)f.raw[0xB0]=9;
        if(fault==12)f.raw[0xB3]=2;
        if(fault==13)f.raw[0x28]=0;
        if(fault==14)f.put(0x38,std::uint32_t(0x100000));
        if(fault==15)bind=nullptr;
        if(fault==16)count=25;
        if(fault==17)count=256;
        if(fault==18)f.bind[10].parent=10;
        if(fault==19)f.bind[25].position.x=NAN;
        if(fault==20)f.bind[10].position={0,0,0};
        if(fault==21)for(unsigned i=0;i<4;++i)f.put(0xC4+i*12,-4.f); // Zero cross-section.
        if(fault==22)f.put(0x78,std::uint64_t(0x105));
        if(fault==23)f.put(0x104,std::uint16_t(1));
        if(fault==24){bytes.data=reinterpret_cast<const std::uint8_t*>(~std::uintptr_t(15));bytes.size=64;}
        CHECK(!character_knife_prepare(bytes,bind,count,out));CHECK(!std::memcmp(&out,&before,sizeof(out)));
    }
    KnifeFixture f;const auto original=f.raw;
    CHECK(!character_knife_prepare(f.bytes(),f.bind.data(),26,*reinterpret_cast<CharacterKnifeOrientation*>(f.raw.data()+0xC0)));
    CHECK(f.raw==original);
    const auto original_bind=f.bind;
    CHECK(!character_knife_prepare(f.bytes(),f.bind.data(),26,*reinterpret_cast<CharacterKnifeOrientation*>(f.bind.data())));
    CHECK(!std::memcmp(f.bind.data(),original_bind.data(),sizeof(f.bind)));
}
void invalid_turn_tests(){
    for(unsigned fault=0;fault<9;++fault){
        CharacterKnifeOrientation orientation{{-2,-4,0},{-1,0,0}};
        ViewMatrix3x4 skin{{{1,0,0,0},{0,1,0,0},{0,0,1,0}}},out{{{8,1,2,3},{4,5,6,7},{9,8,7,6}}};const auto before=out;
        if(fault==0)orientation.bind_axis={0,0,0};
        if(fault==1)orientation.bind_axis.x=NAN;
        if(fault==2)orientation.bind_pivot.z=INFINITY;
        if(fault==3)skin.m[1][2]=NAN;
        if(fault==4)skin.m[1][1]=0;
        if(fault==5)skin.m[1][1]=1e-15f;
        if(fault==6)skin.m[2][3]=std::numeric_limits<float>::max(); // Unrepresentable translation.
        if(fault==7)skin.m[0][3]=INFINITY;
        if(fault==8)skin={};
        CHECK(!character_knife_half_turn(orientation,skin,out));CHECK(!std::memcmp(&out,&before,sizeof(out)));
    }
    CharacterKnifeOrientation orientation{{-2,-4,0},{-1,0,0}};
    ViewMatrix3x4 skin{{{1,0,0,0},{0,1,0,0},{0,0,1,0}}};const auto before=skin;
    CHECK(!character_knife_half_turn(orientation,skin,skin));CHECK(!std::memcmp(&skin,&before,sizeof(skin)));
    alignas(ViewMatrix3x4) std::array<std::uint8_t,sizeof(ViewMatrix3x4)> storage{};
    std::memcpy(storage.data(),&orientation,sizeof(orientation));const auto original=storage;
    CHECK(!character_knife_half_turn(*reinterpret_cast<CharacterKnifeOrientation*>(storage.data()),skin,
        *reinterpret_cast<ViewMatrix3x4*>(storage.data())));CHECK(storage==original);
}
}
int main(){
    KnifeFixture f;CharacterKnifeOrientation orientation;
    const auto original_raw=f.raw;const auto original_bind=f.bind;
    CHECK(character_knife_prepare(f.bytes(),f.bind.data(),f.bind.size(),orientation));
    CHECK(f.raw==original_raw&&!std::memcmp(f.bind.data(),original_bind.data(),sizeof(f.bind)));
    CHECK(near(orientation.bind_pivot,{-2,-4,0})&&near(orientation.bind_axis,{-1,0,0}));
    const ViewMatrix3x4 identity{{{1,0,0,0},{0,1,0,0},{0,0,1,0}}};ViewMatrix3x4 half;
    CHECK(character_knife_half_turn(orientation,identity,half));
    ViewVec3 result;
    CHECK(view_transform_point(half,{-12,-4,-8},result)&&near(result,{-12,-4,8}));
    CHECK(view_transform_point(half,orientation.bind_pivot,result)&&near(result,orientation.bind_pivot));
    proper_rotation(half);
    CHECK(view_transform_point(half,{-12,-6,-8},result)&&near(result,{-12,-2,8}));
    posed_tests();invalid_prepare_tests();invalid_turn_tests();
    std::puts("character knife: bounded preparation, posed grasp half-turn, determinant, lengths, covariance and atomic rejection PASS");
}
