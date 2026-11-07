#include "re4/enemy_health.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>
#define CHECK(c) do{if(!(c)){std::fprintf(stderr,"enemy health line %d: %s\n",__LINE__,#c);std::exit(1);}}while(false)
struct Color {unsigned r,g,b,a;};
struct Draw {
 unsigned prepares=0,begins=0,ends=0,blends=0,formats=0;std::vector<Color> colors;std::vector<re4::ViewVec3> points;
 re4::ReticleDrawBackend backend(){return {this,
  [](void* p){++static_cast<Draw*>(p)->prepares;},
  [](void* p,unsigned a,unsigned b,unsigned c,unsigned d){auto& s=*static_cast<Draw*>(p);CHECK(a==1&&d==0);CHECK((s.blends==0&&b==4&&c==5)||(s.blends==1&&b==1&&c==0));++s.blends;},
  [](void* p,unsigned a,unsigned b,unsigned c,unsigned d,unsigned e){auto& s=*static_cast<Draw*>(p);CHECK(a==0&&b==9&&c==1&&e==0);CHECK(d==(s.formats?3u:4u));++s.formats;},
  [](void* p,unsigned a,unsigned b,unsigned c){CHECK(a==0x98&&b==0&&c==4);++static_cast<Draw*>(p)->begins;},
  [](void* p,float x,float y,float z){static_cast<Draw*>(p)->points.push_back({x,y,z});},
  [](void* p,unsigned r,unsigned g,unsigned b,unsigned a){static_cast<Draw*>(p)->colors.push_back({r,g,b,a});},
  [](void* p,unsigned token){CHECK(token==0x34);++static_cast<Draw*>(p)->ends;}};}
};
int main(){
 using namespace re4;
 CHECK(enemy_health_band(100,100)==EnemyHealthBand::Green);
 CHECK(enemy_health_band(51,100)==EnemyHealthBand::Green);
 CHECK(enemy_health_band(50,100)==EnemyHealthBand::Yellow);
 CHECK(enemy_health_band(26,100)==EnemyHealthBand::Yellow);
 CHECK(enemy_health_band(25,100)==EnemyHealthBand::Red);
 CHECK(enemy_health_band(1,100)==EnemyHealthBand::Red);
 CHECK(enemy_health_band(1,3)==EnemyHealthBand::Yellow);
 CHECK(enemy_health_target(1,0x10,100,100));
 for(unsigned flags:{0u,0x201u,0x10000001u})CHECK(!enemy_health_target(flags,0x10,100,100));
 for(unsigned type:{0u,1u,0xDu,0xFu,0x50u,255u})CHECK(!enemy_health_target(1,type,100,100));
 CHECK(!enemy_health_target(1,0x10,0,100)&&!enemy_health_target(1,0x10,10,0));
 CHECK(enemy_health_pool_member(0x1000,4,0x10C8,0x20C8));
 for(auto p:{std::uintptr_t(0),std::uintptr_t(0x1000),std::uintptr_t(0x1001),std::uintptr_t(0x1000+4*0x10C8)})CHECK(!enemy_health_pool_member(0x1000,4,0x10C8,p));
 CHECK(!enemy_health_pool_member(0x1000,257,0x10C8,0x20C8));
 CHECK(!enemy_health_pool_member(0x1000,4,0x10C9,0x20C9));
 for(int hp:{100,50,25,1,150}){
  Draw draw;CHECK(draw_enemy_health(draw.backend(),{256,100},hp,100));
  CHECK(draw.prepares==1&&draw.begins==1&&draw.ends==1&&draw.blends==2&&draw.formats==2);
  CHECK(draw.colors.size()==4&&draw.points.size()==4);
  const auto color=draw.colors.back();CHECK(color.a==102);
  if(hp>50)CHECK(color.g>color.r&&color.g>color.b);
  else if(hp>25)CHECK(color.r>color.b&&color.g>color.b);
  else CHECK(color.r>color.g&&color.r>color.b);
  const float width=draw.points[1].x-draw.points[0].x;
  CHECK(std::fabs(width-18.f*std::min(hp,100)/100)<.001f);
  CHECK(draw.points[2].y-draw.points[0].y==2);
 }
 for(auto point:{ViewHudPoint{0,0},ViewHudPoint{511,447},ViewHudPoint{std::numeric_limits<float>::quiet_NaN(),100}}){Draw draw;CHECK(!draw_enemy_health(draw.backend(),point,10,100)&&draw.prepares==0);}
 for(auto hp:{0,-1}){Draw draw;CHECK(!draw_enemy_health(draw.backend(),{256,100},hp,100)&&draw.prepares==0);}
 Draw draw;CHECK(!draw_enemy_health(draw.backend(),{256,100},10,0)&&draw.prepares==0);
 auto missing=draw.backend();missing.color=nullptr;CHECK(!draw_enemy_health(missing,{256,100},10,100)&&draw.prepares==0);
 std::puts("enemy health: thresholds, native pool identity, bounded alpha quads and state restoration PASS");
}
