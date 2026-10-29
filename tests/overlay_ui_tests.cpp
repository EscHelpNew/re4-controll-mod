#define main re4_overlay_entry
#include "../source/overlay/main.cpp"
#undef main
#include <cassert>
namespace {
unsigned saves=0,sends=0;
}
namespace overlay_storage {bool ready(){return true;}bool mount(){return true;}void close(){}}
namespace re4 {
bool save_settings_file(const char*,const Settings& s){++saves;return valid_settings(s);}
bool load_settings_file(const char*,Settings&){return false;}
}
void GameLink::initialize(){}
void GameLink::close(){}
bool GameLink::connect(){return true;}
bool GameLink::apply(const re4::Settings& s){++sends;return re4::valid_settings(s);}
int main(){
    settings={};ControlsGui gui;
    auto* frame=static_cast<tsl::elm::OverlayFrame*>(gui.createUI());
    frame->layout(0,0,448,720);
    auto* list=static_cast<tsl::elm::List*>(frame->content());
    unsigned descriptions=0;
    const auto before=sends;
    for(auto* e:list->children)if(auto* item=dynamic_cast<tsl::elm::ListItem*>(e)){
        gui.focused=item;gui.update();
        tsl::gfx::Renderer renderer;frame->draw(&renderer);
        assert(!renderer.text.empty() && !renderer.text.back().value.empty());
        assert(list->y+list->height+16<=unsigned(renderer.text.back().y));
        ++descriptions;
    }
    assert(descriptions==20 && sends==before && saves==0); // Hover must not save/send.
    assert(frame->title=="RE4 Controls");
    for(auto* e:list->children)if(auto* item=dynamic_cast<tsl::elm::ListItem*>(e)){
        if(item->label=="Connect to game"){
            const auto before_connect=sends;
            assert(item->click(HidNpadButton_A));
            assert(sends==before_connect+1 && saves==0); // Saved gyro settings must reach a restarted module.
        }
    }
    tsl::elm::ListItem* language=nullptr;
    for(auto* e:list->children)if(auto* item=dynamic_cast<tsl::elm::ListItem*>(e)){
        assert(item->label.find("Walk")==std::string::npos);
        if(item->label=="Language")language=item;
    }
    assert(language && language->click(HidNpadButton_Right));
    assert(settings.language==re4::Language::Russian && frame->title=="RE4 Управление");
    gui.focused=language;gui.update();tsl::gfx::Renderer russian;frame->draw(&russian);
    assert(russian.text.back().value.find("Русский")!=std::string::npos);
    for(auto* e:list->children)if(auto* item=dynamic_cast<tsl::elm::ListItem*>(e)){
        assert(item->label.find("ходьбы")==std::string::npos);
        if(item->label=="Кривая стиков"){
            assert(item->click(HidNpadButton_Right));assert(settings.stick_curve==re4::StickCurve::Exponential);
        }
        if(item->label=="Скорость поворота"
           || item->label=="Чувствительность камеры" || item->label=="Прицел по горизонтали"
           || item->label=="Прицел по вертикали"){
            for(unsigned i=0;i<12;++i)assert(item->click(HidNpadButton_Right));
        }
    }
    assert(settings.turn_gain==1 && settings.movement_gain==1 && settings.camera_gain==1
           && settings.aim_gain_x==1 && settings.aim_gain_y==1);
    settings.gyro_enabled=true;settings.turn_gain=0.5f;settings.invert_x=settings.invert_y=false;
    for(auto* e:list->children)if(auto* item=dynamic_cast<tsl::elm::ListItem*>(e))
        if(item->label=="Сбросить настройки"){
            const auto before_reset=saves;
            settings.gyro_bias_x=1.2f;
            assert(!item->click(HidNpadButton_Left) && !item->click(HidNpadButton_Right));
            assert(settings.turn_gain==0.5f && settings.gyro_bias_x==1.2f && saves==before_reset);
            assert(item->click(HidNpadButton_A));
        }
    assert(!settings.gyro_enabled && settings.turn_gain==1 && settings.invert_x && settings.invert_y);
    assert(settings.language==re4::Language::English && settings.left_deadzone==0.05f);
    for(auto* e:list->children)if(auto* item=dynamic_cast<tsl::elm::ToggleListItem*>(e)){
        if(item->label=="Invert gyro X"||item->label=="Invert gyro Y")assert(item->state && item->value=="On");
        if(item->label=="Gyro aiming")assert(!item->state && item->value=="Off");
    }
    delete frame;
    std::puts("actual overlay UI: every focused item has help, no hover writes, speed caps and reset PASS");
}
