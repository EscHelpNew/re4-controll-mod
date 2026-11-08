#include "re4/enemy_health_numbers.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

#define CHECK(c) do{if(!(c)){std::fprintf(stderr,"enemy health numbers line %d: %s\n",__LINE__,#c);std::exit(1);}}while(false)

void formatting(){
    // Incorrect fixed divisors, truncation, or floating rounding break these fixtures.
    struct Sample {int hp;unsigned unit;bool russian;const char* want;};
    for(const auto& s:{Sample{750,150,false,"5.0"},Sample{0,150,false,"0.0"},Sample{1,150,false,"0.0"},
            Sample{7,150,false,"0.0"},Sample{8,150,false,"0.1"},
            Sample{74,150,false,"0.5"},Sample{75,150,false,"0.5"},
            Sample{149,150,false,"1.0"},Sample{750,150,true,"5.0"},
            Sample{4,100,false,"0.0"},Sample{5,100,false,"0.1"},
            Sample{95,100,false,"1.0"},Sample{1,20,false,"0.1"},
            Sample{1,3,false,"0.3"},Sample{2,3,false,"0.7"},
            Sample{32767,1,false,"32767.0"},Sample{32767,150,true,"218.4"},
            Sample{32767,std::numeric_limits<unsigned>::max(),false,"0.0"}}){
        char out[8]{};
        CHECK(re4::format_enemy_health_number(s.hp,s.unit,s.russian,out,sizeof(out)));
        CHECK(std::strcmp(out,s.want)==0);
    }
    // Every failure must leave the complete supplied storage untouched.
    for(int hp:{-1,32768,std::numeric_limits<int>::min(),std::numeric_limits<int>::max()}){
        char out[]="sentinel";
        CHECK(!re4::format_enemy_health_number(hp,150,false,out,sizeof(out)));
        CHECK(std::strcmp(out,"sentinel")==0);
    }
    char out[]="sentinel";
    CHECK(!re4::format_enemy_health_number(750,0,false,out,sizeof(out)));
    CHECK(std::strcmp(out,"sentinel")==0);
    for(std::size_t size:{std::size_t(0),std::size_t(1),std::size_t(3)}){
        CHECK(!re4::format_enemy_health_number(750,150,false,out,size));
        CHECK(std::strcmp(out,"sentinel")==0);
    }
    CHECK(!re4::format_enemy_health_number(32767,1,false,out,7));
    CHECK(std::strcmp(out,"sentinel")==0);
    CHECK(!re4::format_enemy_health_number(750,150,false,nullptr,8));
    CHECK(re4::format_enemy_health_number(750,150,false,out,4));
    CHECK(std::strcmp(out,"5.0")==0&&out[4]=='i');
}

