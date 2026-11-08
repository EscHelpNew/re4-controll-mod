#pragma once
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <initializer_list>
#include "re4/reticle_draw.hpp"

namespace re4 {
// HUD projection512x448 is displayed at16:9. Correct glyph X so physical
// letter proportions match the licensed face and the native Leon inscription.
inline constexpr float kSerifHudPixelAspect=9.f/14.f;
struct SerifRun {std::int8_t x,y;std::uint8_t width,alpha;};
struct SerifGlyph {std::uint16_t code,first,count,advance64;};
#include "re4/serif_glyphs.inc"
inline const SerifGlyph* serif_glyph(unsigned code){
    for(const auto& g:serif_glyphs)if(g.code==code)return &g;
    return nullptr;
}
// Strict, bounded UTF-8 decoding. Unsupported or malformed input rejects the
// whole line before any native render commands are enqueued.
inline bool serif_code(const char*& p,unsigned& code){
    const auto a=static_cast<unsigned char>(*p++);
    if(a<128){code=a;return a>=32&&a<127;}
    if(a<0xC2||a>0xDF)return false;
    const auto b=static_cast<unsigned char>(*p);
    if((b&0xC0)!=0x80)return false;
    ++p;code=((a&31)<<6)|(b&63);return true;
}
using SerifRunVisitor=void(*)(void*,float,float,float,float,unsigned);
inline float serif_text_width(const char* text,float capheight){
    if(!text||!*text||!std::isfinite(capheight)||capheight<4||capheight>32)return 0;
    unsigned count=0,total=0;float advance=0;
    for(const char* p=text;*p;){
        unsigned code=0;if(++count>48||!serif_code(p,code))return 0;
        const auto* g=serif_glyph(code);if(!g||total+g->count>6000)return 0;
        total+=g->count;advance+=g->advance64*(capheight/1024)*kSerifHudPixelAspect;
    }
    return advance;
}
// x/y is the left edge / capital top in 512x448 native logical pixels.
// The baked capheight is16; rescaling changes coverage quads, never font state.
// Up to48 glyphs and6000 runs; every bound is checked before the first callback.
inline bool serif_text_runs(const char* text,float x,float y,float capheight,unsigned opacity,
        void* context,SerifRunVisitor visitor,float* width=nullptr){
    if(!text||!*text||!visitor||!opacity||opacity>255||!std::isfinite(x)||!std::isfinite(y)
        ||!std::isfinite(capheight)||capheight<4||capheight>32)return false;
    const SerifGlyph* glyphs[48]{};std::size_t count=0;unsigned total=0;
    float cursor=x;const float scale=capheight/16,xscale=scale*kSerifHudPixelAspect;
    for(const char* p=text;*p;){
        unsigned code=0;if(count==48||!serif_code(p,code))return false;
        const auto* g=serif_glyph(code);if(!g||total+g->count>6000)return false;
        glyphs[count++]=g;total+=g->count;
        for(unsigned i=0;i<g->count;++i){const auto& r=serif_runs[g->first+i];
            const float left=cursor+r.x*xscale,top=y+r.y*scale;
            if(left<0||top<0||left+r.width*xscale>512||top+scale>448)return false;
        }
        cursor+=g->advance64*(xscale/64);
    }
    if(width)*width=cursor-x;
    cursor=x;
    for(std::size_t n=0;n<count;++n){const auto* g=glyphs[n];
        for(unsigned i=0;i<g->count;++i){const auto& r=serif_runs[g->first+i];
            const unsigned alpha=(unsigned(r.alpha)*opacity+127)/255;if(!alpha)continue;
            const float left=cursor+r.x*xscale,top=y+r.y*scale;
            visitor(context,left,top,left+r.width*xscale,top+scale,alpha);
        }
        cursor+=g->advance64*(xscale/64);
    }
    return true;
}
struct SerifTextColor {unsigned red=173,green=173,blue=173;};
inline bool draw_serif_text_color(const ReticleDrawBackend& backend,const char* text,float x,float y,
        float capheight,unsigned opacity,SerifTextColor color){
    if(color.red>255||color.green>255||color.blue>255||!backend.prepare_hud||!backend.blend||!backend.vertex_format||!backend.begin
        ||!backend.position||!backend.color||!backend.end)return false;
    unsigned count=0;
    if(!serif_text_runs(text,x,y,capheight,opacity,&count,
        +[](void* p,float,float,float,float,unsigned){++*static_cast<unsigned*>(p);}))return false;
    if(!count)return true;
    //24FB30 accepts the vertexcount as u16.0x90 is native independent triangles:
    //228FB0's index2 switch at6F5528 selects229028, then270F70(actualcount).
    //Endtoken34 dispatches floatposition/RGBA via230AC0 regardless of count.
    //6000runs*6=36000vertices remains below native position/color u16 counters.
    struct Draw {const ReticleDrawBackend* b;SerifTextColor color;};
    Draw state{&backend,color};
    backend.prepare_hud(backend.context);backend.blend(backend.context,1,4,5,0);
    backend.vertex_format(backend.context,0,9,1,4,0);
    backend.begin(backend.context,0x90,0,count*6);
    const bool result=serif_text_runs(text,x,y,capheight,opacity,&state,
        +[](void* p,float l,float t,float r,float b,unsigned a){
            auto& d=*static_cast<Draw*>(p);const auto& out=*d.b;auto* c=out.context;
            // Same winding as the native strip:TL,TR,BL thenTR,BR,BL.
            for(const auto& v:{ViewHudPoint{l,t},ViewHudPoint{r,t},ViewHudPoint{l,b},
                    ViewHudPoint{r,t},ViewHudPoint{r,b},ViewHudPoint{l,b}}){
                out.position(c,v.x,v.y,0);out.color(c,d.color.red,d.color.green,d.color.blue,a);
            }
        });
    backend.end(backend.context,0x34);
    backend.blend(backend.context,1,1,0,0);backend.vertex_format(backend.context,0,9,1,3,0);
    return result;
}
inline bool draw_serif_text(const ReticleDrawBackend& backend,const char* text,float x,float y,
        float capheight,unsigned opacity,unsigned gray=173){
    return draw_serif_text_color(backend,text,x,y,capheight,opacity,{gray,gray,gray});
}
inline const char* weapon_name(std::uint16_t id,bool russian){
    struct Name {std::uint16_t id;const char* english;const char* russian;};
    static constexpr Name names[]={
        {1,"Hand Grenade","Ручная граната"},{2,"Incendiary Grenade","Зажигательная граната"},
        {3,"Matilda","Матильда"},{14,"Flash Grenade","Световая граната"},{16,"Bowgun","Арбалет"},
        {23,"Rocket Launcher (Special)","Ракетомёт (особый)"},{33,"Punisher","Каратель"},
        {35,"Handgun","Пистолет"},{36,"Handgun w/ Silencer","Пистолет с глушителем"},
        {37,"Red9","Red9"},{38,"Red9 w/ Stock","Red9 с прикладом"},{39,"Blacktail","Чёрный хвост"},
        {40,"Blacktail w/ Silencer","Чёрный хвост с глушителем"},{41,"Broken Butterfly","Сломанная бабочка"},
        {42,"Killer7","Killer7"},{44,"Shotgun","Дробовик"},{45,"Striker","Страйкер"},
        {46,"Rifle","Винтовка"},{47,"Rifle (semi-auto)","Винтовка (полуавтомат)"},{48,"TMP","TMP"},
        {50,"TMP w/ Stock","TMP с прикладом"},{52,"Chicago Typewriter","Чикагская машинка"},
        {53,"Rocket Launcher","Ракетомёт"},{54,"Mine Thrower","Миномёт"},{55,"Handcannon","Ручная пушка"},
        {56,"Combat Knife","Боевой нож"},{62,"Custom TMP","Особый TMP"},{64,"Punisher","Каратель"},
        {65,"P.R.L. 412","P.R.L. 412"},{71,"Shotgun","Дробовик"},
        {81,"Rifle (semi-auto) w/ Infrared Scope","Полуавтомат с тепловизором"},
        {82,"Krauser's Bow","Лук Краузера"},{83,"Chicago Typewriter","Чикагская машинка"},
        {107,"Rifle + Scope","Винтовка с прицелом"},{108,"Rifle (semi-auto) w/ Scope","Полуавтомат с прицелом"},
        {109,"Infinite Launcher","Бесконечный ракетомёт"},{148,"Riot Gun","Полицейский дробовик"},
        {153,"Rifle w/ Infrared Scope","Винтовка с тепловизором"},{171,"Mine Thrower + Scope","Миномёт с прицелом"}
    };
    for(const auto& n:names)if(n.id==id)return russian?n.russian:n.english;
    return nullptr;
}
}
