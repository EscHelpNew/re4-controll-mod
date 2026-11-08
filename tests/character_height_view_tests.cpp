#include "re4/character_height_view.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace {
using namespace re4;
constexpr ViewMatrix3x4 identity{{{1,0,0,0},{0,1,0,0},{0,0,1,0}}};
void near(float a,float b){assert(std::fabs(a-b)<.0003f);}
CharacterRetargetMap measured_map(float difference){
    CharacterRetargetMap map;map.count=26;
    for(unsigned i=0;i<26;++i)map.bones[i].source=std::uint8_t(i);
    map.source_bind[4]={0,1650,0};map.bones[4].translation={0,-difference,0};return map;
}
ViewVec3 transform(const ViewMatrix3x4& matrix,ViewVec3 point){
    ViewVec3 value;assert(view_transform_point(matrix,point,value));return value;
}
void camera_difference_and_direction(){
    // The metric is the immutable donor/Leon head bind displacement, including
    // root height. An animated bend must not become a camera height command.
    auto map=measured_map(-548.309f);const auto immutable=map;float difference=999;
    assert(character_height_difference(map,difference));near(difference,-548.309f);
    assert(!std::memcmp(&map,&immutable,sizeof(map)));
    for(float delta:{-548.309f,-118.731f,0.f,300.f}){
        map=measured_map(delta);assert(character_height_difference(map,difference));near(difference,delta);
        const ViewQfpsOutput native{{-500,1765,-1190},{-260,1630,-125},{-500,1765,1190},.07f,45};
        // Captured native bases can be translated, turned and scaled; the
        // correction must remain world-vertical and move the wall anchor too.
        const ViewMatrix3x4 basis{{{0,0,2,100},{0,3,0,200},{-4,0,0,-300}}};
        const auto before=native;ViewQfpsOutput result;
        assert(character_height_camera(native,basis,difference,result));
        const ViewVec3 inputs[]{native.eye0,native.eye1,native.target};
        const ViewVec3 outputs[]{result.eye0,result.eye1,result.target};
        for(unsigned i=0;i<3;++i){const auto a=transform(basis,inputs[i]),b=transform(basis,outputs[i]);
            near(a.x,b.x);near(a.z,b.z);near(b.y-a.y,delta);}
        near(result.target.x-result.eye0.x,native.target.x-native.eye0.x);
        near(result.target.y-result.eye0.y,native.target.y-native.eye0.y);
        near(result.target.z-result.eye0.z,native.target.z-native.eye0.z);
        assert(result.roll==native.roll&&result.fov==native.fov);
        assert(!std::memcmp(&native,&before,sizeof(native)));
    }
}
void native_projection_framing(){
    // Native perspective convention: clip.w=-view.z, logical512x448.
    const float cot=1/std::tan(45.f*.5f*.0174532925199433f);
    const ViewMatrix4x4 projection{{{cot*.75f,0,0,0},{0,cot,0,0},
        {0,0,-1.001f,-100},{0,0,-1,0}}};
    const ViewMatrix3x4 original_view{{{1,0,0,0},{0,1,0,-1700},{0,0,1,0}}};
    const ViewVec3 leon{0,1650,-1000},donor{0,1101.691f,-1000};
    ViewHudPoint reference,short_before,short_after;
    assert(project_view_point(original_view,projection,leon,reference));
    assert(!project_view_point(original_view,projection,donor,short_before)); // Original short body falls below view.
    ViewQfpsOutput camera{{0,1700,0},{0,1700,0},{0,1700,-1000},0,45},shifted;
    assert(character_height_camera(camera,identity,-548.309f,shifted));
    auto lowered_view=original_view;lowered_view.m[1][3]=-shifted.eye0.y;
    assert(project_view_point(lowered_view,projection,donor,short_after));
    near(short_after.x,reference.x);near(short_after.y,reference.y);
    // Case view lifts geometry instead of moving the native preview actor.
    ViewMatrix3x4 case_view;assert(character_height_case_view(original_view,-548.309f,case_view));
    assert(project_view_point(case_view,projection,donor,short_after));
    near(short_after.x,reference.x);near(short_after.y,reference.y);
}
void case_rotation_and_native_state(){
    const ViewMatrix3x4 native{{{.8f,.3f,.1f,13},{-.2f,.6f,.4f,29},{.1f,-.3f,.9f,-1700}}};
    const auto before=native;ViewMatrix3x4 result;
    assert(character_height_case_view(native,-548.309f,result));
    for(unsigned r=0;r<3;++r){
        for(unsigned c=0;c<3;++c)assert(result.m[r][c]==native.m[r][c]);
        near(result.m[r][3],native.m[r][3]+native.m[r][1]*548.309f);
    }
    // Every body/knife/held mesh receives the same view, including yawed ones.
    for(float yaw:{0.f,.7f,2.4f}){
        const ViewVec3 p{200*std::cos(yaw),1101.691f,200*std::sin(yaw)};
        const auto actual=transform(result,p),expected=transform(native,{p.x,p.y+548.309f,p.z});
        near(actual.x,expected.x);near(actual.y,expected.y);near(actual.z,expected.z);
    }
    assert(!std::memcmp(&native,&before,sizeof(native)));
    assert(character_height_case_view(native,300,result));assert(!std::memcmp(&result,&native,sizeof(native)));
    assert(character_height_case_view(native,0,result));assert(!std::memcmp(&result,&native,sizeof(native)));
}
void invalid_inputs_do_not_publish(){
    auto map=measured_map(-100);float value=17;
    map.count=25;assert(!character_height_difference(map,value));assert(value==17);
    map=measured_map(-100);map.bones[4].source=3;assert(!character_height_difference(map,value));assert(value==17);
    map=measured_map(-100);map.bones[4].translation.y=std::numeric_limits<float>::quiet_NaN();
    assert(!character_height_difference(map,value));assert(value==17);
    const ViewQfpsOutput native{{1,2,3},{4,5,6},{7,8,9},0,45};
    auto output=native;ViewMatrix3x4 singular{};
    assert(!character_height_camera(native,singular,-100,output));assert(!std::memcmp(&output,&native,sizeof(native)));
    auto bad_basis=identity;bad_basis.m[0][3]=std::numeric_limits<float>::quiet_NaN();
    assert(!character_height_camera(native,bad_basis,-100,output));assert(!std::memcmp(&output,&native,sizeof(native)));
    assert(!character_height_camera(native,identity,std::numeric_limits<float>::infinity(),output));
    assert(!std::memcmp(&output,&native,sizeof(native)));
    auto bad=native;bad.eye1.y=std::numeric_limits<float>::quiet_NaN();
    assert(!character_height_camera(bad,identity,-100,output));assert(!std::memcmp(&output,&native,sizeof(native)));
    auto view=identity;const auto unchanged=view;
    assert(!character_height_case_view(singular,-100,view));assert(!std::memcmp(&view,&unchanged,sizeof(view)));
    assert(!character_height_case_view(identity,std::numeric_limits<float>::quiet_NaN(),view));
    assert(!std::memcmp(&view,&unchanged,sizeof(view)));
}
}
int main(){
    camera_difference_and_direction();native_projection_framing();case_rotation_and_native_state();invalid_inputs_do_not_publish();
    std::puts("character height: measured bind delta, camera direction/collision anchor, case projection/rotation PASS");
}
