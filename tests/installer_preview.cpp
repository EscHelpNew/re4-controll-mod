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
    model.page=re4::InstallerPage::Diagnostics;model.settings.language=re4::Language::English;
    model.diagnostic.connected=true;model.diagnostic.sensors=1;model.diagnostic.raw[0]=0.65f;
    model.diagnostic.processed[0]=0.5f;
    renderer.draw(model);assert(renderer.save_preview("build/installer-diagnostics-en.bmp"));
    renderer.close();renderer.close();
    std::puts("actual installer renderer: EN/RU frames, diagnostics and cleanup PASS");
}
