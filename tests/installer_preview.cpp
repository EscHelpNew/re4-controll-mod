#include "re4/installer_ui.hpp"
#include "re4/graphics_preview.hpp"
#include "re4/gyro_preview.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <cstring>
#include <cmath>
#include <limits>
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
void check_deadzone(const char* path,float left,float right){
    auto* image=SDL_LoadBMP(path);assert(image);
    for(unsigned axis=0;axis<4;++axis){
        const int half=int(std::lround((axis<2?left:right)*214.f));
        int first=600,last=0;unsigned hatched=0;
        for(int x=136;x<564;++x)for(int y=281+int(axis)*49;y<290+int(axis)*49;++y){
            const auto c=pixel_at(image,x,y);
            if(x==350)continue; // The always-visible center line is separate.
            if(c.r>=45 && c.r<=120 && c.g<60 && c.b<55){
                assert(c.r>c.g && c.g>=c.b);++hatched;
                first=std::min(first,x);last=std::max(last,x);
                assert(x>=350-half && x<=350+half);
            }
        }
        if(half){assert(hatched>unsigned(half*8));assert(first==350-half && last==350+half);}
        else assert(hatched==0);
    }
    SDL_FreeSurface(image);
}
void check_gyro_marker(const char* path,int x,int y){
    auto* image=SDL_LoadBMP(path);assert(image && image->w==1280 && image->h==720);
    const auto center=pixel_at(image,x,y);
    assert(center.r>=220 && center.g<100 && center.b<80);
    const auto ring=pixel_at(image,x+8,y);
    assert(ring.r>180 && ring.g>180 && ring.b>160);
    SDL_FreeSurface(image);
}
void check_gyro_scene(const char* path,unsigned selection){
    auto* image=SDL_LoadBMP(path);assert(image);
    const auto upper=pixel_at(image,24,200),lower=pixel_at(image,24,400);
    // Both samples are behind the translucent panel. An opaque fill erases
    // their scene variation; a missing panel leaves the original much brighter.
    assert(upper.r>10 && upper.r<24 && lower.r>upper.r && lower.r<30);
    const auto scene=pixel_at(image,380,200);
    assert(scene.r>150 && scene.g>130 && scene.b>90);
    unsigned variety=0;const auto base=pixel_at(image,500,100);
    for(int y=90;y<610;y+=4)for(int x=410;x<1230;x+=4){
        const auto c=pixel_at(image,x,y);
        if(std::abs(int(c.r)-base.r)+std::abs(int(c.g)-base.g)+std::abs(int(c.b)-base.b)>45)++variety;
    }
    assert(variety>3000);
    const auto selected=pixel_at(image,29,selection<7?90+int(selection)*52:465);
    assert(selected.r==172 && selected.g==48 && selected.b==36);
    // All seven settings and apply remain visible together.
    for(unsigned row=0;row<8;++row){
        unsigned letters=0;const int y=row<7?88+int(row)*52:466;
        for(int py=y;py<y+28;++py)for(int x=39;x<250;++x){
            const auto c=pixel_at(image,x,py);
            if(c.r>115 && c.g>110 && c.b>90)++letters;
        }
        assert(letters>50);
    }
    SDL_FreeSurface(image);
}
void check_gyro_native_direction(re4::InstallerRenderer& renderer,re4::InstallerModel& model,const std::string& stem){
    // Native forward=(sin heading,cos heading), screen right=(-cos heading,sin heading).
    // Feed the real preview, then verify the rendered marker follows that basis
    // for both rate signs and X inversion, without changing vertical motion.
    for(float raw_yaw:{-15.f,15.f})for(bool inverted:{false,true}){
        re4::GyroPreview preview;re4::Settings settings;
        settings.gyro_enabled=true;settings.gyro_speed=settings.gyro_gain_x=settings.gyro_gain_y=1;
        settings.gyro_deadzone_dps=0;settings.invert_x=inverted;settings.invert_y=false;
        re4::AimDelta delta;
        for(unsigned frame=0;frame<3;++frame){
            preview.begin_frame(1000000+640000*frame,settings,true);
            for(unsigned sample=0;sample<7;++sample)
                preview.push({1+7*frame+sample,5000000,raw_yaw,-7.5f},true);
            delta=preview.finish_frame();
        }
        const bool positive_yaw=(raw_yaw>0)!=inverted;
        assert(std::fabs(delta.yaw-(positive_yaw?.5f:-.5f))<.000002f);
        assert(std::fabs(delta.pitch+.25f)<.000002f);
        model.diagnostic.gyro_preview[0]=delta.yaw;model.diagnostic.gyro_preview[1]=delta.pitch;
        const auto path=stem+"-direction-"+(raw_yaw>0?"positive":"negative")+(inverted?"-inverted.bmp":"-normal.bmp");
        renderer.draw(model);assert(renderer.save_preview(path.c_str()));
        const int x=positive_yaw?675:945;
        check_gyro_marker(path.c_str(),x,418);
        for(float heading:{-3.14159265f,-1.57079633f,0.f,1.57079633f,3.14159265f}){
            const float screen_right=-std::cos(heading)*std::sin(heading+delta.yaw)
                +std::sin(heading)*std::cos(heading+delta.yaw);
            assert(screen_right*(x-810)>0);
        }
    }
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
        const auto edited_graphics=model.settings;
        re4::copy_profile(model.settings,re4::quality_graphics_profile(),re4::ProfileDomain::Graphics);
        assert(model.item_value(unsigned(re4::GraphicsId::Profile))==(language==re4::Language::English?"Quality":"Качество"));
        renderer.draw(model);assert(renderer.save_preview(language==re4::Language::English?
            "build/installer-graphics-quality-en.bmp":"build/installer-graphics-quality-ru.bmp"));
        model.settings=edited_graphics;
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
    model.diagnostic.raw[0]=0;model.diagnostic.processed[0]=0;
    for(float deadzone:{0.f,0.05f,0.30f}){
        model.settings.left_deadzone=deadzone;model.settings.right_deadzone=deadzone/2;
        renderer.draw(model);assert(renderer.save_preview("build/installer-diagnostics-deadzones.bmp"));
        check_deadzone("build/installer-diagnostics-deadzones.bmp",deadzone,deadzone/2);
    }
    model.settings.language=re4::Language::Russian;model.settings.left_deadzone=0.05f;model.settings.right_deadzone=0.05f;
    renderer.draw(model);assert(renderer.save_preview("build/installer-diagnostics-ru.bmp"));
    check_deadzone("build/installer-diagnostics-ru.bmp",0.05f,0.05f);
    model.settings.left_deadzone=std::numeric_limits<float>::quiet_NaN();
    model.settings.right_deadzone=std::numeric_limits<float>::infinity();
    renderer.draw(model);assert(renderer.save_preview("build/installer-diagnostics-invalid.bmp"));
    check_deadzone("build/installer-diagnostics-invalid.bmp",0.f,0.f);
    for(auto language:{re4::Language::English,re4::Language::Russian}){
        model={};model.settings.language=language;
        model.page=re4::InstallerPage::Diagnostics;
        model.handle(re4::UiEvent::OpenGyroTest);
        model.diagnostic.connected=true;model.diagnostic.sensors=1;
        model.diagnostic.gyro_rate[0]=0.025f;model.diagnostic.gyro_rate[1]=-0.05f;
        model.diagnostic.gyro_preview[0]=0;model.diagnostic.gyro_preview[1]=0;
        const std::string stem="build/installer-gyro-test-"+std::string(language==re4::Language::English?"en":"ru");
        renderer.draw(model);assert(renderer.save_preview((stem+"-first.bmp").c_str()));
        check_gyro_marker((stem+"-first.bmp").c_str(),810,350);
        check_gyro_scene((stem+"-first.bmp").c_str(),0);
        assert(model.item_count()==8);
        model.selected=7;renderer.draw(model);assert(renderer.save_preview((stem+"-apply.bmp").c_str()));
        check_gyro_scene((stem+"-apply.bmp").c_str(),7);
        check_gyro_native_direction(renderer,model,stem);
        model.diagnostic.gyro_preview[0]=0.5f;model.diagnostic.gyro_preview[1]=-0.25f;
        renderer.draw(model);assert(renderer.save_preview((stem+"-moved.bmp").c_str()));
        check_gyro_marker((stem+"-moved.bmp").c_str(),675,418);
        model.diagnostic.gyro_preview[0]=100.f;model.diagnostic.gyro_preview[1]=100.f;
        renderer.draw(model);assert(renderer.save_preview((stem+"-clamped.bmp").c_str()));
        check_gyro_marker((stem+"-clamped.bmp").c_str(),390,70);
        model.diagnostic.gyro_preview[0]=-100.f;model.diagnostic.gyro_preview[1]=-100.f;
        renderer.draw(model);assert(renderer.save_preview((stem+"-clamped-low.bmp").c_str()));
        check_gyro_marker((stem+"-clamped-low.bmp").c_str(),1244,640);
        model.diagnostic.gyro_preview[0]=std::numeric_limits<float>::quiet_NaN();
        renderer.draw(model);assert(renderer.save_preview((stem+"-invalid.bmp").c_str()));
        check_gyro_marker((stem+"-invalid.bmp").c_str(),810,350);
        model.diagnostic.gyro_preview[0]=0;model.diagnostic.gyro_preview[1]=0;
        model.diagnostic.connected=false;model.diagnostic.sensors=0;
        renderer.draw(model);assert(renderer.save_preview((stem+"-no-sensor.bmp").c_str()));
        model.diagnostic.connected=true;model.diagnostic.sensors=1;model.diagnostic.calibration_remaining=100;
        model.status=re4::InstallerStatus::Calibrating;
        renderer.draw(model);assert(renderer.save_preview((stem+"-calibrating.bmp").c_str()));
        auto* progress=SDL_LoadBMP((stem+"-calibrating.bmp").c_str());assert(progress);
        const auto done=pixel_at(progress,194,692),pending=pixel_at(progress,196,692);
        assert(done.r==172 && done.g==48 && done.b==36 && pending.r==35 && pending.g==38 && pending.b==29);
        SDL_FreeSurface(progress);
    }
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
        // Recognize the original 4, rather than accepting the shared red RE logo.
        auto* re4_frame=SDL_LoadBMP(re4_path);assert(re4_frame);
        const auto diagonal=pixel_at(re4_frame,114,63),left_stem=pixel_at(re4_frame,82,68);
        assert(diagonal.r>200 && diagonal.g<30 && diagonal.b<30);
        assert(left_stem.r<30 && left_stem.g<30 && left_stem.b<30);
        SDL_FreeSurface(re4_frame);
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
