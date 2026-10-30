#include "re4/reticle_draw.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

#define CHECK(c) do { if(!(c)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#c);std::exit(1);} } while(0)
struct Command { char kind; float a,b,c,d,e; };
struct Recorder {
    std::vector<Command> commands;
    void add(char kind,float a=0,float b=0,float c=0,float d=0,float e=0){commands.push_back({kind,a,b,c,d,e});}
    re4::ReticleDrawBackend backend(){return {this,
        [](void* p){static_cast<Recorder*>(p)->add('H');},
        [](void* p,unsigned a,unsigned b,unsigned c,unsigned d){static_cast<Recorder*>(p)->add('L',float(a),float(b),float(c),float(d));},
        [](void* p,unsigned a,unsigned b,unsigned c,unsigned d,unsigned e){static_cast<Recorder*>(p)->add('F',float(a),float(b),float(c),float(d),float(e));},
        [](void* p,unsigned a,unsigned b,unsigned c){static_cast<Recorder*>(p)->add('B',float(a),float(b),float(c));},
        [](void* p,float x,float y,float z){static_cast<Recorder*>(p)->add('P',x,y,z);},
        [](void* p,unsigned r,unsigned g,unsigned b,unsigned a){static_cast<Recorder*>(p)->add('C',float(r),float(g),float(b),float(a));},
        [](void* p,unsigned format){static_cast<Recorder*>(p)->add('E',float(format));}};}
};
void command(const Command& c,char kind,float a=0,float b=0,float d=0,float e=0,float f=0){
    CHECK(c.kind==kind&&c.a==a&&c.b==b&&c.c==d&&c.d==e&&c.e==f);
}
void quad(Recorder& recorder,unsigned offset,float left,float top,float right,float bottom){
    auto& c=recorder.commands;
    command(c.at(offset++),'B',0x98,0,4);
    for(const auto& p:std::vector<re4::ViewHudPoint>{{left,top},{right,top},{left,bottom},{right,bottom}}){
        command(c.at(offset++),'P',p.x,p.y,0);
        command(c.at(offset++),'C',255,255,255,128);
    }
    command(c.at(offset),'E',0x34);
}
int main(){
    using namespace re4;
    Recorder recorder;auto backend=recorder.backend();
    CHECK(draw_reticle(backend,{256,224},1,0));
    CHECK(recorder.commands.size()==15);
    command(recorder.commands[0],'H');
    command(recorder.commands[1],'L',1,4,5,0);
    command(recorder.commands[2],'F',0,9,1,4,0);
    quad(recorder,3,255,223,257,225);
    command(recorder.commands[13],'L',1,1,0,0);
    command(recorder.commands[14],'F',0,9,1,3,0);
    // Odd-sized dots stay centered exactly, including half-pixel edges.
    recorder.commands.clear();CHECK(draw_reticle(backend,{256,224},1,1));
    quad(recorder,3,254.5f,222.5f,257.5f,225.5f);
    recorder.commands.clear();CHECK(draw_reticle(backend,{256.25f,224.75f},1,2));
    quad(recorder,3,254.25f,222.75f,258.25f,226.75f);
    recorder.commands.clear();CHECK(draw_reticle(backend,{256,224},2,0));
    CHECK(recorder.commands.size()==45);
    quad(recorder,3,248,223.5f,254,224.5f);
    quad(recorder,13,258,223.5f,264,224.5f);
    quad(recorder,23,255.5f,216,256.5f,222);
    quad(recorder,33,255.5f,226,256.5f,232);
    command(recorder.commands[43],'L',1,1,0,0);
    command(recorder.commands[44],'F',0,9,1,3,0);
    for(unsigned size=0;size<3;++size){
        recorder.commands.clear();CHECK(draw_reticle(backend,{256,224},2,size));
        const float length=6+float(size)*2,thickness=size?2.0f:1.0f,gap=2+float(size);
        quad(recorder,3,256-gap-length,224-thickness/2,256-gap,224+thickness/2);
        quad(recorder,13,256+gap,224-thickness/2,256+gap+length,224+thickness/2);
        quad(recorder,23,256-thickness/2,224-gap-length,256+thickness/2,224-gap);
        quad(recorder,33,256-thickness/2,224+gap,256+thickness/2,224+gap+length);
    }
    recorder.commands.clear();
    CHECK(!draw_reticle(backend,{256,224},0,0));
    CHECK(!draw_reticle(backend,{256,224},3,0));
    CHECK(!draw_reticle(backend,{256,224},1,3));
    CHECK(!draw_reticle(backend,{std::numeric_limits<float>::quiet_NaN(),224},1,0));
    CHECK(!draw_reticle(backend,{256,std::numeric_limits<float>::infinity()},1,0));
    CHECK(!draw_reticle(backend,{-1,224},1,0));
    CHECK(!draw_reticle(backend,{512,224},1,0));
    CHECK(!draw_reticle(backend,{256,448},1,0));
    CHECK(!draw_reticle(backend,{1,224},2,0));
    CHECK(!draw_reticle(backend,{256,7},2,0));
    CHECK(recorder.commands.empty());
    // Missing native callback must not partially mutate renderer state.
    for(unsigned i=0;i<7;++i){
        auto broken=backend;
        if(i==0)broken.prepare_hud=nullptr;
        if(i==1)broken.blend=nullptr;
        if(i==2)broken.vertex_format=nullptr;
        if(i==3)broken.begin=nullptr;
        if(i==4)broken.position=nullptr;
        if(i==5)broken.color=nullptr;
        if(i==6)broken.end=nullptr;
        CHECK(!draw_reticle(broken,{256,224},1,0));CHECK(recorder.commands.empty());
    }
    // Exact viewport edges are accepted with no integer rounding/wrapping.
    CHECK(draw_reticle(backend,{1,1},1,0));quad(recorder,3,0,0,2,2);
    recorder.commands.clear();CHECK(draw_reticle(backend,{511,447},1,0));quad(recorder,3,510,446,512,448);
    std::puts("reticle draw tests: PASS");
}
