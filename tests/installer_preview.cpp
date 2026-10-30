#include "re4/installer_ui.hpp"
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
        model.settings.texture_filtering=re4::TextureFilter::Aniso4;
        model.selected=unsigned(re4::GraphicsId::Profile);
        renderer.draw(model);assert(renderer.save_preview(language==re4::Language::English?
            "build/installer-graphics-profile-en.bmp":"build/installer-graphics-profile-ru.bmp"));
        model.page=re4::InstallerPage::ControlGroup;model.control_group=re4::ControlGroup::Profiles;model.settings.camera_gain=0.6f;
        model.selected=0;
        renderer.draw(model);assert(renderer.save_preview(language==re4::Language::English?
            "build/installer-controls-profile-en.bmp":"build/installer-controls-profile-ru.bmp"));
        re4::copy_profile(model.settings,re4::re9_profile(),re4::ProfileDomain::Controls);
        model.page=re4::InstallerPage::Profiles;model.selected=1;
        renderer.draw(model);assert(renderer.save_preview(language==re4::Language::English?
            "build/installer-re9-profile-en.bmp":"build/installer-re9-profile-ru.bmp"));
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
    renderer.close();renderer.close();
    std::puts("actual installer renderer: EN/RU frames, diagnostics and cleanup PASS");
}
