#include "re4/installer_ui.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>
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
        model.page=re4::InstallerPage::Mapping;model.selected=21;
        renderer.draw(model);assert(renderer.save_preview(language==re4::Language::English?
            "build/installer-mapping-en.bmp":"build/installer-mapping-ru.bmp"));
        model.page=re4::InstallerPage::MappingTargets;model.mapping_source=19;model.selected=20;
        renderer.draw(model);assert(renderer.save_preview(language==re4::Language::English?
            "build/installer-mapping-targets-en.bmp":"build/installer-mapping-targets-ru.bmp"));
        model.page=re4::InstallerPage::Controls;model.selected=unsigned(re4::ControlId::CameraReturn);
        renderer.draw(model);assert(renderer.save_preview(language==re4::Language::English?
            "build/installer-camera-en.bmp":"build/installer-camera-ru.bmp"));
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
