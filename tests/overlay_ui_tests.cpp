#define main re4_overlay_entry
#include "../source/overlay/main.cpp"
#undef main
#include <cassert>
namespace {
unsigned saves=0,sends=0;
bool storage_available=true,have_saved=false,saved_gyro=true,last_sent_gyro=false;
}
namespace overlay_storage {bool ready(){return storage_available;}bool mount(){return storage_available;}void close(){}}
namespace re4 {
bool save_settings_file(const char* path,const Settings& s){if(std::string(path)==overlay_storage::settings_path)++saves;return valid_settings(s);}
bool load_settings_file(const char*,Settings& s){if(have_saved)s.gyro_enabled=saved_gyro;return have_saved;}
}
void GameLink::initialize(){}
void GameLink::close(){}
bool GameLink::connect(){return true;}
bool GameLink::apply(const re4::Settings& s){++sends;last_sent_gyro=s.gyro_enabled;return re4::valid_settings(s);}
int main(){
    {
        ControlsOverlay overlay;tsl::Overlay& lifecycle=overlay;
        have_saved=true;
        for(bool enabled:{false,true}){
            settings={};saved_gyro=enabled;const auto before=sends;
            lifecycle.initServices();assert(settings.gyro_enabled==enabled && last_sent_gyro==enabled && sends==before+1 && saves==0);
            lifecycle.exitServices();
        }
        const auto before=sends;
        storage_available=false;settings={};
        lifecycle.initServices();assert(sends==before && saves==0);
        lifecycle.exitServices();storage_available=true;have_saved=false;
        lifecycle.initServices();assert(sends==before && saves==0);lifecycle.exitServices();
    }
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
    assert(descriptions==unsigned(re4::ControlId::Count)+1 && sends==before && saves==0); // Hover must not save/send.
    assert(frame->title=="RE4 Controls");
    MainGui root_gui;
    auto* root_frame=static_cast<tsl::elm::OverlayFrame*>(root_gui.createUI());
    auto* root_list=static_cast<tsl::elm::List*>(root_frame->content());
    for(auto* e:root_list->children)if(auto* item=dynamic_cast<tsl::elm::ListItem*>(e))if(item->label=="Graphics"){
        const auto before_open_saves=saves,before_open_sends=sends;
        assert(!item->click(HidNpadButton_Left) && !item->click(HidNpadButton_Right));
        assert(item->click(HidNpadButton_A) && tsl::navigation.size()==1);
        auto* graphics_gui=tsl::navigation.back().get();
        auto* graphics_frame=static_cast<tsl::elm::OverlayFrame*>(graphics_gui->root);
        graphics_frame->layout(0,0,448,720);
        auto* graphics_list=static_cast<tsl::elm::List*>(graphics_frame->content());
        assert(graphics_frame->title=="Graphics" && graphics_list->children.size()==6);
        for(auto* child:graphics_list->children){
            graphics_gui->focused=child;graphics_gui->update();
            tsl::gfx::Renderer help;graphics_frame->draw(&help);
            assert(help.text.back().value.size()>20);
        }
        assert(saves==before_open_saves && sends==before_open_sends);
        tsl::goBack();assert(saves==before_open_saves && sends==before_open_sends);
        assert(item->click(HidNpadButton_A));graphics_gui=tsl::navigation.back().get();
        graphics_frame=static_cast<tsl::elm::OverlayFrame*>(graphics_gui->root);
        graphics_list=static_cast<tsl::elm::List*>(graphics_frame->content());
        auto* filter=static_cast<tsl::elm::ListItem*>(graphics_list->children[0]);
        assert(filter->click(HidNpadButton_Right) && settings.texture_filtering==re4::TextureFilter::Bilinear);
        assert(saves==before_open_saves+1 && sends==before_open_sends+1);
        auto* aa=static_cast<tsl::elm::ListItem*>(graphics_list->children[1]);
        assert(aa->click(HidNpadButton_A) && settings.anti_aliasing==re4::AntiAliasing::HandheldSsaa && aa->value=="Handheld SSAA");
        auto* shadow=static_cast<tsl::elm::ListItem*>(graphics_list->children[3]);
        assert(shadow->click(HidNpadButton_Left) && settings.shadow_mode==re4::ShadowMode::Off && shadow->value=="Off");
        auto* distance=static_cast<tsl::elm::ListItem*>(graphics_list->children[2]);
        assert(!distance->click(HidNpadButton_A));
        assert(distance->click(HidNpadButton_Left) && settings.draw_distance_scale==0.75f);
        assert(distance->click(HidNpadButton_Left) && settings.draw_distance_scale==0.5f);
        const auto at_limit=saves;assert(distance->click(HidNpadButton_Left) && saves==at_limit);
        settings.language=re4::Language::Russian;graphics_gui->focused=filter;graphics_gui->update();
        assert(graphics_frame->title=="Графика" && filter->label=="Фильтрация текстур");
        tsl::gfx::Renderer ru;graphics_frame->draw(&ru);assert(ru.text.back().value.find("перезапуск")!=std::string::npos);
        settings.free_camera=true;settings.button_map[0]=255;settings.gyro_speed=0.3f;
        auto* reset=static_cast<tsl::elm::ListItem*>(graphics_list->children[5]);
        assert(!reset->click(HidNpadButton_Left) && !reset->click(HidNpadButton_Right) && saves==at_limit);
        assert(reset->click(HidNpadButton_A) && saves==at_limit+1);
        assert(reset->click(HidNpadButton_A) && saves==at_limit+1);
        assert(settings.texture_filtering==re4::TextureFilter::Native && settings.draw_distance_scale==1
            && settings.anti_aliasing==re4::AntiAliasing::Native && settings.shadow_mode==re4::ShadowMode::Native);
        assert(settings.language==re4::Language::Russian && settings.free_camera && settings.button_map[0]==255 && settings.gyro_speed==0.3f);
        tsl::goBack();settings={};
    }
    const auto after_graphics_saves=saves;
    for(auto* e:list->children)if(auto* item=dynamic_cast<tsl::elm::ListItem*>(e))if(item->label=="Button mapping"){
        assert(!item->click(HidNpadButton_Left) && !item->click(HidNpadButton_Right));
        assert(item->click(HidNpadButton_A));
        assert(tsl::navigation.size()==1 && sends>before && saves==after_graphics_saves);
    }
    auto* mapping_gui=tsl::navigation.back().get();
    auto* mapping_frame=static_cast<tsl::elm::OverlayFrame*>(mapping_gui->root);
    mapping_frame->layout(0,0,448,720);
    auto* mapping_list=static_cast<tsl::elm::List*>(mapping_frame->content());
    assert(mapping_list->children.size()==22);
    for(unsigned source=0;source<20;++source){
        auto* source_item=static_cast<tsl::elm::ListItem*>(mapping_list->children[source+1]);
        assert(source_item->label==re4::button_name(source) && source_item->value==re4::button_name(source));
        const auto old_saves=saves,old_sends=sends;
        mapping_gui->focused=source_item;mapping_gui->update();
        tsl::gfx::Renderer help;mapping_frame->draw(&help);
        assert(help.text.back().value.find("HOME/Capture")!=std::string::npos);
        assert(source_item->click(HidNpadButton_A) && tsl::navigation.size()==2);
        auto* picker=tsl::navigation.back().get();
        auto* picker_frame=static_cast<tsl::elm::OverlayFrame*>(picker->root);
        auto* targets=static_cast<tsl::elm::List*>(picker_frame->content());
        assert(targets->children.size()==21);
        for(unsigned target=0;target<21;++target){
            auto* row=static_cast<tsl::elm::ListItem*>(targets->children[target]);
            assert(row->label==(target==20?"Unbound":re4::button_name(target)));
            picker->focused=row;picker->update();
            assert(!row->click(HidNpadButton_Right));
        }
        assert(saves==old_saves && sends==old_sends);
        tsl::goBack();assert(tsl::navigation.size()==1 && saves==old_saves && sends==old_sends);
        assert(source_item->click(HidNpadButton_A));
        picker_frame=static_cast<tsl::elm::OverlayFrame*>(tsl::navigation.back()->root);
        targets=static_cast<tsl::elm::List*>(picker_frame->content());
        assert(static_cast<tsl::elm::ListItem*>(targets->children[20])->click(HidNpadButton_A));
        assert(tsl::navigation.size()==1 && settings.button_map[source]==255 && saves==old_saves+1 && sends==old_sends+1);
        mapping_gui->update();assert(source_item->value=="Unbound");
    }
    auto* first_source=static_cast<tsl::elm::ListItem*>(mapping_list->children[1]);
    for(unsigned target=0;target<21;++target){
        assert(first_source->click(HidNpadButton_A));
        auto* picker_frame=static_cast<tsl::elm::OverlayFrame*>(tsl::navigation.back()->root);
        auto* targets=static_cast<tsl::elm::List*>(picker_frame->content());
        const auto old_saves=saves,old_sends=sends;
        assert(static_cast<tsl::elm::ListItem*>(targets->children[target])->click(HidNpadButton_A));
        assert(settings.button_map[0]==(target==20?255:target));
        assert(saves==old_saves+1 && sends==old_sends+1);
    }
    assert(first_source->click(HidNpadButton_A));
    auto* same_picker=static_cast<tsl::elm::OverlayFrame*>(tsl::navigation.back()->root);
    const auto unchanged_saves=saves,unchanged_sends=sends;
    assert(static_cast<tsl::elm::ListItem*>(static_cast<tsl::elm::List*>(same_picker->content())->children[20])->click(HidNpadButton_A));
    assert(saves==unchanged_saves && sends==unchanged_sends);
    auto* reset_mapping=static_cast<tsl::elm::ListItem*>(mapping_list->children[21]);
    const auto before_mapping_reset=saves;
    settings.gyro_speed=0.3f;settings.free_camera=true;settings.remap_enabled=false;
    assert(!reset_mapping->click(HidNpadButton_Left) && !reset_mapping->click(HidNpadButton_Right));
    assert(saves==before_mapping_reset && settings.button_map[0]==255);
    assert(reset_mapping->click(HidNpadButton_A) && saves==before_mapping_reset+1);
    assert(settings.remap_enabled && settings.button_map==re4::identity_button_map());
    assert(settings.free_camera && settings.gyro_speed==0.3f);
    assert(reset_mapping->click(HidNpadButton_A) && saves==before_mapping_reset+1);
    settings.language=re4::Language::Russian;
    assert(static_cast<tsl::elm::ListItem*>(mapping_list->children[1])->click(HidNpadButton_A));
    auto* russian_picker=static_cast<tsl::elm::OverlayFrame*>(tsl::navigation.back()->root);
    assert(static_cast<tsl::elm::ListItem*>(static_cast<tsl::elm::List*>(russian_picker->content())->children[20])->label=="Не назначено");
    tsl::goBack();tsl::goBack();settings.language=re4::Language::English;
    for(auto* e:list->children)if(auto* item=dynamic_cast<tsl::elm::ListItem*>(e)){
        if(item->label=="Connect to game"){
            const auto before_connect=sends,before_connect_saves=saves;
            assert(item->click(HidNpadButton_A));
            assert(sends==before_connect+1 && saves==before_connect_saves); // Saved gyro settings must reach a restarted module.
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
    assert(settings.gyro_speed==0.8f && settings.camera_return_speed==1 && !settings.free_camera
        && settings.remap_enabled && settings.button_map==re4::identity_button_map());
    for(auto* e:list->children)if(auto* item=dynamic_cast<tsl::elm::ToggleListItem*>(e)){
        if(item->label=="Invert gyro X"||item->label=="Invert gyro Y")assert(item->state && item->value=="On");
        if(item->label=="Gyro aiming")assert(!item->state && item->value=="Off");
    }
    delete root_frame;delete frame;
    std::puts("actual overlay UI: every focused item has help, no hover writes, speed caps and reset PASS");
}
