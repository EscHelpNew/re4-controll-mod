#include "re4/enemy_health_reference.hpp"
#include "re4/enemy_health_families.hpp"
#include "re4/health_profile.hpp"
#include "re4/enemy_health.hpp"
#include "re4/profile.hpp"
#include <array>
#include <vector>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"reference line%d: %s\n",__LINE__,#x);std::exit(1);}}while(false)
namespace {
std::uintptr_t main_base=0;
class EnemyHealthReader {
public:
 struct Span {std::uintptr_t base;std::size_t size;};
 EnemyHealthReader();
 std::vector<Span> spans,reads;
 bool trace=false;
 template<class T> bool field(std::uintptr_t base,std::size_t offset,T& out){
  if(!base||offset>std::numeric_limits<std::uintptr_t>::max()-base)return false;
  const auto a=base+offset;
  if(trace)reads.push_back({a,sizeof(T)});
  for(const auto s:spans)if(a>=s.base&&a-s.base<=s.size&&sizeof(T)<=s.size-(a-s.base)){
   std::memcpy(&out,reinterpret_cast<const void*>(a),sizeof(T));return true;
  }
  return false;
 }
};
std::vector<EnemyHealthReader::Span> readable;
EnemyHealthReader::EnemyHealthReader():spans(readable){}
}
#include "../source/game/enemy_health_reference.inc"
namespace {
struct Fixture {
 std::vector<std::uint8_t> image=std::vector<std::uint8_t>(0x1281000),pool=std::vector<std::uint8_t>(3*0x10C8),state=std::vector<std::uint8_t>(0x8700);
 std::array<std::uint8_t,0x950> player{};
 std::array<std::uint8_t,0x60> context{},weapon{};
 EnemyHealthReader reader;
 template<class T> void put(std::uintptr_t a,std::size_t off,T value){std::memcpy(reinterpret_cast<void*>(a+off),&value,sizeof(value));}
 Fixture(){
  main_base=reinterpret_cast<std::uintptr_t>(image.data());
  for(const auto& word:re4::health_profile::kAddedWords){
   CHECK(re4::health_profile::immutable_word(word.rva));std::memcpy(image.data()+word.rva,&word.expected,4);
  }
  enemy_health_guard_bind(image.data(),image.size());
  reader.spans={{main_base,image.size()},{reinterpret_cast<std::uintptr_t>(pool.data()),pool.size()},
   {reinterpret_cast<std::uintptr_t>(state.data()),state.size()},{reinterpret_cast<std::uintptr_t>(player.data()),player.size()},
   {reinterpret_cast<std::uintptr_t>(context.data()),context.size()},{reinterpret_cast<std::uintptr_t>(weapon.data()),weapon.size()}};
  readable=reader.spans;
  put(main_base,0xB1AE08,reinterpret_cast<std::uintptr_t>(state.data()));
  put(main_base,0x113B398,reinterpret_cast<std::uintptr_t>(player.data()));
  put(reinterpret_cast<std::uintptr_t>(player.data()),0x938,reinterpret_cast<std::uintptr_t>(context.data()));
  put(reinterpret_cast<std::uintptr_t>(context.data()),0x38,reinterpret_cast<std::uintptr_t>(weapon.data()));
  state[0x5034]=2;state[0x84F0]=6;state[0x500C]=10;
  put(main_base,0xAE6EF8+8,reinterpret_cast<std::uintptr_t>(pool.data()));
  put(main_base,0xAE6EF8+0x10,3u);put(main_base,0xAE6EF8+0x14,0x10C8u);
  put(target(),0,main_base+0xAE7050);put(main_base,0xAE7078,main_base+0x8F470);
  put(target(),8,1u);put(target(),0x118,std::uint8_t(0x10));
  put(target(),0x3C2,std::int16_t(1500));put(target(),0x3C4,std::int16_t(1500));
  put(target(),0xB8C,std::uint8_t(2));put(main_base,0xAF055C+2*4,150);
 }
 std::uintptr_t target()const{return reinterpret_cast<std::uintptr_t>(pool.data()+0x10C8);}
 unsigned unit(){return enemy_health_body_reference(reader,target());}
 std::uintptr_t body()const{return reinterpret_cast<std::uintptr_t>(pool.data()+2*0x10C8);}
 void boss(unsigned type,unsigned subtype){
  std::uintptr_t vtable=0,update=0,table=0;int coefficient=15;
  if(type==0x2C){vtable=0xAE9438;update=0x106890;table=0xAF0AB8;coefficient=25;}
  if(type==0x31){vtable=0xAEA170;update=0x12C0E0;table=0xAF0620;}
  if(type==0x35){vtable=0xAEAFD0;update=0x148E20;table=0xAF0930;}
  if(type==0x38){vtable=0xAEBC60;update=0x1629F0;table=0xAF0930;}
  if(type==0x39){vtable=0xAEBF38;update=0x16A5C0;table=0xAF07A8;}
  put(target(),0,main_base+vtable);put(main_base,vtable+0x28,main_base+update);
  put(target(),0x118,std::uint8_t(type));put(target(),0x119,std::uint8_t(subtype));
  put(main_base,0xAF1440+(type-0x23)*8,main_base+table);
  for(unsigned index:{1u,2u,3u,4u,5u,6u,7u,8u,9u,10u,11u,12u,15u,17u,33u})
   put(main_base,table+index*4,index==11&&type!=0x2C?12:coefficient);
  if(type==0x31||type==0x38){
   put(body(),0,main_base+vtable);put(body(),8,1u);
   put(body(),0x118,std::uint8_t(type));put(body(),0x119,std::uint8_t(0));
   put(target(),type==0x31?0xBC4:0xAE4,body());
   put(body(),type==0x31?0xBCC:0xAFC,target());
  }
 }
};
}
int main(){
 using re4::enemy_health_reference_damage;
 // Class identity owns body arithmetic; actual118 still owns table/rank.
 {
  Fixture f;f.put(f.target(),0x118,std::uint8_t(0x31));
  f.put(main_base,0xAF1440+(0x31-0x23)*8,main_base+0xAF0620);
  f.put(main_base,0xAF0620+2*4,150);CHECK(f.unit()==58);
 }
 // Authenticated em22 class uses plainC, not the common B8C reducer.
 {
  Fixture f;f.put(f.target(),0,main_base+0xAE8170);f.put(main_base,0xAE8198,main_base+0xDF550);
  f.put(f.target(),0x118,std::uint8_t(0x22));f.put(f.target(),0x119,std::uint8_t(255));
  CHECK(f.unit()==127);
 }

 // Nativecensus fixtures covereveryclass with original table/type/gun2 data.
 {
  struct Case {unsigned vptr,update,type,table,coefficient,unit,subtype;};
  const Case cases[]={
   {0xAE7050,0x8F470,0x1E,0xAF055C,150,58,0x17},
   {0xAE7F00,0xD8A50,0x18,0,0,1500,255},
   {0xAE8038,0xDC800,0x21,0,0,0,255},
   {0xAE8170,0xDF550,0x22,0xAF055C,150,127,255},
   {0xAE8450,0xE6350,0x23,0,0,850,255},
   {0xAE85B0,0xE87A0,0x24,0,0,1500,255},
   {0xAE8668,0xE9CF0,0x25,0xAF055C,150,255,255},
   {0xAE8938,0xEDA10,0x26,0xAF055C,150,127,255},
   {0xAE8A18,0xEE9D0,0x27,0,0,1000,255},
   {0xAE8B10,0xF0BF0,0x28,0,0,1500,255},
   {0xAE8C08,0xF2650,0x29,0,0,850,255},
   {0xAE8D38,0xF50F0,0x2A,0,0,1500,255},
   {0xAE8E28,0xF7BF0,0x2B,0,0,0,255},
   {0xAE9438,0x106890,0x2C,0xAF0AB8,25,21,0},
   {0xAE99D0,0x116230,0x2D,0xAF0B7C,50,42,255},
   {0xAE9EA8,0x126010,0x2E,0,0,1500,255},
   {0xAE9F58,0x126FA0,0x2F,0,0,0,255},
   {0xAEA0E0,0x12B120,0x30,0,0,8,255},
   {0xAEA170,0x12C0E0,0x31,0xAF0620,15,2,1},
   {0xAEA7E0,0x139BA0,0x32,0xAF0930,15,12,255},
   {0xAEAB58,0x147DC0,0x34,0,0,8,255},
   {0xAEAFD0,0x148E20,0x35,0xAF0930,15,12,0},
   {0xAEB618,0x154910,0x36,0xAF06E4,20,17,255},
   {0xAEBC60,0x1629F0,0x38,0xAF0930,15,12,3},
   {0xAEBF38,0x16A5C0,0x39,0xAF07A8,15,12,0},
   {0xAEC530,0x180EB0,0x3A,0xAF055C,150,637,255},
   {0xAEC720,0x1855E0,0x3B,0,0,200,255},
   {0xAEC918,0x187780,0x3C,0xAF086C,15,12,255},
   {0xAECCD8,0x18D6D0,0x3D,0,0,0,255},
   {0xAED668,0x1B3610,0x4F,0,0,1,0},
   {0xAECE20,0x18FED0,0x3F,0xAF0C40,15,5,255},
   {0xAED178,0x1A2E20,0x4E,0xAF0DC8,0,0,255}
  };
  CHECK(sizeof(cases)/sizeof(*cases)==sizeof(re4::kEnemyHealthClasses)/sizeof(*re4::kEnemyHealthClasses));
  using F=re4::HealthCaptureField;using Family=re4::EnemyHealthFamily;
  for(const auto& c:cases){
   Fixture f;const auto* known=re4::enemy_health_reference_class(c.vptr,c.update);CHECK(known);
   if(known->family==Family::LegacyBoss)f.boss(c.type,c.subtype);
   else{
    f.put(f.target(),0,main_base+c.vptr);f.put(main_base,c.vptr+0x28,main_base+c.update);
    f.put(f.target(),0x118,std::uint8_t(c.type));f.put(f.target(),0x119,std::uint8_t(c.subtype));
   }
   const bool needs_c=re4::enemy_health_family_needs_coefficient(known->family);
   if(needs_c){
    if(c.type>=0x23&&c.type<=0x4E)f.put(main_base,0xAF1440+(c.type-0x23)*8,main_base+c.table);
    f.put(main_base,c.table+2*4,int(c.coefficient));
   }else{
    f.put(main_base,0xAF055C+2*4,-1);
    if(c.type>=0x23&&c.type<=0x4E)f.put(main_base,0xAF1440+(c.type-0x23)*8,std::uintptr_t(0));
   }
   if(known->family!=Family::Common){
    f.put(f.target(),0xB8C,std::uint8_t(255));f.put(f.target(),0xB4C,std::uint8_t(255));
    f.put(f.target(),0x944,std::uintptr_t(1));f.put(f.target(),0x974,std::uintptr_t(1));
   }
   f.reader.trace=true;re4::HealthCaptureSample observation;const auto native=f.pool;
   CHECK(enemy_health_body_reference(f.reader,f.target(),&observation)==c.unit);CHECK(f.pool==native);
   const auto mask=observation.get(F::ReadMask);
   CHECK(bool(mask&(1u<<unsigned(F::Coefficient)))==needs_c);
   CHECK(bool(mask&(1u<<unsigned(F::BodyStatus)))==(known->family==Family::Common));
   for(const auto& read:f.reader.reads){
    if(known->family!=Family::Common)for(unsigned offset:{0xB8Cu,0xB4Cu,0x944u,0x974u})CHECK(read.base!=f.target()+offset);
    if(!needs_c){CHECK(read.base!=main_base+0xAF055C+2*4);CHECK(read.base!=main_base+0xAF1440+std::size_t(c.type-0x23)*8);}
   }
   f.put(main_base,c.vptr+0x28,main_base+c.update+4);CHECK(!f.unit()); // Exactupdatepair isrequired.
  }
 }
 // Currentactor118, notclassmodulelabel, selectscoefficient/rank semantics.
 for(unsigned type:{0x27u,0x31u,0x35u,0x38u}){
  Fixture f;f.put(f.target(),0,main_base+0xAE8170);f.put(main_base,0xAE8198,main_base+0xDF550);
  f.put(f.target(),0x118,std::uint8_t(type));f.put(f.target(),0x119,std::uint8_t(255));
  f.put(main_base,0xAF1440+(type-0x23)*8,main_base+0xAF055C);
  CHECK(f.unit()==(type==0x27?150u:127u));
 }
 // Directonehit andhitcount have no rank/mode requirement orcurrentHPrescale.
 for(unsigned vptr:{0xAE7F00u,0xAED668u}){
  Fixture f;f.put(f.target(),0,main_base+vptr);f.put(main_base,vptr+0x28,main_base+(vptr==0xAE7F00?0xD8A50:0x1B3610));
  f.state[0x84F0]=0;f.state[0x500C]=255;f.put(f.target(),0x3C2,std::int16_t(100));
  CHECK(f.unit()==(vptr==0xAE7F00?1500u:1u));
  for(auto& span:f.reader.spans)if(span.base==reinterpret_cast<std::uintptr_t>(f.state.data()))span.size=0x5035;
  CHECK(f.unit()==(vptr==0xAE7F00?1500u:1u)); // Missingunusedmode isnotanadmissionfailure.
  if(vptr==0xAED668){f.put(f.target(),0x119,std::uint8_t(10));CHECK(!f.unit());}
 }
 {
  Fixture f;f.put(f.target(),0,main_base+0xAE9F58);f.put(main_base,0xAE9F80,main_base+0x126FA0);
  f.put(f.target(),0x118,std::uint8_t(0x2F));f.state[0x5034]=21;f.state[0x84F0]=0;CHECK(f.unit()==85);
  f.put(f.target(),0,main_base+0xAE8170);f.put(main_base,0xAE8198,main_base+0xDF550);CHECK(!f.unit());
 }
 // Frozen pre-rank native results for ordinary/TMP damage. Group rows:
 //252defaultsubtypes,2(two-thirds),6(immune),A(eighth),D(thirty-second).
 const unsigned body[2][5][3][2][2]={
  {{{{150,150},{100,100}},{{84,84},{56,56}},{{69,69},{46,46}}},
   {{{101,101},{68,68}},{{57,57},{38,38}},{{47,47},{32,32}}},
   {{{0,0},{0,0}},{{0,0},{0,0}},{{0,0},{0,0}}},
   {{{19,19},{19,19}},{{11,11},{11,11}},{{9,9},{9,9}}},
   {{{5,5},{5,5}},{{3,3},{3,3}},{{3,3},{3,3}}}},
  {{{{210,105},{140,70}},{{117,58},{78,39}},{{96,48},{64,32}}},
   {{{70,35},{47,23}},{{39,19},{26,13}},{{32,16},{22,11}}},
   {{{0,0},{0,0}},{{0,0},{0,0}},{{0,0},{0,0}}},
   {{{27,13},{27,13}},{{15,7},{15,7}},{{13,6},{13,6}}},
   {{{7,3},{7,3}},{{4,2},{4,2}},{{4,2},{4,2}}}}
 };
 const float rank_factor[11]={2.f,1.8f,1.6f,1.4f,1.2f,1.f,.97f,.94f,.91f,.88f,.85f};
 for(unsigned subtype=0;subtype<=255;++subtype){
  CHECK(re4::enemy_health_reference_subtype_allowed(subtype));
  const unsigned group=subtype==2?1:subtype==6?2:subtype==0xA?3:subtype==0xD?4:0;
  for(unsigned index:{1u,2u,3u,4u,5u,6u,7u,8u,9u,10u,11u,12u,15u,17u,33u})
   for(unsigned status=0;status<3;++status)for(unsigned links=0;links<2;++links)
    for(unsigned phase:{0u,4u,15u,255u})for(unsigned rank=0;rank<=10;++rank){
     const auto raw=body[index==11][group][status][links][phase==4];
     const auto want=unsigned(float(raw)*rank_factor[rank]);
     CHECK(enemy_health_reference_damage(index,index==11?210:150,0x1E,6,rank,status,links!=0,phase,subtype)==want);
    }
 }
 for(unsigned subtype:{256u,65535u,std::numeric_limits<unsigned>::max()}){
  CHECK(!re4::enemy_health_reference_subtype_allowed(subtype));
  CHECK(!enemy_health_reference_damage(2,150,0x1E,6,10,2,false,15,subtype));
 }
 // Actualprovider exercises all256bytes/allguns/statuses/linkmasks/phases.
 {
  Fixture f;f.put(f.target(),0x118,std::uint8_t(0x1E));
  for(unsigned subtype=0;subtype<=255;++subtype){
   f.put(f.target(),0x119,std::uint8_t(subtype));
   const unsigned group=subtype==2?1:subtype==6?2:subtype==0xA?3:subtype==0xD?4:0;
   for(unsigned index:{1u,2u,3u,4u,5u,6u,7u,8u,9u,10u,11u,12u,15u,17u,33u}){
    f.state[0x5034]=std::uint8_t(index);f.put(main_base,0xAF055C+index*4,index==11?210:150);
    for(unsigned status=0;status<3;++status)for(unsigned mask=0;mask<4;++mask)for(unsigned phase:{0u,4u,15u}){
     f.put(f.target(),0xB8C,std::uint8_t(status));f.put(f.target(),0xB4C,std::uint8_t(phase));
     f.put(f.target(),0x944,mask&1?std::uintptr_t(1):std::uintptr_t(0));
     f.put(f.target(),0x974,mask&2?std::uintptr_t(1):std::uintptr_t(0));
     CHECK(f.unit()==unsigned(float(body[index==11][group][status][mask!=0][phase==4])*.85f));
    }
   }
  }
 }
 // Actual retained17 target: em1E subtype17(hex), status2, phase15(decimal).
 // Fixed1.0 reference is58; both normal and extra upgrades keep initialHPscale.
 {
  Fixture f;f.put(f.target(),0x118,std::uint8_t(0x1E));f.put(f.target(),0x119,std::uint8_t(0x17));
  f.put(f.target(),0xB4C,std::uint8_t(15));
  const auto native=f.pool;const auto stamp=enemy_health_current_weapon_stamp();
  CHECK(f.unit()==58);
  for(unsigned level:{5u,6u}){
   f.state[0x5037]=std::uint8_t(level);f.put(main_base,0x124D918+6,level==6?0xFFFFFFFFu:0u);
   CHECK(f.unit()==58&&enemy_health_current_weapon_stamp()==stamp&&f.pool==native);
  }
 }

 CHECK(enemy_health_reference_damage(2,150,0x10,6,10,2)==58);
 CHECK(enemy_health_reference_damage(12,15,0x31,6,10,0)==2); //0C is a hit-shape bonus, not Chicago weaponindex12.
 CHECK(enemy_health_reference_damage(2,150,0x10,6,10,1)==71);
 CHECK(enemy_health_reference_damage(2,150,0x10,6,10,0)==127);
 CHECK(enemy_health_reference_damage(11,210,0x10,6,10,2)==81);
 // Frozen native float32/truncation results, including all Normal ranks/statuses.
 const unsigned expected[3][11]={{300,270,240,210,180,150,145,141,136,132,127},
  {168,151,134,117,100,84,81,78,76,73,71},{138,124,110,96,82,69,66,64,62,60,58}};
 for(unsigned status=0;status<3;++status)for(unsigned rank=0;rank<=10;++rank)
  CHECK(enemy_health_reference_damage(2,150,0x10,5,rank,status)==expected[status][rank]);
 //84F0 is native unsignedbyte. Only0/1 branch to stochastic easy boosts;
 // every actual byte2..255 shares ordinary math, using observedrank.
 for(unsigned mode=2;mode<=255;++mode)for(unsigned status=0;status<3;++status)for(unsigned rank=0;rank<=10;++rank)
  CHECK(enemy_health_reference_damage(2,150,0x10,mode,rank,status)==expected[status][rank]);
 for(unsigned mode:{0u,1u,256u,65535u,std::numeric_limits<unsigned>::max()})CHECK(!enemy_health_reference_damage(2,150,0x10,mode,10,2));
 CHECK(enemy_health_reference_damage(2,150,0x10,6,9,2)==60);
 CHECK(!enemy_health_reference_damage(2,150,0x10,5,11,2));
 for(unsigned index:{1u,2u,3u,4u,5u,6u,7u,8u,9u,10u,11u,12u,15u,17u,33u})CHECK(enemy_health_reference_damage(index,150,0x10,6,10,2)==58);
 for(unsigned index:{0u,13u,14u,16u,19u,20u,22u,23u,46u,48u,255u})CHECK(!enemy_health_reference_damage(index,150,0x10,6,10,2));
 for(int coefficient:{-1,0,9999,29999,std::numeric_limits<int>::max()})CHECK(!enemy_health_reference_damage(2,coefficient,0x10,6,10,2));
 CHECK(!enemy_health_reference_damage(2,150,0x10,6,10,3));
 CHECK(!enemy_health_reference_damage(2,150,0x0F,6,10,2));
 CHECK(!enemy_health_reference_damage(2,150,0x50,6,10,2));
 CHECK(enemy_health_reference_damage(2,150,0x27,6,10,2)==69); // Native type mask excludes27.
 CHECK(enemy_health_reference_damage(2,150,0x43,6,10,2)==58); // High mask bits remain present.
 {
  Fixture f;CHECK(f.unit()==58);const auto stamp=enemy_health_current_weapon_stamp();CHECK(stamp==3);
  const auto unchanged_image=f.image,unchanged_pool=f.pool,unchanged_state=f.state;
  const auto unchanged_player=f.player;const auto unchanged_context=f.context,unchanged_weapon=f.weapon;
  CHECK(f.unit()==58&&enemy_health_current_weapon_stamp()==stamp);
  CHECK(f.image==unchanged_image&&f.pool==unchanged_pool&&f.state==unchanged_state);
  CHECK(f.player==unchanged_player&&f.context==unchanged_context&&f.weapon==unchanged_weapon);
  f.state[0x84F0]=5;
  for(unsigned status=0;status<3;++status)for(unsigned rank=0;rank<=10;++rank){
   f.state[0x500C]=std::uint8_t(rank);f.put(f.target(),0xB8C,std::uint8_t(status));
   CHECK(f.unit()==expected[status][rank]);
  }
  f.state[0x84F0]=6;f.state[0x500C]=10;f.put(f.target(),0xB8C,std::uint8_t(2));
  const auto native_pool=f.pool;
  for(unsigned level=0;level<7;++level){
   f.state[0x5037]=std::uint8_t(level);
   f.put(main_base,0x124D918+6,0xFFF80000u|level); // Irrelevant loaded rounds/native upgrade bits.
   f.weapon[0]=std::uint8_t(level); // Irrelevant weapon animation/ammo data.
   CHECK(f.unit()==58&&enemy_health_current_weapon_stamp()==stamp);
  }
  f.put(f.target(),0x3E0,std::uintptr_t(0xDEADBEEF)); // Last head/critical record never defines scale.
  CHECK(f.unit()==58);f.put(f.target(),0x3E0,std::uintptr_t(0));CHECK(f.pool==native_pool);
  f.state[0x5034]=11;f.put(main_base,0xAF055C+11*4,210);CHECK(f.unit()==81&&enemy_health_current_weapon_stamp()==12);
  f.state[0x5034]=13;CHECK(!f.unit()&&enemy_health_current_weapon_stamp()==14);
 }
 // Authenticated8F470 canonical selector0: real em10 subtypes1/3/4,
 // flag48D independent of selector0, and one reduction for either/both links.
 const unsigned linked[3][11]={{200,180,160,140,120,100,97,94,91,88,85},
  {112,100,89,78,67,56,54,52,50,49,47},{92,82,73,64,55,46,44,43,41,40,39}};
 const unsigned tmp[4][3][11]={
  {{420,378,336,294,252,210,203,197,191,184,178},{234,210,187,163,140,117,113,109,106,102,99},{192,172,153,134,115,96,93,90,87,84,81}},
  {{280,252,224,196,168,140,135,131,127,123,119},{156,140,124,109,93,78,75,73,70,68,66},{128,115,102,89,76,64,62,60,58,56,54}},
  {{210,189,168,147,126,105,101,98,95,92,89},{116,104,92,81,69,58,56,54,52,51,49},{96,86,76,67,57,48,46,45,43,42,40}},
  {{140,126,112,98,84,70,67,65,63,61,59},{78,70,62,54,46,39,37,36,35,34,33},{64,57,51,44,38,32,31,30,29,28,27}}};
 for(unsigned subtype:{0u,1u,3u,4u}){
  Fixture f;f.put(f.target(),0x119,std::uint8_t(subtype));CHECK(f.unit()==58);
  for(unsigned index:{1u,2u,3u,4u,5u,6u,7u,8u,9u,10u,11u,12u,15u,17u,33u}){
   f.state[0x5034]=std::uint8_t(index);f.put(main_base,0xAF055C+index*4,index==11?210:150);
   const auto stamp=enemy_health_current_weapon_stamp();
   for(unsigned mask=0;mask<4;++mask)for(unsigned phase:{0u,4u}){
    f.put(f.target(),0x944,mask&1?std::uintptr_t(0xDEADBEEF):std::uintptr_t(0));
    f.put(f.target(),0x974,mask&2?std::uintptr_t(0xBADF00D):std::uintptr_t(0));
    f.put(f.target(),0xB4C,std::uint8_t(phase));f.put(f.target(),0x48D,std::uint8_t(0xFF));
    f.state[0x84F0]=5;
    for(unsigned status=0;status<3;++status)for(unsigned rank=0;rank<=10;++rank){
     f.put(f.target(),0xB8C,std::uint8_t(status));f.state[0x500C]=std::uint8_t(rank);
     const unsigned want=index==11?tmp[(mask?1:0)+(phase==4?2:0)][status][rank]
      :mask?linked[status][rank]:expected[status][rank];
     CHECK(f.unit()==want&&enemy_health_current_weapon_stamp()==stamp);
    }
    f.state[0x84F0]=6;f.state[0x500C]=10;
    const auto pool_before=f.pool;const auto state_before=f.state;const auto image_before=f.image;
    for(unsigned level:{0u,6u}){
     f.state[0x5037]=std::uint8_t(level);f.put(main_base,0x124D918+6,level?0xFFFFFFFFu:0u);
     CHECK(f.unit()==(index==11?tmp[(mask?1:0)+(phase==4?2:0)][2][10]:mask?39u:58u));
    }
    CHECK(f.pool==pool_before);f.state=state_before;f.image=image_before;
   }
  }
 }
 // Native phase4 TMP halves instead of applying1000cap; rifle15 bypasses cap.
 for(unsigned index:{2u,11u,15u}){
  Fixture f;f.state[0x5034]=std::uint8_t(index);f.put(main_base,0xAF055C+index*4,9998);
  f.put(f.target(),0xB8C,std::uint8_t(0));f.put(f.target(),0xB4C,std::uint8_t(4));
  CHECK(f.unit()==(index==11?4249u:index==15?8498u:850u));
 }
 // Actual provider reads84F0/500C asbytes; subtype/class ownership is
 // unchanged. Mode6 is not required to carryrank10 in alternate campaigns.
 {
  Fixture f;
  for(unsigned mode=2;mode<=255;++mode)for(unsigned status=0;status<3;++status)for(unsigned rank=0;rank<=10;++rank){
   f.state[0x84F0]=std::uint8_t(mode);f.state[0x500C]=std::uint8_t(rank);f.put(f.target(),0xB8C,std::uint8_t(status));
   CHECK(f.unit()==expected[status][rank]);
  }
 }
 for(unsigned gate=0;gate<25;++gate){
  Fixture f;
  if(gate==0)f.put(f.target(),0,main_base+0xAE7058);
  if(gate==1)f.put(main_base,0xAE7078,main_base+0x8F474);
  if(gate==2)f.put(main_base,0xAE6EF8+8,std::uintptr_t(0));
  if(gate==3)f.put(f.target(),8,2u);
  if(gate==4)f.put(f.target(),0,std::uintptr_t(0));
  if(gate==5)f.put(f.target(),0x118,std::uint8_t(0x0F));
  if(gate==6)f.put(f.target(),0x118,std::uint8_t(0x50));
  if(gate==7)f.put(f.target(),0xB8C,std::uint8_t(3));
  if(gate==8)f.state[0x84F0]=1;
  if(gate==9)f.state[0x500C]=11;
  if(gate==10)f.put(f.target(),8,0x201u);
  if(gate==11)f.put(f.target(),8,0x10000001u);
  if(gate==12)f.put(f.target(),0x3C2,std::int16_t(0));
  if(gate==13)f.put(f.target(),0x3C4,std::int16_t(0));
  if(gate==14)f.put(main_base,0xAE6EF8+0x14,0x10C9u);
  if(gate==15)f.put(main_base,0xAE6EF8+0x10,257u);
  if(gate==16)f.put(main_base,0xAF055C+2*4,-1);
  if(gate==17)f.put(main_base,0xAF055C+2*4,0);
  if(gate==18)f.put(main_base,0xAF055C+2*4,9999);
  if(gate==19)f.put(main_base,0xB1AE08,std::uintptr_t(0));
  if(gate==20)f.put(main_base,0x113B398,std::uintptr_t(0));
  if(gate==21)f.put(reinterpret_cast<std::uintptr_t>(f.player.data()),0x938,std::uintptr_t(0));
  if(gate==22)f.put(reinterpret_cast<std::uintptr_t>(f.context.data()),0x38,std::uintptr_t(0));
  if(gate==23)f.state[0x5034]=255;
  if(gate==24){f.reader.spans.clear();readable.clear();}
  CHECK(!f.unit());
 }
 {
  Fixture f;f.put(f.target(),0x118,std::uint8_t(0x43));
  f.put(main_base,0xAF1440+(0x43-0x23)*8,main_base+0xAF09F4);
  f.put(main_base,0xAF09F4+2*4,30);CHECK(f.unit()==11);
  for(unsigned table=0;table<12;++table){
   const auto address=main_base+0xAF055C+table*0xC4;
   f.put(main_base,0xAF1440+(0x43-0x23)*8,address);f.put(address,2*4,30);CHECK(f.unit()==11);
  }
  f.put(main_base,0xAF1440+(0x43-0x23)*8,main_base+0xAF055C+12*0xC4);CHECK(!f.unit());
  f.put(main_base,0xAF1440+(0x43-0x23)*8,main_base+0xAF055C-0xC4);CHECK(!f.unit());
  f.put(main_base,0xAF1440+(0x43-0x23)*8,main_base+0xAF09F8);CHECK(!f.unit());
  f.put(main_base,0xAF1440+(0x43-0x23)*8,std::numeric_limits<std::uintptr_t>::max());CHECK(!f.unit());
  f.put(f.target(),0x118,std::uint8_t(0x4F));f.put(main_base,0xAF055C+2*4,150);CHECK(f.unit()==58);
  CHECK(!enemy_health_body_reference(f.reader,f.target()+1));
  CHECK(!enemy_health_body_reference(f.reader,reinterpret_cast<std::uintptr_t>(f.pool.data())));
 }
 // Shotgun spread queues a target's damage message only once. Its canonical
 // close normal-body unit has no pellet-count, upgrade or live distance input.
 for(unsigned index:{7u,8u,33u}){
  Fixture f;f.state[0x5034]=std::uint8_t(index);f.put(main_base,0xAF055C+index*4,150);
  const auto stamp=enemy_health_current_weapon_stamp();CHECK(stamp==index+1);
  for(float distance_squared:{1.f,35999999.f,36000000.f,1.e10f})for(unsigned level=0;level<7;++level){
   f.put(f.target(),0x3E0,reinterpret_cast<std::uintptr_t>(f.weapon.data()));
   f.put(reinterpret_cast<std::uintptr_t>(f.weapon.data()),0x28,distance_squared);
   f.state[0x5037]=std::uint8_t(level);f.put(main_base,0x124D918+6,0xFFF80000u|level);
   CHECK(f.unit()==58&&enemy_health_current_weapon_stamp()==stamp);
  }
 }
 // A fixed ordinary-body reference ignores freeze, spine/head and actual-hit bonuses.
 for(unsigned type:{0x2Cu,0x31u,0x35u,0x38u,0x39u}){
  const unsigned subtype=type==0x31?1:type==0x38?3:0;
  Fixture f;f.boss(type,subtype);
  const unsigned expected_unit=type==0x2C?21:type==0x31?2:12;
  CHECK(f.unit()==expected_unit);
  for(unsigned index:{1u,2u,3u,4u,5u,6u,7u,8u,9u,10u,11u,12u,15u,17u,33u}){
   f.state[0x5034]=std::uint8_t(index);
   const unsigned unit=type==0x2C?21:type==0x31?(index==11?1:2):(index==11?10:12);
   CHECK(f.unit()==unit);
  }
  f.state[0x5034]=2;
  const auto stamp=enemy_health_current_weapon_stamp();
  for(unsigned bonus:{0u,0x800u,0x200u,0x40800u}){
   f.put(f.target(),0x4C4,bonus);f.put(f.body(),0x4C4,bonus);
   f.put(f.target(),0x840,std::numeric_limits<float>::quiet_NaN());
   f.put(f.target(),0x3E0,std::uintptr_t(0xDEADBEEF));
   f.state[0x5037]=bonus?6:0;
   CHECK(f.unit()==expected_unit&&enemy_health_current_weapon_stamp()==stamp);
  }
  f.state[0x5034]=11;CHECK(f.unit()==(type==0x2C?21:type==0x31?1:10));
  f.state[0x5034]=12;CHECK(f.unit()==(type==0x2C?21:type==0x31?2:12)); // Chicago uses ordinary-body /5 too.
  for(unsigned selector:{0u,5u,12u}){
   f.put(f.target(),0x3E0,reinterpret_cast<std::uintptr_t>(f.weapon.data()));
   f.put(reinterpret_cast<std::uintptr_t>(f.weapon.data()),0x26,std::uint16_t(selector));
   CHECK(f.unit()==(type==0x2C?21:type==0x31?2:12));
  }
  if(type==0x31||type==0x38){
   f.state[0x5034]=2;f.put(f.target(),8,0x10000001u);CHECK(f.unit()==expected_unit);
  }else {f.put(f.target(),8,0x10000001u);CHECK(!f.unit());}
 }
 for(unsigned type:{0x2Cu,0x35u,0x39u}){
  Fixture f;f.boss(type,1);CHECK(f.unit()==(type==0x2C?21u:12u));
  f.put(f.target(),0x119,std::uint8_t(2));CHECK(!f.unit());
  f.put(f.target(),0x119,std::uint8_t(3));CHECK(!f.unit());
 }
 for(unsigned type:{0x31u,0x38u})for(unsigned gate=0;gate<10;++gate){
  Fixture f;f.boss(type,type==0x31?1:3);
  const unsigned link=type==0x31?0xBC4:0xAE4,back=type==0x31?0xBCC:0xAFC;
  if(gate==0)f.put(f.target(),link,std::uintptr_t(0));
  if(gate==1)f.put(f.target(),link,f.target());
  if(gate==2)f.put(f.target(),link,f.body()+1);
  if(gate==3)f.put(f.body(),back,std::uintptr_t(0));
  if(gate==4)f.put(f.body(),0x119,std::uint8_t(1));
  if(gate==5)f.put(f.body(),0x118,std::uint8_t(0x30));
  if(gate==6)f.put(f.body(),0,std::uintptr_t(1));
  if(gate==7)f.put(f.body(),8,0x201u);
  if(gate==8)f.put(f.target(),0x119,std::uint8_t(0));
  if(gate==9)f.put(f.target(),0x3C2,std::int16_t(0));
  CHECK(!f.unit());
 }
 for(unsigned type:{0x2Bu,0x2Fu,0x32u,0x3Fu}){
  Fixture f;f.put(f.target(),0x118,std::uint8_t(type));CHECK(!f.unit());
 }
 main_base=0;CHECK(!enemy_health_current_weapon_stamp());
 std::puts("enemy health fixed reference: native body arithmetic, tables, state and upgrade invariance PASS");
}
