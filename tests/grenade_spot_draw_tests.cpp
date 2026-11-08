#include "re4/grenade_spot_draw.hpp"
#include "re4/grenade_focus.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <utility>
#include <vector>

#define CHECK(c) do { if(!(c)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#c);std::exit(1);} } while(0)
using namespace re4;
struct Command {char kind;float a=0,b=0,c=0,d=0,e=0;};
struct Recorder {
    std::vector<Command> commands;
    void add(char k,float a=0,float b=0,float c=0,float d=0,float e=0){commands.push_back({k,a,b,c,d,e});}
    ReticleDrawBackend backend(){return {this,
        [](void* p){static_cast<Recorder*>(p)->add('H');},
        [](void* p,unsigned a,unsigned b,unsigned c,unsigned d){static_cast<Recorder*>(p)->add('L',float(a),float(b),float(c),float(d));},
        [](void* p,unsigned a,unsigned b,unsigned c,unsigned d,unsigned e){static_cast<Recorder*>(p)->add('F',float(a),float(b),float(c),float(d),float(e));},
        [](void* p,unsigned a,unsigned b,unsigned c){static_cast<Recorder*>(p)->add('B',float(a),float(b),float(c));},
        [](void* p,float x,float y,float z){static_cast<Recorder*>(p)->add('P',x,y,z);},
        [](void* p,unsigned r,unsigned g,unsigned b,unsigned a){static_cast<Recorder*>(p)->add('C',float(r),float(g),float(b),float(a));},
        [](void* p,unsigned value){static_cast<Recorder*>(p)->add('E',float(value));}};}
};
const ViewMatrix3x4 view{{{1,0,0,0},{0,.6f,.8f,-1.2f},{0,.8f,-.6f,-1000}}};
// Native27A140 perspective: near100, far100000, cot(fovy/2)=1,
// aspect1. m22=-near/(far-near), m23=-near*far/(far-near).
// Its near/far NDC depth endpoints are -1 and0, respectively.
const ViewMatrix4x4 projection{{{1,0,0,0},{0,1,0,0},
    {0,0,-.001001001f,-100.1001f},{0,0,-1,0}}};
