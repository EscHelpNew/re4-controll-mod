#define SDL_MAIN_HANDLED
#include <SDL.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>
#include "re4/installer_ui.hpp"
#include "re4/input.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <vector>

namespace re4 {
namespace {
constexpr int width=1280,height=720;
constexpr SDL_Color cream{220,211,184,255},muted{146,143,121,255},red{172,48,36,255};
constexpr SDL_Color panel{24,25,20,245},line{73,73,57,255},highlight{77,31,25,255};
const char* tr(const InstallerModel& m,const char* en,const char* ru){return m.settings.language==Language::Russian?ru:en;}
std::uint32_t next_codepoint(const char*& p){
    const unsigned char c=static_cast<unsigned char>(*p++);
    if(c<128)return c;
    unsigned count=0;std::uint32_t value=0;
    if((c&0xE0)==0xC0){count=1;value=c&31;}
    else if((c&0xF0)==0xE0){count=2;value=c&15;}
    else if((c&0xF8)==0xF0){count=3;value=c&7;}
    else return '?';
    for(unsigned i=0;i<count;++i){
        const auto part=static_cast<unsigned char>(*p);
        if(!part || (part&0xC0)!=0x80)return '?';
        value=(value<<6)|(part&63);++p;
    }
    return value;
}
}
struct InstallerRenderer::Impl {
    struct Glyph{SDL_Texture* texture=nullptr;int w=0,h=0,x=0,y=0;float advance=0;};
    SDL_Window* window=nullptr;
    SDL_Renderer* renderer=nullptr;
    SDL_Texture* background=nullptr;
    SDL_Texture* logo=nullptr;
    stbtt_fontinfo font{};
    std::map<std::uint64_t,Glyph> glyphs;
    std::vector<std::uint32_t> preview;
    std::string failure;
    bool sdl=false,hidden=false;
    int ascent=0;
    void fill(SDL_Rect rect,SDL_Color c){
        SDL_SetRenderDrawColor(renderer,c.r,c.g,c.b,c.a);SDL_RenderFillRect(renderer,&rect);
    }
    void border(SDL_Rect rect,SDL_Color c){
        SDL_SetRenderDrawColor(renderer,c.r,c.g,c.b,c.a);SDL_RenderDrawRect(renderer,&rect);
    }
    Glyph& glyph(std::uint32_t code,unsigned size){
        const auto key=(std::uint64_t(size)<<32)|code;
        const auto found=glyphs.find(key);if(found!=glyphs.end())return found->second;
        Glyph g;
        const float scale=stbtt_ScaleForPixelHeight(&font,float(size));
        int advance=0,left=0;stbtt_GetCodepointHMetrics(&font,int(code),&advance,&left);g.advance=advance*scale;
        auto* bitmap=stbtt_GetCodepointBitmap(&font,scale,scale,int(code),&g.w,&g.h,&g.x,&g.y);
        if(bitmap && g.w>0 && g.h>0){
            std::vector<std::uint32_t> pixels(std::size_t(g.w)*g.h);
            for(std::size_t i=0;i<pixels.size();++i)pixels[i]=0x00FFFFFFu|(std::uint32_t(bitmap[i])<<24);
            g.texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STATIC,g.w,g.h);
            if(g.texture){
                SDL_UpdateTexture(g.texture,nullptr,pixels.data(),g.w*4);
                SDL_SetTextureBlendMode(g.texture,SDL_BLENDMODE_BLEND);
            }
        }
        stbtt_FreeBitmap(bitmap,nullptr);
        return glyphs.emplace(key,g).first->second;
    }
    float measure(const std::string& s,unsigned size){
        const char* p=s.c_str();float result=0;
        while(*p && *p!='\n')result+=glyph(next_codepoint(p),size).advance;
        return result;
    }
    void text(const std::string& s,int x,int y,unsigned size,SDL_Color c,int maximum=0,bool wrap=false,unsigned max_lines=8){
        const float baseline=ascent*stbtt_ScaleForPixelHeight(&font,float(size));
        float cursor=float(x);int row=y;unsigned lines=1;const char* p=s.c_str();
        while(*p){
            const auto cp=next_codepoint(p);
            if(cp=='\n'){if(++lines>max_lines)break;cursor=float(x);row+=int(size)+7;continue;}
            auto& g=glyph(cp,size);
            if(maximum && cursor+g.advance>x+maximum){
                if(!wrap || ++lines>max_lines)break;
                cursor=float(x);row+=int(size)+7;
            }
            if(g.texture){
                SDL_SetTextureColorMod(g.texture,c.r,c.g,c.b);SDL_SetTextureAlphaMod(g.texture,c.a);
                const SDL_Rect dest{int(cursor)+g.x,row+int(baseline)+g.y,g.w,g.h};
                SDL_RenderCopy(renderer,g.texture,nullptr,&dest);
            }
            cursor+=g.advance;
        }
    }
    void curve(const InstallerModel& m){
        const SDL_Rect graph{864,420,336,140};
        fill(graph,{12,14,11,255});border(graph,line);
        SDL_SetRenderDrawColor(renderer,46,49,36,255);
        for(int i=1;i<4;++i){
            SDL_RenderDrawLine(renderer,graph.x+i*graph.w/4,graph.y,graph.x+i*graph.w/4,graph.y+graph.h);
            SDL_RenderDrawLine(renderer,graph.x,graph.y+i*graph.h/4,graph.x+graph.w,graph.y+i*graph.h/4);
        }
        SDL_SetRenderDrawColor(renderer,cream.r,cream.g,cream.b,255);
        for(int i=1;i<=150;++i){
            const float a=(i-1)/150.f,b=i/150.f;
            const int y0=graph.y+graph.h-int(shape_axis(a,0,1,m.settings.stick_curve)*graph.h);
            const int y1=graph.y+graph.h-int(shape_axis(b,0,1,m.settings.stick_curve)*graph.h);
            SDL_RenderDrawLine(renderer,graph.x+(i-1)*graph.w/150,y0,graph.x+i*graph.w/150,y1);
            SDL_RenderDrawLine(renderer,graph.x+(i-1)*graph.w/150,y0+1,graph.x+i*graph.w/150,y1+1);
        }
        if(m.diagnostic.connected){
            const float x=std::fabs(shape_axis(m.diagnostic.raw[0],m.settings.left_deadzone,1));
            const float y=shape_axis(x,0,1,m.settings.stick_curve);
            fill({graph.x+int(x*graph.w)-4,graph.y+graph.h-int(y*graph.h)-4,8,8},red);
        }
        text("0",graph.x-2,graph.y+graph.h+5,14,muted);
        text("1",graph.x+graph.w-8,graph.y+graph.h+5,14,muted);
    }
    void diagnostics(const InstallerModel& m){
        char out[256];
        text(tr(m,"LIVE INPUT","ПОКАЗАНИЯ КОНТРОЛЛЕРА"),76,184,17,muted);
        text(m.diagnostic.connected?tr(m,"CONTROLLER CONNECTED","КОНТРОЛЛЕР ПОДКЛЮЧЁН")
                                     :tr(m,"NO CONTROLLER","КОНТРОЛЛЕР НЕ ПОДКЛЮЧЁН"),76,223,22,cream);
        for(unsigned i=0;i<4;++i){
            const int y=273+int(i)*49;
            static const char* axes[]={"LX","LY","RX","RY"};
            text(axes[i],78,y,20,muted);
            fill({136,y+8,428,9},{11,13,10,255});
            const float value=std::clamp(m.diagnostic.raw[i],-1.f,1.f);
            const int mid=350,delta=int(value*214);
            fill({value<0?mid+delta:mid,y+8,std::abs(delta),9},cream);
            fill({mid,y+4,1,17},red);
            std::snprintf(out,sizeof(out),"%+.3f  ->  %+.3f",m.diagnostic.raw[i],m.diagnostic.processed[i]);
            text(out,587,y+1,17,cream,190);
        }
        std::snprintf(out,sizeof(out),"GYRO  %+.3f  %+.3f  %+.3f",m.diagnostic.gyro_rate[0],m.diagnostic.gyro_rate[1],m.diagnostic.gyro_rate[2]);
        text(out,78,487,18,cream,690);
        std::snprintf(out,sizeof(out),"PREVIEW  X %+.3f   Y %+.3f",m.diagnostic.gyro_preview[0],m.diagnostic.gyro_preview[1]);
        text(out,78,519,18,cream,690);
        std::snprintf(out,sizeof(out),"STYLE %08X   SENSORS %u   RC %08X   SAMPLES %u",
                      m.diagnostic.style,m.diagnostic.sensors,m.diagnostic.sensor_result,m.diagnostic.samples);
        text(out,78,554,14,muted,690);
        if(m.diagnostic.calibration_remaining){
            fill({78,588,690,6},{10,12,9,255});
            const unsigned left=std::min(200u,m.diagnostic.calibration_remaining);
            fill({78,588,int(690*(200-left)/200),6},red);
        }else if(!m.detail.empty())text(m.detail,78,580,14,muted,690);
    }
};
InstallerRenderer::InstallerRenderer()=default;
InstallerRenderer::~InstallerRenderer(){close();}
bool InstallerRenderer::initialize(const void* data,std::size_t size,const char* logo_path,bool hidden){
    close();impl_=new Impl;impl_->hidden=hidden;
    if(!data || size<1000){impl_->failure="Shared font unavailable";return false;}
    const auto* bytes=static_cast<const unsigned char*>(data);
    const int offset=stbtt_GetFontOffsetForIndex(bytes,0);
    if(offset<0 || std::size_t(offset)>=size || !stbtt_InitFont(&impl_->font,bytes,offset)){
        impl_->failure="Could not open shared font";return false;
    }
    stbtt_GetFontVMetrics(&impl_->font,&impl_->ascent,nullptr,nullptr);
    SDL_SetMainReady();SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"1");
    if(SDL_Init(SDL_INIT_VIDEO)!=0){impl_->failure=SDL_GetError();return false;}
    impl_->sdl=true;
    impl_->window=SDL_CreateWindow("RE4 Controls",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,width,height,
                                   hidden?SDL_WINDOW_HIDDEN:SDL_WINDOW_SHOWN);
    if(!impl_->window){impl_->failure=SDL_GetError();return false;}
    impl_->renderer=SDL_CreateRenderer(impl_->window,-1,hidden?SDL_RENDERER_SOFTWARE:(SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC));
    if(!impl_->renderer)impl_->renderer=SDL_CreateRenderer(impl_->window,-1,SDL_RENDERER_SOFTWARE);
    if(!impl_->renderer){impl_->failure=SDL_GetError();return false;}
    SDL_RenderSetLogicalSize(impl_->renderer,width,height);
    SDL_SetRenderDrawBlendMode(impl_->renderer,SDL_BLENDMODE_BLEND);
    if(logo_path){
        SDL_Surface* surface=SDL_LoadBMP(logo_path);
        if(surface){impl_->logo=SDL_CreateTextureFromSurface(impl_->renderer,surface);SDL_FreeSurface(surface);}
    }
    std::vector<std::uint32_t> pixels(width*height);
    for(int y=0;y<height;++y)for(int x=0;x<width;++x){
        const float dx=(x-width*.5f)/(width*.7f),dy=(y-height*.5f)/(height*.8f);
        const unsigned grain=(unsigned(x)*1973u+unsigned(y)*9277u)^unsigned(x*y);
        const int value=9+int(8*(1-std::fmin(1,dx*dx+dy*dy)))+int(grain%3);
        pixels[std::size_t(y)*width+x]=std::uint32_t(value+2)|(std::uint32_t(value+3)<<8)
            |(std::uint32_t(value)<<16)|0xFF000000u;
    }
    impl_->background=SDL_CreateTexture(impl_->renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STATIC,width,height);
    if(impl_->background)SDL_UpdateTexture(impl_->background,nullptr,pixels.data(),width*4);
    if(hidden)impl_->preview.resize(width*height);
    return true;
}
void InstallerRenderer::draw(const InstallerModel& m){
    if(!impl_ || !impl_->renderer)return;
    auto& p=*impl_;
    SDL_SetRenderDrawColor(p.renderer,10,12,9,255);SDL_RenderClear(p.renderer);
    if(p.background)SDL_RenderCopy(p.renderer,p.background,nullptr,nullptr);
    if(p.logo){const SDL_Rect dest{42,23,128,128};SDL_RenderCopy(p.renderer,p.logo,nullptr,&dest);}
    p.text("RE4 CONTROLS",183,43,46,cream);
    p.text(m.page_title(),186,106,18,muted);
    p.text("0.3.0",1150,51,18,muted);
    p.fill({52,151,1176,1},line);
    p.fill({52,167,750,449},panel);p.border({52,167,750,449},line);
    p.fill({838,167,390,449},panel);p.border({838,167,390,449},line);
    if(m.page==InstallerPage::Diagnostics)p.diagnostics(m);
    else{
        p.text(m.page==InstallerPage::Home?tr(m,"OPERATIONS","ДЕЙСТВИЯ"):tr(m,"SETTINGS","НАСТРОЙКИ"),76,184,17,muted);
        const unsigned count=m.item_count(),visible=8;
        unsigned first=m.selected>=visible?m.selected-visible+1:0;
        if(count<visible)first=0;
        for(unsigned i=first;i<count && i<first+visible;++i){
            const int y=222+int(i-first)*49;
            if(i==m.selected){p.fill({65,y-2,723,43},highlight);p.fill({65,y-2,4,43},red);}
            p.text(m.item_label(i),81,y+7,23,i==m.selected?cream:muted,485);
            const auto value=m.item_value(i);
            if(!value.empty())p.text(value,766-int(p.measure(value,18)),y+10,18,cream,240);
        }
        if(count>visible){
            p.fill({793,223,2,383},{43,45,34,255});
            p.fill({793,223+int(m.selected*340/std::max(1u,count-1)),2,43},red);
        }
    }
    p.text(tr(m,"FIELD NOTES","ОПИСАНИЕ"),862,185,17,muted);
    p.text(m.selected_help(),862,224,19,cream,344,true,5);
    p.text(tr(m,"STICK RESPONSE","КРИВАЯ СТИКОВ"),862,360,16,muted);
    p.text(curve_name(m.settings.stick_curve,m.settings.language),862,386,18,cream);
    p.curve(m);
    p.text(m.page==InstallerPage::Home?tr(m,"In game: L + R + UP","В игре: L + R + вверх")
                                     :tr(m,"Native stick maximum: 1.00","Максимум скорости стиков: 1.00"),862,585,15,muted,344);
    const bool error=m.status==InstallerStatus::Failed||m.status==InstallerStatus::Conflict||m.status==InstallerStatus::GameRunning;
    p.text(m.status_text(),55,632,21,error?red:cream,1170);
    p.fill({52,665,1176,1},line);
    p.text(tr(m,"UP/DOWN: select    A: confirm    LEFT/RIGHT: change    B: back    +: exit",
                "Вверх/вниз: выбор    A: подтвердить    Влево/вправо: изменить    B: назад    +: выход"),
           55,682,16,muted,1170);
    if(p.hidden)SDL_RenderReadPixels(p.renderer,nullptr,SDL_PIXELFORMAT_RGBA32,p.preview.data(),width*4);
    SDL_RenderPresent(p.renderer);
}
bool InstallerRenderer::save_preview(const char* path){
    if(!impl_ || impl_->preview.empty() || !path)return false;
    auto* surface=SDL_CreateRGBSurfaceWithFormatFrom(impl_->preview.data(),width,height,32,width*4,SDL_PIXELFORMAT_RGBA32);
    if(!surface)return false;
    const bool ok=SDL_SaveBMP(surface,path)==0;SDL_FreeSurface(surface);return ok;
}
void InstallerRenderer::close(){
    if(!impl_)return;
    for(auto& entry:impl_->glyphs)if(entry.second.texture)SDL_DestroyTexture(entry.second.texture);
    if(impl_->logo)SDL_DestroyTexture(impl_->logo);
    if(impl_->background)SDL_DestroyTexture(impl_->background);
    if(impl_->renderer)SDL_DestroyRenderer(impl_->renderer);
    if(impl_->window)SDL_DestroyWindow(impl_->window);
    if(impl_->sdl)SDL_Quit();
    delete impl_;impl_=nullptr;
}
const char* InstallerRenderer::error()const{return impl_?impl_->failure.c_str():"Not initialized";}
}
