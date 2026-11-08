#define main reticle_baseline_main
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#include "reticle_draw_tests.cpp"
#pragma GCC diagnostic pop
#undef main
#include "re4/weapon_mist.hpp"
int main(){
    Recorder recorder;const auto backend=recorder.backend();
    CHECK(!re4::draw_weapon_mist(backend,{300,412},100,0,1));
    CHECK(recorder.commands.empty());
    CHECK(re4::draw_weapon_mist(backend,{300,412},100,1,1));
    float left=512,right=0,top=448,bottom=0;unsigned peak=0,transparent=0,begins=0,ends=0;
    for(const auto& c:recorder.commands){
        if(c.kind=='P'){CHECK(c.a>=0&&c.a<=512&&c.b>=0&&c.b<=448);left=std::fmin(left,c.a);right=std::fmax(right,c.a);top=std::fmin(top,c.b);bottom=std::fmax(bottom,c.b);}
        if(c.kind=='C'){peak=std::max(peak,unsigned(c.d));transparent+=c.d==0;CHECK(c.a==155&&c.b==153&&c.c==149);}
        if(c.kind=='B'){++begins;CHECK(c.a==0x90&&c.c==24*4*6);}
        if(c.kind=='E')++ends;
    }
    CHECK(left<220&&right>380&&top<405&&bottom>430&&peak==46&&transparent>0);
    CHECK(begins==1&&ends==1);
    recorder.commands.clear();CHECK(re4::draw_weapon_mist(backend,{300,412},100,.5f,1.15f));
    unsigned faded_peak=0;float wider_left=512;
    for(const auto& c:recorder.commands){if(c.kind=='C')faded_peak=std::max(faded_peak,unsigned(c.d));if(c.kind=='P')wider_left=std::fmin(wider_left,c.a);}
    CHECK(faded_peak==23&&wider_left<left);
    recorder.commands.clear();CHECK(!re4::draw_weapon_mist(backend,{300,412},100,1,3));CHECK(recorder.commands.empty());
    auto broken=backend;broken.color=nullptr;CHECK(!re4::draw_weapon_mist(broken,{300,412},100,1,1));CHECK(recorder.commands.empty());
}
