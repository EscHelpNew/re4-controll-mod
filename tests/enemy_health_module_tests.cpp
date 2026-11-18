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
  const auto empty=re4::initial_health_capture();
  auto* bytes=reinterpret_cast<volatile std::uint8_t*>(&health_capture);
  for(std::size_t i=0;i<sizeof(empty);++i)bytes[i]=reinterpret_cast<const std::uint8_t*>(&empty)[i];
  health_capture_writer.clear();
  f.view_mode();f.aim();settings.enemy_health_bars=true;health_color_probe=true;
  *reinterpret_cast<int*>(f.image.data()+0xAFF39C)=2;
  set(f.image.data()+0xAE6EF8,8,reinterpret_cast<std::uintptr_t>(pool.data()));
  set(f.image.data()+0xAE6EF8,0x10,3u);set(f.image.data()+0xAE6EF8,0x14,0x10C8u);
  set(enemy(),8,1u);enemy()[0x118]=0x10;enemy()[0x11A]=2;
  set(enemy(),0,main_base+0xAE7050);set(f.image.data(),0xAE7078,main_base+0x8F470);set(enemy(),0xB8C,std::int32_t(2));
  set(f.input_state.data(),0x84F0,std::int32_t(6));set(f.input_state.data(),0x500C,std::int32_t(10));
  f.input_state[0x5034]=2;set(f.image.data(),0xAF055C+2*4,std::int32_t(150));
  set(enemy(),0x3C2,std::int16_t(100));set(enemy(),0x3C4,std::int16_t(100));
  set(enemy(),0xA4,re4::ViewVec3{0,0,-1000});set(enemy(),0x108,reinterpret_cast<std::uintptr_t>(bones.data()));
  set(bones[0].data(),0x80,re4::ViewVec3{0,50,-1000});set(bones[1].data(),0x80,re4::ViewVec3{0,150,-1000});
  set(bones[0].data(),0x108,reinterpret_cast<std::uintptr_t>(bones[1].data()));
  set(f.weapon,0x418,reinterpret_cast<std::uintptr_t>(enemy()));
  game_test::memory_maps={{reinterpret_cast<std::uintptr_t>(f.image.data()),f.image.size(),1},
   {reinterpret_cast<std::uintptr_t>(f.weapon),sizeof(f.weapon),1},
   {reinterpret_cast<std::uintptr_t>(pool.data()),pool.size(),1},
   {reinterpret_cast<std::uintptr_t>(bones.data()),sizeof(bones),1},
   {reinterpret_cast<std::uintptr_t>(f.actor),sizeof(f.actor),1},
   {reinterpret_cast<std::uintptr_t>(f.weapon_context),sizeof(f.weapon_context),1},
   {reinterpret_cast<std::uintptr_t>(f.input_state.data()),f.input_state.size(),1}};
  publish_view_settings();
 }
 void capture(unsigned reuse=0){WeaponLaser::Callback(f.weapon,1,reuse,0);}
 void scene(){SceneSubmit::Callback();view_frame.fetch_add(1);SceneDraw::Callback();}
 unsigned hud(){
  const auto before=hud_rects.size();HudDraw::Callback(nullptr);return unsigned(hud_rects.size()-before);
 }
 unsigned advance(unsigned steps){
  unsigned drawn=0;for(unsigned i=0;i<steps;++i){game_test::tick+=192000;drawn=hud();}return drawn;
 }
 unsigned display(){scene();hud();return advance(30);}
 std::vector<std::vector<std::uint8_t>> packets(){
  const auto before=hud_rects.size();hud();return {hud_rects.begin()+before,hud_rects.end()};
 }
 void number(int current,unsigned unit,const char* text){
  char formatted[8]{};assert(re4::format_enemy_health_number(current,unit,false,formatted,sizeof(formatted)));
  assert(!std::strcmp(formatted,text));
  const auto actual=packets();assert(actual.size()>1);
  re4::ViewVec3 anchor;re4::ViewMatrix3x4 view;re4::ViewMatrix4x4 projection;re4::ViewHudPoint point;
  assert(read_aim_point(displayed_enemy_health,f.actor,view_frame.load(),anchor));
  assert(read_aim_point(enemy_health_camera,f.actor,view_frame.load(),anchor,&view,&projection,nullptr,nullptr,false));
  assert(read_aim_point(displayed_enemy_health,f.actor,view_frame.load(),anchor)&&re4::project_view_point(view,projection,anchor,point));
  const auto before=hud_rects.size();assert(re4::draw_enemy_health_number(native_hud_backend(),point,current,unit,false));
  const std::vector<std::vector<std::uint8_t>> expected{hud_rects.begin()+before,hud_rects.end()};
  assert(actual.size()==expected.size()+1&&std::equal(expected.begin(),expected.end(),actual.begin()+1));
 }
};
}
int main(){
 // An optional HP word must never participate in mandatory controls startup.
 {
  using F=re4::HealthCaptureField;HealthFixture h;
  const auto index=re4::health_profile::class_index(0xAE8170);
  const auto& descriptor=re4::health_profile::kClasses[index];assert(descriptor.count);
  const auto word=re4::health_profile::kAddedWords[re4::health_profile::kClassWordIndices[descriptor.begin]];
  h.set(h.f.image.data(),word.rva,word.expected^1u);
  const auto installed_before=game_test::installed_offsets.size();exl_main(nullptr,nullptr);
  assert(control.status==1&&game_test::installed==64&&game_test::initialized==1);
  for(unsigned hook:{0x25F680u,0x19B20u,0x33DF80u})
   assert(std::find(game_test::installed_offsets.begin()+installed_before,game_test::installed_offsets.end(),hook)!=game_test::installed_offsets.end());
  h.f.aim();settings.enemy_health_bars=true;publish_view_settings();
  h.set(h.enemy(),0,main_base+0xAE8170);h.set(h.f.image.data(),0xAE8198,main_base+0xDF550);h.enemy()[0x118]=0x22;
  const auto native=h.pool;h.capture();assert(enemy_health_point.reference_unit.load()==0);
  assert(control.status==1&&health_capture.words[unsigned(F::FailedGate)]==unsigned(re4::HealthReportGate::Class));
  assert(health_capture.words[unsigned(F::GuardRva)]==word.rva);
  assert(health_capture.words[unsigned(F::GuardExpected)]==word.expected&&health_capture.words[unsigned(F::GuardObserved)]==(word.expected^1u));
  assert(health_capture.words[unsigned(F::ReadMask)]&(1u<<unsigned(F::GuardObserved)));
  h.set(h.f.image.data(),word.rva,word.expected);h.capture();assert(enemy_health_point.reference_unit.load()==0&&h.pool==native); // Retain cachedfirstfailure.
  h.set(h.enemy(),0,main_base+0xAE8938);h.set(h.f.image.data(),0xAE8960,main_base+0xEDA10);h.enemy()[0x118]=0x26;
  h.set(h.f.image.data(),0xAF1440+(0x26-0x23)*8,main_base+0xAF055C);const auto other_native=h.pool;h.capture();
  assert(enemy_health_point.reference_unit.load()==127&&h.pool==other_native); // Anotherclass remainsavailable.
 }
 {
  HealthFixture h;h.set(h.enemy(),0,main_base+0xAE8170);h.set(h.f.image.data(),0xAE8198,main_base+0xDF550);h.enemy()[0x118]=0x22;
  h.capture();assert(enemy_health_point.reference_unit.load()==127); // Freshprocess/fixture restoresacceptance.
 }
 {
  using F=re4::HealthCaptureField;HealthFixture h;
  const auto index=re4::health_profile::class_index(0xAE7050);
  settings.enemy_health_bars=false;publish_view_settings();h.capture();assert(enemy_health_guards.entries[index].state.load()==0);
  settings.enemy_health_bars=true;publish_view_settings();enemy_health_guards.entries[index].state.store(1);
  h.capture();assert(enemy_health_point.reference_unit.load()==0); // Busyvalidator neverblocks anothercapture.
  assert(!(health_capture.words[unsigned(F::ReadMask)]&(1u<<unsigned(F::GuardRva))));
 }

 // Newcensus finite class22 has127rawHP per reference1.0 at currentrank10.
 {
  using F=re4::HealthCaptureField;HealthFixture h;settings.enemy_health_numbers=true;publish_view_settings();
  h.set(h.enemy(),0,main_base+0xAE8170);h.set(h.f.image.data(),0xAE8198,main_base+0xDF550);
  h.enemy()[0x118]=0x22;h.enemy()[0x119]=255;h.enemy()[0xB8C]=2;h.enemy()[0xB4C]=15;
  h.set(h.enemy(),0x3C2,std::int16_t(1500));h.set(h.enemy(),0x3C4,std::int16_t(1500));
  const auto native=h.pool;h.capture();assert(enemy_health_point.reference_unit.load()==127);
  assert(health_capture.words[unsigned(F::FailedGate)]==unsigned(re4::HealthReportGate::None));
  assert(!(health_capture.words[unsigned(F::ReadMask)]&(1u<<unsigned(F::BodyStatus))));
  h.display();h.number(1500,127,"11.8");assert(h.pool==native);
 }

 // Common11831/38 usescommonbody, neverlegacycore math orlinkedownership.
 for(unsigned type:{0x31u,0x38u}){
  HealthFixture h;settings.enemy_health_numbers=true;publish_view_settings();h.enemy()[0x118]=std::uint8_t(type);
  h.set(h.f.image.data(),0xAF1440+(type-0x23)*8,main_base+0xAF055C);const auto native=h.pool;
  h.capture();assert(enemy_health_point.reference_unit.load()==58);h.display();h.number(100,58,"1.7");assert(h.pool==native);
 }
 // Hitcount familyneedsnoneoftheforeignbodyfields,modeorrank foritsunit1.
 {
  using F=re4::HealthCaptureField;HealthFixture h;settings.enemy_health_numbers=true;publish_view_settings();
  h.set(h.enemy(),0,main_base+0xAED668);h.set(h.f.image.data(),0xAED690,main_base+0x1B3610);
  h.enemy()[0x118]=0x4F;h.enemy()[0x119]=9;h.f.input_state[0x84F0]=0;h.f.input_state[0x500C]=255;
  for(auto& map:game_test::memory_maps){
   if(map.addr==reinterpret_cast<std::uintptr_t>(h.pool.data())){map.addr=reinterpret_cast<std::uintptr_t>(h.enemy());map.size=0x3C6;}
   if(map.addr==reinterpret_cast<std::uintptr_t>(h.f.input_state.data()))map.size=0x5035;
  }
  const auto native=h.pool;h.capture();assert(enemy_health_point.reference_unit.load()==1);
  assert(!(health_capture.words[unsigned(F::ReadMask)]&(1u<<unsigned(F::BodyStatus))));
  assert(!(health_capture.words[unsigned(F::ReadMask)]&(1u<<unsigned(F::Coefficient))));
  assert(!(health_capture.words[unsigned(F::ReadMask)]&(1u<<unsigned(F::Mode))));
  h.display();h.number(100,1,"100.0");h.enemy()[0x119]=10;h.capture();h.scene();h.number(100,re4::kEnemyHealthPowerUnit,"0.7");
  h.enemy()[0x119]=9;assert(h.pool==native);
 }
 // Knownclass/typecrossmapping mustnotfabricateSaddler/Salazarcorelinks.
 for(unsigned type:{0x31u,0x38u}){
  HealthFixture h;settings.enemy_health_numbers=true;publish_view_settings();
  h.set(h.enemy(),0,main_base+0xAE8170);h.set(h.f.image.data(),0xAE8198,main_base+0xDF550);
  h.enemy()[0x118]=std::uint8_t(type);h.enemy()[0x119]=255;
  h.set(h.f.image.data(),0xAF1440+(type-0x23)*8,main_base+0xAF055C);
  h.capture();assert(enemy_health_point.reference_unit.load()==127);h.display();h.number(100,127,"0.8");
  h.set(h.enemy(),0,main_base+0xAE8178);h.capture();assert(enemy_health_point.frame.load()==0); // Unknownvptrstaysrejected.
 }
 // Onehitunit isfullmaximumHP andremainsfixed afterthetargetiswounded.
 {
  HealthFixture h;settings.enemy_health_numbers=true;publish_view_settings();
  h.set(h.enemy(),0,main_base+0xAE7F00);h.set(h.f.image.data(),0xAE7F28,main_base+0xD8A50);
  h.enemy()[0x118]=0x18;h.enemy()[0x119]=255;h.f.input_state[0x84F0]=0;h.f.input_state[0x500C]=255;
  h.set(h.enemy(),0x3C2,std::int16_t(1500));h.set(h.enemy(),0x3C4,std::int16_t(1500));
  h.capture();assert(enemy_health_point.reference_unit.load()==1500);h.display();h.number(1500,1500,"1.0");
  h.set(h.enemy(),0x3C2,std::int16_t(750));h.capture();h.scene();h.number(750,1500,"0.5");
 }
 for(bool scripted:{false,true}){
  HealthFixture h;settings.enemy_health_numbers=true;publish_view_settings();
  const unsigned vptr=scripted?0xAE8038:0xAE8450,update=scripted?0xDC800:0xE6350;
  h.set(h.enemy(),0,main_base+vptr);h.set(h.f.image.data(),vptr+0x28,main_base+update);
  h.enemy()[0x118]=scripted?0x21:0x23;h.enemy()[0x119]=255;h.enemy()[0xB8C]=255;
  h.f.input_state[0x84F0]=0;h.set(h.f.image.data(),0xAF1440,std::uintptr_t(0));
  h.capture();const unsigned unit=scripted?0:850;assert(enemy_health_point.reference_unit.load()==unit);
  h.display();h.number(100,unit?unit:re4::kEnemyHealthPowerUnit,scripted?"0.7":"0.1");
 }
 // Exact live retained tuple that previously fell back to raw150 units.
 {
  using F=re4::HealthCaptureField;HealthFixture h;settings.enemy_health_numbers=true;publish_view_settings();
  h.enemy()[0x118]=0x1E;h.enemy()[0x119]=0x17;h.enemy()[0xB8C]=2;h.enemy()[0xB4C]=15;
  h.set(h.enemy(),0x3C2,std::int16_t(1500));h.set(h.enemy(),0x3C4,std::int16_t(1500));
  const auto native=h.pool;h.capture();assert(enemy_health_point.reference_unit.load()==58);
  assert(health_capture.words[unsigned(F::Type)]==0x1E&&health_capture.words[unsigned(F::Subtype)]==0x17);
  assert(health_capture.words[unsigned(F::Phase)]==15&&health_capture.words[unsigned(F::BodyStatus)]==2);
  assert(health_capture.words[unsigned(F::FailedGate)]==unsigned(re4::HealthReportGate::None));
  h.display();h.number(1500,58,"25.9");const auto initial=h.packets();const auto key=displayed_enemy_health.target_key.load();
  for(bool extra:{false,true}){
   settings.extended_weapon_upgrade=extra;extended_upgrade_mask=extra?1:0;h.f.input_state[0x5037]=std::uint8_t(extra?6:5);
   h.capture();h.scene();assert(h.packets()==initial&&displayed_enemy_health.target_key.load()==key);
   assert(displayed_enemy_health.reference_unit.load()==58&&h.pool==native);
  }
  // Verifiednative losses: 2.0=>116raw=2.0display,2.2=>128raw=2.2display.
  h.set(h.enemy(),0x3C2,std::int16_t(1384));h.capture();h.scene();h.number(1384,58,"23.9");
  h.set(h.enemy(),0x3C2,std::int16_t(1372));h.capture();h.scene();h.number(1372,58,"23.7");
  // Native table changes after capture do not recompute the displayed record.
  h.capture();const auto captured_unit=enemy_health_point.reference_unit.load();
  h.set(h.f.image.data(),0xAF055C+2*4,std::int32_t(900));h.scene();
  assert(displayed_enemy_health.reference_unit.load()==captured_unit);h.number(1372,58,"23.7");
 }
 for(unsigned subtype:{2u,6u,0xAu,0xDu}){
  HealthFixture h;settings.enemy_health_numbers=true;publish_view_settings();
  h.enemy()[0x118]=0x1E;h.enemy()[0x119]=std::uint8_t(subtype);h.capture();
  const unsigned unit=subtype==2?39:subtype==0xA?7:subtype==0xD?2:0;
  assert(enemy_health_point.reference_unit.load()==unit);h.display();
  h.number(100,unit?unit:re4::kEnemyHealthPowerUnit,subtype==2?"2.6":subtype==0xA?"14.3":subtype==0xD?"50.0":"0.7");
 }

 {
  using F=re4::HealthCaptureField;
  HealthFixture h;assert(!health_capture.valid);control.frames=123;view_frame=7;
  const auto native=h.pool;h.capture();assert(health_capture.valid==1&&!(health_capture.sequence&1));
  assert(health_capture.words[unsigned(F::ComputedUnit)]==58&&health_capture.words[unsigned(F::PublishedUnit)]==58);
  assert(health_capture.words[unsigned(F::FailedGate)]==unsigned(re4::HealthReportGate::None));
  assert(health_capture.words[unsigned(F::Health)]==((100u<<16)|100u));
  assert(health_capture.words[unsigned(F::ViewFrameLo)]==7&&health_capture.words[unsigned(F::ControlFrameLo)]==123);
  assert(health_capture.words[unsigned(F::VptrRva)]==0xAE7050&&health_capture.words[unsigned(F::UpdateRva)]==0x8F470);
  assert(health_capture.words[unsigned(F::WeaponBefore)]==3&&health_capture.words[unsigned(F::WeaponAfter)]==3);
  assert(health_capture.words[unsigned(F::ReadMask)]&(1u<<unsigned(F::Coefficient)));
  assert(health_capture.words[unsigned(F::Coefficient)]==150&&health_capture.words[unsigned(F::BodyStatus)]==2);
  assert(h.pool==native);const auto sequence=health_capture.sequence;
  h.set(h.f.weapon,0x418,std::uintptr_t(0));h.f.actor[0x115]=0;game_test::native_aim=0;h.capture();h.scene();
  assert(health_capture.sequence==sequence&&health_capture.words[unsigned(F::ComputedUnit)]==58);
  re4::HealthCaptureSample sample;sample.set(F::ComputedUnit,999);
  health_capture_writer.test_and_set();assert(!re4::publish_health_capture(health_capture,health_capture_writer,sample));
  assert(health_capture.sequence==sequence);health_capture_writer.clear();
 }
 {
  using F=re4::HealthCaptureField;HealthFixture h;h.enemy()[0x118]=0x2C;h.enemy()[0x119]=2;
  h.set(h.enemy(),0,main_base+0xAE9438);h.set(h.f.image.data(),0xAE9460,main_base+0x106890);
  const auto native=h.pool;h.capture();
  assert(health_capture.valid==1&&health_capture.words[unsigned(F::ComputedUnit)]==0);
  assert(health_capture.words[unsigned(F::PublishedUnit)]==0&&health_capture.words[unsigned(F::Subtype)]==2);
  assert(health_capture.words[unsigned(F::FailedGate)]==unsigned(re4::HealthReportGate::Subtype));
  const auto mask=health_capture.words[unsigned(F::ReadMask)];
  assert((mask&(1u<<unsigned(F::Subtype)))&&!(mask&(1u<<unsigned(F::BodyStatus)))&&!(mask&(1u<<unsigned(F::Coefficient))));
  assert(h.pool==native);
 }
 {
  using F=re4::HealthCaptureField;HealthFixture h;re4::HealthCaptureSample sample;
  sample.set(F::FailedGate,unsigned(re4::HealthReportGate::None));
  const auto old=enemy_health_point.sequence.load();
  finish_enemy_health_capture(h.f.actor,h.f.weapon,{0,250,-1000},(100u<<16)|100u,123,58,3,4,sample);
  assert(health_capture.valid==1&&health_capture.words[unsigned(F::ComputedUnit)]==58);
  assert(health_capture.words[unsigned(F::PublishedUnit)]==0&&health_capture.words[unsigned(F::Outcome)]==2);
  assert(health_capture.words[unsigned(F::WeaponBefore)]==3&&health_capture.words[unsigned(F::WeaponAfter)]==4);
  assert(enemy_health_point.sequence.load()==old); // Preserve actual publication rejection.
 }

 {
  using F=re4::HealthCaptureField;HealthFixture h;h.set(h.f.image.data(),0xAF055C+2*4,std::int32_t(-1));
  h.capture();assert(health_capture.valid==1&&health_capture.words[unsigned(F::ComputedUnit)]==0);
  assert(health_capture.words[unsigned(F::FailedGate)]==unsigned(re4::HealthReportGate::Coefficient));
  assert(health_capture.words[unsigned(F::Coefficient)]==UINT32_MAX);
  assert(health_capture.words[unsigned(F::ReadMask)]&(1u<<unsigned(F::Coefficient)));
 }
 {
  using F=re4::HealthCaptureField;HealthFixture h;
  // Snapshot fields remain readable, but the provider's firstB8C readfails.
  for(auto& map:game_test::memory_maps)if(map.addr==reinterpret_cast<std::uintptr_t>(h.pool.data())){
   map.addr=reinterpret_cast<std::uintptr_t>(h.enemy());map.size=0xB8C;
  }
  h.capture();assert(health_capture.valid==1&&health_capture.words[unsigned(F::ComputedUnit)]==0);
  assert(health_capture.words[unsigned(F::FailedGate)]==unsigned(re4::HealthReportGate::BodyStatus));
  assert(!(health_capture.words[unsigned(F::ReadMask)]&(1u<<unsigned(F::BodyStatus))));
  assert(health_capture.words[unsigned(F::ReadMask)]&(1u<<unsigned(F::Health)));
 }
 {
  using F=re4::HealthCaptureField;HealthFixture h;re4::HealthCaptureSample sample;
  const auto old=enemy_health_point.sequence.load();
  finish_enemy_health_capture(h.f.actor,h.f.weapon,{0,250,-1000},(100u<<16)|100u,123,58,0,0,sample);
  assert(enemy_health_point.sequence.load()==old+2&&enemy_health_point.reference_unit.load()==0);
  assert(health_capture.words[unsigned(F::ComputedUnit)]==58&&health_capture.words[unsigned(F::PublishedUnit)]==0);
  assert(health_capture.words[unsigned(F::Outcome)]==1); // Originalzero-stamp publication semantics.
 }
 {
  HealthFixture h;settings.enemy_health_numbers=true;publish_view_settings();
  auto* core=h.pool.data()+2*0x10C8;const auto body=reinterpret_cast<std::uintptr_t>(h.enemy());
  const auto owner=reinterpret_cast<std::uintptr_t>(core);
  h.set(h.enemy(),0,main_base+0xAEA170);h.enemy()[0x118]=0x31;h.enemy()[0x119]=0;
  h.set(h.enemy(),0xBCC,owner);h.set(h.enemy(),0x3C2,std::int16_t(1500));h.set(h.enemy(),0x3C4,std::int16_t(3000));
  h.set(core,0,main_base+0xAEA170);h.set(core,8,0x10000001u);core[0x118]=0x31;core[0x119]=1;
  h.set(core,0xBC4,body);h.set(core,0x3C2,std::int16_t(1000));h.set(core,0x3C4,std::int16_t(2000));
  h.set(core,0x108,reinterpret_cast<std::uintptr_t>(h.bones[1].data()));core[0x11A]=1;
  h.set(core,0xA4,re4::ViewVec3{400,0,-1000});
  h.set(h.f.image.data(),0xAEA198,main_base+0x12C0E0);
  h.set(h.f.image.data(),0xAF14B0,main_base+0xAF0620);h.set(h.f.image.data(),0xAF0620+2*4,std::int32_t(15));
  h.capture();assert(enemy_health_point.reference_unit.load()==2);
  assert(enemy_health_point.health.load()==((2000u<<16)|1000u));
  assert(view_float_value(enemy_health_point.x.load())==0.f&&view_float_value(enemy_health_point.y.load())==250.f);
  const auto key=enemy_health_point.target_key.load();h.display();h.number(1000,2,"500.0");
  h.set(core,0x4C4,0xFFFFFFFFu);h.capture();h.scene();h.number(1000,2,"500.0");
  assert(displayed_enemy_health.target_key.load()==key); // Freeze/bonus flags cannot redefine the unit.
  // The same core now directly aimed has one canonical identity, a new aimed anchor.
  h.set(core,8,1u);h.set(h.f.weapon,0x418,owner);h.capture();
  assert(enemy_health_point.target_key.load()==key&&enemy_health_point.reference_unit.load()==2);
  assert(view_float_value(enemy_health_point.x.load())==400.f);
 }

 for(unsigned subtype:{0u,1u,3u,4u})for(unsigned links=0;links<4;++links)for(unsigned phase:{0u,4u}){
  HealthFixture h;settings.enemy_health_numbers=true;publish_view_settings();
  h.enemy()[0x119]=std::uint8_t(subtype);h.enemy()[0x48D]=2;h.enemy()[0xB4C]=std::uint8_t(phase);
  h.set(h.enemy(),0x944,std::uintptr_t(links&1?0xDEAD000:0));
  h.set(h.enemy(),0x974,std::uintptr_t(links&2?0xDEAD008:0));
  h.set(h.enemy(),0x3C2,std::int16_t(1500));h.set(h.enemy(),0x3C4,std::int16_t(1500));
  const auto native=h.pool;h.capture();h.display();
  const unsigned unit=links?39:58;
  assert(displayed_enemy_health.reference_unit.load()==unit);
  h.number(1500,unit,links?"38.5":"25.9");
  settings.extended_weapon_upgrade=true;extended_upgrade_mask=1;h.f.input_state[0x5037]=6;
  h.capture();h.scene();h.number(1500,unit,links?"38.5":"25.9");
  assert(h.pool==native); // Reference and rendering never change any native enemy byte.
 }
 for(bool linked:{false,true}){
  HealthFixture h;settings.enemy_health_numbers=true;publish_view_settings();
  h.f.input_state[0x5034]=11;h.set(h.f.image.data(),0xAF055C+11*4,std::int32_t(210));
  h.enemy()[0x119]=3;h.enemy()[0xB4C]=4;h.enemy()[0x48D]=2;
  h.set(h.enemy(),0x944,std::uintptr_t(linked?0xDEAD000:0));
  h.set(h.enemy(),0x3C2,std::int16_t(1500));h.set(h.enemy(),0x3C4,std::int16_t(1500));
  h.capture();h.display();const unsigned unit=linked?27:40;
  assert(displayed_enemy_health.reference_unit.load()==unit);h.number(1500,unit,linked?"55.6":"37.5");
 }
 for(unsigned index:{7u,8u,33u}){
  HealthFixture h;settings.enemy_health_numbers=true;publish_view_settings();
  h.f.input_state[0x5034]=std::uint8_t(index);h.set(h.f.image.data(),0xAF055C+index*4,std::int32_t(150));
  h.set(h.enemy(),0x3C2,std::int16_t(1500));h.set(h.enemy(),0x3C4,std::int16_t(1500));
  h.capture();h.display();h.number(1500,58,"25.9");const auto key=displayed_enemy_health.target_key.load();
  for(float distance:{1000.f,2000.f})for(bool extra:{false,true}){
   h.set(h.enemy(),0xA4,re4::ViewVec3{0,0,-distance});
   h.set(h.bones[0].data(),0x80,re4::ViewVec3{0,50,-distance});
   h.set(h.bones[1].data(),0x80,re4::ViewVec3{0,150,-distance});
   settings.extended_weapon_upgrade=extra;extended_upgrade_mask=extra?0xFFFF:0;
   h.f.input_state[0x5037]=std::uint8_t(extra?6:5);h.capture();h.scene();
   assert(displayed_enemy_health.reference_unit.load()==58&&displayed_enemy_health.reference_weapon.load()==index+1);
   assert(displayed_enemy_health.target_key.load()==key);h.number(1500,58,"25.9");
  }
 }
 for(unsigned mode:{2u,3u,4u,5u,6u,7u,255u})for(unsigned rank:{0u,5u,10u}){
  HealthFixture h;settings.enemy_health_numbers=true;publish_view_settings();
  h.f.input_state[0x84F0]=std::uint8_t(mode);h.f.input_state[0x500C]=std::uint8_t(rank);
  h.set(h.enemy(),0x3C2,std::int16_t(1500));h.set(h.enemy(),0x3C4,std::int16_t(1500));
  const auto native=h.pool;h.capture();h.display();
  const unsigned unit=rank==0?138:rank==5?69:58;
  assert(displayed_enemy_health.reference_unit.load()==unit);
  h.number(1500,unit,rank==0?"10.9":rank==5?"21.7":"25.9");
  assert(h.pool==native);
 }
 for(unsigned gate=0;gate<4;++gate){
  HealthFixture h;settings.enemy_health_numbers=true;publish_view_settings();h.capture();h.display();assert(h.hud()>1);
  const auto old_pixels=h.packets();
  h.capture();
  if(gate==0){h.f.input_state[0x5034]=7;h.scene();}
  if(gate==1){SceneSubmit::Callback();h.f.input_state[0x5034]=7;view_frame.fetch_add(1);SceneDraw::Callback();}
  if(gate==2){h.scene();h.f.input_state[0x5034]=7;}
  if(gate==3){h.f.input_state[0x5034]=7;game_test::native_aim=0;h.f.actor[0x116]=4;h.scene();}
  assert(h.hud()>1); // Keep the copied number through the original bar fade.
  assert(h.packets()==old_pixels); // Same captured denominator, glyphs, color and opacity at this tick.
  if(gate!=2)assert(enemy_health_camera.reference_weapon.load()==8);
 }
 for(unsigned gate=0;gate<6;++gate){
  HealthFixture h;settings.enemy_health_numbers=true;publish_view_settings();
  if(gate==0)h.set(h.enemy(),0x119,std::uint8_t(6)); // Native normal-body immunity has no finite normalization unit.
  if(gate==1)h.set(h.f.image.data(),0xAF055C+2*4,std::int32_t(0)); // No damage unit.
  if(gate==2)h.f.input_state[0x5034]=13; // Unqualified rocket/special path.
  if(gate==3)h.f.input_state[0x84F0]=0; // Easy/random path retains its previous number scale.
  if(gate==4)h.f.input_state[0x500C]=11; // Unknown rank.
  if(gate==5)h.set(h.enemy(),0,main_base+0xAE7058); // Unknown damage family.
  h.capture();assert(h.display()>1&&displayed_enemy_health.reference_unit.load()==0);
  h.number(100,re4::kEnemyHealthPowerUnit,"0.7"); // Exact previous numeric scale and pixels.
 }
 {
  HealthFixture h;settings.enemy_health_numbers=true;publish_view_settings();
  h.set(h.enemy(),0x3C2,std::int16_t(1500));h.set(h.enemy(),0x3C4,std::int16_t(1500));
  h.capture();assert(enemy_health_point.reference_unit.load()==58&&enemy_health_point.reference_weapon.load()==3);
  SceneSubmit::Callback();assert(pending_enemy_health.reference_unit.load()==58&&pending_enemy_health.reference_weapon.load()==3);
  // Changing native coefficients after capture cannot recalculate this scene's unit.
  h.set(h.f.image.data(),0xAF055C+2*4,std::int32_t(900));
  view_frame.fetch_add(1);SceneDraw::Callback();h.hud();h.advance(30);
  assert(displayed_enemy_health.reference_unit.load()==58&&displayed_enemy_health.reference_weapon.load()==3);
  const auto captured=h.packets();game_test::memory_maps.clear();
  assert(h.packets()==captured); // HUD uses copied scalars, not enemy/coefficient readers.
 }
 {
  HealthFixture h;h.capture();
  re4::ViewVec3 anchor;std::uint32_t hp=0,unit=0;std::uint64_t key=0,stamp=0;
  enemy_health_point.sequence.fetch_add(1);
  assert(!read_aim_point(enemy_health_point,h.f.actor,view_frame.load(),anchor,nullptr,nullptr,&hp,&key,true,&unit,&stamp));
  enemy_health_point.sequence.fetch_add(1);
  assert(read_aim_point(enemy_health_point,h.f.actor,view_frame.load(),anchor,nullptr,nullptr,&hp,&key,true,&unit,&stamp));
  assert(unit==58&&stamp==3&&key&&hp==((100u<<16)|100u));
  publish_aim_point(aim_point,h.f.actor,h.f.weapon,{},view_frame.load());
  assert(!aim_point.reference_unit.load()&&!aim_point.reference_weapon.load()); // Non-health payload defaults.
 }
 {
  HealthFixture h;settings.enemy_health_numbers=true;publish_view_settings();
  h.set(h.enemy(),0x3C2,std::int16_t(1500));h.set(h.enemy(),0x3C4,std::int16_t(1500));
  h.capture();h.display();h.number(1500,58,"25.9");
  const auto baseline=h.packets();const auto key=displayed_enemy_health.target_key.load();
  for(bool extra:{false,true}){
   settings.extended_weapon_upgrade=extra;extended_upgrade_mask=extra?1:0;
   h.f.input_state[0x5037]=std::uint8_t(extra?6:5);h.capture();h.scene();
   assert(h.packets()==baseline&&displayed_enemy_health.target_key.load()==key);
  }
  h.set(h.enemy(),0x3C2,std::int16_t(1384));h.capture();h.scene();h.number(1384,58,"23.9");
  h.set(h.enemy(),0x3C2,std::int16_t(1372));h.capture();h.scene();h.number(1372,58,"23.7");
  // A critical/head bonus reduces more actual HP, never the reference unit.
  h.set(h.enemy(),0x3C2,std::int16_t(1000));h.capture();h.scene();h.number(1000,58,"17.2");
 }
 {
  HealthFixture h;h.capture();h.scene();assert(h.hud()==0); // Fade begins transparent.
  assert(h.advance(10)==1&&gx_colors.back()[3]==51); // Half visible after 100 ms.
  assert(h.advance(10)==1&&gx_colors.back()[3]==102); // Fully visible after 200 ms.
  h.set(h.f.weapon,0x418,std::uintptr_t(0));h.capture();h.scene();
  assert(h.hud()==1&&gx_colors.back()[3]==102);
  assert(h.advance(15)==1&&gx_colors.back()[3]==51);
  assert(h.advance(15)==0); // Copied target fades away in300ms.
 }
 {
  HealthFixture h;settings.enemy_health_numbers=true;publish_view_settings();h.capture();h.scene();
  assert(h.hud()==0); // Bar and number both begin transparent.
  auto before=gx_colors.size();assert(h.advance(10)>1);
  // At 100 ms both use half their own peak alpha: digits are more opaque.
  before=gx_colors.size();assert(h.hud()>1);
  for(auto i=before;i<gx_colors.size();++i)assert(gx_colors[i][3]==(gx_colors[i][0]==235?64u:51u));
  assert(h.advance(10)>1);before=gx_colors.size();assert(h.hud()>1);
  for(auto i=before;i<gx_colors.size();++i)assert(gx_colors[i][3]==(gx_colors[i][0]==235?128u:102u));
  h.set(h.f.weapon,0x418,std::uintptr_t(0));h.capture();h.scene();h.hud();
  assert(h.advance(15)>1);before=gx_colors.size();assert(h.hud()>1);
  for(auto i=before;i<gx_colors.size();++i)assert(gx_colors[i][3]==(gx_colors[i][0]==235?64u:51u));
  assert(h.advance(15)==0);
 }
 {
  HealthFixture h;h.capture();assert(h.display()==1);
  const auto key=displayed_enemy_health.target_key.load();assert(key);
  h.set(h.f.weapon,0x418,std::uintptr_t(0));h.capture();h.scene();assert(h.hud()==1);
  assert(h.advance(15)==1&&gx_colors.back()[3]==51);
  h.set(h.f.weapon,0x418,reinterpret_cast<std::uintptr_t>(h.enemy()));
  h.capture();h.scene();assert(h.hud()==1&&gx_colors.back()[3]==51);
  assert(displayed_enemy_health.target_key.load()==key);
  assert(h.advance(10)==1&&gx_colors.back()[3]==77); // Reacquisition starts at half opacity.
  assert(h.advance(10)==1&&gx_colors.back()[3]==102); // It reaches full opacity in 200 ms.
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
  assert(h.advance(10)==2&&gx_colors.back()[3]==51); // New target is half visible at 100 ms.
  assert(h.advance(20)==1&&gx_colors.back()[3]==102); // Old target finishes its 300 ms fade.
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
  assert(gx_colors.back()[0]==235&&gx_colors.back()[1]==235&&gx_colors.back()[2]==235&&gx_colors.back()[3]==128);
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
  assert(h.advance(30)==1&&gx_colors.back()[1]>gx_colors.back()[0]);
  h.f.input_state[0x5034]++;assert(h.hud()==1); // Weapon identity invalidates selection, camera still owns the tail.
  assert(h.advance(30)==0);
 }
 std::puts("actual enemy health callbacks: native target, displayed scene, colors and camera independence PASS");
}
