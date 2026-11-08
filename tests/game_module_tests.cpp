// Exercise the actual game adapter callbacks against a bounded fake game image.
// Only external engine/SDK calls are substituted; no extracted game is needed.
#include <cstdint>
#include <cstddef>
namespace re4 {struct ViewVec3;struct ViewMatrix3x4;struct CharacterWeight8;}
namespace exl::hook {struct InlineFloatCtx;}
namespace {
struct NpadState;
void test_FullPad(NpadState*, const std::uint32_t&);
void test_HandheldPad(NpadState*, const std::uint32_t&);
void test_DualPad(NpadState*, const std::uint32_t&);
int test_PadNormalize(void*);
void test_MovementTurn(void*);
unsigned test_MotionAdvance(void*);
void test_CameraInput(void*);
void test_CameraCalculate();
void test_CameraFollow(void*,void*);
void test_FinalLookAt(void*,const re4::ViewVec3*,const re4::ViewVec3*,const re4::ViewVec3*);
void test_CameraApply(void*,void*);
void test_CameraBasis(void*,void*);
void test_CameraCollision(void*,void*,void*,void*);
void test_ModelSubmit(void*);
void test_WeaponLaser(void*,unsigned,unsigned,unsigned);
void test_HudDraw(void*);
void test_ModernPlayerUpdate(void*);
unsigned test_ModernRoot(void*,void*);
void test_ModernPose(void*,int);
void test_AutomaticFire(void*);
void test_ScopeUpdate(void*);
void test_ScopedZoom(exl::hook::InlineFloatCtx*);
void test_PickupConfirmation(exl::hook::InlineFloatCtx*);
void test_PickupSuccess(exl::hook::InlineFloatCtx*);
void test_InventoryDamageName(exl::hook::InlineFloatCtx*);
void test_StartupLogos(void*);
std::uint64_t test_ExtendedCanTune(void*,void*);
void test_ExtendedFullyTuned(exl::hook::InlineFloatCtx*);
void test_ExtendedRowPrice(exl::hook::InlineFloatCtx*);
void test_ExtendedConfirmEnter(exl::hook::InlineFloatCtx*);
void test_ExtendedCommit(exl::hook::InlineFloatCtx*);
void test_ExtendedLevelBar(void*,unsigned,unsigned);
void test_ExtendedPanel(void*,void*,unsigned);
void test_ExtendedSquareDraw(void*);
void test_ExtendedDamage(exl::hook::InlineFloatCtx*);
std::uint64_t test_ExtendedSnapshotTake(void*,std::uint64_t,std::uint64_t,std::uint64_t);
std::uint64_t test_ExtendedSnapshotRestore(void*,std::uint64_t,std::uint64_t,std::uint64_t);
void test_ExtendedSave(exl::hook::InlineFloatCtx*);
void test_ExtendedLoad(exl::hook::InlineFloatCtx*);
void test_RootDisplacement(void*,void*,unsigned,re4::ViewVec3*,re4::ViewVec3*);
int test_AimSensitivity(void*, int);
void test_AimSway(void*, unsigned, float*, float*);
void test_AimRaise(void*);
unsigned test_RomMount(const char*,void*,std::size_t);
void test_SamplerCreate(void*,void*,const std::uint8_t*);
void test_ResolutionMap(int,int,int*,int*);
void test_FrustumUpdate(void*);
void test_SceneSubmit();
void test_SceneDraw();
int test_CharacterSkin(void*,void*);
void test_CharacterDraw(void*,void*,const float*,unsigned);
void test_CharacterFrameEnd(void*,unsigned);
void test_CharacterTextureFree(void*,unsigned);
void test_CharacterTextureGroup(void*,unsigned,unsigned);
void test_CharacterTextureMasked(void*,unsigned,unsigned,unsigned);
void test_CharacterTextureSweep(void*);
void test_CharacterRendererDestroy();
void test_CharacterMenuDestroy(void*);
std::uintptr_t test_CharacterRendererInitialize(void*,void*,unsigned,unsigned,unsigned);
int test_CharacterPackOpen(const char*,void*);
void* test_CharacterPackAllocate(unsigned,unsigned);
std::uintptr_t character_emitter_caller=0;
void test_CharacterEmitterPoint(const re4::ViewMatrix3x4*,const re4::ViewVec3*,re4::ViewVec3*);
void test_CharacterEmitterProduct(const re4::ViewMatrix3x4*,const re4::ViewMatrix3x4*,re4::ViewMatrix3x4*);
void test_CharacterEmitterCopy(const re4::ViewMatrix3x4*,re4::ViewMatrix3x4*);
}
#include "../source/game/module.cpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include "re4/reticle_preview.hpp"
extern "C" std::size_t character_pack_budget_headroom(const void*){return 256*1024*1024;}
extern "C" std::size_t re4_native_alloc_size(const void*,const void*){return 0;}
#undef assert
#define assert(c) do { if(!(c)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#c);std::exit(1);} } while(0)
namespace {
int (*character_skin_probe)(void*,void*)=nullptr;
void (*character_draw_probe)(void*,void*,const float*,unsigned)=nullptr;
void (*character_free_probe)(void*,unsigned)=nullptr;
void (*character_frame_probe)()=nullptr;
void (*character_destroy_probe)()=nullptr;
int test_CharacterSkin(void* actor,void* chain){return character_skin_probe?character_skin_probe(actor,chain):1;}
void test_CharacterDraw(void* actor,void* chain,const float* view,unsigned flags){if(character_draw_probe)character_draw_probe(actor,chain,view,flags);}
void test_CharacterFrameEnd(void*,unsigned){if(character_frame_probe)character_frame_probe();}
void test_CharacterTextureFree(void* ctx,unsigned id){if(character_free_probe)character_free_probe(ctx,id);}
void test_CharacterTextureGroup(void*,unsigned,unsigned){}
void test_CharacterTextureMasked(void*,unsigned,unsigned,unsigned){}
void test_CharacterTextureSweep(void*){}
void test_CharacterMenuDestroy(void*){}
void test_CharacterRendererDestroy(){if(character_destroy_probe)character_destroy_probe();}
std::uintptr_t test_CharacterRendererInitialize(void*,void*,unsigned,unsigned,unsigned){return 0;}
int test_CharacterPackOpen(const char*,void*){return 0;}
void* test_CharacterPackAllocate(unsigned,unsigned){return nullptr;}
unsigned character_emitter_point_calls=0,character_emitter_product_calls=0,character_emitter_copy_calls=0;
void test_CharacterEmitterPoint(const re4::ViewMatrix3x4* matrix,const re4::ViewVec3* point,re4::ViewVec3* out){
    ++character_emitter_point_calls;
    const auto& m=matrix->m;const auto p=*point;
    const re4::ViewVec3 value{m[0][0]*p.x+m[0][1]*p.y+m[0][2]*p.z+m[0][3],
        m[1][0]*p.x+m[1][1]*p.y+m[1][2]*p.z+m[1][3],
        m[2][0]*p.x+m[2][1]*p.y+m[2][2]*p.z+m[2][3]};
    *out=value;
}
void test_CharacterEmitterProduct(const re4::ViewMatrix3x4* a,const re4::ViewMatrix3x4* b,re4::ViewMatrix3x4* out){
    ++character_emitter_product_calls;re4::ViewMatrix3x4 value{};
    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c){
        float v=c==3?a->m[r][3]:0;
        for(unsigned k=0;k<3;++k)v+=a->m[r][k]*b->m[k][c];
        value.m[r][c]=v;
    }
    *out=value;
}
void test_CharacterEmitterCopy(const re4::ViewMatrix3x4* matrix,re4::ViewMatrix3x4* out){
    ++character_emitter_copy_calls;std::memmove(out,matrix,sizeof(*matrix));
}
bool modern_probe=false,unified_probe=false;
unsigned aim_raise_native_calls=0,aim_raise_native_change=0;
void test_AimRaise(void* actor){
    ++aim_raise_native_calls;
    if(actor){
        auto* context=*reinterpret_cast<std::uint8_t**>(static_cast<std::uint8_t*>(actor)+0x938);
        if(context)*reinterpret_cast<float*>(context+0x28)=0;
        if(aim_raise_native_change==1)static_cast<std::uint8_t*>(actor)[0x117]=1;
    }
    auto* pitch=reinterpret_cast<float*>(main_base+0x1265A08);pitch[0]=pitch[1]=pitch[2]=0;
    if(aim_raise_native_change==2){auto* state=*reinterpret_cast<std::uint8_t**>(main_base+0xB1AE08);state[0x5092]|=4;}
    if(aim_raise_native_change==3)settings.unified_controls=false;
}
unsigned unified_flags_seen=0,update_state_change=0;
void change_reload_identity(void* actor,unsigned change){
    if(change==2){auto& handler=*reinterpret_cast<std::uintptr_t*>(main_base+0x1265A28);handler=main_base+(handler==main_base+0x31CC60?0x3351A0:0x31CC60);}
    if(change==3){auto* context=*reinterpret_cast<std::uint8_t**>(static_cast<std::uint8_t*>(actor)+0x938);*reinterpret_cast<std::uintptr_t*>(context+0x38)+=16;}
    if(change==4){auto* state=*reinterpret_cast<std::uint8_t**>(main_base+0xB1AE08);++state[0x5034];}
}
unsigned modern_original_updates=0,modern_original_roots=0,modern_original_poses=0;
void (*modern_root_observer)(void*)=nullptr;
bool native_root_probe=false,grenade_native_order=false;
re4::ViewVec3 grenade_release_position{};
re4::ViewVec3 native_primary{0,3,10},native_secondary{0,5,6},root_result{};
void test_RootDisplacement(void* actor,void* motion,unsigned flags,re4::ViewVec3* out,re4::ViewVec3* rotation){
 assert(flags==0);*out=motion==static_cast<std::uint8_t*>(actor)+0x228?native_primary:native_secondary;*rotation={1,2,3};
}
void test_ModernPlayerUpdate(void* actor){
    ++modern_original_updates;
    if(update_state_change==1)static_cast<std::uint8_t*>(actor)[0x114]=1;
    if(update_state_change>=2)change_reload_identity(actor,update_state_change);
    if(grenade_native_order){
        assert(ModernRoot::Callback(actor,nullptr)==17);
        // Verified grenade order: root/pose, optional frame5 release, then
        // shared aim33DF80. Retain this native input/release timing.
        const auto* b=static_cast<const std::uint8_t*>(actor);
        if(b[0x116]==2)std::memcpy(&grenade_release_position,b+0xA4,sizeof(grenade_release_position));
        AimSensitivity::Callback(actor,0);return;
    }
    if(unified_probe){unified_flags_seen=unsigned(*reinterpret_cast<std::uint64_t*>(main_base+re4::profile::input_flags)&15);MovementTurn::Callback(actor);}
    else if(settings.unified_controls&&static_cast<std::uint8_t*>(actor)[0x115]<=4)MovementTurn::Callback(actor);
    if(modern_probe)AimSensitivity::Callback(actor,0);
    assert(ModernRoot::Callback(actor,nullptr)==17);
}
unsigned test_ModernRoot(void* actor,void*){
 ++modern_original_roots;
 if(modern_root_observer)modern_root_observer(actor);
 if(native_root_probe){
  auto* bytes=static_cast<std::uint8_t*>(actor);re4::ViewVec3 a,b,r;
  RootDisplacement::Callback(actor,bytes+0x228,0,&a,&r);assert(r.x==1&&r.y==2&&r.z==3);
  RootDisplacement::Callback(actor,*reinterpret_cast<void**>(bytes+0x320),0,&b,&r);
  root_result={.25f*a.x+.75f*b.x,.25f*a.y+.75f*b.y,.25f*a.z+.75f*b.z};
 }
 ModernPose::Callback(actor,0);return 17;
}
void test_ModernPose(void*,int){++modern_original_poses;}
void test_AutomaticFire(void*){}
unsigned scope_original_calls=0;
bool scope_inputs_suppressed=false;
bool scope_zoom_feedback=false;
[[maybe_unused]] void test_ScopeUpdate(void* scope){
    ++scope_original_calls;
    auto* b=static_cast<std::uint8_t*>(scope);
    auto* axes=reinterpret_cast<std::int32_t*>(main_base+0x124E9C8);
    auto* directions=reinterpret_cast<std::uint32_t*>(main_base+0x124E9DC);
    auto* zoom_input=reinterpret_cast<std::int8_t*>(main_base+0x124E9D1);
    scope_inputs_suppressed=axes[0]==0&&axes[1]==0&&!*zoom_input&&!(*directions&0x6F);
    auto* actor=*reinterpret_cast<std::uint8_t**>(main_base+re4::profile::player_slot);
    auto& zoom=*reinterpret_cast<float*>(b+0x13C);
    const float previous_zoom=zoom; // Native15490 snapshots old zoom for HUD feedback.
    zoom=std::clamp(zoom+float(*zoom_input)*.001f+((*directions&0x20)?.05f:0)-((*directions&0x40)?.05f:0),0.f,1.f);
    exl::hook::InlineFloatCtx context{};
    std::memcpy(context.bytes,&zoom,sizeof(zoom));
    std::memcpy(context.bytes+0x200+19*8,&scope,sizeof(scope));
    ScopedZoom::Callback(&context);
    std::memcpy(&zoom,context.bytes,sizeof(zoom));
    *reinterpret_cast<float*>(b+0x118)+=float(axes[1])*(-.000872664626f)*(1-.9f*zoom);
    *reinterpret_cast<float*>(actor+0xB4)+=float(axes[0])*(-.000872664626f)*(1-.9f*zoom);
    *reinterpret_cast<float*>(b+0xC8)=45+zoom*(b[0x159]==1?-42.f:-36.f);
    scope_zoom_feedback=zoom!=previous_zoom;
    *directions|=0x100; // Preserve unrelated native changes on restoration.
}
void test_ScopedZoom(exl::hook::InlineFloatCtx*){}
void test_PickupConfirmation(exl::hook::InlineFloatCtx*){}
void test_PickupSuccess(exl::hook::InlineFloatCtx*){}
void test_InventoryDamageName(exl::hook::InlineFloatCtx*){}
#ifndef RE4_STARTUP_LOGOS_TEST_NATIVE
void test_StartupLogos(void*){}
#endif
#ifndef RE4_EXTENDED_UPGRADE_TEST_NATIVE
std::uint64_t test_ExtendedCanTune(void*,void*){return 0;}
void test_ExtendedLevelBar(void*,unsigned,unsigned){}
void test_ExtendedPanel(void*,void*,unsigned){}
std::uint64_t test_ExtendedSnapshotTake(void*,std::uint64_t,std::uint64_t,std::uint64_t){return 0;}
std::uint64_t test_ExtendedSnapshotRestore(void*,std::uint64_t,std::uint64_t,std::uint64_t){return 0;}
#endif
#ifndef RE4_EXTENDED_SQUARE_TEST_NATIVE
void test_ExtendedSquareDraw(void*){}
#endif
void test_ExtendedFullyTuned(exl::hook::InlineFloatCtx*){}
void test_ExtendedRowPrice(exl::hook::InlineFloatCtx*){}
void test_ExtendedConfirmEnter(exl::hook::InlineFloatCtx*){}
void test_ExtendedCommit(exl::hook::InlineFloatCtx*){}
void test_ExtendedDamage(exl::hook::InlineFloatCtx*){}
void test_ExtendedSave(exl::hook::InlineFloatCtx*){}
void test_ExtendedLoad(exl::hook::InlineFloatCtx*){}
std::vector<re4::SensorState> sensor_batch;
std::uint8_t sampler_seen[20]{};
std::uint32_t shadow_flags_seen=0;
unsigned rom_result=0;
bool native_view_test=false,submit_models=false,short_laser_ray=false,simulate_weapon_sway=false;
bool native_follow_probe=false;
bool world_follow_probe=false;
unsigned world_follow_gate=0;
re4::ViewVec3 follow_near_correction{};
float follow_coefficient_seen=0;
unsigned follow_calls=0;
float native_collision_fraction=1;
unsigned camera_collision_calls=0;
using NativeFollowOutput=re4::ViewCameraOutput;
NativeFollowOutput camera_collision_result{};
bool camera_collision_invalid=false;
unsigned camera_native_calls=0,basis_native_calls=0,laser_native_calls=0,hud_native_calls=0,ortho_native_calls=0;
unsigned laser_draw_seen=0,laser_reuse_seen=0;
std::vector<void*> submitted_models;
std::vector<std::vector<std::uint8_t>> hud_rects;
unsigned gx_vertices=0,gx_last_alpha=0;
void* test_weapon=nullptr;void* test_other_model=nullptr;
re4::ViewMatrix3x4 native_basis{{{1,0,0,0},{0,1,0,0},{0,0,1,0}}};
re4::ViewQfpsOutput native_profile{{-500,1765,-1190},{-180,1700,-110},{0,1340,1480},0.2f,50},camera_collision_input{};
re4::ViewVec3 laser_endpoint{0,0,-1000};
bool native_target_probe=false;
void* native_trace_target=nullptr;
void test_CameraBasis(void*,void* out){++basis_native_calls;std::memcpy(out,&native_basis,sizeof(native_basis));}
void test_ModelSubmit(void* model){submitted_models.push_back(model);}
void test_WeaponLaser(void* weapon,unsigned draw,unsigned reuse,unsigned){
    ++laser_native_calls;laser_draw_seen=draw;laser_reuse_seen=reuse;
    if(!(reuse&1)){
        if(native_target_probe&&weapon)std::memcpy(static_cast<std::uint8_t*>(weapon)+0x418,&native_trace_target,sizeof(native_trace_target));
        std::memcpy(reinterpret_cast<void*>(main_base+0x1265320),&laser_endpoint,sizeof(laser_endpoint));
        if(!short_laser_ray)std::memcpy(static_cast<std::uint8_t*>(weapon)+0x40C,&laser_endpoint,sizeof(laser_endpoint));
    }
}
void test_HudDraw(void*){++hud_native_calls;}
bool calculate_probe=false,camera_projection_probe=false;unsigned look_calls=0;re4::ViewVec3 look_seen{};
void test_FinalLookAt(void* out,const re4::ViewVec3* eye,const re4::ViewVec3* up,const re4::ViewVec3* target){
 ++look_calls;look_seen=*target;
 re4::ViewVec3 back{eye->x-target->x,eye->y-target->y,eye->z-target->z};
 auto normalize=[](re4::ViewVec3& v){const float l=std::hypot(v.x,v.y,v.z);v.x/=l;v.y/=l;v.z/=l;};normalize(back);
 re4::ViewVec3 right{up->y*back.z-up->z*back.y,up->z*back.x-up->x*back.z,up->x*back.y-up->y*back.x};normalize(right);
 re4::ViewVec3 actual_up{back.y*right.z-back.z*right.y,back.z*right.x-back.x*right.z,back.x*right.y-back.y*right.x};
 re4::ViewMatrix3x4 view{};const re4::ViewVec3 rows[]={right,actual_up,back};
 for(unsigned i=0;i<3;++i){view.m[i][0]=rows[i].x;view.m[i][1]=rows[i].y;view.m[i][2]=rows[i].z;view.m[i][3]=-(rows[i].x*eye->x+rows[i].y*eye->y+rows[i].z*eye->z);}
 std::memcpy(out,&view,sizeof(view));
}
void test_CameraCalculate(){
 if(!calculate_probe)return;
 CameraInput::Callback(reinterpret_cast<void*>(main_base+0x11319C0));
 auto* s=*reinterpret_cast<std::uint8_t**>(main_base+0xB1AE08);
 if(world_follow_probe){
  auto* input=reinterpret_cast<void*>(main_base+0x1131998);
  std::memcpy(input,s+0x14C,12);std::memcpy(static_cast<std::uint8_t*>(input)+12,s+0x158,12);
  if(camera_projection_probe)std::memcpy(static_cast<std::uint8_t*>(input)+24,s+0x164,8);
  auto* follow=reinterpret_cast<std::uint8_t*>(main_base+0x1131F50);
  auto* actor=*reinterpret_cast<std::uint8_t**>(main_base+re4::profile::player_slot);
  if(world_follow_gate==1)*reinterpret_cast<float*>(main_base+0x1131718+0x2A0)=1;
  if(world_follow_gate==2)settings.re6_response=false;
  if(world_follow_gate==3)settings.first_person=true;
  if(world_follow_gate==4)settings.third_person_distance=1.4f;
  if(world_follow_gate==5)actor[0x115]=5;
  if(world_follow_gate==6)s[0x503C]=2;
  if(world_follow_gate==7)*reinterpret_cast<std::uint8_t*>(main_base+0x11319C0+0x222)=1;
  if(world_follow_gate==8)*reinterpret_cast<float*>(actor+0xB4)+=.1f;
  if(world_follow_gate==9)unified_pitch+=.1f;
  if(world_follow_gate==10)s[0x86FF]=1;
  if(world_follow_gate==11)*reinterpret_cast<float*>(s+0xA4)=0;
  if(world_follow_gate==12)*reinterpret_cast<float*>(input)=std::numeric_limits<float>::quiet_NaN();
  if(world_follow_gate==13)unified_camera_calculation=false;
  if(world_follow_gate==14)*reinterpret_cast<float*>(main_base+0x1131718+0x2A0)=std::numeric_limits<float>::quiet_NaN();
  CameraFollow::Callback(follow,input);
  std::memcpy(s+0x14C,follow+0x100,12);std::memcpy(s+0x158,follow+0x10C,12);
  if(camera_projection_probe)std::memcpy(s+0x164,follow+0x118,8);
 }
 if(camera_projection_probe){
  // FDB0 consumes the collision/follow FOV before building its current
  // projection, then calls the final look-at and rebuilds the frustum.
  const float fov=*reinterpret_cast<const float*>(s+0x168);
  assert(std::isfinite(fov)&&fov>0&&fov<180);
  const float cot=1/std::tan(fov*.00872664626f);
  const re4::ViewMatrix4x4 projection{{{cot*.75f,0,0,0},{0,cot,0,0},
      {0,0,-.001001001f,-100.1001f},{0,0,-1,0}}};
  std::memcpy(s+0x10C,&projection,sizeof(projection));
 }
 FinalLookAt::Callback(s+0xD8,reinterpret_cast<re4::ViewVec3*>(s+0x14C),reinterpret_cast<re4::ViewVec3*>(s+0x16C),reinterpret_cast<re4::ViewVec3*>(s+0x158));
 // FDB0's native frustum construction occurs only after this view is built.
 assert(look_calls>0);
}
void test_CameraFollow(void* object,void* input){
 ++follow_calls;auto* f=static_cast<std::uint8_t*>(object);
 const auto* s=*reinterpret_cast<const std::uint8_t**>(main_base+0xB1AE08);
 follow_coefficient_seen=*reinterpret_cast<const float*>(f+0xFC);
 if(f[0x12C])std::memcpy(f+0x100,f+0x130,32);
 const float alpha=(f[0xF8]&1)?0:std::pow(follow_coefficient_seen,*reinterpret_cast<const float*>(s+0xA4));
 auto* history=reinterpret_cast<float*>(f+0x100);const auto* desired=static_cast<const float*>(input);
 const auto* delta=reinterpret_cast<const float*>(s+0x4FE8);
 for(unsigned i=0;i<(camera_projection_probe?8u:6u);++i)
  history[i]=(history[i]+(i<6?delta[i%3]:0))*alpha+desired[i]*(1-alpha);
 f[0xF8]&=~1;f[0x12C]=0;std::memcpy(f+0x130,f+0x100,32);
 // Native near-body correction comes after the smoothing; the hook must
 // preserve it rather than copying the desired eye over the final result.
 history[0]+=follow_near_correction.x;history[1]+=follow_near_correction.y;history[2]+=follow_near_correction.z;
}
void test_native_ortho(void* out,float top,float bottom,float left,float right,float near_plane,float far_plane){
    ++ortho_native_calls;assert(top==0&&bottom==448&&left==0&&right==512&&near_plane==0&&far_plane==-100);
    std::memset(out,0,sizeof(re4::ViewMatrix4x4));
}
void test_native_projection(const void*,unsigned mode){assert(mode==1);}
void test_native_pose(const void*,unsigned index){assert(index==0);}
void test_native_matrix(unsigned index){assert(index==0);}
void test_native_rect(const void* packet){const auto* p=static_cast<const std::uint8_t*>(packet);re4::ViewReticleRect r;std::memcpy(&r,p+0xC,sizeof(r));if(r.width&&r.height)hud_rects.emplace_back(p,p+0x18);}
std::vector<re4::ViewVec3> gx_quad;unsigned gx_blend_last=0,gx_format_last=0;
bool health_color_probe=false;
std::vector<std::array<unsigned,4>> gx_colors;
void test_gx_blend(unsigned a,unsigned b,unsigned c,unsigned d){assert(a==1&&d==0);assert((b==4&&c==5)||(b==1&&c==0));gx_blend_last=b;}
void test_gx_format(unsigned a,unsigned b,unsigned c,unsigned d,unsigned e){assert(a==0&&b==9&&c==1&&e==0);gx_format_last=d;}
void test_gx_begin(unsigned a,unsigned b,unsigned n){assert(a==0x98&&b==0&&n==4);gx_quad.clear();}
void test_gx_position(float x,float y,float z){++gx_vertices;gx_quad.push_back({x,y,z});}
void test_gx_color(unsigned r,unsigned g,unsigned b,unsigned a){if(!health_color_probe)assert(r==255&&g==255&&b==255&&a==128);gx_colors.push_back({r,g,b,a});gx_last_alpha=a;}
void test_gx_end(unsigned token){
 assert(token==0x34&&gx_quad.size()==4);const auto a=gx_quad[0],b=gx_quad[3];
 re4::ViewReticleRect rect{std::uint16_t(std::lround(a.x)),std::uint16_t(std::lround(a.y)),std::uint16_t(std::lround(b.x-a.x)),std::uint16_t(std::lround(b.y-a.y))};
 std::vector<std::uint8_t> packet(0x18);packet[8]=packet[9]=packet[10]=255;packet[11]=128;std::memcpy(packet.data()+0xC,&rect,sizeof(rect));hud_rects.push_back(packet);
}

