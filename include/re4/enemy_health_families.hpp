#pragma once
#include "re4/enemy_health_reference.hpp"
#include <cstdint>
namespace re4 {
enum class EnemyHealthFamily : unsigned {Unknown,Common,LegacyBoss,Plain,Double,HalfWeapons56,CloseHalf,Five,RifleTenth,Fixed23,Fixed27,Fixed29,Fixed30,Fixed3B,Fixed2F,InstantKill,ScriptOnly,HitCount};
struct EnemyHealthClass {std::uint32_t vptr,update;EnemyHealthFamily family;};
inline constexpr EnemyHealthClass kEnemyHealthClasses[]={
 {0xAE7050,0x8F470,EnemyHealthFamily::Common},
 {0xAE7F00,0xD8A50,EnemyHealthFamily::InstantKill},
 {0xAE8038,0xDC800,EnemyHealthFamily::ScriptOnly},
 {0xAE8170,0xDF550,EnemyHealthFamily::Plain},
 {0xAE8450,0xE6350,EnemyHealthFamily::Fixed23},
 {0xAE85B0,0xE87A0,EnemyHealthFamily::InstantKill},
 {0xAE8668,0xE9CF0,EnemyHealthFamily::Double},
 {0xAE8938,0xEDA10,EnemyHealthFamily::Plain},
 {0xAE8A18,0xEE9D0,EnemyHealthFamily::Fixed27},
 {0xAE8B10,0xF0BF0,EnemyHealthFamily::InstantKill},
 {0xAE8C08,0xF2650,EnemyHealthFamily::Fixed29},
 {0xAE8D38,0xF50F0,EnemyHealthFamily::InstantKill},
 {0xAE8E28,0xF7BF0,EnemyHealthFamily::ScriptOnly},
 {0xAE9438,0x106890,EnemyHealthFamily::LegacyBoss},
 {0xAE99D0,0x116230,EnemyHealthFamily::Plain},
 {0xAE9EA8,0x126010,EnemyHealthFamily::InstantKill},
 {0xAE9F58,0x126FA0,EnemyHealthFamily::Fixed2F},
 {0xAEA0E0,0x12B120,EnemyHealthFamily::Fixed30},
 {0xAEA170,0x12C0E0,EnemyHealthFamily::LegacyBoss},
 {0xAEA7E0,0x139BA0,EnemyHealthFamily::HalfWeapons56},
 {0xAEAB58,0x147DC0,EnemyHealthFamily::Fixed30},
 {0xAEAFD0,0x148E20,EnemyHealthFamily::LegacyBoss},
 {0xAEB618,0x154910,EnemyHealthFamily::Plain},
 {0xAEBC60,0x1629F0,EnemyHealthFamily::LegacyBoss},
 {0xAEBF38,0x16A5C0,EnemyHealthFamily::LegacyBoss},
 {0xAEC530,0x180EB0,EnemyHealthFamily::Five},
 {0xAEC720,0x1855E0,EnemyHealthFamily::Fixed3B},
 {0xAEC918,0x187780,EnemyHealthFamily::Plain},
 {0xAECCD8,0x18D6D0,EnemyHealthFamily::ScriptOnly},
 {0xAED668,0x1B3610,EnemyHealthFamily::HitCount},
 {0xAECE20,0x18FED0,EnemyHealthFamily::CloseHalf},
 {0xAED178,0x1A2E20,EnemyHealthFamily::RifleTenth},
};
inline const EnemyHealthClass* enemy_health_reference_class(std::uint32_t vptr,std::uint32_t update){
 for(const auto& c:kEnemyHealthClasses)if(c.vptr==vptr&&c.update==update)return &c;
 return nullptr;
}
inline bool enemy_health_family_needs_coefficient(EnemyHealthFamily f){
 using F=EnemyHealthFamily;
 return f==F::Common||f==F::LegacyBoss||f==F::Plain||f==F::Double||f==F::HalfWeapons56||f==F::CloseHalf||f==F::Five||f==F::RifleTenth;
}
// Fixed/script handlers do not consume the firearm power calculator's mode.
inline bool enemy_health_family_needs_rank(EnemyHealthFamily f){
 return f!=EnemyHealthFamily::Unknown&&f!=EnemyHealthFamily::InstantKill&&f!=EnemyHealthFamily::ScriptOnly&&f!=EnemyHealthFamily::HitCount;
}
inline bool enemy_health_family_weapon_allowed(EnemyHealthFamily f,unsigned weapon){
 return enemy_health_reference_weapon_allowed(weapon)||(f==EnemyHealthFamily::Fixed2F&&weapon==21);
}
// Canonical ordinary-close, unfrozen, unbonused body. Fixed random handlers use
// their nominal1000 outcome (or normal low10 for30/34), not the last RNG result.
// Instant-kill units use full maximumHP so an injured target never changes units.
// C/power, body fields and upgrade level never leak into unrelated handlers.
inline unsigned enemy_health_family_damage(EnemyHealthFamily family,unsigned weapon,int coefficient,unsigned type,unsigned mode,unsigned rank,unsigned status,bool linked,unsigned phase,unsigned subtype,unsigned maximum){
 using F=EnemyHealthFamily;
 if(type<0x10||type>0x4F||subtype>255||!enemy_health_family_weapon_allowed(family,weapon))return 0;
 if(family==F::Unknown||family==F::ScriptOnly)return 0;
 if(family==F::InstantKill)return maximum>0&&maximum<=32767?maximum:0;
 if(family==F::HitCount)return subtype<10?1u:0u;
 if(rank>10)return 0;
 if(family==F::LegacyBoss)
  return enemy_health_reference_damage(weapon,coefficient,type,mode,rank,status,linked,phase,subtype);
 int damage=0;
 if(family==F::Common){
  // Select the common handler's arithmetic with a neutral rank multiplier1.
  // Actor118 selects table/rank, never a different virtual handler's formula.
  damage=int(enemy_health_reference_damage(weapon,coefficient,0x10,mode,5,status,linked,phase,subtype));
 }else if(enemy_health_family_needs_coefficient(family)){
  if(coefficient<=0||coefficient>=9999||mode<2||mode>255)return 0;
  damage=coefficient;
  switch(family){
   case F::Plain:break;
   case F::Double:damage*=2;break;
   case F::HalfWeapons56:if(weapon==5||weapon==6)damage/=2;break;
   case F::CloseHalf:damage=int(float(damage)*.5f);break;
   case F::Five:damage*=5;break;
   case F::RifleTenth:if(weapon==15)damage/=10;break; // Other native x10gun0D is outside allowed set.
   default:return 0;
  }
 }else{
  const bool shotgun=weapon==7||weapon==8||weapon==33;
  const bool heavy=weapon==5||weapon==6||weapon==15;
  switch(family){
   case F::Fixed23:damage=(weapon==2||weapon==11)?1000:9999;break;
   case F::Fixed27:damage=(weapon==5||weapon==6||weapon==9||weapon==10||weapon==15)?9999:1000;break;
   case F::Fixed29:damage=(heavy||shotgun)?9999:1000;break;
   case F::Fixed30:damage=heavy?50:10;break;
   case F::Fixed3B:if(subtype==1||subtype==2)return 0;damage=heavy?1000:shotgun?500:200;break;
   case F::Fixed2F:
    if(weapon>0x2B)damage=1000;
    else if((0x9C028131E7Full>>weapon)&1ull)damage=1;
    else if(shotgun)damage=1;
    else if(weapon==21)damage=100;
    else damage=1000;
    break;
   default:return 0;
  }
 }
 // These handlers call1D1520 directly or through1D2030. Unlike common8F470,
 // they have no local9999 clamp after their own multipliers/fixed damage.
 if(enemy_health_reference_ranked_type(type)){
  const float factor=rank<6?2.f-float(rank)*0.20000000298023224f
   :1.f-float(rank-5)*0.029999999329447746f;
  damage=int(float(damage)*factor);
 }
 return damage>0?unsigned(damage):0;
}
}