bool near(float a,float b){return std::fabs(a-b)<.002f;}
int main(){
    Recorder recorder;const auto backend=recorder.backend();
    const GrenadeLanding landing{{0,0,0},{0,1,0},1,1};
    CHECK(draw_grenade_landing_spot(backend,view,projection,landing));
    const auto& commands=recorder.commands;
    CHECK(commands.size()==645);
    CHECK(commands[0].kind=='H');
    CHECK(commands[1].kind=='L'&&commands[1].a==1&&commands[1].b==4&&commands[1].c==5);
    CHECK(commands[2].kind=='F'&&commands[2].d==4);
    CHECK(commands[643].kind=='L'&&commands[643].a==1&&commands[643].b==1&&commands[643].c==0);
    CHECK(commands[644].kind=='F'&&commands[644].d==3);
    const unsigned rgb[5][3]={{255,12,8},{249,21,16},{244,30,24},{238,39,32},{232,48,40}};
    const unsigned alpha[5]={92,59,27,7,0};
    unsigned last_inner=93;
    for(unsigned ring=0;ring<4;++ring){
        const unsigned first=3+ring*160;
        const unsigned inner=unsigned(commands[first+2].d),outer=unsigned(commands[first+4].d);
        CHECK(inner<last_inner&&outer<inner);last_inner=inner;
        if(ring==0)CHECK(inner==92);
        if(ring==3)CHECK(outer==0);
        for(unsigned segment=0;segment<16;++segment){
            const unsigned start=first+segment*10;
            CHECK(commands[start].kind=='B'&&commands[start].a==0x98&&commands[start].b==0&&commands[start].c==4);
            CHECK(commands[start+9].kind=='E'&&commands[start+9].a==0x34);
            for(unsigned vertex=0;vertex<4;++vertex){
                const auto& p=commands[start+1+2*vertex];const auto& c=commands[start+2+2*vertex];
                CHECK(p.kind=='P'&&std::isfinite(p.a)&&std::isfinite(p.b)&&p.a>=0&&p.a<=512&&p.b>=0&&p.b<=448&&p.c==0);
                const unsigned color_ring=ring+vertex%2;
                CHECK(c.kind=='C'&&c.a==rgb[color_ring][0]&&c.b==rgb[color_ring][1]&&c.c==rgb[color_ring][2]);
                CHECK(c.d==alpha[color_ring]);
                CHECK(c.d==float(vertex%2?outer:inner));
            }
            const auto& a=commands[start+1];const auto& b=commands[start+3];
            const auto& c=commands[start+5];const auto& d=commands[start+7];
            const float area0=(b.a-a.a)*(c.b-a.b)-(b.b-a.b)*(c.a-a.a);
            const float area1=(b.a-c.a)*(d.b-c.b)-(b.b-c.b)*(d.a-c.a);
            if(ring==0)CHECK(std::fabs(area0)<.001f);else CHECK(area0<-.001f);
            CHECK(area1<-.001f); // Valid same-winding triangle after strip parity.
        }
    }
    CHECK(near(commands[4].a,256)&&near(commands[4].b,224));
    // A more saturated, denser center retains the transparent outer edge.
    CHECK(commands[5].a>commands[487].a&&commands[5].b<commands[487].b&&commands[5].c<commands[487].c);
    CHECK(commands[5].d==92&&commands[487].d==0);
    // The outer x-axis point is at world x=200, biased two units off floor.
    CHECK(near(commands[486].a,307.28204f)&&near(commands[486].b,224));
    const float flat_x=commands[486].a;
    auto translated=view;translated.m[0][3]=100;
    recorder.commands.clear();CHECK(draw_grenade_landing_spot(backend,translated,projection,landing));
    CHECK(recorder.commands[486].a>flat_x+25);
    recorder.commands.clear();CHECK(draw_grenade_landing_spot(backend,view,projection,landing,200,.5f));
    CHECK(recorder.commands[5].d==46);
    // A tilted floor changes projected geometry using its own tangent plane.
    recorder.commands.clear();auto slope=landing;slope.normal={0,.8f,.6f};
    CHECK(draw_grenade_landing_spot(backend,view,projection,slope));
    CHECK(!near(recorder.commands[486].b,224));
    recorder.commands.clear();
    for(unsigned mode=0;mode<14;++mode){
        auto v=view;auto p=projection;auto hit=landing;float radius=200,opacity=1;
        if(mode==0)radius=0;
        if(mode==1)radius=1e20f;
        if(mode==2)opacity=0;
        if(mode==3)opacity=1.01f;
        if(mode==4)opacity=std::numeric_limits<float>::quiet_NaN();
        if(mode==5)hit.position.x=std::numeric_limits<float>::infinity();
        if(mode==6)hit.normal={0,0,0};
        if(mode==7)hit.normal={0,-1,0};
        if(mode==8)hit.position.x=2000; // Entire disc is outside the viewport.
        if(mode==9)hit.position.z=-1700; // Some ring vertices cross the camera plane.
        if(mode==10)p.m[0][0]=std::numeric_limits<float>::quiet_NaN();
        if(mode==11)v.m[1][1]=std::numeric_limits<float>::infinity();
        if(mode==12)v={};
        if(mode==13)p={};
        CHECK(!draw_grenade_landing_spot(backend,v,p,hit,radius,opacity));CHECK(recorder.commands.empty());
    }
    for(unsigned i=0;i<7;++i){
        auto broken=backend;
        if(i==0)broken.prepare_hud=nullptr;
        if(i==1)broken.blend=nullptr;
        if(i==2)broken.vertex_format=nullptr;
        if(i==3)broken.begin=nullptr;
        if(i==4)broken.position=nullptr;
        if(i==5)broken.color=nullptr;
        if(i==6)broken.end=nullptr;
        CHECK(!draw_grenade_landing_spot(broken,view,projection,landing));CHECK(recorder.commands.empty());
    }
    // A visible center remains visible as the outer disc crosses the viewport.
    // Keep its actual projected coordinates; native HUD viewport clipping trims
    // the geometry, rather than moving a world marker onto the screen edge.
    auto late=landing;late.position.x=-850;
    CHECK(draw_grenade_landing_spot(backend,view,projection,late));
    CHECK(recorder.commands[4].a>0&&recorder.commands[4].a<64);
    bool outside=false;
    for(const auto& c:recorder.commands)if(c.kind=='P')outside|=c.a<0;
    CHECK(outside);recorder.commands.clear();
    for(unsigned edge=0;edge<4;++edge){
        auto partial=view;
        if(edge<2)partial.m[0][3]=edge?940.f:-940.f;
        else partial.m[1][3]+=(edge==2?970.f:-970.f);
        ViewHudPoint center;
        CHECK(project_view_point(partial,projection,{0,2,0},center));
        CHECK(draw_grenade_landing_spot(backend,partial,projection,landing));
        CHECK(near(recorder.commands[4].a,center.x)&&near(recorder.commands[4].b,center.y));
        bool crosses=false;
        for(const auto& c:recorder.commands)if(c.kind=='P')crosses|=c.a<0||c.a>512||c.b<0||c.b>448;
        CHECK(crosses);recorder.commands.clear();
    }
    // Keep x/y inside the viewport to independently exercise native GX depth.
    // y=2 bias contributes+1.6 to view z, so these circles are centered at
    // depths50 (before near),100 (crossing near),100500 (past far),100000
    // (crossing far). A clipped last ring must leave the stream untouched.
    for(const auto& sample:std::vector<std::pair<float,float>>{
            {-51.6f,1},{-101.6f,1},{-100501.6f,200},{-100001.6f,200}}){
        auto depth=view;depth.m[2][3]=sample.first;
        CHECK(!draw_grenade_landing_spot(backend,depth,projection,landing,sample.second));
        CHECK(recorder.commands.empty());
    }
    // Entire circles just inside each depth plane remain drawable.
    for(const auto& sample:std::vector<std::pair<float,float>>{{-103.6f,1},{-99701.6f,200}}){
        auto depth=view;depth.m[2][3]=sample.first;
        CHECK(draw_grenade_landing_spot(backend,depth,projection,landing,sample.second));
        CHECK(recorder.commands.size()==645);recorder.commands.clear();
    }
    {
        GrenadeAimZoom zoom;
        CHECK(zoom.update(1,100,true,45)==45);
        CHECK(near(zoom.update(1,960100,true,45),55.16999f));
        CHECK(near(zoom.update(1,1920100,true,45),63.41959f));
        CHECK(near(zoom.update(1,3840100,true,45),67.60092f));
        CHECK(zoom.update(1,3840100,true,45)==zoom.update(1,3840100,true,45));
        CHECK(zoom.update(1,3840100,true,80)==80); // Never narrow a wider native lens.
    }
    {
        GrenadeAimZoom zoom;
        zoom.update(1,100,true,45);CHECK(near(zoom.update(1,19200100,true,45),68));
        CHECK(near(zoom.update(1,20160100,false,45),57.83001f));
        CHECK(near(zoom.update(1,23040100,false,45),45.39908f));
        CHECK(zoom.update(2,23040100,true,45)==45); // New actor, no inherited zoom.
        zoom.update(2,24000100,true,45);
        CHECK(zoom.update(2,200,true,45)==45); // Nonmonotonic clock resets safely.
        CHECK(zoom.update(2,19200201,true,45)==45); // A suspended scene starts fresh.
    }
    {
        GrenadeAimZoom a,b;a.update(1,100,true,45);b.update(1,100,true,45);
        const float one=a.update(1,3840100,true,45);float many=0;
        for(unsigned i=1;i<=20;++i)many=b.update(1,100+i*192000,true,45);
        CHECK(near(one,many)); // Elapsed time, independent of frame rate.
        b.reset();CHECK(b.update(1,3840100,true,45)==45);
        CHECK(b.update(0,3840100,true,45)==45);
        CHECK(std::isnan(b.update(1,3840100,true,std::numeric_limits<float>::quiet_NaN())));
        CHECK(b.update(1,3840100,true,45)==45);
    }
    std::puts("grenade spot draw tests: PASS");
}