unsigned test_RomMount(const char*,void*,std::size_t){return rom_result;}
extern "C" Result nn_mount_rom(const char*,void*,std::size_t){return rom_result;}
extern "C" int get_operation_mode(){return game_test::operation_mode;}
void test_SamplerCreate(void*,void*,const std::uint8_t* descriptor){std::memcpy(sampler_seen,descriptor,20);}
void test_ResolutionMap(int width,int height,int* display,int* back){*display=width==1280&&height==720?2:0;*back=*display==2?3:1;}
void test_FrustumUpdate(void* frustum){auto* value=static_cast<std::uint8_t*>(frustum);std::memcpy(value+0x24,value+0x18,4);}
void test_SceneSubmit(){
    if(submit_models){
        ModelSubmit::Callback(*reinterpret_cast<void**>(main_base+re4::profile::player_slot));
        ModelSubmit::Callback(test_weapon);ModelSubmit::Callback(test_other_model);
    }
    auto* state=*reinterpret_cast<std::uint8_t**>(main_base+0xB1AE08);
    if(state){auto* flags=reinterpret_cast<std::uint32_t*>(state+0x7C);shadow_flags_seen=*flags;*flags|=0x400;}
}
void test_SceneDraw(){test_SceneSubmit();}
bool consume_aim_axes=false;
int seen_aim_x=0,seen_aim_y=0;
float aim_full_scale=63,cached_aim_yaw=0,cached_aim_pitch=0,native_pitch_sign=1;
void test_FullPad(NpadState* state, const std::uint32_t&) { *state={}; state->attributes=game_test::pad_attributes;state->buttons=game_test::pad_buttons; }
void test_HandheldPad(NpadState* state, const std::uint32_t& id) { test_FullPad(state,id); }
void test_DualPad(NpadState* state, const std::uint32_t& id) { test_FullPad(state,id); }
int test_PadNormalize(void*) { return 7; }
unsigned test_MotionAdvance(void* motion) {
    game_test::motion_speed_seen=*reinterpret_cast<float*>(static_cast<std::uint8_t*>(motion)+0xDC);
    return 3;
}
void test_MovementTurn(void* actor) {
    auto* b=static_cast<std::uint8_t*>(actor);
    auto* flags=reinterpret_cast<std::uint64_t*>(main_base+re4::profile::input_flags);
    game_test::directions_seen=*flags&15;
    *flags=(*flags&~64u)|128u; // The native routine consumes other input bits.
    *reinterpret_cast<float*>(b+0xB4)+=b[0x115]==5 ? 3.14159265f : (settings.unified_controls?0.f:0.04f);
    MotionAdvance::Callback(b+0x228);
}
void test_CameraInput(void* camera) {
    const bool alternate=*reinterpret_cast<int*>(main_base+0x11C7D68)==1;
    auto* in=reinterpret_cast<std::int8_t*>(main_base+0x124EC38+(alternate?0xC0:8));
    game_test::camera_x=in[0]; game_test::camera_y=in[1];
    if(camera){
        auto* bytes=static_cast<std::uint8_t*>(camera);
        auto* offsets=reinterpret_cast<float*>(bytes+0x270);
        if(in[0]||in[1]){
            const auto* type=*reinterpret_cast<const int**>(main_base+0xB1AE10);
            offsets[0]=std::clamp(float(in[1])/59*(type&&*type<0?-1:1),-1.f,1.f);
            offsets[1]=float(in[0])/59*-1.04719758f;
        }else{offsets[0]*=0.9f;offsets[1]*=0.9f;}
        if(simulate_weapon_sway&&game_test::native_aim)offsets[0]=*reinterpret_cast<const float*>(main_base+0x1265A08);
        if(game_test::camera_gate_change==1)bytes[0x26C]=0;
        if(game_test::camera_gate_change==2)bytes[0x222]=1;
        if(native_view_test){
            re4::ViewMatrix3x4 basis{};CameraBasis::Callback(camera,&basis);
            re4::ViewQfpsOutput out{};CameraApply::Callback(camera,&out);
            camera_collision_input=out; // Native18990 collision is the next consumer.
            if(native_follow_probe){
                NativeFollowOutput result{};CameraCollision::Callback(nullptr,&basis,&out,&result);
                // Verified native19D0C..19D44, including its persistent local-X
                // history. The old fixture stopped before this downstream stage.
                auto* history=reinterpret_cast<float*>(main_base+0x1132A30);
                result.eye.x=.3999999761581421f*result.eye.x+.6000000238418579f*history[0];
                result.target.x=.3999999761581421f*result.target.x+.6000000238418579f*history[3];
                history[0]=result.eye.x;history[3]=result.target.x;
                auto* state=*reinterpret_cast<std::uint8_t**>(main_base+0xB1AE08);
                re4::ViewVec3 eye,target;assert(re4::view_transform_point(basis,result.eye,eye));
                assert(re4::view_transform_point(basis,result.target,target));
                std::memcpy(state+0x14C,&eye,sizeof(eye));std::memcpy(state+0x158,&target,sizeof(target));
                if(camera_projection_probe)std::memcpy(state+0x168,&result.fov,4);
                const re4::ViewVec3 up{0,1,0};std::memcpy(state+0x16C,&up,sizeof(up));
            }
        }else CameraApply::Callback(camera,nullptr);
    }
}
void test_CameraCollision(void*,void*,void* input,void* output){
    ++camera_collision_calls;
    const auto& p=*static_cast<re4::ViewQfpsOutput*>(input);
    NativeFollowOutput result{p.eye0,p.target,p.roll,p.fov};
    result.eye={p.eye1.x+(p.eye0.x-p.eye1.x)*native_collision_fraction,
        p.eye1.y+(p.eye0.y-p.eye1.y)*native_collision_fraction,
        p.eye1.z+(p.eye0.z-p.eye1.z)*native_collision_fraction};
    if(camera_collision_invalid)result.eye.x=std::numeric_limits<float>::quiet_NaN();
    camera_collision_result=result;
    std::memcpy(output,&result,sizeof(result));
}
void test_CameraApply(void* camera,void* output){
    ++camera_native_calls;
    if(output)std::memcpy(output,&native_profile,sizeof(native_profile));
    const auto* offsets=reinterpret_cast<const float*>(static_cast<const std::uint8_t*>(camera)+0x270);
    game_test::rendered_pitch=offsets[0];game_test::rendered_camera_yaw=offsets[1];
}
int test_AimSensitivity(void* actor,int) {
    auto* b=static_cast<std::uint8_t*>(actor);
    auto* axes=reinterpret_cast<std::int32_t*>(main_base+0x124E9C8);
    seen_aim_x=axes[0];seen_aim_y=axes[1];
    const auto raw_dpad=*reinterpret_cast<std::uint32_t*>(main_base+0x124E9C8+0x14);
    const float x=(consume_aim_axes?float(axes[0])/aim_full_scale:1)+((raw_dpad&1)?1:0);
    const float y=(consume_aim_axes?float(axes[1])/aim_full_scale:1)*native_pitch_sign;
    *reinterpret_cast<float*>(b+0xB4)+=0.04f*x;
    *reinterpret_cast<float*>(b+0x524)+=0.02f*x;
    auto* pitch=reinterpret_cast<float*>(main_base+0x1265A08);
    pitch[0]+=game_test::aim_step*y; pitch[1]+=game_test::aim_step*y;
    if (game_test::apply_sway) {
        AimSway::Callback(actor,0,pitch,reinterpret_cast<float*>(b+0x524));
        pitch[1]=pitch[0];
    }
    if(simulate_weapon_sway){pitch[0]+=.03f;pitch[1]+=.03f;}
    cached_aim_yaw=*reinterpret_cast<float*>(b+0xB4);cached_aim_pitch=pitch[0];
    return 13;
}
void test_AimSway(void*,unsigned flags,float* pitch,float*) {
    if(flags&1) game_test::sway_baseline=*pitch;
    else *pitch=std::fmax(game_test::sway_baseline-0.02f,std::fmin(game_test::sway_baseline+0.02f,*pitch));
}
extern "C" void get_full(NpadState*,const std::uint32_t&) {}
extern "C" void get_handheld(NpadState*,const std::uint32_t&) {}
extern "C" void get_dual(NpadState*,const std::uint32_t&) {}
extern "C" Result sensor_handles(std::uint32_t* h,int* count,int cap,std::uint32_t id,std::uint32_t style) {
    assert(cap==2 && (id==0||id==32) && (style==1||style==2||style==4));
    h[0]=10;h[1]=20;*count=style==4?2:1;return 0;
}
extern "C" Result sensor_start(const std::uint32_t&) { ++game_test::sensor_starts;return game_test::start_result; }
extern "C" Result sensor_stop(const std::uint32_t&) { ++game_test::sensor_stops;return 0; }
extern "C" Result sensor_read(int* count,re4::SensorState* out,int capacity,const std::uint32_t&) {
    ++game_test::sensor_reads;assert(sensor_batch.size()<=std::size_t(capacity));
    *count=int(sensor_batch.size());std::copy(sensor_batch.begin(),sensor_batch.end(),out);return 0;
}
int test_is_aiming(void*) { return game_test::native_aim; }
bool near(float a,float b) {return std::fabs(a-b)<0.0001f;}
struct Fixture {
    std::vector<std::uint8_t> image=std::vector<std::uint8_t>(0x1281000);
    alignas(16) std::uint8_t actor[0xA00]{};
    std::vector<std::uint8_t> input_state=std::vector<std::uint8_t>(0x8700);
    alignas(4) std::uint8_t input_type[16]{};
    alignas(16) std::uint8_t weapon_context[0x60]{},weapon[0x500]{},other_model[0x500]{};
    Fixture() {
        sensor.stop();sensor={};gyro.reset();gyro_frame.reset();gyro_tuning_ready=false;gyro_session={};gyro_pending={};last_sensor_tick=0;sensor_batch.clear();
        free_orbit.reset();camera_context=nullptr;
        unified_look.reset();unified_update=nullptr;unified_pitch=0;unified_pitch_actor=0;unified_pitch_route=0;unified_pitch_character=0;
        unified_aim_session={};unified_reload_session={};unified_new_aim=false;unified_command_frame=0;unified_command_tick=0;
        reset_scoped_controls();scoped_zoom_target=nullptr;modern_aim_session={};
        unified_root_actor=nullptr;unified_camera_calculation=false;
        native_view_test=submit_models=short_laser_ray=simulate_weapon_sway=false;laser_endpoint={0,0,-1000};camera_native_calls=basis_native_calls=laser_native_calls=hud_native_calls=ortho_native_calls=0;
        native_follow_probe=false;world_follow_probe=false;world_follow_gate=0;follow_near_correction={};follow_calls=0;native_collision_fraction=1;camera_collision_calls=0;camera_collision_invalid=false;
        camera_projection_probe=false;
        submitted_models.clear();hud_rects.clear();test_weapon=test_other_model=nullptr;
        native_basis={{{1,0,0,0},{0,1,0,0},{0,0,1,0}}};
        native_profile={{-500,1765,-1190},{-180,1700,-110},{0,1340,1480},0.2f,50};
        view_settings_snapshot=0;view_frame=1;view_camera_frame=0;first_person_frame=0;
        view_camera_actor=0;view_camera_tick=0;view_camera_aim=false;aim_point.sequence=0;aim_point.frame=0;
        pending_aim_point.sequence=0;pending_aim_point.frame=0;displayed_aim_point.sequence=0;displayed_aim_point.frame=0;
        for(auto* point:{&enemy_health_point,&pending_enemy_health,&displayed_enemy_health,&enemy_health_camera}){point->sequence=0;point->frame=0;point->health=0;point->target_key=0;}
        enemy_health_fade.reset();enemy_health_fade_generation=0;enemy_health_fade_seen=enemy_health_fade_tick=enemy_health_fade_frame=0;
        enemy_health_fade_actor=0;enemy_health_fade_character=0;
        fps_override_count=0;reticle_draw_count=0;
        main_view_actor=nullptr;
        health_color_probe=false;gx_colors.clear();native_target_probe=false;native_trace_target=nullptr;
        modern_probe=false;grenade_native_order=false;grenade_release_position={};modern_original_updates=modern_original_roots=modern_original_poses=0;modern_root_observer=nullptr;
        aim_raise_native_calls=aim_raise_native_change=0;
        game_test::memory_maps.clear();modern_update_actor=nullptr;modern_cycle.reset();modern_pose.discard();modern_body_pose.discard();modern_body_owner={};modern_body_yaw=0;
        inventory_damage_enabled=false;inventory_damage_clear();weapon_popup_clear();
        inventory_damage_glyph_native={};grenade_spot_zoom_reset();
        modern_native={};modern_bound_owner={};modern_bound_asset=modern_bound_clip=0;
        modern_epoch=0;modern_root_done=modern_stepped=modern_pose_done=false;
        native_ortho=test_native_ortho;native_projection=test_native_projection;
        native_pose=test_native_pose;native_matrix=test_native_matrix;native_rect=test_native_rect;
        native_gx_blend=test_gx_blend;native_gx_format=test_gx_format;native_gx_begin=test_gx_begin;
        native_gx_position=test_gx_position;native_gx_color=test_gx_color;native_gx_end=test_gx_end;
        gx_vertices=gx_last_alpha=0;gx_blend_last=gx_format_last=0;gx_quad.clear();
        graphics_snapshot.store(0x3F80000000000000ull);filter_latched=255;aa_latched=255;
        sampler_calls=0;sampler_overrides=0;scene_submits=0;scene_draws=0;cull_updates=0;resolution_overrides=0;
        last_cull_pair=0;rom_settings_attempted.clear();rom_result=0;
        consume_aim_axes=true;seen_aim_x=seen_aim_y=0;aim_full_scale=63;cached_aim_yaw=cached_aim_pitch=0;
        native_pitch_sign=1;
        sensor_pad=0;sensor_style=0;sensor_attributes=0;
        game_test::installed=game_test::initialized=game_test::mounts=game_test::fs_opens=0;
        game_test::mount_result=1;game_test::mount_failures=game_test::fs_reads=game_test::fs_closes=0;
        game_test::settings_primary.clear();game_test::settings_backup.clear();
        game_test::sensor_starts=game_test::sensor_stops=game_test::sensor_reads=0;
        game_test::start_result=0;game_test::tick=1;game_test::native_aim=1;
        game_test::apply_sway=false;game_test::aim_step=0.08f;game_test::sway_baseline=0;
        game_test::pad_buttons=0;
        game_test::camera_gate_change=0;
        game_test::operation_mode=0;
        game_test::program=re4::profile::title_id;
        settings={};settings.gyro_speed=1;settings.invert_x=settings.invert_y=false;
        settings.left_deadzone=settings.right_deadzone=0.15f;
        file_read=false;file_mounted=false;file_attempts=0;next_file_retry=0;
        last_sequence=0;raw_left_x=0;dpad_move=dpad_turn=false;movement_actor=nullptr;
        std::memset(const_cast<re4::SharedControl*>(&control),0,sizeof(control));
        std::memcpy(image.data()+re4::profile::build_id_offset,re4::profile::build_id,32);
        for(auto s:re4::profile::sites)std::memcpy(image.data()+s.offset,&s.instruction,4);
        for(auto s:re4::profile::relative_pointers){
            const std::uint64_t target=reinterpret_cast<std::uintptr_t>(image.data())+s.target_offset;
            std::memcpy(image.data()+s.offset,&target,sizeof(target));
        }
        main_base=reinterpret_cast<std::uintptr_t>(image.data());
        game_test::module.m_Total={main_base,image.size()};
        *reinterpret_cast<void**>(image.data()+re4::profile::player_slot)=actor;
        *reinterpret_cast<float*>(actor+0x304)=1.25f;
    }
    std::uint64_t& flags(){return *reinterpret_cast<std::uint64_t*>(image.data()+re4::profile::input_flags);}
    float& yaw(){return *reinterpret_cast<float*>(actor+0xB4);}
    float* pitch(){return reinterpret_cast<float*>(image.data()+0x1265A08);}
    void aim(){game_test::native_aim=1;actor[0x115]=6;actor[0x116]=1;actor[0x117]=1;
        *reinterpret_cast<std::uintptr_t*>(image.data()+0x1265A28)=main_base+0x31CC60;
        if(!*reinterpret_cast<void**>(image.data()+0xB1AE08))*reinterpret_cast<void**>(image.data()+0xB1AE08)=input_state.data();
        flags()=0x10;aiming_predicate=test_is_aiming;}
    std::uint8_t* camera(){return image.data()+0x11319C0;}
    float* look(){return reinterpret_cast<float*>(camera()+0x270);}
    void camera_mode(){
        actor[0x115]=1;game_test::native_aim=0;aiming_predicate=test_is_aiming;
        image[0x1131718+0x54]=10;camera()[0x26C]=2;
        *reinterpret_cast<void**>(image.data()+0xB1AE08)=input_state.data();
        *reinterpret_cast<void**>(image.data()+0xB1AE10)=input_type;
    }
    void view_mode(){
        camera_mode();image[0x1131718+0x48]=1;native_view_test=true;
        re4::ViewMatrix3x4 body{{{1,0,0,100},{0,1,0,200},{0,0,1,300}}};
        std::memcpy(actor+0x18,&body,sizeof(body));
        *reinterpret_cast<void**>(actor+0x938)=weapon_context;
        *reinterpret_cast<void**>(weapon_context+0x38)=weapon;
        test_weapon=weapon;test_other_model=other_model;
        const re4::ViewMatrix3x4 view{{{1,0,0,0},{0,1,0,0},{0,0,1,0}}};
        const re4::ViewMatrix4x4 projection{{{0.75f,0,0,0},{0,1,0,0},{0,0,-0.001001f,-100.1001f},{0,0,-1,0}}};
        std::memcpy(input_state.data()+0xD8,&view,sizeof(view));
        std::memcpy(input_state.data()+0x10C,&projection,sizeof(projection));
    }
    void sample(unsigned n) {
        re4::SensorState s{};s.number=n;s.interval_ns=5'000'000;s.attributes=1;
        s.angular_velocity[0]=s.angular_velocity[1]=-1/6.28318530718f;
        sensor_batch={s};
    }
    std::uint8_t* scope_mode(){
        view_mode();aim();auto* scope=image.data()+0x1131718+0x530;
        image[0x1131718+0x54]=16;
        *reinterpret_cast<void**>(image.data()+0x1131718+0x7B0)=scope;
        input_state[0x5090]|=0x40;
        *reinterpret_cast<std::uintptr_t*>(image.data()+0x1265A28)=main_base+0x332CC0;
        *reinterpret_cast<float*>(scope+0x124)=-1.22173059f;
        *reinterpret_cast<float*>(scope+0x130)=1.22173059f;
        *reinterpret_cast<float*>(scope+0xC8)=45;
        *reinterpret_cast<float*>(input_state.data()+0xA4)=1;
        *reinterpret_cast<int*>(image.data()+0xAFF39C)=2;sensor_attributes=1;
        settings.unified_controls=true;return scope;
    }
};
}
int main(){
    for(unsigned gate=0;gate<7;++gate){
        Fixture f;f.view_mode();f.aim();f.camera()[0x26C]=0;
        settings.unified_controls=true;settings.re6_response=true;
        sensor_attributes=1;*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;
        *reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        const re4::ViewQfpsOutput neutral{{-470,1715,-615},{},{-200,1345,1525},0,50};
        const auto resource=reinterpret_cast<std::uintptr_t>(f.image.data()+0x1000);
        std::memcpy(reinterpret_cast<void*>(resource+0x2C),&neutral,sizeof(neutral));
        *reinterpret_cast<std::uintptr_t*>(f.camera()+0x1B0)=resource;
        game_test::memory_maps={{reinterpret_cast<std::uintptr_t>(f.image.data()),f.image.size(),1}};
        if(gate==1)f.actor[0x116]=4; // Native aim predicate can stay true in reload.
        if(gate==2)f.actor[0x116]=0;
        if(gate==3){f.actor[0x115]=11;f.flags()=re4::kKnifeHeldMask;}
        if(gate==4)settings.first_person=true;
        if(gate==5)settings.re6_response=false;
        if(gate==6){f.actor[0x115]=1;game_test::native_aim=0;f.camera()[0x26C]=2;}
        publish_view_settings();unified_pitch_actor=reinterpret_cast<std::uintptr_t>(f.actor);unified_pitch=1.2f;
        calculate_probe=true;CameraCalculate::Callback();calculate_probe=false;
        assert(view_camera_native_pitch.load()==(gate==0));
        if(gate!=0)assert(near(view_camera_pitch.load(),unified_pitch));
    }
    // Native profile0's extreme framing is about +/-60degrees, even though
    // its pose weight is +/-1. A point on that native aiming axis must remain
    // visible when the owned command reaches either end of its full range.
    for(float sign:{-1.f,1.f})for(float heading:{0.f,1.2f,-2.4f})for(float distance:{1.f,1.15f,2.f})for(float clipping:{1.f,.4f})for(float weight:{0.f,.4f,.75f,1.f}){
        Fixture f;f.view_mode();f.aim();f.camera()[0x26C]=0;
        settings.unified_controls=true;settings.re6_response=true;settings.third_person_distance=distance;
        settings.reticle=re4::ReticleStyle::Crosshair;
        sensor_attributes=1;*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;
        *reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        native_profile=sign>0?re4::ViewQfpsOutput{{-527,600,-680},{-180,1700,-110},{-220,4080,1100},.2f,50}
            :re4::ViewQfpsOutput{{-393,2058,-5},{-180,1700,-110},{-179,365,943},.2f,50};
        const re4::ViewQfpsOutput neutral{{-470,1715,-615},{-180,1700,-110},{-200,1345,1525},0,50};
        const auto mix=[weight](re4::ViewVec3 a,re4::ViewVec3 b){return re4::ViewVec3{a.x+(b.x-a.x)*weight,a.y+(b.y-a.y)*weight,a.z+(b.z-a.z)*weight};};
        native_profile.eye0=mix(neutral.eye0,native_profile.eye0);native_profile.target=mix(neutral.target,native_profile.target);
        const auto resource=reinterpret_cast<std::uintptr_t>(f.image.data()+0x1000);
        std::memcpy(reinterpret_cast<void*>(resource+0x2C),&neutral,sizeof(neutral));
        *reinterpret_cast<std::uintptr_t*>(f.camera()+0x1B0)=resource;
        game_test::memory_maps.push_back({reinterpret_cast<std::uintptr_t>(f.image.data()),f.image.size(),1});
        const double dx=native_profile.target.x-native_profile.eye0.x,dy=native_profile.target.y-native_profile.eye0.y,dz=native_profile.target.z-native_profile.eye0.z;
        const float physical=std::atan2(dy,std::hypot(dx,dz));
        const float c=std::cos(heading-.4f),s=std::sin(heading-.4f);
        native_basis={{{c,0,s,450},{0,1,0,-180},{-s,0,c,900}}};
        const float scale=1/std::tan(50.f*.00872664626f);
        const re4::ViewMatrix4x4 projection{{{scale*.5625f,0,0,0},{0,scale,0,0},{0,0,-1.001f,-100},{0,0,-1,0}}};
        std::memcpy(f.input_state.data()+0x10C,&projection,sizeof(projection));
        ModernPlayerUpdate::Callback(f.actor);f.yaw()=heading;unified_pitch=sign*weight*1.570796327f;
        f.pitch()[0]=f.pitch()[1]=sign*weight;
        auto* follow=f.image.data()+0x1131F50;*reinterpret_cast<float*>(follow+0xFC)=.8f;
        native_follow_probe=world_follow_probe=calculate_probe=true;native_collision_fraction=clipping;CameraCalculate::Callback();
        const re4::ViewVec3 target{100+std::sin(heading)*std::cos(physical)*50000,1900+std::sin(physical)*50000,300+std::cos(heading)*std::cos(physical)*50000};
        const auto& view=*reinterpret_cast<re4::ViewMatrix3x4*>(f.input_state.data()+0xD8);
        re4::ViewHudPoint point;assert(re4::project_view_point(view,projection,target,point));
        assert(point.y>16&&point.y<432&&point.x>16&&point.x<496);
        assert(near(unified_pitch,sign*weight*1.570796327f)&&near(f.pitch()[1],sign*weight)); // Full native weapon range retained.
        // Exercise the displayed-frame marker rather than replacing it with a
        // fake edge marker. The fresh native endpoint still drives the HUD.
        laser_endpoint=target;WeaponLaser::Callback(f.weapon,1,0,0);SceneSubmit::Callback();
        view_frame.fetch_add(1);SceneDraw::Callback();CameraCalculate::Callback();
        const auto before=hud_rects.size();HudDraw::Callback(nullptr);assert(hud_rects.size()>before);
        if(weight==1&&heading==0&&distance==1&&clipping==1){
            assert(view_camera_native_pitch.load());
            f.actor[0x115]=1;game_test::native_aim=0;f.camera()[0x26C]=2;unified_pitch=.3f;
            CameraCalculate::Callback();assert(!view_camera_native_pitch.load()&&near(view_camera_pitch.load(),.3f));
            f.actor[0x115]=6;game_test::native_aim=1;f.camera()[0x26C]=0;unified_pitch=sign*1.570796327f;
            settings.first_person=true;CameraCalculate::Callback();assert(!view_camera_native_pitch.load());
            settings.first_person=false;*reinterpret_cast<std::uintptr_t*>(f.camera()+0x1B0)=0;
            CameraCalculate::Callback();assert(!view_camera_native_pitch.load()&&near(view_camera_pitch.load(),unified_pitch));
        }
        calculate_probe=false;
    }
    {
        Fixture f;f.view_mode();
        const auto current=reinterpret_cast<std::uintptr_t>(f.image.data()+0x1000),previous=current+0x100;
        const re4::ViewQfpsOutput a{{0,100,0},{},{0,600,1000},0,50},b{{0,300,0},{},{0,0,1500},0,50};
        std::memcpy(reinterpret_cast<void*>(current+0x2C),&a,sizeof(a));std::memcpy(reinterpret_cast<void*>(previous+0x2C),&b,sizeof(b));
        *reinterpret_cast<std::uintptr_t*>(f.camera()+0x1B0)=current;*reinterpret_cast<std::uintptr_t*>(f.camera()+0x1B8)=previous;
        *reinterpret_cast<std::uint32_t*>(f.camera()+0x280)=4;
        game_test::memory_maps={{reinterpret_cast<std::uintptr_t>(f.image.data()),f.image.size(),1}};
        re4::ViewQfpsOutput neutral;
        for(float weight:{0.f,.25f,1.f}){
            *reinterpret_cast<float*>(f.camera()+0x254)=weight;assert(camera_aim_neutral(f.camera(),neutral));
            assert(near(neutral.eye0.y,100+200*weight)&&near(neutral.target.y,600*(1-weight))&&near(neutral.target.z,1000+500*weight));
        }
        *reinterpret_cast<float*>(f.camera()+0x254)=std::numeric_limits<float>::quiet_NaN();assert(!camera_aim_neutral(f.camera(),neutral));
        *reinterpret_cast<float*>(f.camera()+0x254)=.5f;
        game_test::memory_maps.clear();assert(!camera_aim_neutral(f.camera(),neutral));
        game_test::memory_maps={{current+0x2C,sizeof(a)-1,1}};assert(!camera_aim_neutral(f.camera(),neutral));
        game_test::memory_maps={{current+0x2C,sizeof(a),0}};assert(!camera_aim_neutral(f.camera(),neutral));
        *reinterpret_cast<std::uintptr_t*>(f.camera()+0x1B0)=std::numeric_limits<std::uintptr_t>::max();assert(!camera_aim_neutral(f.camera(),neutral));
    }
    for(unsigned gate=1;gate<=14;++gate){
        Fixture f;f.view_mode();settings.unified_controls=true;settings.re6_response=true;settings.third_person_distance=1.15f;
        sensor_attributes=1;*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;
        *reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        auto* follow=f.image.data()+0x1131F50;*reinterpret_cast<float*>(follow+0xFC)=.8f;follow[0xF8]=1;
        ModernPlayerUpdate::Callback(f.actor);native_follow_probe=world_follow_probe=calculate_probe=true;world_follow_gate=gate;
        CameraCalculate::Callback();assert(follow_coefficient_seen==.8f&&*reinterpret_cast<float*>(follow+0xFC)==.8f);
        calculate_probe=false;
    }
    {
        Fixture f;f.view_mode();settings.unified_controls=true;settings.re6_response=true;
        sensor_attributes=1;*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;
        *reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        auto* follow=f.image.data()+0x1131F50;*reinterpret_cast<float*>(follow+0xFC)=.8f;
        ModernPlayerUpdate::Callback(f.actor);native_follow_probe=world_follow_probe=calculate_probe=true;
        native_collision_fraction=.2f;follow_near_correction={17,-4,9};CameraCalculate::Callback();
        re4::ViewVec3 clipped;assert(re4::view_transform_point(native_basis,camera_collision_result.eye,clipped));
        const auto* eye=reinterpret_cast<const re4::ViewVec3*>(f.input_state.data()+0x14C);
        assert(near(eye->x,clipped.x+17)&&near(eye->y,clipped.y-4)&&near(eye->z,clipped.z+9));
        assert(follow_coefficient_seen==0);calculate_probe=false;
        CameraFollow::Callback(follow,reinterpret_cast<void*>(main_base+0x1131998));
        assert(follow_coefficient_seen==.8f); // No token survives calculate scope.
    }
    // Native world follow runs after the earlier local-X filter. Sustained
    // right strafe used to bring the character all the way to screen center.
    for(unsigned character:{0u,1u})for(float dt:{.5f,1.f,2.f}){
        Fixture f;f.view_mode();f.input_state[0x503C]=std::uint8_t(character);
        settings.unified_controls=true;settings.re6_response=true;settings.third_person_distance=1.15f;
        sensor_attributes=1;*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;
        *reinterpret_cast<float*>(f.input_state.data()+0xA4)=dt;
        auto* follow=f.image.data()+0x1131F50;*reinterpret_cast<float*>(follow+0xFC)=.8f;follow[0xF8]=1;
        ModernPlayerUpdate::Callback(f.actor);native_follow_probe=world_follow_probe=calculate_probe=true;
        CameraCalculate::Callback();
        auto* body=reinterpret_cast<re4::ViewMatrix3x4*>(f.actor+0x18);
        auto* view=reinterpret_cast<re4::ViewMatrix3x4*>(f.input_state.data()+0xD8);
        auto* projection=reinterpret_cast<re4::ViewMatrix4x4*>(f.input_state.data()+0x10C);
        re4::ViewHudPoint initial;assert(re4::project_view_point(*view,*projection,{100,1900,300},initial));
        for(unsigned n=0;n<120;++n){
            body->m[0][3]+=(n<60?-137.f:n<100?137.f:0)*dt;
            std::memcpy(f.actor+0xA4,&body->m[0][3],4);
            if(n>30&&n%7==0){follow[0x12C]=1;std::memset(follow+0x130,0,32);}
            const float delta[3]={12,-3,7};std::memcpy(f.input_state.data()+0x4FE8,delta,12);
            view_frame.fetch_add(1);game_test::tick+=640000;CameraCalculate::Callback();
            re4::ViewHudPoint point;assert(re4::project_view_point(*view,*projection,{body->m[0][3],1900,300},point));
            assert(std::fabs(point.x-initial.x)<.02f&&std::fabs(point.y-initial.y)<.02f);
            assert(follow_coefficient_seen==0&&*reinterpret_cast<float*>(follow+0xFC)==.8f);
        }
        assert(follow_calls==121);calculate_probe=false;
    }
    for(unsigned character:{0u,1u}){
        Fixture f;f.view_mode();f.input_state[0x503C]=std::uint8_t(character);
        settings.unified_controls=true;settings.re6_response=true;settings.third_person_distance=1.15f;
        sensor_attributes=1;*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;
        *reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        ModernPlayerUpdate::Callback(f.actor);native_follow_probe=world_follow_probe=calculate_probe=true;
        auto* follow=f.image.data()+0x1131F50;*reinterpret_cast<float*>(follow+0xFC)=.8f;follow[0xF8]=1;
        // Seed a settled native cache, then turn quickly against a delayed basis.
        auto* history=reinterpret_cast<float*>(f.image.data()+0x1132A30);
        history[0]=-448;history[3]=-448;
        CameraCalculate::Callback();
        re4::ViewHudPoint initial{};
        const re4::ViewVec3 torso{100,1900,300};
        auto* view=reinterpret_cast<re4::ViewMatrix3x4*>(f.input_state.data()+0xD8);
        auto* projection=reinterpret_cast<re4::ViewMatrix4x4*>(f.input_state.data()+0x10C);
        assert(re4::project_view_point(*view,*projection,torso,initial));
        for(float heading:{.25f,.5f,1.f,1.6f,2.8f,-2.8f,-1.6f,-.5f,0.f})for(float pitch:{-.5f,0.f,.5f}){
            f.yaw()=heading;unified_pitch=pitch;
            // Delayed moving/rotated native basis, independent of current look.
            const float old=heading-.4f,c=std::cos(old),s=std::sin(old);
            native_basis={{{c,0,s,450},{0,1,0,-180},{-s,0,c,900}}};
            view_frame.fetch_add(1);game_test::tick+=640000;CameraCalculate::Callback();
            re4::ViewHudPoint point{};assert(re4::project_view_point(*view,*projection,torso,point));
            assert(std::fabs(point.x-initial.x)<.02f&&std::fabs(point.y-initial.y)<.02f);
        }
        // Actual right-stick commands, including reversals, must remain fixed
        // through BOTH native follow stages, not only at the collision rig.
        unified_pitch=0;control.raw_ry=0;
        for(unsigned n=0;n<120;++n){
            control.raw_rx=n<40?1.f:n<80?-1.f:0.f;
            view_frame.fetch_add(1);game_test::tick+=640000;ModernPlayerUpdate::Callback(f.actor);
            const float old=f.yaw()-.4f,c=std::cos(old),s=std::sin(old);
            native_basis={{{c,0,s,450},{0,1,0,-180},{-s,0,c,900}}};
            CameraCalculate::Callback();
            re4::ViewHudPoint point{};assert(re4::project_view_point(*view,*projection,torso,point));
            assert(std::fabs(point.x-initial.x)<.02f&&std::fabs(point.y-initial.y)<.02f);
            assert(follow_coefficient_seen==0&&*reinterpret_cast<float*>(follow+0xFC)==.8f);
        }
        for(float fraction:{1.f,.5f,.1f}){
            native_collision_fraction=fraction;history[0]=5000;history[3]=-5000;
            view_frame.fetch_add(1);game_test::tick+=640000;CameraCalculate::Callback();
            re4::ViewVec3 expected,eye;
            assert(re4::view_transform_point(native_basis,camera_collision_result.eye,expected));
            std::memcpy(&eye,f.input_state.data()+0x14C,sizeof(eye));
            assert(std::hypot(eye.x-expected.x,eye.y-expected.y,eye.z-expected.z)<.01f);
        }
        assert(camera_collision_calls==151);calculate_probe=false;native_follow_probe=world_follow_probe=false;
    }
    for(unsigned gate=0;gate<23;++gate){
        Fixture f;f.view_mode();settings.unified_controls=true;settings.re6_response=true;settings.third_person_distance=1.15f;
        sensor_attributes=1;*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;
        *reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        ModernPlayerUpdate::Callback(f.actor);
        CameraContext context{};context.camera=f.camera();context.actor=f.actor;context.view_allowed=true;
        camera_context=&context;
        re4::ViewMatrix3x4 basis{};CameraBasis::Callback(f.camera(),&basis);
        re4::ViewQfpsOutput profile{};CameraApply::Callback(f.camera(),&profile);
        assert(context.shoulder);
        auto* history=reinterpret_cast<float*>(f.image.data()+0x1132A30);
        for(unsigned i=0;i<8;++i)history[i]=100.f+float(i);
        re4::ViewMatrix3x4 other_basis=basis;re4::ViewQfpsOutput other_profile=profile;
        void* basis_arg=&basis;void* profile_arg=&profile;
        if(gate==1)settings.first_person=true;
        if(gate==2)settings.unified_controls=false;
        if(gate==3)settings.re6_response=false;
        if(gate==4)settings.third_person_distance=1.2f;
        if(gate==5)basis_arg=&other_basis;
        if(gate==6)profile_arg=&other_profile;
        if(gate==7)camera_context=nullptr;
        if(gate==8)context.shoulder=false;
        if(gate==9)f.camera()[0x222]=1;
        if(gate==10)f.input_state[0x503C]=1;
        if(gate==11)f.actor[0x114]=1;
        if(gate==12)f.actor[0x115]=5;
        if(gate==13)unified_pitch=.1f;
        if(gate==14)f.yaw()=.1f;
        if(gate==15)f.input_state[0x86FF]=1;
        if(gate==16)sensor_attributes=0;
        if(gate==17)camera_collision_invalid=true;
        if(gate==18)view_frame.fetch_add(1);
        if(gate==19)f.camera()[0x26C]=0;
        if(gate==20)context.actor=f.other_model;
        if(gate==21)f.input_state[0x5092]=4;
        if(gate==22)*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=1;
        native_collision_fraction=.35f;NativeFollowOutput output{};
        CameraCollision::Callback(nullptr,basis_arg,profile_arg,&output);camera_context=nullptr;
        assert(camera_collision_calls==1);
        assert(!std::memcmp(&output,&camera_collision_result,sizeof(output))); // Clipped eye never moves afterward.
        for(unsigned i=0;i<8;++i){
            if(gate==0&&i==0)assert(near(history[i],output.eye.x));
            else if(gate==0&&i==3)assert(near(history[i],output.target.x));
            else assert(history[i]==100.f+float(i));
        }
    }
    {
        Fixture f;auto* scope=f.scope_mode();control.raw_ry=1;control.raw_ly=1;
        ScopeUpdate::Callback(scope);
        for(unsigned n=0;n<480;++n){game_test::tick+=640000;ScopeUpdate::Callback(scope);}
        assert(near(*reinterpret_cast<float*>(scope+0x118),-1.22173059f));
        assert(*reinterpret_cast<float*>(scope+0x13C)==1&&near(*reinterpret_cast<float*>(scope+0xC8),9));
        scope[0x159]=1;control.raw_ry=-1;control.raw_ly=-1;
        for(unsigned n=0;n<240;++n){game_test::tick+=640000;ScopeUpdate::Callback(scope);}
        assert(near(*reinterpret_cast<float*>(scope+0x118),1.22173059f));
        assert(*reinterpret_cast<float*>(scope+0x13C)==0&&near(*reinterpret_cast<float*>(scope+0xC8),45));
        *reinterpret_cast<float*>(scope+0x13C)=1;control.raw_ly=0;
        settings.right_aim_gain_y=0;game_test::tick+=640000;ScopeUpdate::Callback(scope);
        assert(near(*reinterpret_cast<float*>(scope+0xC8),3));
    }
    {
        Fixture f;auto* scope=f.scope_mode();settings.gyro_enabled=true;
        *reinterpret_cast<int*>(f.input_type)=-1;control.raw_ry=.5f;
        ScopeUpdate::Callback(scope);game_test::tick+=640000;ScopeUpdate::Callback(scope);
        assert(*reinterpret_cast<float*>(scope+0x118)>0); // Stick follows native inversion.
        settings.right_aim_gain_y=0;gyro_pending={0,.1f};assert(gyro_aim_ready(f.actor,true));
        const float before=*reinterpret_cast<float*>(scope+0x118);
        game_test::tick+=640000;ScopeUpdate::Callback(scope);
        assert(near(*reinterpret_cast<float*>(scope+0x118),before-.1f)); // Gyro inversion stays independent.
        const auto yaw=f.yaw();f.flags()=0;ScopeUpdate::Callback(scope);
        f.flags()=0x10;control.raw_rx=1;game_test::tick+=640000;ScopeUpdate::Callback(scope);
        assert(near(f.yaw(),yaw)); // Release/re-entry cannot replay a prior velocity.
    }
    {
        Fixture f;auto* scope=f.scope_mode();scoped_session={};f.actor[0x117]=4;
        control.raw_rx=1;control.raw_ly=1;ScopeUpdate::Callback(scope);
        game_test::tick+=640000;ScopeUpdate::Callback(scope);
        assert(f.yaw()==0&&*reinterpret_cast<float*>(scope+0x13C)==0); // Raising is not ready.
        f.actor[0x117]=1;ScopeUpdate::Callback(scope);game_test::tick+=640000;ScopeUpdate::Callback(scope);
        const float before=f.yaw();assert(before<0);
        f.input_state[0x86FF]=1;ScopeUpdate::Callback(scope);f.input_state[0x86FF]=0;
        game_test::tick+=640000;ScopeUpdate::Callback(scope);assert(near(f.yaw(),before));
    }
    {
        // Inline zoom only touches saved s0 and the exact active scope owner.
        Fixture f;auto* scope=f.scope_mode();exl::hook::InlineFloatCtx context{};
        std::memset(context.bytes,0xA5,sizeof(context.bytes));
        const auto wrong=reinterpret_cast<std::uintptr_t>(scope)+4;
        std::memcpy(context.bytes+0x200+19*8,&wrong,sizeof(wrong));
        auto expected=context;ScopedZoomTarget target{scope,.6f};scoped_zoom_target=&target;
        ScopedZoom::Callback(&context);assert(!std::memcmp(&context,&expected,sizeof(context)));
        const auto owner=reinterpret_cast<std::uintptr_t>(scope);
        std::memcpy(context.bytes+0x200+19*8,&owner,sizeof(owner));expected=context;
        std::memcpy(expected.bytes,&target.zoom,sizeof(target.zoom));ScopedZoom::Callback(&context);
        assert(!std::memcmp(&context,&expected,sizeof(context))&&near(*reinterpret_cast<float*>(scope+0x13C),.6f));
        scoped_zoom_target=nullptr;
    }
    for(bool measured:{false,true}){
        Fixture f;f.view_mode();f.aim();
        auto* scope=f.image.data()+0x1131718+0x530;
        f.image[0x1131718+0x54]=16;
        *reinterpret_cast<void**>(f.image.data()+0x1131718+0x7B0)=scope;
        f.input_state[0x5090]|=0x40;
        *reinterpret_cast<std::uintptr_t*>(f.image.data()+0x1265A28)=main_base+0x332CC0;
        *reinterpret_cast<float*>(scope+0x124)=-1.22173059f;
        *reinterpret_cast<float*>(scope+0x130)=1.22173059f;
        *reinterpret_cast<float*>(scope+0xC8)=45;
        *reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        *reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;sensor_attributes=1;
        settings.unified_controls=true;settings.re6_response=measured;
        auto* axes=reinterpret_cast<int*>(f.image.data()+0x124E9C8);axes[0]=63;axes[1]=-63;
        auto* zoom_byte=reinterpret_cast<std::int8_t*>(f.image.data()+0x124E9D1);*zoom_byte=63;
        auto* directions=reinterpret_cast<std::uint32_t*>(f.image.data()+0x124E9DC);*directions=0x6F;
        control.raw_rx=.5f;control.raw_ry=.5f;control.raw_ly=.5f;scope_original_calls=0;
        ScopeUpdate::Callback(scope);assert(scope_inputs_suppressed&&scope_original_calls==1);
        assert(f.yaw()==0&&*reinterpret_cast<float*>(scope+0x118)==0);
        assert(*zoom_byte==63&&axes[0]==63&&axes[1]==-63&&*directions==0x16F);
        game_test::tick+=640000;ScopeUpdate::Callback(scope);
        const float yaw=f.yaw(),pitch=*reinterpret_cast<float*>(scope+0x118),zoom=*reinterpret_cast<float*>(scope+0x13C);
        assert(yaw<0&&yaw>-.04f&&pitch<0&&pitch>-.04f);
        assert(zoom>0&&zoom<.03f&&near(*reinterpret_cast<float*>(scope+0xC8),45-36*zoom));
        assert(scope_zoom_feedback);
        ScopeUpdate::Callback(scope);assert(near(f.yaw(),yaw)&&near(*reinterpret_cast<float*>(scope+0x13C),zoom));
        // Ordinary aim must leave gyro for the scope's separate pitch storage.
        settings.gyro_enabled=true;assert(gyro_aim_ready(f.actor,true));gyro_pending={.01f,.02f};
        AimSensitivity::Callback(f.actor,0);assert(near(f.yaw(),yaw)&&near(gyro_pending.pitch,.02f));
        control.raw_rx=0;control.raw_ry=0;control.raw_ly=0;
        game_test::tick+=640000;ScopeUpdate::Callback(scope);
        assert(gyro_pending.yaw==0&&gyro_pending.pitch==0);
        const auto after=f.yaw();ScopeUpdate::Callback(scope);assert(near(f.yaw(),after));
        // Zero sensitivity discards old velocity immediately, independently of zoom.
        settings.gyro_enabled=false;settings.right_aim_gain_x=settings.right_aim_gain_y=0;
        control.raw_rx=1;control.raw_ry=1;game_test::tick+=640000;ScopeUpdate::Callback(scope);
        assert(near(f.yaw(),after));
        // All controls disabled: exactly one original call with native inputs.
        settings.unified_controls=settings.analog_enabled=settings.move_while_aiming=false;
        const unsigned before=scope_original_calls;ScopeUpdate::Callback(scope);
        assert(scope_original_calls==before+1&&!scope_inputs_suppressed&&f.yaw()!=after);
    }
    // Weapon controls must retain the native +/-1 pitch range. The overview's
    // 35-degree comfort band must not prevent aiming a firearm/knife downward.
    for(unsigned routine:{6u,11u})for(float sign:{-1.f,1.f}){
        Fixture f;f.view_mode();f.aim();
        if(routine==11){f.actor[0x115]=11;f.flags()=re4::kKnifeHeldMask;}
        settings.unified_controls=true;settings.analog_enabled=false;
        sensor_attributes=1;*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;
        *reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        modern_probe=true;control.raw_ry=sign;ModernPlayerUpdate::Callback(f.actor);
        for(unsigned n=0;n<80;++n){game_test::tick+=640000;ModernPlayerUpdate::Callback(f.actor);}
        assert(near(unified_pitch,sign*1.570796327f));
        assert(near(f.pitch()[0],sign)&&near(f.pitch()[1],sign));
        modern_probe=false;
    }
    for(unsigned character:{0u,1u}){
        Fixture f;f.view_mode();f.input_state[0x503C]=std::uint8_t(character);
        settings.unified_controls=true;settings.re6_response=true;
        sensor_attributes=1;*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;
        *reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        control.raw_rx=1;control.raw_ry=1;ModernPlayerUpdate::Callback(f.actor);
        for(unsigned i=0;i<4;++i){game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);}
        assert(f.yaw()<-.1f&&unified_pitch>.1f);
        const float before=f.yaw();f.input_state[0x503C]=std::uint8_t(1-character);
        control.raw_rx=0;control.raw_ry=0;game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);
        assert(near(f.yaw(),before)&&near(unified_pitch,0)); // Reused actor, different playable character.
        control.raw_rx=1;game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);
        assert(f.yaw()<before); // New character's live look remains available.
    }
    struct AshleyWalkCase {float x,y,want_x,want_z,gain;unsigned routine;};
    for(const auto c:{AshleyWalkCase{0,1,0,7,1,1},{1,0,-7,0,1,1},
            {-1,0,7,0,1,1},{1,1,-4.949747f,4.949747f,1,3},
            {0,-1,0,-7,1,2},{.5f,0,-7,0,.5f,1}}){
        Fixture f;f.view_mode();f.input_state[0x503C]=1;f.actor[0x115]=std::uint8_t(c.routine);
        settings.unified_controls=true;settings.re6_response=true;settings.left_deadzone=0;
        sensor_attributes=1;*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;
        *reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        void* secondary=f.other_model;*reinterpret_cast<void**>(f.actor+0x320)=secondary;
        native_primary={0,3,c.routine==2?-10.f:10.f};native_secondary={0,5,c.routine==2?-6.f:6.f};
        control.raw_lx=c.x;control.raw_ly=c.y;f.flags()=15;native_root_probe=true;
        ModernPlayerUpdate::Callback(f.actor);native_root_probe=false;
        assert(near(root_result.x,c.want_x)&&near(root_result.z,c.want_z)&&near(root_result.y,4.5f));
        assert(near(std::hypot(root_result.x,root_result.z),7)); // Rotate the native budget; no extra speed.
        assert(near(game_test::motion_speed_seen,1.25f*c.gain));
        assert(near(*reinterpret_cast<float*>(f.actor+0x304),1.25f));
        assert((game_test::directions_seen&15)==(c.y<0?2u:1u)&&(f.flags()&15)==15);
        assert(modern_original_updates==1&&modern_original_roots==1&&modern_original_poses==1);
        control.raw_lx=0;control.raw_ly=0;ModernPlayerUpdate::Callback(f.actor);
        assert((game_test::directions_seen&15)==0&&near(game_test::motion_speed_seen,0));
    }
    native_primary={0,3,10};native_secondary={0,5,6};
    for(unsigned character:{0u,1u})for(bool aiming:{false,true}){
        if(character==1&&aiming)continue;
        Fixture f;f.view_mode();if(aiming){f.aim();f.camera()[0x26C]=0;}
        f.input_state[0x503C]=std::uint8_t(character);
        settings.unified_controls=true;settings.re6_response=true;settings.third_person_distance=1.15f;
        sensor_attributes=1;*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;
        *reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        publish_view_settings();unified_pitch_actor=reinterpret_cast<std::uintptr_t>(f.actor);unified_pitch=0;
        CameraInput::Callback(f.camera());
        // Root(100,200,300) + the neutral shoulder arm at distance1.15.
        assert(near(camera_collision_input.eye0.x,-448)&&near(camera_collision_input.eye0.y,1974.75f)
            &&near(camera_collision_input.eye0.z,-1052));
        assert(std::memcmp(&camera_collision_input.eye1,&native_profile.eye1,sizeof(re4::ViewVec3))==0);
        const auto expected=camera_collision_input.eye0;
        // Native profile blending and delayed/transformed frame bases cannot
        // drag the desired world eye. Native18990 still receives the output.
        native_profile.eye0={-700,1800,-900};native_profile.target={80,1400,1800};
        native_basis={{{0,0,1,800},{0,1,0,-300},{-1,0,0,1200}}};
        CameraInput::Callback(f.camera());re4::ViewVec3 world;
        assert(re4::view_transform_point(native_basis,camera_collision_input.eye0,world));
        assert(near(world.x,expected.x)&&near(world.y,expected.y)&&near(world.z,expected.z));
        assert(camera_collision_input.fov==native_profile.fov&&camera_collision_input.roll==native_profile.roll);
        auto* body=reinterpret_cast<re4::ViewMatrix3x4*>(f.actor+0x18);
        body->m[0][3]+=320;body->m[2][3]-=180;
        CameraInput::Callback(f.camera());assert(re4::view_transform_point(native_basis,camera_collision_input.eye0,world));
        assert(near(world.x,expected.x+320)&&near(world.z,expected.z-180));
        f.yaw()=1.57079632679f;
        CameraInput::Callback(f.camera());assert(re4::view_transform_point(native_basis,camera_collision_input.eye0,world));
        assert(std::fabs(world.x+932)<.001f&&std::fabs(world.z-668)<.001f);
    }
    for(unsigned character:{0u,1u})for(unsigned gate=0;gate<12;++gate){
        Fixture f;f.view_mode();settings.unified_controls=true;settings.re6_response=true;settings.third_person_distance=1.15f;
        f.input_state[0x503C]=std::uint8_t(character);
        sensor_attributes=1;*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;
        *reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        if(gate==0)settings.first_person=true;
        if(gate==1)settings.re6_response=false;
        if(gate==2)settings.unified_controls=false;
        if(gate==3)f.input_state[0x503C]=2;
        if(gate==4)f.actor[0x115]=5;
        if(gate==5)f.camera()[0x222]=1;
        if(gate==6)f.camera()[0x26C]=0;
        if(gate==7)f.input_state[0x5092]=4;
        if(gate==8)sensor_attributes=0;
        if(gate==9)*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=1;
        if(gate==10)std::memset(f.actor+0x18,0,sizeof(re4::ViewMatrix3x4));
        if(gate==11)f.yaw()=std::numeric_limits<float>::quiet_NaN();
        publish_view_settings();unified_pitch_actor=reinterpret_cast<std::uintptr_t>(f.actor);unified_pitch=0;
        CameraInput::Callback(f.camera());
        if(gate==0){
            assert(near(camera_collision_input.eye0.x,100)&&near(camera_collision_input.eye0.y,1900)&&near(camera_collision_input.eye0.z,300));
            assert(first_person_frame.load()==view_frame.load());
        }else if(gate==1||gate==2||gate==3||gate==8||gate==9){
            re4::ViewQfpsOutput original;assert(re4::third_person_view(native_profile,1.15f,original));
            assert(std::memcmp(&camera_collision_input,&original,sizeof(original))==0);
        }else assert(std::memcmp(&camera_collision_input,&native_profile,sizeof(native_profile))==0);
    }
    for(unsigned sub=0;sub<4;++sub){
        Fixture f;f.view_mode();f.aim();settings.unified_controls=true;settings.move_while_reloading=true;
        sensor_attributes=1;*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;*reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        *reinterpret_cast<std::uint16_t*>(f.actor+0x352)=0x300;
        game_test::memory_maps={{reinterpret_cast<std::uintptr_t>(f.actor),sizeof(f.actor),3},
            {reinterpret_cast<std::uintptr_t>(f.input_state.data()),f.input_state.size(),3}};
        f.actor[0x116]=4;f.actor[0x117]=std::uint8_t(sub);f.flags()=0;game_test::native_aim=0;
        unified_pitch_actor=reinterpret_cast<std::uintptr_t>(f.actor);unified_pitch=.4f;
        assert(modern_move_reload_active(f.actor)&&!modern_move_aim_active(f.actor));
        assert(unified_view_ready(f.actor)); // Keep the angle when ZL is released during reload.
        settings.gyro_enabled=true;assert(!gyro_aim_ready(f.actor,true));
        settings.move_while_reloading=false;assert(!unified_view_ready(f.actor));
        settings.move_while_reloading=true;f.actor[0x117]=4;assert(!unified_view_ready(f.actor));
    }
    // Reload look must run even though native reload never calls firearm aim,
    // with aim released and no left-stick motion/assets available.
    for(std::size_t handler:{0x31CC60u,0x3351A0u,0x3270F0u,0x332CC0u,0x342F90u})
    for(bool fps:{false,true})for(bool held:{false,true}){
        Fixture f;f.view_mode();f.aim();settings.unified_controls=true;settings.move_while_reloading=true;
        settings.first_person=fps;settings.gyro_enabled=true;settings.re6_response=held;
        sensor_attributes=1;*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;
        *reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        *reinterpret_cast<std::uint16_t*>(f.actor+0x352)=0x300;
        *reinterpret_cast<std::uintptr_t*>(f.image.data()+0x1265A28)=main_base+handler;
        game_test::memory_maps={{reinterpret_cast<std::uintptr_t>(f.actor),sizeof(f.actor),3},
            {reinterpret_cast<std::uintptr_t>(f.input_state.data()),f.input_state.size(),3}};
        f.actor[0x116]=4;f.actor[0x117]=1;f.flags()=held?0x10:0;game_test::native_aim=held?1:0;
        control.raw_rx=1;control.raw_ry=1;gyro_pending={.5f,.5f};
        ModernPlayerUpdate::Callback(f.actor);
        assert(near(f.yaw(),0)); // Entry drains old velocity.
        for(unsigned n=0;n<30;++n){game_test::tick+=640000;ModernPlayerUpdate::Callback(f.actor);}
        assert(f.yaw()<-1&&unified_pitch>.3f&&unified_pitch<=.610866f);
        assert(!control.modern_move_steps&&!control.aim_calls);
        assert(f.pitch()[0]==0&&f.pitch()[1]==0); // No native aim pitch writes.
        assert(modern_original_updates==31&&modern_original_roots==31&&modern_original_poses==31);
        assert(!gyro_aim_ready(f.actor,true)&&unified_view_ready(f.actor));
        const re4::ViewVec3 eye{},up{0,1,0},target{0,0,1000};
        std::memcpy(f.input_state.data()+0x14C,&eye,sizeof(eye));
        std::memcpy(f.input_state.data()+0x158,&target,sizeof(target));
        std::memcpy(f.input_state.data()+0x16C,&up,sizeof(up));
        f.camera()[0x26C]=held?0:2;calculate_probe=true;CameraCalculate::Callback();calculate_probe=false;
        assert(near(look_seen.x/1000,std::sin(f.yaw())*std::cos(unified_pitch)));
        assert(near(look_seen.y/1000,std::sin(unified_pitch)));
        const float yaw=f.yaw();
        settings.camera_gain=0;game_test::tick+=640000;ModernPlayerUpdate::Callback(f.actor);
        assert(near(f.yaw(),yaw));
        settings.camera_gain=1;f.input_state[0x86FF]=1;
        game_test::tick+=640000;ModernPlayerUpdate::Callback(f.actor);assert(near(f.yaw(),yaw));
        f.input_state[0x86FF]=0;game_test::tick+=640000;ModernPlayerUpdate::Callback(f.actor);
        assert(near(f.yaw(),yaw)); // Resume also drains its first sample.
        game_test::tick+=640000;ModernPlayerUpdate::Callback(f.actor);assert(f.yaw()<yaw);
        const float resumed=f.yaw();update_state_change=1;
        game_test::tick+=640000;ModernPlayerUpdate::Callback(f.actor);update_state_change=0;
        assert(near(f.yaw(),resumed)); // Native damage transition before root cancels look.
        f.actor[0x114]=0;settings.move_while_reloading=false;
        game_test::tick+=640000;ModernPlayerUpdate::Callback(f.actor);assert(near(f.yaw(),resumed));
    }
    for(unsigned change:{2u,3u,4u}){
        Fixture f;f.view_mode();f.aim();settings.unified_controls=true;settings.move_while_reloading=true;
        sensor_attributes=1;*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;
        *reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        *reinterpret_cast<std::uint16_t*>(f.actor+0x352)=0x300;
        game_test::memory_maps={{reinterpret_cast<std::uintptr_t>(f.actor),sizeof(f.actor),3},
            {reinterpret_cast<std::uintptr_t>(f.input_state.data()),f.input_state.size(),3}};
        f.actor[0x116]=4;f.actor[0x117]=1;f.flags()=0;game_test::native_aim=0;control.raw_rx=1;
        ModernPlayerUpdate::Callback(f.actor);
        game_test::tick+=640000;ModernPlayerUpdate::Callback(f.actor);assert(f.yaw()<0);
        float before=f.yaw();change_reload_identity(f.actor,change);
        game_test::tick+=640000;ModernPlayerUpdate::Callback(f.actor);assert(near(f.yaw(),before));
        game_test::tick+=640000;ModernPlayerUpdate::Callback(f.actor);assert(f.yaw()<before);
        before=f.yaw();update_state_change=change;
        game_test::tick+=640000;ModernPlayerUpdate::Callback(f.actor);update_state_change=0;
        assert(near(f.yaw(),before)); // A different reload cannot consume an entry-time command.
        game_test::tick+=640000;ModernPlayerUpdate::Callback(f.actor);assert(near(f.yaw(),before));
    }
    for(bool grenade:{false,true}){
        // Exercise steering and translation together through the actual update,
        // root and pose hooks. Only external animation/physics calls are fake.
        Fixture f;f.view_mode();f.aim();settings.unified_controls=true;settings.move_while_reloading=true;
        sensor_attributes=1;*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;
        *reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        *reinterpret_cast<std::uint16_t*>(f.actor+0x352)=0x300;
        alignas(16) std::uint8_t asset[80]{},table[0x330]{};
        alignas(16) std::uint8_t bones[26][0x1E0]{};
        const auto store=[]<class T>(std::uint8_t* p,unsigned offset,T v){std::memcpy(p+offset,&v,sizeof(v));};
        store(asset,0,std::uint16_t(40));asset[2]=1;store(asset,3,std::uint16_t(1));asset[5]=0;
        store(asset,12,std::uint64_t(32));
        for(unsigned axis=0;axis<3;++axis)store(asset,32+16*axis,std::uint16_t(1));
        for(unsigned offset:{0x10u,0x40u,0x308u,0x318u})store(table,offset,reinterpret_cast<std::uintptr_t>(asset));
        store(f.actor,0x108,reinterpret_cast<std::uintptr_t>(bones));store(f.actor,0x428,reinterpret_cast<std::uintptr_t>(bones));
        store(f.actor,0x548,reinterpret_cast<std::uintptr_t>(table));f.actor[0x11A]=26;
        const int parents[]={-1,0,1,2,3,2,5,5,7,8,9,2,11,11,13,14,15,0,17,18,19,20,17,22,23,24};
        for(unsigned i=0;i<26;++i){
            store(bones[i],0x78,reinterpret_cast<std::uintptr_t>(i?bones[parents[i]]:f.actor));
            store(bones[i],0x108,i<25?reinterpret_cast<std::uintptr_t>(bones[i+1]):std::uintptr_t(0));
            for(unsigned j=0;j<3;++j){store(bones[i],0xBC+4*j,1.f);store(bones[i],0x48+20*j,1.f);}
        }
        game_test::memory_maps={{reinterpret_cast<std::uintptr_t>(f.actor),sizeof(f.actor),3},
            {reinterpret_cast<std::uintptr_t>(f.input_state.data()),f.input_state.size(),3},
            {reinterpret_cast<std::uintptr_t>(asset),sizeof(asset),3},
            {reinterpret_cast<std::uintptr_t>(table),sizeof(table),3},
            {reinterpret_cast<std::uintptr_t>(bones),sizeof(bones),3}};
        modern_native.move.sample=+[](const re4::ModernMoveSample* s,re4::ModernMoveVec3* out,std::uint16_t*,unsigned,unsigned){*out={0,0,10*s->frame};};
        modern_native.move.advance=+[](void*)->std::uint32_t{return 0;};
        modern_native.move.root=+[](void*,void* motion,unsigned,re4::ModernMoveVec3* out,re4::ModernMoveVec3* r){*out={0,0,10**reinterpret_cast<float*>(static_cast<std::uint8_t*>(motion)+0xDC)};*r={};};
        modern_native.move.translate=+[](void* actor,const re4::ModernMoveVec3* d){auto* p=static_cast<std::uint8_t*>(actor);*reinterpret_cast<float*>(p+0xA4)+=d->x;*reinterpret_cast<float*>(p+0xAC)+=d->z;};
        modern_native.evaluate=+[](void*,void*){};
        modern_native.rotation=+[](void*,const void*){};modern_native.position=+[](void*,const void*){};
        modern_native.scale=+[](void*,const void*){};modern_native.copy_matrix=+[](const void*,void*){};
        modern_native.healthy=+[]()->int{return 1;};
        if(grenade){
            *reinterpret_cast<std::uintptr_t*>(f.image.data()+0x1265A28)=main_base+0x31B9F0;
            settings.move_while_aiming=true;grenade_native_order=true;
        }else{f.actor[0x116]=4;f.actor[0x117]=1;f.flags()=0;game_test::native_aim=0;}
        control.raw_ly=1;control.raw_rx=1;
        ModernPlayerUpdate::Callback(f.actor);assert(near(*reinterpret_cast<float*>(f.actor+0xAC),10));
        game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);
        assert(near(f.yaw(),-.038524245f));
        assert(near(*reinterpret_cast<float*>(f.actor+0xA4),grenade?0:10*std::sin(-.038524245f)));
        assert(near(*reinterpret_cast<float*>(f.actor+0xAC),grenade?20:10+10*std::cos(-.038524245f)));
        assert(control.modern_move_steps==2&&control.modern_pose_steps==2);
        const float x=*reinterpret_cast<float*>(f.actor+0xA4),z=*reinterpret_cast<float*>(f.actor+0xAC);
        ModernRoot::Callback(f.actor,nullptr);
        assert(near(*reinterpret_cast<float*>(f.actor+0xA4),x)&&near(*reinterpret_cast<float*>(f.actor+0xAC),z));
        if(grenade){
            const float heading=f.yaw();f.actor[0x116]=2;f.actor[0x117]=1;
            game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);
            assert(near(grenade_release_position.x,x+10*std::sin(heading)));
            assert(near(grenade_release_position.z,z+10*std::cos(heading)));
            assert(control.modern_move_steps==3&&control.modern_pose_steps==3);
        }
    }
    {
        Fixture f;f.view_mode();f.aim();settings.unified_controls=true;settings.re6_response=true;settings.analog_enabled=false;
        settings.camera_gain=0;settings.right_aim_gain_x=1;settings.right_aim_gain_y=0;
        sensor_attributes=1;*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;*reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        control.raw_rx=1;control.raw_ry=1;modern_probe=true;
        ModernPlayerUpdate::Callback(f.actor);
        for(unsigned n=0;n<40;++n){game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);}
        assert(f.yaw()<-1.5f&&near(unified_pitch,0)); // RE6 aim gain is independent of overview gain.
        const float before=f.yaw();settings.right_aim_gain_x=0;settings.right_aim_gain_y=1;
        game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);assert(near(f.yaw(),before));
        for(unsigned n=0;n<10;++n){game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);}
        assert(near(f.yaw(),before)&&unified_pitch>.1f); // Gain0 drains its axis without freezing the other.
        modern_probe=false;game_test::native_aim=0;f.actor[0x115]=1;f.flags()=1;
        const float walking_before=f.yaw();
        for(unsigned n=0;n<10;++n){game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);}
        assert(near(f.yaw(),walking_before)); // Overview still follows its own zero gain.
        settings.camera_gain=1;game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);
        for(unsigned n=0;n<10;++n){game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);}
        assert(f.yaw()<walking_before-1.f);
    }
    {
        Fixture f;f.view_mode();
        re6::BuildId id{};std::memcpy(id.data(),re4::profile::build_id,32);
        assert(telemetry_ring.initialize(100,id,re6::kRe4TitleId));
        telemetry_last_frame=0;
        settings.remap_enabled=true;settings.button_map[0]=1;
        NpadState pad{};pad.sample=53;pad.buttons=1;pad.lx=-32768;pad.ly=321;pad.rx=17321;pad.ry=-12345;pad.attributes=1;
        game_test::tick=192000;capture(pad,0,4);assert(pad.buttons!=1);
        game_test::tick+=640000;CameraCalculate::Callback();
        re6::Record r{};std::uint64_t words[32]{};
        for(unsigned i=0;i<32;++i)words[i]=telemetry_ring.storage.slots[0].words[i].load();
        std::memcpy(&r,words,sizeof(r));
        assert(r.validity==15&&r.buttons==1&&r.sample_id==53&&r.input_tick==192000);
        assert(r.tick==832000&&r.frame==view_frame&&r.sticks[0]==-32768&&r.sticks[2]==17321);
        assert(r.player[3]==100&&r.player[7]==200&&r.player[11]==300&&r.view[0]==1&&r.projection[0]==.75f);
        CameraCalculate::Callback();assert(telemetry_ring.storage.header.sequence.load()==4); // No duplicate simulation frame.
        view_frame.fetch_add(1);game_test::tick+=1920001;CameraCalculate::Callback();
        for(unsigned i=0;i<32;++i)words[i]=telemetry_ring.storage.slots[1].words[i].load();
        std::memcpy(&r,words,sizeof(r));assert(!(r.validity&re6::InputValid));
        NpadState other=pad;other.rx=-99;other.attributes=1;capture(other,1,1);
        re6::Record input{};assert(!telemetry_read_input(game_test::tick,input)); // Secondary pad cannot replace primary snapshot.
        pad.attributes=0;capture(pad,0,4);assert(!telemetry_read_input(game_test::tick,input));
    }
    for(bool aim:{false,true}){
        Fixture f;f.view_mode();if(aim)f.aim();settings.unified_controls=true;settings.analog_enabled=false;
        sensor_attributes=1;*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;*reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        modern_probe=aim;control.raw_rx=1;control.raw_ry=1;ModernPlayerUpdate::Callback(f.actor);
        game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);const float yaw=f.yaw(),pitch=unified_pitch;
        settings.camera_gain=0;game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);
        assert(near(f.yaw(),yaw)&&near(unified_pitch,pitch));
        settings.camera_gain=1;game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);assert(near(f.yaw(),yaw));
        game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);assert(near(f.yaw()-yaw,-.038524245f));
        modern_probe=false;
    }

    for(bool fps:{false,true}){
        Fixture f;f.view_mode();f.aim();settings.unified_controls=true;settings.analog_enabled=false;settings.first_person=fps;
        sensor_attributes=1;*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;*reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        f.camera()[0x26C]=0;modern_probe=true;simulate_weapon_sway=true;
        ModernPlayerUpdate::Callback(f.actor);assert(near(unified_pitch,0)&&f.pitch()[1]>.02f);
        game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);assert(near(unified_pitch,0));
        const re4::ViewVec3 eye{},up{0,1,0},target{0,0,1000};std::memcpy(f.input_state.data()+0x14C,&eye,sizeof(eye));std::memcpy(f.input_state.data()+0x158,&target,sizeof(target));std::memcpy(f.input_state.data()+0x16C,&up,sizeof(up));
        calculate_probe=true;CameraCalculate::Callback();calculate_probe=false;
        assert(near(look_seen.x,0)&&near(look_seen.y,0));assert(near(game_test::rendered_pitch,0));assert(f.look()[0]>.02f);
        modern_probe=false;
    }
    for(bool aim:{false,true}){
        Fixture f;f.view_mode();if(aim)f.aim();settings.unified_controls=true;settings.analog_enabled=false;
        sensor_attributes=1;*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;*reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        modern_probe=aim;ModernPlayerUpdate::Callback(f.actor);unified_pitch=.6f;
        f.pitch()[0]=f.pitch()[1]=re4::unified_pitch_weight(.6f);control.raw_ry=1;
        game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);
        assert(aim?unified_pitch>.610865238f:near(unified_pitch,.610865238f));
        for(unsigned i=0;i<5;++i){
            const float previous=unified_pitch;game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);
            assert(aim?unified_pitch>previous:near(unified_pitch,.610865238f));
        }
        modern_probe=false;
    }
    {
        Fixture f;f.view_mode();f.aim();settings.unified_controls=true;settings.analog_enabled=false;settings.gyro_enabled=true;
        sensor_attributes=1;*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;*reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        modern_probe=true;ModernPlayerUpdate::Callback(f.actor);unified_pitch=1.2f;f.pitch()[0]=f.pitch()[1]=.763943727f;
        assert(gyro_aim_ready(f.actor,true));gyro_pending={0,.1f};control.raw_ry=0;
        game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);assert(near(unified_pitch,1.3f));
        control.raw_ry=0;game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);assert(near(unified_pitch,1.3f));
        control.raw_ry=-1;game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);assert(unified_pitch>1.2f&&unified_pitch<1.3f);
        // Gyro can reach the native pole; stable analytic up keeps the view finite.
        gyro_pending={0,.5f};game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);assert(near(unified_pitch,1.570796327f));
        const re4::ViewVec3 eye{},up{0,1,0},target{0,0,1000};std::memcpy(f.input_state.data()+0x14C,&eye,sizeof(eye));std::memcpy(f.input_state.data()+0x158,&target,sizeof(target));std::memcpy(f.input_state.data()+0x16C,&up,sizeof(up));
        f.camera()[0x26C]=0;calculate_probe=true;CameraCalculate::Callback();calculate_probe=false;assert(look_seen.y>990);
        const auto* view=reinterpret_cast<const re4::ViewMatrix3x4*>(f.input_state.data()+0xD8);for(const auto& row:view->m)for(float value:row)assert(std::isfinite(value));
        modern_probe=false;
    }

    {
        Fixture f;f.view_mode();settings.unified_controls=true;settings.analog_enabled=false;
        sensor_attributes=1;*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;
        *reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        control.raw_lx=1;control.raw_ly=0;control.raw_rx=1;control.raw_ry=1;f.flags()=15;unified_probe=true;
        ModernPlayerUpdate::Callback(f.actor);assert(unified_flags_seen==1);assert((f.flags()&15)==15);
        game_test::tick+=960000;const float before=f.yaw();ModernPlayerUpdate::Callback(f.actor);
        assert(near(f.yaw()-before,-.038524245f));assert(near(game_test::motion_speed_seen,1.25f));
        unified_probe=false;
    }

    {
        Fixture f;f.view_mode();f.aim();settings.unified_controls=true;settings.analog_enabled=false;
        sensor_attributes=1;*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;*reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        control.raw_rx=1;control.raw_ry=1;control.raw_lx=-1;control.raw_ly=-1;modern_probe=true;
        auto* axes=reinterpret_cast<int*>(f.image.data()+0x124E9C8);axes[0]=63;axes[1]=-63;
        ModernPlayerUpdate::Callback(f.actor);assert(seen_aim_x==0&&seen_aim_y==0);
        game_test::tick+=960000;const auto before=f.yaw();ModernPlayerUpdate::Callback(f.actor);
        assert(near(f.yaw()-before,-.038524245f));assert(near(f.pitch()[1],.018393972f));
        assert(axes[0]==63&&axes[1]==-63);assert(near(f.pitch()[0],f.pitch()[1]));
        const auto yaw=f.yaw();AimSensitivity::Callback(f.actor,0);assert(near(f.yaw(),yaw));
        const float expected_yaw[]={-.080367656f,-.095760823f};unsigned interval=0;
        for(unsigned phase:{2u,1u}){
            f.actor[0x116]=phase;f.actor[0x117]=phase==1?4:1;
            game_test::tick+=960000;const float value=f.yaw();ModernPlayerUpdate::Callback(f.actor);
            assert(near(f.yaw()-value,expected_yaw[interval++]));
        }
        settings.unified_controls=false;publish_view_settings();settings.unified_controls=true;publish_view_settings();
        game_test::tick+=960000;const float toggled=f.yaw();ModernPlayerUpdate::Callback(f.actor);assert(near(f.yaw(),toggled));
        NpadState switched{};switched.attributes=1;switched.rx=switched.ry=32767;
        capture(switched,0,4);game_test::tick+=960000;const float stopped=f.yaw();ModernPlayerUpdate::Callback(f.actor);assert(near(f.yaw(),stopped));
        f.actor[0x117]=1;game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);
        f.flags()=0;AimSensitivity::Callback(f.actor,0);f.flags()=0x10;f.actor[0x117]=4;
        game_test::tick+=960000;const float released=f.yaw();ModernPlayerUpdate::Callback(f.actor);assert(near(f.yaw(),released));
        modern_probe=false;
    }
    {
        Fixture f;f.view_mode();settings.unified_controls=true;settings.analog_enabled=false;
        sensor_attributes=1;*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;*reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        const re4::ViewMatrix3x4 basis{{{2,0,0,0},{0,3,0,0},{0,0,4,0}}};std::memcpy(f.actor+0x18,&basis,sizeof(basis));
        void* secondary=f.other_model;*reinterpret_cast<void**>(f.actor+0x320)=secondary;
        control.raw_lx=1;control.raw_ly=0;native_root_probe=true;
        ModernPlayerUpdate::Callback(f.actor);
        re4::ViewVec3 world;assert(re4::view_transform_point(basis,root_result,world));
        assert(near(world.x,-28)&&near(world.y,13.5f)&&std::fabs(world.z)<.0001f);
        assert(control.modern_move_steps==0);native_root_probe=false;
    }
    {
        Fixture f;f.view_mode();f.aim();settings.unified_controls=true;settings.reticle=re4::ReticleStyle::Crosshair;
        sensor_attributes=1;*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;*reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        f.camera()[0x26C]=0;modern_probe=true;ModernPlayerUpdate::Callback(f.actor);modern_probe=false;
        laser_endpoint={500,300,1000};
        const re4::ViewVec3 eye{10,20,30},up{0,1,0},target{0,0,100};
        std::memcpy(f.input_state.data()+0x14C,&eye,sizeof(eye));std::memcpy(f.input_state.data()+0x158,&target,sizeof(target));std::memcpy(f.input_state.data()+0x16C,&up,sizeof(up));
        WeaponLaser::Callback(f.weapon,1,0,0);calculate_probe=true;CameraCalculate::Callback();calculate_probe=false;
        assert(near(look_seen.x,eye.x)&&near(look_seen.y,eye.y)&&look_seen.z>eye.z);
        re4::ViewMatrix3x4 view;re4::ViewMatrix4x4 projection;std::memcpy(&view,f.input_state.data()+0xD8,sizeof(view));std::memcpy(&projection,f.input_state.data()+0x10C,sizeof(projection));
        re4::ViewHudPoint point;assert(re4::project_view_point(view,projection,laser_endpoint,point));assert(std::fabs(point.x-256)>5);
        SceneSubmit::Callback();view_frame.fetch_add(1);SceneDraw::Callback();
        CameraInput::Callback(f.camera());HudDraw::Callback(nullptr);assert(hud_rects.size()==4);
        const auto drawn=std::size_t(hud_rects.size());
        // Animated/near-wall endpoints must move the truthful marker, never the view.
        for(auto ray:{re4::ViewVec3{15,21,40},re4::ViewVec3{-300,200,800},re4::ViewVec3{10,20,50000}}){
            laser_endpoint=ray;WeaponLaser::Callback(f.weapon,1,0,0);calculate_probe=true;CameraCalculate::Callback();calculate_probe=false;
            const auto* current=reinterpret_cast<const re4::ViewMatrix3x4*>(f.input_state.data()+0xD8);
            for(unsigned i=0;i<3;++i)for(unsigned j=0;j<4;++j)assert(near(current->m[i][j],view.m[i][j]));
        }
        view_frame.fetch_add(1);CameraInput::Callback(f.camera());HudDraw::Callback(nullptr);assert(hud_rects.size()==drawn);
    }
    {
        Fixture f;f.view_mode();settings.unified_controls=true;settings.analog_enabled=false;
        sensor_attributes=1;*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;*reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        control.raw_ry=1;ModernPlayerUpdate::Callback(f.actor);game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);
        control.raw_ry=0;f.aim();modern_probe=true;
        f.actor[0x116]=0;game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);
        f.actor[0x116]=1;f.actor[0x117]=4;game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);
        f.actor[0x117]=1;game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);
        assert(near(f.pitch()[1],.018393972f));modern_probe=false;
        f.actor[0x115]=1;game_test::native_aim=0;dpad_move=true;f.flags()=1;unified_probe=true;
        ModernPlayerUpdate::Callback(f.actor);assert(unified_flags_seen==1);unified_probe=false;
    }
    for(bool fps:{false,true}){
        Fixture f;f.view_mode();settings.unified_controls=true;settings.first_person=fps;settings.analog_enabled=false;
        sensor_attributes=1;*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;*reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        control.raw_rx=1;control.raw_ry=1;ModernPlayerUpdate::Callback(f.actor);game_test::tick+=960000;ModernPlayerUpdate::Callback(f.actor);
        const re4::ViewVec3 eye{},up{0,1,0},target{0,0,1000};std::memcpy(f.input_state.data()+0x14C,&eye,sizeof(eye));std::memcpy(f.input_state.data()+0x158,&target,sizeof(target));std::memcpy(f.input_state.data()+0x16C,&up,sizeof(up));
        calculate_probe=true;CameraCalculate::Callback();calculate_probe=false;
        assert(near(look_seen.x/1000,std::sin(-.038524245f)*std::cos(.028893184f)));
        assert(near(look_seen.y/1000,std::sin(.028893184f)));assert(f.look()[0]==0&&f.look()[1]==0);
    }
    {
        Fixture f;f.view_mode();f.aim();settings.unified_controls=true;settings.analog_enabled=false;settings.gyro_enabled=true;
        sensor_attributes=1;*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;*reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        game_test::apply_sway=true;assert(gyro_aim_ready(f.actor,true));gyro_pending={.02f,.03f};AimSensitivity::Callback(f.actor,0);
        assert(near(f.yaw(),.02f)&&near(f.pitch()[1],.03f*.636619772368f));assert(near(game_test::sway_baseline,f.pitch()[1]));
    }
    {
        Fixture f;f.view_mode();settings.unified_controls=true;settings.analog_enabled=false;
        sensor_attributes=1;*reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;*reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        control.raw_rx=1;control.raw_ry=1;unified_probe=true;ModernPlayerUpdate::Callback(f.actor);
        game_test::tick+=960000;const float before=f.yaw();update_state_change=1;ModernPlayerUpdate::Callback(f.actor);
        assert(near(f.yaw(),before)&&unified_pitch==0);update_state_change=0;unified_probe=false;
    }
    {
        Fixture f;f.aim();settings.gyro_enabled=true;game_test::aim_step=0;
        NpadState pad{};pad.attributes=1;capture(pad,0,1);
        f.actor[0x116]=0;f.sample(1);poll_sensors();
        f.actor[0x116]=1;sensor_batch.clear();game_test::tick+=96'000;poll_sensors();
        // An empty readiness-edge read does not flush the hardware FIFO.
        f.sample(2);const auto s=sensor_batch[0];sensor_batch={s,s,s};
        sensor_batch[0].number=4;sensor_batch[1].number=3;
        game_test::tick+=96'000;poll_sensors();AimSensitivity::Callback(f.actor,0);
        assert(f.pitch()[1]==0 && f.yaw()==0);
        game_test::tick+=96'000;poll_sensors();AimSensitivity::Callback(f.actor,0);
        assert(f.pitch()[1]==0 && f.yaw()==0); // Repeated FIFO must not replay the drained samples.
        for(unsigned n=5;n<=8;++n){game_test::tick+=96'000;f.sample(n);poll_sensors();AimSensitivity::Callback(f.actor,0);}
        assert(f.pitch()[1]>0);
    }
    {
        Fixture f;f.aim();settings.gyro_enabled=true;game_test::aim_step=0;
        assert(gyro_aim_ready(f.actor,true));gyro_pending={0.2f,0.2f};
        f.flags()=0;AimSensitivity::Callback(f.actor,0); // Release after the poll.
        f.flags()=0x10;AimSensitivity::Callback(f.actor,0);
        assert(f.pitch()[1]==0 && f.yaw()==0); // Old session cannot be revived by re-holding.
    }
    {
        Fixture f;f.aim();settings.gyro_enabled=true;game_test::aim_step=0;
        NpadState pad{};pad.attributes=1;capture(pad,0,1);
        // Broad native camera-aim predicate is already true while the weapon
        // is rising. Neither this motion nor its queued FIFO may move aim.
        for(auto phase : {0,1}){
            f.actor[0x116]=std::uint8_t(phase);f.actor[0x117]=phase?4:1;
            for(unsigned n=1;n<=8;++n){
                game_test::tick+=96'000;f.sample(unsigned(phase)*8+n);poll_sensors();
                AimSensitivity::Callback(f.actor,0);
                assert(f.pitch()[0]==0 && f.pitch()[1]==0 && f.yaw()==0);
            }
        }
        f.actor[0x117]=1;
        // Native state can become ready after polling, within actor update.
        // Consumption must not arm early and replay a pre-transition sample.
        gyro_pending={0.2f,0.2f};AimSensitivity::Callback(f.actor,0);
        assert(f.pitch()[1]==0 && f.yaw()==0);
        game_test::tick+=96'000;f.sample(17);poll_sensors();
        AimSensitivity::Callback(f.actor,0);assert(f.pitch()[1]==0); // Drain readiness-edge batch.
        for(unsigned n=18;n<=22;++n){game_test::tick+=96'000;f.sample(n);poll_sensors();AimSensitivity::Callback(f.actor,0);}
        assert(f.pitch()[1]>0);
        const float ready_pitch=f.pitch()[1];
        f.actor[0x116]=2;f.actor[0x117]=1;
        for(unsigned n=23;n<=25;++n){game_test::tick+=96'000;f.sample(n);poll_sensors();AimSensitivity::Callback(f.actor,0);}
        assert(f.pitch()[1]>ready_pitch); // Fire must not restart the entry gate.
        f.flags()=0;game_test::tick+=96'000;f.sample(26);poll_sensors();
        f.flags()=0x10;f.actor[0x116]=1;f.actor[0x117]=4;
        const float before=f.pitch()[1];
        for(unsigned n=27;n<=31;++n){game_test::tick+=96'000;f.sample(n);poll_sensors();AimSensitivity::Callback(f.actor,0);}
        assert(f.pitch()[1]==before); // A new raise cannot reuse the previous latch.
    }
    for(const auto handler : {0x31CC60u,0x31B9F0u,0x3212E0u,0x3270F0u,0x332CC0u,0x333DF0u,0x3351A0u,0x342F90u}){
        Fixture f;f.aim();settings.gyro_enabled=true;game_test::aim_step=0;
        *reinterpret_cast<std::uintptr_t*>(f.image.data()+0x1265A28)=main_base+handler;
        f.actor[0x117]=4;assert(!gyro_aim_ready(f.actor,true));
        f.actor[0x117]=0;assert(!gyro_aim_ready(f.actor,true));
        f.actor[0x117]=1;assert(gyro_aim_ready(f.actor,true));
        for(unsigned sub : {2,3,4,5}){f.actor[0x117]=std::uint8_t(sub);assert(gyro_aim_ready(f.actor));}
        f.actor[0x116]=2;assert(gyro_aim_ready(f.actor));
        f.actor[0x116]=4;gyro_pending={0.1f,0.1f};AimSensitivity::Callback(f.actor,0);
        assert(f.yaw()==0 && f.pitch()[1]==0); // Reload began after sample collection.
        f.actor[0x116]=1;f.actor[0x117]=4;assert(!gyro_aim_ready(f.actor,true));
        f.actor[0x117]=1;assert(gyro_aim_ready(f.actor,true));
        f.input_state[0x5034]++;gyro_pending={0.1f,0.1f};AimSensitivity::Callback(f.actor,0);
        assert(f.yaw()==0 && f.pitch()[1]==0); // Late weapon switch cannot consume the old delta.
        assert(gyro_aim_ready(f.actor,true));
        f.input_state[0x86FF]=1;assert(!gyro_aim_ready(f.actor));
        f.input_state[0x86FF]=0;f.actor[0x117]=4;assert(!gyro_aim_ready(f.actor,true));
        f.actor[0x117]=1;assert(gyro_aim_ready(f.actor,true));
        settings.gyro_enabled=false;assert(!gyro_aim_ready(f.actor));
        settings.gyro_enabled=true;f.actor[0x117]=4;assert(!gyro_aim_ready(f.actor,true));
    }
    for(unsigned style=0;style<3;++style)for(unsigned size=0;size<3;++size){
        Fixture f;f.view_mode();f.aim();f.camera()[0x26C]=0;
        settings.reticle=static_cast<re4::ReticleStyle>(style);settings.reticle_size=static_cast<re4::ReticleSize>(size);publish_view_settings();
        CameraInput::Callback(f.camera());WeaponLaser::Callback(f.weapon,1,0,0);SceneSubmit::Callback();
        view_frame.fetch_add(1);SceneDraw::Callback();CameraInput::Callback(f.camera());HudDraw::Callback(nullptr);
        const auto preview=re4::reticle_marks(settings.reticle,settings.reticle_size);
        assert(hud_rects.size()==preview.count);
        for(unsigned i=0;i<preview.count;++i)assert(!std::memcmp(hud_rects[i].data()+0xC,&preview.rects[i],sizeof(re4::ViewReticleRect)));
    }
    for(bool knife:{false,true}){
        Fixture f;f.view_mode();f.aim();f.actor[0x117]=1;
        if(knife){f.actor[0x115]=11;f.flags()=0x800ull;}
        *reinterpret_cast<std::uint16_t*>(f.actor+0x352)=0x300;
        *reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        *reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;
        *reinterpret_cast<std::uintptr_t*>(f.image.data()+0x1265A28)=main_base+0x31CC60;
        sensor_attributes=1;
        game_test::memory_maps={{reinterpret_cast<std::uintptr_t>(f.actor),sizeof(f.actor),3},
            {reinterpret_cast<std::uintptr_t>(f.input_state.data()),f.input_state.size(),3}};
        settings.analog_enabled=settings.gyro_enabled=false;
        settings.move_while_aiming=!knife;settings.move_while_knife=knife;
        control.raw_lx=1;control.raw_ly=-1;control.raw_rx=0.5f;control.raw_ry=0.25f;
        auto expected=re4::shape_aim_input(1,-1,0.5f,0.25f,settings,{true,63});
        modern_probe=true;ModernPlayerUpdate::Callback(f.actor);
        assert(seen_aim_x==expected.x && seen_aim_y==expected.y && control.aim_source==1);
        assert(modern_original_updates==1 && modern_original_roots==1 && modern_original_poses==1 && modern_update_actor==nullptr);
        assert(control.modern_move_steps==0 && control.modern_pose_steps==0); // Missing assets keep original frame safe.
        control.raw_lx=0;control.raw_ly=0;control.raw_rx=-0.25f;
        expected=re4::shape_aim_input(0,0,-0.25f,0.25f,settings,{true,63});
        ModernPlayerUpdate::Callback(f.actor);
        assert(seen_aim_x==expected.x && seen_aim_y==expected.y); // Neutral left still owns right aiming.
        settings.move_while_aiming=settings.move_while_knife=false;
        auto* axes=reinterpret_cast<std::int32_t*>(f.image.data()+0x124E9C8);axes[0]=51;axes[1]=-31;
        ModernPlayerUpdate::Callback(f.actor);
        assert(seen_aim_x==51 && seen_aim_y==-31 && axes[0]==51 && axes[1]==-31);
        assert(control.modern_move_status==0 && modern_original_updates==3 && modern_original_roots==3 && modern_original_poses==3);
    }
    {
        Fixture f;rom_result=7;assert(RomMount::Callback("rom",nullptr,0)==7 && game_test::mounts==0);
        std::uint8_t descriptor[20]{};descriptor[5]=1;descriptor[6]=0x29;
        settings.texture_filtering=re4::TextureFilter::Aniso16;publish_graphics_settings();
        SamplerCreate::Callback(nullptr,nullptr,descriptor);
        assert(filter_latched.load()==255 && sampler_seen[5]==1 && sampler_seen[6]==0x29);
        file_read=true;publish_graphics_settings();SamplerCreate::Callback(nullptr,nullptr,descriptor);
        assert(filter_latched.load()==unsigned(re4::TextureFilter::Aniso16) && sampler_seen[5]==16);
        int display,back;ResolutionMap::Callback(1280,720,&display,&back);assert(display==2 && back==3);
        settings.anti_aliasing=re4::AntiAliasing::HandheldSsaa;publish_graphics_settings();
        ResolutionMap::Callback(1280,720,&display,&back);assert(back==3); // Native AA latch also requires restart.
    }
    {
        Fixture f;re4::Settings saved;saved.texture_filtering=re4::TextureFilter::Aniso8;
        game_test::settings_primary.resize(re4::kSettingsBytes);
        assert(re4::encode_settings(saved,game_test::settings_primary.data(),re4::kSettingsBytes));
        game_test::mount_result=0;rom_result=0;
        assert(RomMount::Callback("rom",nullptr,0)==0 && file_read);
        std::uint8_t descriptor[20]{};descriptor[5]=1;descriptor[6]=0x29;
        SamplerCreate::Callback(nullptr,nullptr,descriptor);
        assert(sampler_seen[5]==8 && sampler_seen[6]==0x6A && descriptor[5]==1 && descriptor[6]==0x29);
        settings.texture_filtering=re4::TextureFilter::Aniso16;publish_graphics_settings();
        SamplerCreate::Callback(nullptr,nullptr,descriptor);assert(sampler_seen[5]==8); // Process-latched, no cache variants.
        descriptor[6]=0xAA;SamplerCreate::Callback(nullptr,nullptr,descriptor);
        assert(sampler_seen[6]==0xAA && sampler_seen[5]==1); // Comparison sampler untouched.
    }
    {
        Fixture f;file_read=true;settings.anti_aliasing=re4::AntiAliasing::HandheldSsaa;publish_graphics_settings();
        int display=-1,back=-1;ResolutionMap::Callback(1280,720,&display,&back);assert(display==2 && back==1);
        game_test::operation_mode=1;ResolutionMap::Callback(1280,720,&display,&back);assert(display==2 && back==3);
        game_test::operation_mode=0;
        settings.anti_aliasing=re4::AntiAliasing::Native;publish_graphics_settings();
        ResolutionMap::Callback(1280,720,&display,&back);assert(display==2 && back==1); // Restart required.
        ResolutionMap::Callback(1920,1080,&display,&back);assert(display==0 && back==1);
    }
    {
        Fixture f;f.camera_mode();file_read=true;settings.shadow_mode=re4::ShadowMode::Off;
        settings.draw_distance_scale=1.5f;publish_graphics_settings();
        auto* flags=reinterpret_cast<std::uint32_t*>(f.input_state.data()+0x7C);*flags=0x80;
        SceneSubmit::Callback();assert((shadow_flags_seen&re4::kShadowDisableMask)==re4::kShadowDisableMask && *flags==0x480);
        SceneDraw::Callback();assert(*flags==0x480);
        auto* frustum=f.image.data()+0x1272FA0;
        *reinterpret_cast<float*>(frustum+0x18)=1000;
        *reinterpret_cast<float*>(f.image.data()+0x124ED40)=100;
        *reinterpret_cast<float*>(f.image.data()+0x124ED44)=1'000'000;
        FrustumUpdate::Callback(frustum);
        assert(*reinterpret_cast<float*>(frustum+0x18)==1000 && *reinterpret_cast<float*>(frustum+0x24)==1500);
        FrustumUpdate::Callback(frustum);assert(*reinterpret_cast<float*>(frustum+0x24)==1500); // No compounded scaling.
        settings.draw_distance_scale=1;publish_graphics_settings();FrustumUpdate::Callback(frustum);
        assert(*reinterpret_cast<float*>(frustum+0x24)==1000);
        settings.draw_distance_scale=2;publish_graphics_settings();
        *reinterpret_cast<float*>(frustum+0x18)=1;FrustumUpdate::Callback(frustum);
        assert(*reinterpret_cast<float*>(frustum+0x18)==1 && *reinterpret_cast<float*>(frustum+0x24)==1);
        alignas(16) std::uint8_t other[0x40]{};*reinterpret_cast<float*>(other+0x18)=1000;
        FrustumUpdate::Callback(other);assert(*reinterpret_cast<float*>(other+0x24)==1000);
        settings.shadow_mode=re4::ShadowMode::Native;publish_graphics_settings();*flags=0x02000000;
        SceneDraw::Callback();assert(shadow_flags_seen==0x02000000 && *flags==0x02000400);
    }
    {
        Fixture f;f.view_mode();settings.third_person_distance=1.35f;
        // A framing setting alone must create the verified view context, even
        // with analog controls, first person and the reticle disabled.
        settings.analog_enabled=false;settings.free_camera=false;
        CameraInput::Callback(f.camera());
        assert(camera_native_calls==1&&basis_native_calls==1);
        assert(near(camera_collision_input.eye0.x,-612)&&near(camera_collision_input.eye0.y,1787.75f)&&near(camera_collision_input.eye0.z,-1568));
        assert(near(camera_collision_input.target.x,-112)&&near(camera_collision_input.target.y,1362.75f)&&near(camera_collision_input.target.z,1102));
        assert(std::memcmp(&camera_collision_input.eye1,&native_profile.eye1,sizeof(re4::ViewVec3))==0);
        assert(camera_collision_input.fov==native_profile.fov&&camera_collision_input.roll==native_profile.roll);
        assert(first_person_frame.load()==0&&fps_override_count.load()==0);
        submit_models=true;SceneSubmit::Callback();
        assert(submitted_models.size()==3&&submitted_models[0]==f.actor); // Framing never hides the player.
        settings.third_person_distance=2;CameraInput::Callback(f.camera());
        assert(near(camera_collision_input.eye0.x,-820)&&near(camera_collision_input.target.z,400));
        settings.third_person_distance=1;CameraInput::Callback(f.camera());
        assert(std::memcmp(&camera_collision_input,&native_profile,sizeof(native_profile))==0);
        settings.third_person_distance=1.35f;settings.first_person=true;CameraInput::Callback(f.camera());
        assert(near(camera_collision_input.eye0.x,100)&&near(camera_collision_input.eye0.y,1900)&&near(camera_collision_input.eye0.z,300));
        assert(near(camera_collision_input.target.x-camera_collision_input.eye0.x,500));
    }
    for(unsigned routine:{6u,11u,15u})for(unsigned phase:{1u,2u}){
        Fixture f;f.view_mode();f.aim();settings.third_person_distance=1.35f;
        f.actor[0x115]=routine;f.actor[0x116]=phase;f.actor[0x117]=1;f.camera()[0x26C]=0;
        f.flags()=routine==11?0x800ull:0x10;
        CameraInput::Callback(f.camera());
        assert(near(camera_collision_input.eye0.z,-1568)&&near(camera_collision_input.target.z,1102));
        assert(std::memcmp(&camera_collision_input.eye1,&native_profile.eye1,sizeof(re4::ViewVec3))==0);
        assert(first_person_frame.load()==0);
    }
    for(unsigned gate=0;gate<17;++gate){
        Fixture f;f.view_mode();settings.third_person_distance=1.35f;
        if(gate==0)f.camera()[0x222]=1;
        if(gate==1)f.image[0x1131718+0x54]=16;
        if(gate==2)f.image[0x1131718+0x54]=17;
        if(gate==3)f.input_state[0x5092]=4;
        if(gate==4)f.input_state[0x509E]=0x80;
        if(gate==5)f.input_state[0x1AB]=0x40;
        if(gate==6)f.actor[0x114]=1;
        if(gate==7)f.actor[0x115]=5;
        if(gate==8)f.image[0x1131718+0x48]=0;
        if(gate==9)native_basis.m[2][2]=0;
        if(gate==10)game_test::camera_gate_change=2;
        if(gate==11)game_test::camera_gate_change=1;
        if(gate==12)native_basis.m[0][0]=std::numeric_limits<float>::quiet_NaN();
        if(gate==13)settings.third_person_distance=std::numeric_limits<float>::quiet_NaN();
        if(gate==14)settings.third_person_distance=2.01f;
        if(gate==15)settings.third_person_distance=0.99f;
        if(gate==16)aiming_predicate=nullptr;
        CameraInput::Callback(f.camera());
        assert(std::memcmp(&camera_collision_input,&native_profile,sizeof(native_profile))==0);
        assert(first_person_frame.load()==0&&camera_context==nullptr);
    }
    {
        Fixture f;f.view_mode();settings.third_person_distance=2;
        native_basis={{{0,0,2,30},{0,3,0,20},{-4,0,0,10}}};
        CameraInput::Callback(f.camera());
        // Adjustment is in native profile space before this exact frame basis
        // and collision consume it; no actor-yaw approximation is involved.
        assert(near(camera_collision_input.eye0.x,-820)&&near(camera_collision_input.eye0.z,-2270));
        assert(std::memcmp(&camera_collision_input.eye1,&native_profile.eye1,sizeof(re4::ViewVec3))==0);
    }
    for(unsigned invalid=0;invalid<8;++invalid){
        Fixture f;f.view_mode();settings.third_person_distance=1.35f;
        const float nan=std::numeric_limits<float>::quiet_NaN();
        if(invalid==0)native_profile.eye0.x=nan;
        if(invalid==1)native_profile.eye1.y=nan;
        if(invalid==2)native_profile.target.z=nan;
        if(invalid==3)native_profile.roll=nan;
        if(invalid==4)native_profile.fov=nan;
        if(invalid==5)native_profile.eye0=native_profile.eye1;
        if(invalid==6)native_profile.target=native_profile.eye0;
        if(invalid==7){native_profile.eye0.x=std::numeric_limits<float>::max();native_profile.eye1.x=-native_profile.eye0.x;}
        CameraInput::Callback(f.camera());
        assert(std::memcmp(&camera_collision_input,&native_profile,sizeof(native_profile))==0);
    }
    for(unsigned gate=0;gate<5;++gate){
        Fixture f;f.view_mode();settings.third_person_distance=1.35f;
        CameraContext context{};context.camera=f.camera();context.actor=f.actor;
        context.view_allowed=context.basis_valid=true;context.basis=native_basis;
        auto* camera=f.camera();
        if(gate==0)context.actor=f.other_model; // A captured actor must still be the player.
        if(gate==1)camera=f.camera()+0x400;
        if(gate==2)context.basis_valid=false;
        if(gate==3)context.view_allowed=false;
        camera_context=gate==4?nullptr:&context;
        re4::ViewQfpsOutput result{};CameraApply::Callback(camera,&result);camera_context=nullptr;
        assert(std::memcmp(&result,&native_profile,sizeof(native_profile))==0);
    }
    {
        Fixture f;f.view_mode();settings.third_person_distance=1;
        // Native bypass must retain payload bits, including signed zero/NaN.
        native_profile.eye0.x=-0.f;
        const std::uint32_t nan_bits=0x7FC12345;
        std::memcpy(&native_profile.target.z,&nan_bits,sizeof(nan_bits));
        CameraInput::Callback(f.camera());
        assert(std::memcmp(&camera_collision_input,&native_profile,sizeof(native_profile))==0);
    }
    {
        Fixture f;f.view_mode();CameraInput::Callback(f.camera());
        assert(camera_native_calls==1&&basis_native_calls==1);
        assert(std::memcmp(&camera_collision_input,&native_profile,sizeof(native_profile))==0);
        assert(first_person_frame.load()==0); // Disabled means exact original output.
        settings.first_person=true;settings.analog_enabled=false;settings.free_camera=false;
        CameraInput::Callback(f.camera());
        assert(near(camera_collision_input.eye0.x,100)&&near(camera_collision_input.eye0.y,1900)&&near(camera_collision_input.eye0.z,300));
        assert(std::memcmp(&camera_collision_input.eye0,&camera_collision_input.eye1,sizeof(re4::ViewVec3))==0);
        assert(near(camera_collision_input.target.x-camera_collision_input.eye0.x,500));
        assert(near(camera_collision_input.target.y-camera_collision_input.eye0.y,-425));
        assert(camera_collision_input.fov==native_profile.fov&&camera_collision_input.roll==native_profile.roll);
        assert(first_person_frame.load()==view_frame.load());
        publish_graphics_diagnostics();assert(control.fps_overrides==1);
        control.raw_rx=1;game_test::tick+=320'000;CameraInput::Callback(f.camera());
        for(unsigned n=0;n<30;++n){game_test::tick+=320'000;CameraInput::Callback(f.camera());}
        assert(f.look()[1]<-1); // FPS enables continuous orbit independently of analog/free toggles.
        f.look()[0]=f.look()[1]=0;control.raw_rx=0; // Native14380 consumed yaw before entering weapon mode.
        game_test::tick+=320'000;CameraInput::Callback(f.camera());assert(f.look()[0]==0&&f.look()[1]==0);
        // FPS cannot restore its cached pre-aim orbit after the native handoff.
        submit_models=true;SceneSubmit::Callback();
        assert(submitted_models.size()==2&&submitted_models[0]==f.weapon&&submitted_models[1]==f.other_model);
        ModelSubmit::Callback(f.actor);assert(submitted_models.back()==f.actor); // Independent shadow/outside submit scope remains native.
        submitted_models.clear();settings.first_person=false;publish_view_settings();SceneSubmit::Callback();
        assert(submitted_models.size()==3&&submitted_models[0]==f.actor);
    }
    for(unsigned gate=0;gate<10;++gate){
        Fixture f;f.view_mode();settings.first_person=true;
        if(gate==0)f.camera()[0x222]=1;
        if(gate==1)f.image[0x1131718+0x54]=16;
        if(gate==2)f.input_state[0x5092]=4;
        if(gate==3)f.input_state[0x509E]=0x80;
        if(gate==4)f.actor[0x114]=1;
        if(gate==5)f.actor[0x115]=5;
        if(gate==6)f.image[0x1131718+0x48]=0;
        if(gate==7)native_basis.m[2][2]=0;
        if(gate==8)game_test::camera_gate_change=2;
        if(gate==9)std::memset(f.actor+0x18,0,sizeof(re4::ViewMatrix3x4));
        CameraInput::Callback(f.camera());
        assert(std::memcmp(&camera_collision_input,&native_profile,sizeof(native_profile))==0);
        assert(first_person_frame.load()==0&&camera_context==nullptr);
    }
    {
        Fixture f;f.view_mode();settings.first_person=true;settings.reticle=re4::ReticleStyle::Dot;
        f.aim();f.camera()[0x26C]=0;
        auto* axes=reinterpret_cast<std::int8_t*>(f.image.data()+0x124EC38+8);axes[0]=7;axes[1]=-9;
        control.raw_rx=1;control.raw_ry=-1;gyro_pending={0.2f,0.3f};
        CameraInput::Callback(f.camera());
        assert(game_test::camera_x==7&&game_test::camera_y==-9&&axes[0]==7&&axes[1]==-9);
        assert(near(gyro_pending.yaw,0.2f)&&near(gyro_pending.pitch,0.3f));
        assert(first_person_frame.load()==view_frame.load());
        WeaponLaser::Callback(f.weapon,1,0,0);HudDraw::Callback(nullptr);
        assert(laser_native_calls==1&&laser_draw_seen==1&&laser_reuse_seen==0);
        assert(hud_native_calls==1&&ortho_native_calls==0&&hud_rects.empty()); // Current simulation has not been displayed yet.
        SceneSubmit::Callback();
        view_frame.fetch_add(1);SceneDraw::Callback(); // Display previous submitted scene.
        CameraInput::Callback(f.camera());
        // Current simulation camera differs: late HUD must use the displayed camera captured above.
        *reinterpret_cast<float*>(f.input_state.data()+0xD8+0xC)=9000;
        HudDraw::Callback(nullptr);assert(ortho_native_calls==1&&!hud_rects.empty());
        assert(gx_vertices>0&&gx_last_alpha==128&&gx_blend_last==1&&gx_format_last==3);
        publish_graphics_diagnostics();assert(control.reticle_draws==1);
        const auto size=hud_rects.size();
        f.input_state[0x5034]=1;HudDraw::Callback(nullptr);assert(hud_rects.size()==size); // Same model, changed weapon ID.
        f.input_state[0x5034]=0;
        view_frame.fetch_add(1);HudDraw::Callback(nullptr);assert(hud_rects.size()==size); // Stale display stamp.
        CameraInput::Callback(f.camera());WeaponLaser::Callback(f.weapon,1,1,0);SceneSubmit::Callback();
        view_frame.fetch_add(1);SceneDraw::Callback();CameraInput::Callback(f.camera());HudDraw::Callback(nullptr);
        assert(hud_rects.size()==size); // Native reuse does not freshen old endpoint.
        WeaponLaser::Callback(f.other_model,1,0,0);SceneSubmit::Callback();
        view_frame.fetch_add(1);SceneDraw::Callback();CameraInput::Callback(f.camera());HudDraw::Callback(nullptr);
        assert(hud_rects.size()==size);
        *reinterpret_cast<float*>(f.input_state.data()+0xD8+0xC)=0;
        settings.reticle=re4::ReticleStyle::Crosshair;settings.reticle_size=re4::ReticleSize::Large;publish_view_settings();
        WeaponLaser::Callback(f.weapon,1,0,0);SceneSubmit::Callback();
        view_frame.fetch_add(1);SceneDraw::Callback();CameraInput::Callback(f.camera());HudDraw::Callback(nullptr);
        assert(hud_rects.size()==size+4);
        const auto cross_size=hud_rects.size();f.image[0x1131718+0x54]=17;HudDraw::Callback(nullptr);
        assert(hud_rects.size()==cross_size);
        f.image[0x1131718+0x54]=10;f.input_state[0x7D]=0x20;HudDraw::Callback(nullptr);
        assert(hud_rects.size()==cross_size); // Native HUDdisable bit13 respected.
        f.input_state[0x7D]=0;game_test::tick+=2'000'000;HudDraw::Callback(nullptr);assert(hud_rects.size()==cross_size);
    }
    {
        Fixture f;f.view_mode();f.aim();settings.first_person=true;
        for(unsigned routine:{6u,11u,15u})for(unsigned phase:{1u,2u}){
            f.actor[0x115]=routine;f.actor[0x116]=phase;f.actor[0x117]=1;f.camera()[0x26C]=0;
            f.flags()=routine==11?(0x800ull):0x10;
            CameraInput::Callback(f.camera());assert(first_person_frame.load()==view_frame.load());
        }
        f.actor[0x115]=11;f.actor[0x116]=1;f.flags()=0x800ull;settings.gyro_enabled=true;
        settings.left_deadzone=0;control.raw_lx=0.5f;control.raw_ly=0.5f;
        gyro_pending={0.3f,0.4f};f.yaw()=0;
        AimSensitivity::Callback(f.actor,0);
        assert(near(f.yaw(),0.04f*32/63)&&near(f.pitch()[0],0.08f*32/63)); // Knife gets shaped stick, zero gyro injection.
        assert(!firearm_aim(f.actor));
        NpadState pad{};pad.attributes=1;capture(pad,0,1);f.sample(1);poll_sensors();
        game_test::tick+=320'000;f.sample(2);poll_sensors();
        assert(gyro_pending.yaw==0&&gyro_pending.pitch==0&&control.gyro_active==0);
    }
    {
        Fixture f;f.view_mode();f.aim();settings.reticle=re4::ReticleStyle::Dot;f.camera()[0x26C]=0;
        CameraInput::Callback(f.camera());
        const re4::ViewVec3 stale{10000,0,-1000};std::memcpy(f.weapon+0x40C,&stale,sizeof(stale));
        short_laser_ray=true;laser_endpoint={0,0,-200}; // Native skips model+40C copy below500units.
        WeaponLaser::Callback(f.weapon,1,0,0);SceneSubmit::Callback();
        view_frame.fetch_add(1);SceneDraw::Callback();CameraInput::Callback(f.camera());HudDraw::Callback(nullptr);
        assert(hud_rects.size()==1); // Fresh global clipped endpoint wins over stale model cache.
    }
    {
        Fixture f;f.view_mode();settings.analog_enabled=false;settings.free_camera=true;
        settings.camera_return_speed=0;settings.reticle=re4::ReticleStyle::Dot;
        f.look()[0]=0.5f;f.look()[1]=0.7f;CameraInput::Callback(f.camera());
        assert(near(f.look()[0],0.45f)&&near(f.look()[1],0.63f)); // Reticle alone cannot enable camera controls.
        assert(std::memcmp(&camera_collision_input,&native_profile,sizeof(native_profile))==0);
        assert(fps_override_count.load()==0);
    }
    {
        Fixture f;f.view_mode();settings.first_person=true;
        native_basis={{{0,0,2,30},{0,3,0,20},{-4,0,0,10}}};
        CameraInput::Callback(f.camera());
        assert(basis_native_calls==1&&near(camera_collision_input.eye0.x,-72.5f));
        assert(near(camera_collision_input.eye0.y,1880.f/3)&&near(camera_collision_input.eye0.z,35));
        assert(camera_collision_input.fov==50&&camera_collision_input.roll==0.2f);
    }
    {
        Fixture f;f.view_mode();settings.first_person=true;f.aim();
        f.actor[0x115]=11;f.actor[0x116]=2;f.flags()=0x800ull;f.camera()[0x26C]=0;
        for(unsigned recovery=0;recovery<5;++recovery){f.actor[0x117]=recovery;CameraInput::Callback(f.camera());assert(first_person_frame.load()==view_frame.load());}
        f.actor[0x115]=6;f.actor[0x116]=4;f.flags()=0;game_test::native_aim=0;f.camera()[0x26C]=2;
        CameraInput::Callback(f.camera());assert(first_person_frame.load()==view_frame.load()); // Native generic reload remains FPS.
    }
    for(unsigned style:{1u,2u,4u}){
        Fixture f;f.actor[0x114]=9;f.actor[0x115]=99; // Menus/QTE remain remapped too.
        settings.button_map[0]=1;settings.button_map[1]=0;settings.button_map[2]=re4::kUnboundButton;
        settings.button_map[4]=8;game_test::pad_buttons=1ull|4ull|16ull|(1ull<<40);
        NpadState pad{};const std::uint32_t id=style==2?32:0;
        if(style==1)FullPad::Callback(&pad,id);
        else if(style==2)HandheldPad::Callback(&pad,id);
        else DualPad::Callback(&pad,id);
        assert(pad.buttons==(2ull|256ull|(1ull<<40)) && pad.attributes==game_test::pad_attributes);
        settings.remap_enabled=false;FullPad::Callback(&pad,id);assert(pad.buttons==game_test::pad_buttons);
    }
    {
        Fixture f;f.camera_mode();f.look()[0]=0.8f;f.look()[1]=-1;
        settings.camera_return_speed=0.5f;CameraInput::Callback(f.camera());
        assert(near(game_test::rendered_pitch,0.8f*std::sqrt(0.9f)));
        assert(near(game_test::rendered_camera_yaw,-std::sqrt(0.9f)));
        assert(game_test::rendered_camera_yaw==f.look()[1]); // Correction precedes native consumers.
        settings.camera_return_speed=0;const auto held=f.look()[1];CameraInput::Callback(f.camera());
        assert(f.look()[1]==held);
        settings.camera_return_speed=1;CameraInput::Callback(f.camera());assert(near(f.look()[1],held*0.9f));
        settings.camera_return_speed=0;control.raw_rx=1;CameraInput::Callback(f.camera());
        assert(near(f.look()[1],-63.f/59*1.04719758f) && f.look()[0]==0); // Joint neutral, not per-axis decay.
    }
    {
        Fixture f;f.camera_mode();settings.free_camera=true;settings.right_deadzone=0;control.raw_rx=1;
        CameraInput::Callback(f.camera());
        for(unsigned n=0;n<60;++n){game_test::tick+=320'000;CameraInput::Callback(f.camera());}
        assert(near(game_test::rendered_camera_yaw,-2.0943951f) && f.yaw()==0);
        control.raw_rx=0;const auto held=f.look()[1];game_test::tick+=320'000;CameraInput::Callback(f.camera());
        assert(f.look()[1]==held); // Free orbit remains held after release.
        *reinterpret_cast<int*>(f.input_type)=-1;control.raw_ry=1;
        game_test::tick+=320'000;CameraInput::Callback(f.camera());assert(f.look()[0]<0);
        f.look()[0]=f.look()[1]=0;control.raw_ry=0; // Native aim entry consumes and clears camera state.
        game_test::tick+=320'000;CameraInput::Callback(f.camera());assert(f.look()[1]==0);
        control.raw_rx=1;game_test::tick+=320'000;CameraInput::Callback(f.camera());assert(f.look()[1]<0);
        f.input_state[0x5092]=4;const auto before=f.look()[1];game_test::tick+=320'000;
        CameraInput::Callback(f.camera());assert(near(f.look()[1],-63.f/59*1.04719758f) && f.look()[1]!=before);
        f.input_state[0x5092]=0;control.raw_rx=0;game_test::tick+=320'000;CameraInput::Callback(f.camera());
        const auto resumed=f.look()[1];game_test::tick+=320'000;CameraInput::Callback(f.camera());
        assert(f.look()[1]==resumed); // Pause resets continuity instead of replaying old orbit.
    }
    for(unsigned gate=0;gate<11;++gate){
        Fixture f;f.camera_mode();settings.free_camera=true;settings.camera_return_speed=0;
        f.look()[0]=0.5f;f.look()[1]=0.7f;
        if(gate==0)f.camera()[0x222]=1;
        if(gate==1)f.camera()[0x26C]=0;
        if(gate==2)f.image[0x1131718+0x54]=9;
        if(gate==3)f.actor[0x114]=1;
        if(gate==4)f.actor[0x115]=6;
        if(gate==5)game_test::native_aim=1;
        if(gate==6)settings.analog_enabled=false;
        if(gate==7)*reinterpret_cast<void**>(f.image.data()+0xB1AE08)=nullptr;
        if(gate==8)f.input_state[0x5092]=4;
        if(gate==9)game_test::camera_gate_change=1;
        if(gate==10)game_test::camera_gate_change=2;
        CameraInput::Callback(f.camera());
        assert(near(game_test::rendered_pitch,0.45f) && near(game_test::rendered_camera_yaw,0.63f));
        assert(camera_context==nullptr);
    }
    {
        Fixture f;f.camera_mode();settings.free_camera=true;settings.right_deadzone=0;
        settings.camera_gain=0.5f;settings.stick_curve=re4::StickCurve::Exponential;
        *reinterpret_cast<int*>(f.image.data()+0x11C7D68)=1;control.raw_rx=0.5f;
        CameraInput::Callback(f.camera());
        for(unsigned n=0;n<60;++n){game_test::tick+=320'000;CameraInput::Callback(f.camera());}
        assert(near(f.look()[1],-2.0943951f*0.125f)); // Curve and sensitivity applied once in the alternate input route.
        assert(f.image[0x124EC38+0xC0]==0 && f.image[0x124EC38+0xC1]==0);
    }
    {
        Fixture f;re4::Settings legacy;legacy.gyro_enabled=true;legacy.gyro_speed=1.7f;
        std::uint8_t current[re4::kSettingsBytes];assert(re4::encode_settings(legacy,current,sizeof(current)));
        game_test::settings_primary.assign(current,current+re4::kLegacySettingsBytes);
        auto& old=game_test::settings_primary;old[4]=6;
        std::uint32_t sum=2166136261u;for(unsigned i=0;i<60;++i){sum^=old[i];sum*=16777619u;}
        for(unsigned i=0;i<4;++i)old[60+i]=std::uint8_t(sum>>(i*8));
        game_test::mount_result=0;PadNormalize::Callback(nullptr);
        assert(settings.gyro_enabled && settings.gyro_speed==1.7f);
        assert(settings.button_map==re4::identity_button_map() && !settings.free_camera && settings.camera_return_speed==1);
    }
    for(bool enabled:{false,true}){
        Fixture f;re4::Settings saved;saved.gyro_enabled=enabled;saved.gyro_speed=2.5f;
        saved.gyro_deadzone_dps=0.7f;saved.turn_gain=0.37f;
        game_test::settings_primary.resize(re4::kSettingsBytes);
        assert(re4::encode_settings(saved,game_test::settings_primary.data(),re4::kSettingsBytes));
        game_test::mount_result=0;game_test::mount_failures=1;
        f.aim();NpadState pad{};pad.attributes=1;capture(pad,0,1);
        assert(PadNormalize::Callback(nullptr)==7 && !settings.gyro_enabled);
        for(unsigned n=0;n<60;++n)PadNormalize::Callback(nullptr);
        assert(game_test::mounts==1 && game_test::fs_opens==0);
        game_test::tick+=19'200'000;PadNormalize::Callback(nullptr);
        assert(settings.gyro_enabled==enabled && near(settings.gyro_speed,2.5f));
        assert(near(settings.turn_gain,0.37f) && near(settings.gyro_deadzone_dps,0.7f));
        assert(sensor.running()==enabled && game_test::mounts==2 && game_test::fs_closes==1);
        const auto opens=game_test::fs_opens;
        game_test::tick+=19'200'000;PadNormalize::Callback(nullptr);
        assert(game_test::mounts==2 && game_test::fs_opens==opens);
    }
    {
        Fixture f;re4::Settings saved;saved.gyro_enabled=true;
        game_test::settings_primary.assign(re4::kSettingsBytes,0); // Corrupt primary, valid recovery.
        game_test::settings_backup.resize(re4::kSettingsBytes);
        assert(re4::encode_settings(saved,game_test::settings_backup.data(),re4::kSettingsBytes));
        game_test::mount_result=0;f.aim();NpadState pad{};pad.attributes=1;capture(pad,0,1);
        PadNormalize::Callback(nullptr);
        assert(settings.gyro_enabled && sensor.running() && control.settings_source==2);
        assert(game_test::fs_closes==2 && control.settings_read_result==0);
        for(unsigned n=1;n<=5;++n){game_test::tick+=320'000;f.sample(n);PadNormalize::Callback(nullptr);}
        AimSensitivity::Callback(f.actor,0);
        assert(std::fabs(f.yaw())>0 && std::fabs(f.pitch()[0])>0); // No overlay or toggle required.
    }
    {
        Fixture f;
        for(unsigned n=0;n<20;++n){PadNormalize::Callback(nullptr);game_test::tick+=19'200'000;}
        assert(file_read && game_test::mounts==8 && control.settings_attempts==8);
        assert(control.settings_source==3 && !settings.gyro_enabled && !sensor.running());
        assert(game_test::fs_opens==0); // Exhausted mount attempts never poll SD every frame.
    }
    {
        Fixture f;game_test::mount_result=0;PadNormalize::Callback(nullptr);
        assert(game_test::mounts==1 && game_test::fs_opens==2 && !file_read);
        re4::Settings saved;saved.gyro_enabled=true;
        game_test::settings_primary.resize(re4::kSettingsBytes);
        assert(re4::encode_settings(saved,game_test::settings_primary.data(),re4::kSettingsBytes));
        game_test::tick+=19'200'000;PadNormalize::Callback(nullptr);
        assert(settings.gyro_enabled && control.settings_source==1 && file_read);
        assert(game_test::mounts==1 && game_test::fs_opens==3); // Retry reads without a duplicate mount.
    }
    {
        Fixture f;publish_settings();PadNormalize::Callback(nullptr); // Initial mount fails.
        re4::Settings live;live.gyro_enabled=true;live.gyro_speed=3.25f;
        std::uint8_t bytes[re4::kSettingsBytes];assert(re4::encode_settings(live,bytes,sizeof(bytes)));
        for(unsigned i=0;i<sizeof(bytes);++i)control.settings[i]=bytes[i];
        control.sequence=last_sequence+2;
        game_test::mount_result=0;game_test::tick+=19'200'000;PadNormalize::Callback(nullptr);
        assert(settings.gyro_enabled && settings.gyro_speed==3.25f && control.settings_source==4);
        assert(file_read && game_test::mounts==1 && game_test::fs_opens==0); // Live change wins over retry.
    }
    {
        Fixture f;game_test::program=0;exl_main(nullptr,nullptr);
        assert(control.status==2 && game_test::installed==0 && game_test::initialized==0);
        game_test::program=re4::profile::title_id;f.image[0x19B20]^=1;exl_main(nullptr,nullptr);
        assert(control.status==2 && game_test::installed==0);
        f.image[0x19B20]^=1;
        for(const std::size_t offset:{0xB280F0,0xB0AB70}){
            f.image[offset]^=1;exl_main(nullptr,nullptr);
            assert(control.status==2 && game_test::installed==0 && game_test::initialized==0);
            f.image[offset]^=1;
        }
        exl_main(nullptr,nullptr);
        assert(control.status==1 && game_test::installed==64 && game_test::initialized==1);
        assert(PadNormalize::Callback(nullptr)==7);PadNormalize::Callback(nullptr);
        assert(control.frames==2 && game_test::mounts==1); // No filesystem polling per frame.
    }
    {
        Fixture f;f.actor[0x115]=1;f.flags()=0x45;
        raw_left_x=0.575f;control.raw_ly=0.575f;settings.turn_gain=0;
        MovementTurn::Callback(f.actor);
        assert(near(f.yaw(),0) && game_test::directions_seen==5);
        assert(f.flags()==0x85); // Only temporarily remapped direction bits restored.
        assert(near(game_test::motion_speed_seen,1.25f*0.5f));
        assert(*reinterpret_cast<float*>(f.actor+0x304)==1.25f);
        dpad_turn=dpad_move=true;MovementTurn::Callback(f.actor);
        assert(near(f.yaw(),0.04f) && game_test::motion_speed_seen==1.25f);
        f.actor[0x115]=5;MovementTurn::Callback(f.actor);
        assert(f.yaw()>3.14f); // Quick-turn untouched.
    }
    {
        Fixture f;control.raw_rx=0.575f;control.raw_ry=-1;
        for(int mode=0;mode<=1;++mode){
            *reinterpret_cast<int*>(f.image.data()+0x11C7D68)=mode;
            auto* in=reinterpret_cast<std::int8_t*>(f.image.data()+0x124EC38+(mode?0xC0:8));in[0]=12;in[1]=-9;
            CameraInput::Callback(nullptr);
            assert(game_test::camera_x>=31 && game_test::camera_x<=32 && game_test::camera_y==-63);
            assert(in[0]==12 && in[1]==-9);
            f.actor[0x114]=1;CameraInput::Callback(nullptr);
            assert(game_test::camera_x==12 && game_test::camera_y==-9);f.actor[0x114]=0;
        }
    }
    {
        Fixture f;f.actor[0x115]=1;f.flags()=5;
        settings.stick_curve=re4::StickCurve::Exponential;raw_left_x=0.575f;control.raw_ly=0.575f;
        MovementTurn::Callback(f.actor);
        assert(near(f.yaw(),0.01f) && near(game_test::motion_speed_seen,1.25f*0.25f));
        control.raw_rx=0.575f;CameraInput::Callback(nullptr);assert(game_test::camera_x==16);
        f.aim();control.raw_lx=0.575f;control.raw_ly=0.575f;f.yaw()=0;
        AimSensitivity::Callback(f.actor,0);assert(near(f.yaw(),0.04f*16/63) && near(f.pitch()[0],0.08f*16/63));
    }
    {
        Fixture f;f.aim();consume_aim_axes=true;aim_full_scale=127;
        std::vector<std::uint8_t> global(0x8700);std::uint8_t type[16]{};type[11]=2;
        *reinterpret_cast<void**>(f.image.data()+0xB1AE08)=global.data();
        *reinterpret_cast<void**>(f.image.data()+0xB1AE10)=type;
        *reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;
        auto* axes=reinterpret_cast<std::int32_t*>(f.image.data()+0x124E9C8);axes[0]=31;axes[1]=-63;
        control.raw_lx=1;control.raw_ly=1;control.raw_rx=0.25f;control.raw_ry=-0.5f;
        settings.left_deadzone=settings.right_deadzone=0;
        settings.right_aim_gain_x=0.5f;settings.right_aim_gain_y=0.25f;
        settings.gyro_enabled=true;assert(gyro_aim_ready(f.actor,true));
        gyro_pending.yaw=0.01f;gyro_pending.pitch=0.02f;
        AimSensitivity::Callback(f.actor,0);
        assert(seen_aim_x==16 && seen_aim_y==-16);
        assert(near(f.yaw(),0.01f+0.04f*16/127) && near(f.pitch()[0],0.02f-0.08f*16/127));
        assert(near(cached_aim_yaw,f.yaw()) && near(cached_aim_pitch,f.pitch()[0]));
        assert(axes[0]==31 && axes[1]==-63); // Scoped engine axes restored.
        global[0x5099]=0x80;assert(!aim_route().right && aim_route().full_scale==63);
        global[0x5096]=0x80;*reinterpret_cast<float*>(global.data()+0xA4)=0.5f;
        assert(aim_route().right && aim_route().full_scale==31.5f);
        type[11]=1;assert(!aim_route().right);type[11]=2;
        *reinterpret_cast<void**>(f.image.data()+0xB1AE08)=nullptr;assert(!aim_route().right);
        *reinterpret_cast<void**>(f.image.data()+0xB1AE10)=nullptr;assert(!aim_route().right);
    }
    {
        Fixture f;f.aim();settings.aim_gain_x=settings.aim_gain_y=0;
        auto* dpad=reinterpret_cast<std::uint32_t*>(f.image.data()+0x124E9C8+0x14);*dpad=1;
        AimSensitivity::Callback(f.actor,0);assert(near(f.yaw(),0.04f) && *dpad==1);
        *dpad=0;settings.aim_gain_y=0.5f;settings.left_deadzone=0;control.raw_ly=1;
        native_pitch_sign=-1;settings.invert_y=true; // Gyro inversion cannot replace native stick inversion.
        AimSensitivity::Callback(f.actor,0);assert(near(f.pitch()[0],-0.08f*32/63));
    }
    {
        Fixture f;NpadState pad{};pad.attributes=1|16|32;
        capture(pad,0,4);assert(sensor_attributes==17);
        pad.attributes=1|4|8;capture(pad,0,4);assert(sensor_attributes==5);
    }
    {
        Fixture f;f.aim();settings.gyro_enabled=true;
        game_test::apply_sway=true;game_test::aim_step=0;
        assert(gyro_aim_ready(f.actor,true));
        for(unsigned i=0;i<40;++i){gyro_pending.pitch=0.01f;AimSensitivity::Callback(f.actor,0);}
        assert(near(f.pitch()[0],0.4f)); // Gyro must update native baseline, beyond its small sway window.
        assert(near(game_test::sway_baseline,0.4f));
    }
    {
        Fixture f;f.aim();settings.gyro_enabled=true;
        NpadState pad{};const std::uint32_t id=32;HandheldPad::Callback(&pad,id);
        game_test::start_result=5;poll_sensors();assert(!sensor.running());
        const auto attempts=game_test::sensor_starts;game_test::start_result=0;
        for(unsigned i=0;i<20;++i){game_test::tick+=480'000;poll_sensors();}
        assert(game_test::sensor_starts==attempts); // No retry storm in first500ms.
        game_test::tick+=19'200'000;f.sample(1);poll_sensors();
        assert(sensor.running() && game_test::sensor_starts==attempts+1 && gyro_pending.yaw==0);
    }
    {
        Fixture f;f.aim();settings.gyro_enabled=true;
        NpadState pad{};const std::uint32_t id=32;HandheldPad::Callback(&pad,id);
        f.sample(1);poll_sensors();assert(sensor.running() && gyro_pending.yaw==0);
        game_test::tick+=640'000;f.sample(2);
        const auto s=sensor_batch[0];sensor_batch={s,s,s};
        sensor_batch[0].number=4;sensor_batch[1].number=3;
        poll_sensors();assert(gyro_pending.yaw==0); // The first10ms endpoint is presented after10ms.
        game_test::tick+=96'000;f.sample(5);poll_sensors();
        assert(near(gyro_pending.yaw,(1-0.5f*0.01745329252f)*0.005f));
        const float filtered=gyro_pending.yaw;
        control.raw_lx=0.575f;control.raw_ly=0.575f;settings.aim_gain_x=settings.aim_gain_y=0.3f;
        assert(AimSensitivity::Callback(f.actor,0)==13);
        assert(near(f.yaw(),filtered+0.04f*9/63));
        assert(near(f.pitch()[0],filtered+0.08f*9/63)); // Additive gyro and stick in same frame.
        const float before=f.yaw();AimSensitivity::Callback(f.actor,0);
        assert(near(f.yaw()-before,0.04f*9/63)); // Sensor delta applied once.
        game_test::tick+=2'000'000;f.sample(6);poll_sensors();assert(gyro_pending.yaw==0); // Pause backlog.
        game_test::tick+=640'000;game_test::native_aim=0;f.sample(7);poll_sensors();assert(gyro_pending.yaw==0);
        game_test::native_aim=1;f.flags()=0;f.sample(8);poll_sensors();assert(gyro_pending.yaw==0);
        game_test::pad_attributes=0;HandheldPad::Callback(&pad,id);poll_sensors();
        assert(!sensor.running() && game_test::sensor_stops==1);
    }
    {
        // Real module, filter and native aiming callbacks must turn a uniform
        // sensor rate into uniform frame steps despite 6/7-sample FIFO batches.
        Fixture f;f.view_mode();f.aim();settings.unified_controls=true;settings.gyro_enabled=true;
        settings.analog_enabled=false;settings.gyro_speed=1;settings.gyro_deadzone_dps=0;
        game_test::apply_sway=true;game_test::aim_step=0;
        *reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;*reinterpret_cast<float*>(f.input_state.data()+0xA4)=1;
        NpadState pad{};pad.attributes=1;capture(pad,32,2);
        unsigned number=1;
        for(unsigned frame=0;frame<24;++frame){
            game_test::tick+=640000;
            const unsigned count=(frame+1)*200/30-frame*200/30;
            number+=count;
            f.sample(number-1);const auto sample=sensor_batch.front();sensor_batch.clear();
            for(unsigned i=0;i<count;++i){auto s=sample;s.number=number-1-i;sensor_batch.push_back(s);}
            if(frame==8||frame==9)sensor_batch.clear(); // Connected sensor dropout, not a game pause.
            const float before=f.yaw(),pitch_before=unified_pitch;
            poll_sensors();AimSensitivity::Callback(f.actor,0);
            if(frame>=3&&(frame<8||frame>=14)){
                assert(near(f.yaw()-before,1.f/30));assert(near(unified_pitch-pitch_before,1.f/30));
            }
        }
        // A tuning change to zero discards the delayed tail immediately.
        settings.gyro_speed=0;game_test::tick+=640000;poll_sensors();
        const float before=f.yaw(),pitch_before=unified_pitch;AimSensitivity::Callback(f.actor,0);
        assert(f.yaw()==before&&unified_pitch==pitch_before&&gyro_pending.yaw==0);
    }
    std::puts("actual game callbacks: identity, flags, motion, camera, additive aim/gyro, native raise, FIFO and pause PASS");
}
