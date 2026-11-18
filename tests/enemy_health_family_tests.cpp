#include "re4/enemy_health_families.hpp"
#include <cassert>
#include <cstdio>
int main(){
 using F=re4::EnemyHealthFamily;
 const auto d=[](F f,unsigned w,int c,unsigned t,unsigned rank=10,unsigned sub=0,unsigned max=1500){return re4::enemy_health_family_damage(f,w,c,t,6,rank,255,true,255,sub,max);};
 // Exact common class must not inherit a different handler from actor118.
 assert(re4::enemy_health_family_damage(F::Common,2,150,0x31,6,10,2,false,15,0x17,1500)==58);
 assert(d(F::Plain,2,150,0x22)==127);
 assert(d(F::Double,2,150,0x25)==255);
 assert(d(F::Plain,11,210,0x26)==178);
 assert(d(F::Plain,2,50,0x2D)==42);
 assert(d(F::HalfWeapons56,2,15,0x32)==12);
 assert(d(F::HalfWeapons56,5,15,0x32)==5);
 assert(d(F::HalfWeapons56,11,12,0x32)==10);
 assert(d(F::CloseHalf,2,15,0x3F)==5);
 assert(d(F::Five,2,150,0x3A)==637);
 assert(d(F::Five,11,210,0x3A)==892);
 assert(d(F::RifleTenth,15,10,0x4E)==0);
 assert(d(F::RifleTenth,15,10,0x4E,0)==2);
 assert(d(F::Fixed23,2,9999,0x23)==850);
 assert(d(F::Fixed23,5,9999,0x23)==8499);
 assert(d(F::Fixed27,2,9999,0x27)==1000);
 assert(d(F::Fixed27,5,9999,0x27)==9999);
 assert(d(F::Fixed29,2,9999,0x29)==850);
 assert(d(F::Fixed29,5,9999,0x29)==8499);
 assert(d(F::Fixed30,2,0,0x30)==8);
 assert(d(F::Fixed30,5,0,0x30)==42);
 assert(d(F::Fixed3B,2,0,0x3B)==200);
 assert(d(F::Fixed3B,5,0,0x3B)==1000);
 assert(d(F::Fixed3B,7,0,0x3B)==500);
 assert(d(F::Fixed3B,2,0,0x3B,10,1)==0);
 assert(d(F::Fixed3B,2,0,0x3B,10,2)==0);
 assert(d(F::Fixed2F,2,0,0x2F)==0);
 assert(d(F::Fixed2F,15,0,0x2F)==850);
 assert(d(F::Fixed2F,11,0,0x2F)==0);
 for(unsigned sub=0;sub<256;++sub){
  assert(d(F::Plain,2,150,0x22,10,sub)==127);
  assert(d(F::InstantKill,2,9999,0x18,10,sub)==1500);
  assert(d(F::InstantKill,2,0,0x18,0,sub,1000)==1000);
  assert(d(F::ScriptOnly,2,9999,0x21,10,sub)==0);
 }
 for(unsigned sub=0;sub<256;++sub){
  assert(d(F::HitCount,2,9999,0x3E,255,sub)==(sub<10?1u:0u));
 }
 assert(re4::enemy_health_family_damage(F::Fixed2F,21,0,0x2F,6,10,255,true,255,0,1500)==85);
 assert(re4::enemy_health_family_damage(F::InstantKill,2,0,0x18,0,255,255,true,255,0,1500)==1500);
 assert(re4::enemy_health_family_damage(F::Fixed30,2,0,0x30,0,10,255,true,255,0,1500)==8);
 for(const auto& c:re4::kEnemyHealthClasses){assert(re4::enemy_health_reference_class(c.vptr,c.update)==&c);assert(!re4::enemy_health_reference_class(c.vptr,c.update+4));}
 assert(!re4::enemy_health_reference_class(0,0));
 assert(d(F::Plain,2,150,0x22,10,256)==0);
 assert(d(F::Plain,2,9999,0x22)==0);
 assert(d(F::InstantKill,2,0,0x18,10,0,0)==0);
 assert(d(F::InstantKill,2,0,0x18,10,0,32768)==0);
 // New multiplied families must not inherit the common handler's local9999cap.
 assert(d(F::Double,2,9998,0x25,0)==39992);
 assert(d(F::Five,2,9998,0x3A,0)==99980);
 assert(d(F::Fixed23,5,0,0x23,0)==19998);
 // Actual118 determines generic rank; the module label does not substitute it.
 assert(d(F::Plain,2,150,0x27)==150);
 std::puts("all native health families: canonical fixed-body transforms PASS");
}
