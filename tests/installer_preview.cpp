#include "re4/installer_ui.hpp"
#include "re4/graphics_preview.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <cstring>
unsigned white_pixels(const char* path,bool translucent=false){
    auto* image=SDL_LoadBMP(path);assert(image);unsigned count=0;
    for(int y=412;y<576;++y)for(int x=862;x<1206;++x){
        Uint32 pixel=0;std::memcpy(&pixel,static_cast<const unsigned char*>(image->pixels)+y*image->pitch+x*image->format->BytesPerPixel,image->format->BytesPerPixel);
        Uint8 r,g,b;SDL_GetRGB(pixel,image->format,&r,&g,&b);
        if(translucent?(r>=120&&r<=190&&g>=120&&g<=190&&b>=120&&b<=190):(r==255&&g==255&&b==255))++count;
    }
    SDL_FreeSurface(image);return count;
}
SDL_Color pixel_at(SDL_Surface* image,int x,int y){
    Uint32 pixel=0;
    std::memcpy(&pixel,static_cast<const unsigned char*>(image->pixels)+y*image->pitch+x*image->format->BytesPerPixel,image->format->BytesPerPixel);
    SDL_Color color{};SDL_GetRGB(pixel,image->format,&color.r,&color.g,&color.b);return color;
}
void check_game_frame(const char* path,bool re6){
    auto* image=SDL_LoadBMP(path);assert(image && image->w==1280 && image->h==720);
    // Home actions begin at the panel's 16px top inset, without a second heading.
    const auto accent=pixel_at(image,66,190);
    assert(re6?(accent.b>accent.r && accent.r>accent.g):(accent.r>accent.g && accent.g>accent.b));
    // Both borders enclose content, with an unobstructed 16px gutter between them.
    for(int y=168;y<615;++y){
        const auto left=pixel_at(image,821,y),right=pixel_at(image,838,y);
        assert(left.r==73 && left.g==73 && left.b==57);
        assert(right.r==73 && right.g==73 && right.b==57);
        for(int x=822;x<838;++x){
            const auto gap=pixel_at(image,x,y);
            assert(gap.r<30 && gap.g<30 && gap.b<30);
        }
    }
    unsigned help_pixels=0;
    for(int y=185;y<210;++y)for(int x=862;x<1206;++x){
        const auto c=pixel_at(image,x,y);
        if(c.r>190 && c.g>180 && c.b>160)++help_pixels;
    }
    assert(help_pixels>100); // Context help itself occupies the top, not a muted description heading.
    unsigned logo_accent=0;
    for(int y=23;y<151;++y)for(int x=42;x<170;++x){
        const auto c=pixel_at(image,x,y);
        if(re6?(c.b>c.r+20 && c.r>c.g+20 && c.b>100):(c.r>c.g+30 && c.r>c.b+30 && c.r>100))++logo_accent;
    }
    assert(logo_accent>100); // The actual selected-game bitmap must be present.
    if(re6){
        // RE6 cannot display the RE4 stick-response plot or extra home rows.
        const auto blank=pixel_at(image,864,420);
        assert(blank.r==23 && blank.g==24 && blank.b==19);
        const auto fifth_row=pixel_at(image,66,420);
        assert(fifth_row.r==23 && fifth_row.g==24 && fifth_row.b==19);
    }
    SDL_FreeSurface(image);
}
#undef assert
#define assert(c) do{if(!(c)){std::fprintf(stderr,"installer preview line %d: %s\n",__LINE__,#c);std::exit(1);}}while(false)
void check_graphics_picture(const char* path){
    auto* image=SDL_LoadBMP(path);assert(image);
    unsigned varied=0;const auto base=pixel_at(image,870,350);
    for(int y=345;y<530;y+=3)for(int x=866;x<1202;x+=3){const auto c=pixel_at(image,x,y);
        if(std::abs(int(c.r)-base.r)+std::abs(int(c.g)-base.g)+std::abs(int(c.b)-base.b)>45)++varied;}
    assert(varied>1000); // Actual textured scene, not a placeholder/solid panel.
    SDL_FreeSurface(image);
}
int main(){
    std::ifstream stream("C:/Windows/Fonts/segoeui.ttf",std::ios::binary);
    std::vector<unsigned char> font((std::istreambuf_iterator<char>(stream)),{});
    assert(font.size()>10000);
    re4::InstallerRenderer renderer;
    assert(!renderer.initialize(nullptr,0,"build/app-logo.bmp",true));
    assert(renderer.initialize(font.data(),font.size(),"build/app-logo.bmp",true));
    re4::InstallerModel model;
    renderer.draw(model);assert(renderer.save_preview("build/installer-home-en.bmp"));
    model.page=re4::InstallerPage::Controls;model.selected=1;
    model.settings.language=re4::Language::Russian;model.settings.stick_curve=re4::StickCurve::Hybrid;
    renderer.draw(model);assert(renderer.save_preview("build/installer-controls-ru.bmp"));
    for(auto language:{re4::Language::English,re4::Language::Russian}){
        model.settings.language=language;
        model.page=re4::InstallerPage::Graphics;model.selected=0;
        renderer.draw(model);assert(renderer.save_preview(language==re4::Language::English?
            "build/installer-graphics-en.bmp":"build/installer-graphics-ru.bmp"));
        check_graphics_picture(language==re4::Language::English?"build/installer-graphics-en.bmp":"build/installer-graphics-ru.bmp");
        for(unsigned row=0;row<6;++row){model.selected=row;renderer.draw(model);
            const auto file="build/installer-graphics-"+std::string(language==re4::Language::English?"en":"ru")+"-"+std::to_string(row)+".bmp";
            assert(renderer.save_preview(file.c_str()));check_graphics_picture(file.c_str());}
        model.settings.texture_filtering=re4::TextureFilter::Aniso4;
        model.selected=unsigned(re4::GraphicsId::Profile);
        renderer.draw(model);assert(renderer.save_preview(language==re4::Language::English?
            "build/installer-graphics-profile-en.bmp":"build/installer-graphics-profile-ru.bmp"));
        model.page=re4::InstallerPage::Controls;model.settings.camera_gain=0.6f;
        model.selected=0;
        renderer.draw(model);assert(renderer.save_preview(language==re4::Language::English?
            "build/installer-controls-profile-en.bmp":"build/installer-controls-profile-ru.bmp"));
        re4::copy_profile(model.settings,re4::re6_profile(),re4::ProfileDomain::Controls);
        model.selected=0;
        assert(model.item_count()==8 && model.item_value(0)=="RE6");
        renderer.draw(model);assert(renderer.save_preview(language==re4::Language::English?
            "build/installer-re6-profile-en.bmp":"build/installer-re6-profile-ru.bmp"));
        model.page=re4::InstallerPage::ControlGroup;model.control_group=re4::ControlGroup::Camera;model.selected=0;
        renderer.draw(model);assert(renderer.save_preview(language==re4::Language::English?
            "build/installer-re6-overview-en.bmp":"build/installer-re6-overview-ru.bmp"));
        model.control_group=re4::ControlGroup::Aiming;model.selected=2;
        renderer.draw(model);assert(renderer.save_preview(language==re4::Language::English?
            "build/installer-re6-aim-en.bmp":"build/installer-re6-aim-ru.bmp"));
        model.selected=6;
        assert(model.selected_control()==re4::ControlId::MoveReloading && model.settings.move_while_reloading);
        renderer.draw(model);assert(renderer.save_preview(language==re4::Language::English?
            "build/installer-re6-reload-en.bmp":"build/installer-re6-reload-ru.bmp"));
        re4::copy_profile(model.settings,re4::re6_profile(),re4::ProfileDomain::Controls);
        model.page=re4::InstallerPage::Mapping;model.selected=21;
        renderer.draw(model);assert(renderer.save_preview(language==re4::Language::English?
            "build/installer-mapping-en.bmp":"build/installer-mapping-ru.bmp"));
        model.page=re4::InstallerPage::MappingTargets;model.mapping_source=19;model.selected=20;
        renderer.draw(model);assert(renderer.save_preview(language==re4::Language::English?
            "build/installer-mapping-targets-en.bmp":"build/installer-mapping-targets-ru.bmp"));
        model.page=re4::InstallerPage::ControlGroup;model.control_group=re4::ControlGroup::Camera;model.selected=4;
        renderer.draw(model);assert(renderer.save_preview(language==re4::Language::English?
            "build/installer-camera-en.bmp":"build/installer-camera-ru.bmp"));
        model.control_group=re4::ControlGroup::Aiming;model.selected=5;
        renderer.draw(model);assert(renderer.save_preview(language==re4::Language::English?
            "build/installer-gameplay-en.bmp":"build/installer-gameplay-ru.bmp"));
        model.control_group=re4::ControlGroup::Reticle;
        for(unsigned style=0;style<3;++style){
            unsigned last=0;
            for(unsigned size=0;size<3;++size){
                model.settings.reticle=static_cast<re4::ReticleStyle>(style);model.settings.reticle_size=static_cast<re4::ReticleSize>(size);
                model.selected=size%2;renderer.draw(model);
                const auto file="build/installer-reticle-"+std::string(language==re4::Language::English?"en":"ru")+"-"+std::to_string(style)+"-"+std::to_string(size)+".bmp";
                assert(renderer.save_preview(file.c_str()));const unsigned pixels=white_pixels(file.c_str(),style!=0);
                if(style!=0)assert(white_pixels(file.c_str())==0);
                assert(style==0?pixels==0:pixels>last);last=pixels;
            }
        }
        model.control_group=re4::ControlGroup::Sticks;model.selected=0;renderer.draw(model);
        assert(renderer.save_preview("build/installer-unrelated.bmp") && white_pixels("build/installer-unrelated.bmp")==0);
    }
    model.page=re4::InstallerPage::Diagnostics;model.settings.language=re4::Language::English;
    model.diagnostic.connected=true;model.diagnostic.sensors=1;model.diagnostic.raw[0]=0.65f;
    model.diagnostic.processed[0]=0.5f;
    renderer.draw(model);assert(renderer.save_preview("build/installer-diagnostics-en.bmp"));
    model.page=re4::InstallerPage::Updates;model.update.stage=re4::UpdateStage::Available;
    model.update.version="0.4.0";model.update.total=8*1024*1024;model.selected=1;
    renderer.draw(model);assert(renderer.save_preview("build/installer-updates-en.bmp"));
    model.settings.language=re4::Language::Russian;model.update.stage=re4::UpdateStage::Downloading;model.update.received=4*1024*1024;
    renderer.draw(model);assert(renderer.save_preview("build/installer-updates-ru.bmp"));
    assert(renderer.initialize(font.data(),font.size(),"build/app-logo.bmp",true,"build/app-logo-re6.bmp"));
    for(auto language:{re4::Language::English,re4::Language::Russian}){
        model={};model.settings.language=language;
        const auto* re4_path=language==re4::Language::English?"build/installer-home-en.bmp":"build/installer-home-ru.bmp";
        renderer.draw(model);assert(renderer.save_preview(re4_path));
        check_game_frame(re4_path,false);
        assert(model.handle(re4::UiEvent::NextGame)==re4::InstallerAction::None);
        renderer.draw(model);
        const auto* path=language==re4::Language::English?"build/installer-re6-home-en.bmp":"build/installer-re6-home-ru.bmp";
        assert(renderer.save_preview(path));check_game_frame(path,true);
        model.selected=2;assert(model.item_count()==5&&model.handle(re4::UiEvent::Confirm)==re4::InstallerAction::None);
        assert(model.page==re4::InstallerPage::Graphics&&model.item_count()==3);
        for(unsigned row=0;row<3;++row){model.selected=row;renderer.draw(model);
            const auto file="build/installer-re6-graphics-"+std::string(language==re4::Language::English?"en":"ru")+"-"+std::to_string(row)+".bmp";
            assert(renderer.save_preview(file.c_str()));check_graphics_picture(file.c_str());}
        assert(model.handle(re4::UiEvent::Back)==re4::InstallerAction::None&&model.page==re4::InstallerPage::Home&&model.selected==2);
    }
    renderer.close();renderer.close();
    assert(SDL_WasInit(SDL_INIT_VIDEO)==0 && !renderer.save_preview("build/installer-closed.bmp"));
    // Reinitialize after freeing both logos and cached EN/RU glyphs.
    assert(renderer.initialize(font.data(),font.size(),"build/app-logo.bmp",true,"build/app-logo-re6.bmp"));
    model={};model.handle(re4::UiEvent::PreviousGame);renderer.draw(model);
    assert(renderer.save_preview("build/installer-re6-reopened.bmp"));
    check_game_frame("build/installer-re6-reopened.bmp",true);
    renderer.close();assert(SDL_WasInit(SDL_INIT_VIDEO)==0);
    std::puts("actual installer renderer: EN/RU frames, diagnostics and cleanup PASS");
}
