#define main game_module_baseline_main
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#include "game_module_tests.cpp"
#pragma GCC diagnostic pop
#undef main
namespace {
struct HealthFixture {
 Fixture f;
 std::vector<std::uint8_t> pool=std::vector<std::uint8_t>(3*0x10C8);
 std::array<std::array<std::uint8_t,0x1F0>,2> bones{};
 std::uint8_t* enemy(){return pool.data()+0x10C8;}
 template<class T>void set(void* p,unsigned offset,T value){std::memcpy(static_cast<std::uint8_t*>(p)+offset,&value,sizeof(value));}
 HealthFixture(){
  f.view_mode();f.aim();settings.enemy_health_bars=true;health_color_probe=true;
  *reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;
  set(f.image.data()+0xAE6EF8,8,reinterpret_cast<std::uintptr_t>(pool.data()));
  set(f.image.data()+0xAE6EF8,0x10,3u);set(f.image.data()+0xAE6EF8,0x14,0x10C8u);
  set(enemy(),8,1u);enemy()[0x118]=0x10;enemy()[0x11A]=2;
  set(enemy(),0x3C2,std::int16_t(100));set(enemy(),0x3C4,std::int16_t(100));
  set(enemy(),0xA4,re4::ViewVec3{0,0,-1000});set(enemy(),0x108,reinterpret_cast<std::uintptr_t>(bones.data()));
  set(bones[0].data(),0x80,re4::ViewVec3{0,50,-1000});set(bones[1].data(),0x80,re4::ViewVec3{0,150,-1000});
  set(bones[0].data(),0x108,reinterpret_cast<std::uintptr_t>(bones[1].data()));
  set(f.weapon,0x418,reinterpret_cast<std::uintptr_t>(enemy()));
  game_test::memory_maps={{reinterpret_cast<std::uintptr_t>(f.image.data()),f.image.size(),1},
   {reinterpret_cast<std::uintptr_t>(f.weapon),sizeof(f.weapon),1},
   {reinterpret_cast<std::uintptr_t>(pool.data()),pool.size(),1},
   {reinterpret_cast<std::uintptr_t>(bones.data()),sizeof(bones),1}};
  publish_view_settings();
 }
 void capture(unsigned reuse=0){WeaponLaser::Callback(f.weapon,1,reuse,0);}
 void scene(){SceneSubmit::Callback();view_frame.fetch_add(1);SceneDraw::Callback();}
 unsigned hud(){
  const auto before=hud_rects.size();HudDraw::Callback(nullptr);return unsigned(hud_rects.size()-before);
 }
 unsigned advance(unsigned steps){
  unsigned drawn=0;for(unsigned i=0;i<steps;++i){game_test::tick+=576000;drawn=hud();}return drawn;
 }
 unsigned display(){scene();hud();return advance(10);}
};
}
int main(){
 {
  HealthFixture h;h.capture();h.scene();assert(h.hud()==0); // Fade begins transparent.
  assert(h.advance(5)==1&&gx_colors.back()[3]==51);
  assert(h.advance(5)==1&&gx_colors.back()[3]==102);
  h.set(h.f.weapon,0x418,std::uintptr_t(0));h.capture();h.scene();
  assert(h.hud()==1&&gx_colors.back()[3]==102);
  assert(h.advance(5)==1&&gx_colors.back()[3]==51);
  assert(h.advance(5)==0); // Copied target fades away in300ms.
 }
 {
  HealthFixture h;h.capture();assert(h.display()==1);
  const auto key=displayed_enemy_health.target_key.load();assert(key);
  h.set(h.f.weapon,0x418,std::uintptr_t(0));h.capture();h.scene();assert(h.hud()==1);
  assert(h.advance(5)==1&&gx_colors.back()[3]==51);
  h.set(h.f.weapon,0x418,reinterpret_cast<std::uintptr_t>(h.enemy()));
  h.capture();h.scene();assert(h.hud()==1&&gx_colors.back()[3]==51);
  assert(displayed_enemy_health.target_key.load()==key);
  assert(h.advance(10)==1&&gx_colors.back()[3]==102); // Same target resumes without flashing.
  h.set(h.enemy(),0x3C2,std::int16_t(25));h.capture();h.scene();
  assert(h.hud()==1&&gx_colors.back()[3]==102&&gx_colors.back()[0]>gx_colors.back()[1]);
 }
 {
  HealthFixture h;h.capture();assert(h.display()==1);
  auto* other=h.pool.data()+2*0x10C8;std::memcpy(other,h.enemy(),0x10C8);
  h.set(other,0xA4,re4::ViewVec3{200,0,-1000});
  const auto old_key=displayed_enemy_health.target_key.load();
  h.set(h.f.weapon,0x418,reinterpret_cast<std::uintptr_t>(other));h.capture();h.scene();
  assert(h.hud()==1&&displayed_enemy_health.target_key.load()!=old_key);
  assert(h.advance(5)==2&&gx_colors.back()[3]==51); // New target and old fading tail.
  assert(h.advance(5)==1&&gx_colors.back()[3]==102);
 }
 for(unsigned routine:{1u,6u,11u,15u}){
  HealthFixture h;h.capture();assert(h.display()==1);const float old_x=gx_quad[0].x;
  h.f.flags()=0;game_test::native_aim=0;h.f.actor[0x115]=std::uint8_t(routine);h.f.actor[0x116]=4;
  h.scene();assert(h.hud()==1); // Walk, lowered gun, reload, knife and mounted camera can fade.
  game_test::memory_maps.clear(); // HUD must use its copied world anchor.
  game_test::tick+=576000;
  const float a=.2f;const re4::ViewMatrix3x4 rotated{{{std::cos(a),0,std::sin(a),0},{0,1,0,0},{-std::sin(a),0,std::cos(a),0}}};
  h.set(h.f.input_state.data(),0xD8,rotated);h.scene();
  assert(h.hud()==1&&std::fabs(gx_quad[0].x-old_x)>10&&gx_colors.back()[3]<102);
  assert(enemy_health_camera.frame.load()==view_frame.load());
 }
 {
  HealthFixture h;h.set(h.enemy(),0x3C2,std::int16_t(150));h.set(h.enemy(),0x3C4,std::int16_t(300));
  h.capture();assert(h.display()==1); // Numbers defaultOff: only one colored strip.
  settings.enemy_health_numbers=true;publish_view_settings();assert(h.hud()>1);
  assert(gx_colors.back()[0]==235&&gx_colors.back()[1]==235&&gx_colors.back()[2]==235&&gx_colors.back()[3]==102);
  const auto english_quads=h.hud();settings.language=re4::Language::Russian;publish_view_settings();
  assert(h.hud()==english_quads); // Decimal point is identical in both languages.
  settings.enemy_health_numbers=false;publish_view_settings();assert(h.hud()==1);
 }
 for(unsigned gate=0;gate<9;++gate){
  HealthFixture h;h.capture();assert(h.display()==1);
  std::array<std::uint8_t,0xA00> replacement_actor{};
  if(gate==0)h.f.input_state[0x86FF]=1;
  if(gate==1)h.set(h.f.input_state.data(),0x7C,1u<<13);
  if(gate==2){settings.enemy_health_bars=false;publish_view_settings();}
  if(gate==3)enemy_health_camera.frame=0;
  if(gate==4)h.f.input_state[0x503C]++;
  if(gate==5){game_test::tick+=1'920'001;view_frame.fetch_add(1);SceneDraw::Callback();}
  if(gate==6)game_test::tick=1; // System clock rollback.
  if(gate==7){h.f.actor[0x114]=1;}
  if(gate==8){
   std::memcpy(replacement_actor.data(),h.f.actor,sizeof(h.f.actor));
   h.set(h.f.image.data(),re4::profile::player_slot,static_cast<void*>(replacement_actor.data()));
  }
  assert(h.hud()==0);
  for(const auto& entry:enemy_health_fade.entries())assert(!entry.active||entry.opacity==0);
 }
 {
  HealthFixture h;h.set(h.f.weapon,0x418,std::uintptr_t(0));
  native_target_probe=true;native_trace_target=h.enemy();h.capture();assert(h.display()==1);
  native_trace_target=nullptr;h.capture();assert(h.display()==0);
 }
 {
  HealthFixture h;const auto before=h.pool;h.capture();assert(h.display()==1);
  assert(h.pool==before&&gx_colors.back()[1]>gx_colors.back()[0]&&gx_colors.back()[3]<255);
  assert(camera_native_calls==0&&fps_override_count.load()==0); // Works with camera/reticle modes off.
 }
 for(int hp:{51,50,26,25,1,0}){
  HealthFixture h;h.set(h.enemy(),0x3C2,std::int16_t(hp));h.capture();
  assert(h.display()==(hp?1u:0u));
  if(hp){const auto color=gx_colors.back();if(hp>50)assert(color[1]>color[0]);else if(hp>25)assert(color[0]>color[2]&&color[1]>color[2]);else assert(color[0]>color[1]);}
 }
 for(unsigned gate=0;gate<24;++gate){
  HealthFixture h;
  if(gate==0)h.f.flags()=0;
  if(gate==1)h.set(h.f.weapon,0x418,std::uintptr_t(0)); // Native world hit/no enemy.
  if(gate==2)h.enemy()[0x118]=1;
  if(gate==3)h.set(h.enemy(),8,0x201u);
  if(gate==4)h.set(h.enemy(),8,0x10000001u);
  if(gate==5)h.set(h.f.image.data()+0xAE6EF8,0x10,257u);
  if(gate==6)h.set(h.f.image.data()+0xAE6EF8,0x14,0x10C9u);
  if(gate==7)h.set(h.f.weapon,0x418,reinterpret_cast<std::uintptr_t>(h.enemy()+1));
  if(gate==8)h.set(h.f.weapon,0x418,reinterpret_cast<std::uintptr_t>(h.pool.data()));
  if(gate==9)h.f.actor[0x114]=1;
  if(gate==10)h.f.input_state[0x86FF]=1;
  if(gate==11)h.f.input_state[0x5092]|=4;
  if(gate==12)h.f.image[0x1131718+0x54]=7;
  if(gate==13)h.set(h.f.input_state.data(),0x7C,1u<<13);
  if(gate==14)h.set(h.enemy(),0x3C4,std::int16_t(0));
  if(gate==15)h.enemy()[0x11A]=0;
  if(gate==16)h.set(h.bones[0].data(),0x108,reinterpret_cast<std::uintptr_t>(h.bones[0].data()));
  if(gate==17)h.set(h.bones[0].data(),0x80,re4::ViewVec3{0,std::numeric_limits<float>::quiet_NaN(),-1000});
  if(gate==18)game_test::memory_maps.clear();
  if(gate==19)h.set(h.f.weapon,0x418,std::numeric_limits<std::uintptr_t>::max());
  if(gate==20){settings.enemy_health_bars=false;publish_view_settings();}
  if(gate==21)h.f.actor[0x116]=4; // Reload can retain the broad native predicate.
  if(gate==22)h.f.actor[0x116]=0;
  if(gate==23)for(auto& bone:h.bones)h.set(bone.data(),0x1D8,2u);
  h.capture();const auto drawn=h.display();if(drawn)std::fprintf(stderr,"enemy exclusion gate %u\n",gate);assert(drawn==0);
 }
 {
  HealthFixture h;h.capture();h.set(h.f.weapon,0x418,std::uintptr_t(0));h.capture();assert(h.display()==0);
 }
 {
  HealthFixture h;h.capture();assert(h.display()==1);
  h.capture(1);assert(h.display()==0); // Cached draw cannot republish last frame's target.
  h.capture();assert(h.display()==1);
  settings.enemy_health_bars=false;publish_view_settings();const auto before=hud_rects.size();HudDraw::Callback(nullptr);assert(hud_rects.size()==before);
  settings.enemy_health_bars=true;publish_view_settings();HudDraw::Callback(nullptr);assert(hud_rects.size()==before);
 }
 {
  HealthFixture h;h.f.scope_mode();h.capture();assert(h.display()==1); // Same native trace, separate scoped camera owner.
 }
 {
  HealthFixture h;h.capture();SceneSubmit::Callback();view_frame.fetch_add(1);SceneDraw::Callback();
  // Only captured scalars/matrices are used in late HUD, after this scene's
  // enemy memory or main camera has changed.
  game_test::memory_maps.clear();std::memset(h.f.input_state.data()+0xD8,0,48);
  h.set(h.enemy(),0x3C2,std::int16_t(25));assert(h.hud()==0);
  assert(h.advance(10)==1&&gx_colors.back()[1]>gx_colors.back()[0]);
  h.f.input_state[0x5034]++;assert(h.hud()==1); // Weapon identity invalidates selection, camera still owns the tail.
  assert(h.advance(10)==0);
 }
 std::puts("actual enemy health callbacks: native target, displayed scene, colors and camera independence PASS");
}