struct Command {char kind;float a,b,c,d,e;};
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
        [](void* p,unsigned a){static_cast<Recorder*>(p)->add('E',float(a));}};}
};
void command(const Command& c,char kind,float a=0,float b=0,float d=0,float e=0,float f=0){
    CHECK(c.kind==kind&&c.a==a&&c.b==b&&c.c==d&&c.d==e&&c.e==f);
}
void raster(const Recorder& r,float left,float top,unsigned width,unsigned height,unsigned alpha,const char* const* rows){
    CHECK(r.commands.size()>=15&&(r.commands.size()-5)%10==0);
    command(r.commands[0],'H');command(r.commands[1],'L',1,4,5,0);command(r.commands[2],'F',0,9,1,4,0);
    unsigned pixels[6][26]{};
    for(std::size_t i=3;i+2<r.commands.size();i+=10){
        command(r.commands[i],'B',0x98,0,4);
        const auto& a=r.commands[i+1];const auto& b=r.commands[i+3];
        const auto& c=r.commands[i+5];const auto& d=r.commands[i+7];
        CHECK(a.kind=='P'&&b.kind=='P'&&c.kind=='P'&&d.kind=='P');
        CHECK(a.c==0&&b.c==0&&c.c==0&&d.c==0);
        CHECK(a.a==c.a&&b.a==d.a&&a.b==b.b&&c.b==d.b&&c.b==a.b+1&&a.a<b.a);
        CHECK(a.a>=0&&a.b>=0&&d.a<=512&&d.b<=448);
        CHECK(a.a>=left&&a.b>=top&&d.a<=left+width&&d.b<=top+height);
        CHECK(a.a-left==std::floor(a.a-left)&&a.b-top==std::floor(a.b-top));
        CHECK(b.a-left==std::floor(b.a-left));
        for(unsigned vertex=0;vertex<4;++vertex)command(r.commands[i+2+vertex*2],'C',235,235,235,float(alpha));
        command(r.commands[i+9],'E',0x34);
        for(unsigned x=unsigned(a.a-left);x<unsigned(b.a-left);++x)++pixels[unsigned(a.b-top)][x];
    }
    command(r.commands[r.commands.size()-2],'L',1,1,0,0);
    command(r.commands.back(),'F',0,9,1,3,0);
    if(rows)for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x)CHECK(pixels[y][x]==unsigned(rows[y][x]=='#'));
}
void drawing(){
    using namespace re4;
    // Literal rasters detect misplaced glyphs, punctuation, gaps, or centering.
    const char* english[]={"###...###","#.#...#.#","#.#...#.#","#.#...#.#","###.#.###"};
    const char* const* russian=english;
    Recorder en;CHECK(draw_enemy_health_number(en.backend(),{256,100},1,150,false));
    CHECK(en.commands.size()==175);raster(en,251.5f,85,9,5,128,english);
    Recorder ru;CHECK(draw_enemy_health_number(ru.backend(),{256,100},1,150,true,.5f));
    CHECK(ru.commands.size()==175);raster(ru,251.5f,85,9,5,64,russian);
    const char* digits[][5]={
        {"###","#.#","#.#","#.#","###"},
        {".#.","##.",".#.",".#.","###"},
        {"###","..#","###","#..","###"},
        {"###","..#","###","..#","###"},
        {"#.#","#.#","###","..#","..#"},
        {"###","#..","###","..#","###"},
        {"###","#..","###","#.#","###"},
        {"###","..#",".#.",".#.",".#."},
        {"###","#.#","###","#.#","###"},
        {"###","#.#","###","..#","###"}};
    for(int digit=0;digit<10;++digit){
        Recorder draw;CHECK(draw_enemy_health_number(draw.backend(),{256,100},digit*150+1,150,false));
        char rows[5][10]{};const char* expected[5]{};
        for(unsigned y=0;y<5;++y){std::memcpy(rows[y],english[y],10);std::memcpy(rows[y],digits[digit][y],3);expected[y]=rows[y];}
        raster(draw,251.5f,85,9,5,128,expected);
    }
    Recorder large;CHECK(draw_enemy_health_number(large.backend(),{256,100},32767,1,true));
    CHECK((large.commands.size()-5)/10<=64);raster(large,243.5f,85,25,5,128,nullptr);
    for(const auto anchor:{ViewHudPoint{4,100},ViewHudPoint{508,100},ViewHudPoint{256,14},
            ViewHudPoint{256,459},ViewHudPoint{std::numeric_limits<float>::quiet_NaN(),100},
            ViewHudPoint{256,std::numeric_limits<float>::infinity()}}){
        Recorder draw;CHECK(!draw_enemy_health_number(draw.backend(),anchor,1,150,false));CHECK(draw.commands.empty());
    }
    // The decimal point and text bounds are identical in both interface languages.
    Recorder tail;CHECK(draw_enemy_health_number(tail.backend(),{256,458},1,150,true));raster(tail,251.5f,443,9,5,128,english);
    Recorder bottom;CHECK(draw_enemy_health_number(bottom.backend(),{507.5f,458},1,150,false));raster(bottom,503,443,9,5,128,english);
    Recorder edge;CHECK(draw_enemy_health_number(edge.backend(),{4.5f,15},1,150,false));raster(edge,0,0,9,5,128,english);
    for(float opacity:{0.f,.001f,-.1f,1.1f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){
        Recorder draw;CHECK(!draw_enemy_health_number(draw.backend(),{256,100},1,150,false,opacity));CHECK(draw.commands.empty());
    }
    for(int hp:{-1,0,32768}){Recorder draw;CHECK(!draw_enemy_health_number(draw.backend(),{256,100},hp,150,false));CHECK(draw.commands.empty());}
    Recorder unit;CHECK(!draw_enemy_health_number(unit.backend(),{256,100},1,0,false));CHECK(unit.commands.empty());
    for(unsigned i=0;i<7;++i){
        Recorder draw;auto backend=draw.backend();
        if(i==0)backend.prepare_hud=nullptr;
        if(i==1)backend.blend=nullptr;
        if(i==2)backend.vertex_format=nullptr;
        if(i==3)backend.begin=nullptr;
        if(i==4)backend.position=nullptr;
        if(i==5)backend.color=nullptr;
        if(i==6)backend.end=nullptr;
        CHECK(!draw_enemy_health_number(backend,{256,100},1,150,false));CHECK(draw.commands.empty());
    }
}

int main(){
    formatting();drawing();
    std::puts("enemy health numbers: formatter, pixel glyphs, native commands, clipping and fade PASS");
}
