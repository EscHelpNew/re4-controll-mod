#define OVERLAY_TEST_TRACK_ALLOCATIONS
#define main re4_overlay_entry
#include "../source/overlay/main.cpp"
#undef main
#include <cassert>
#include <set>
#undef assert
#define assert(c) do{if(!(c)){std::fprintf(stderr,"overlay UI line %d: %s\n",__LINE__,#c);std::exit(1);}}while(false)
namespace {
unsigned saves=0,sends=0;
bool storage_available=true,have_saved=false,saved_gyro=true,last_sent_gyro=false;
bool saved_enemy=false,last_sent_enemy=false,saved_numbers=false,last_sent_numbers=false;
tsl::elm::List* list(tsl::elm::OverlayFrame* f){return static_cast<tsl::elm::List*>(f->content());}
tsl::elm::ListItem* find(tsl::elm::OverlayFrame* f,const std::string& label){
    for(auto* e:list(f)->children)if(auto* row=dynamic_cast<tsl::elm::ListItem*>(e))if(row->label==label)return row;
    return nullptr;
}
unsigned markers(const tsl::gfx::Renderer& r){unsigned n=0;for(const auto& rect:r.rects)if(rect.color==0x8fff)++n;return n;}
void graphics_illustrations(){
    settings={};note.clear();GraphicsGui gui;
    auto* frame=static_cast<tsl::elm::OverlayFrame*>(gui.createUI());frame->layout(0,0,448,720);
    assert(list(frame)->y+list(frame)->height<=322 && list(frame)->children.size()==6);
    const int regions[][4]={{179,147,51,29},{241,77,72,44},{174,0,76,46},{193,151,40,26},{0,0,0,0},{0,0,0,0}};
    std::set<std::string> captions;
    const auto old_saves=saves,old_sends=sends;
    for(auto language:{re4::Language::English,re4::Language::Russian}){
        settings.language=language;
        for(unsigned row=0;row<6;++row){
            gui.focused=list(frame)->children[row];gui.update();tsl::gfx::Renderer render;
            overlay_test_allocations::largest=0;overlay_test_allocations::track=true;
            frame->draw(&render);overlay_test_allocations::track=false;
            assert(overlay_test_allocations::largest<65536);
            assert(render.bitmaps.size()==180 && render.text.size()==2);
            assert(render.text[0].x==20 && render.text[0].y==353 && render.text.back().y==635);
            assert(!render.text[0].value.empty() && !render.text.back().value.empty());
            captions.insert(render.text.back().value);
            unsigned varied=0;
            for(unsigned line=0;line<180;++line){const auto& b=render.bitmaps[line];
                assert(b.x==64 && b.y==443+int(line) && b.w==320 && b.h==1);
                assert(b.source==render.bitmaps.front().source);varied+=b.distinct;
            }
            assert(varied>10000);
            assert(render.rects.size()==(row<4?5u:1u));
            if(row<4){const auto& outline=render.rects[1];
                assert(outline.x==64+regions[row][0] && outline.y==443+regions[row][1]);
                assert(outline.w==regions[row][2] && outline.h==2 && outline.color==0xF93B);
                for(unsigned i=1;i<render.rects.size();++i){const auto& r=render.rects[i];
                    assert(r.x>=64 && r.y>=443 && r.x+r.w<=384 && r.y+r.h<=623);
                }
            }
        }
    }
    assert(captions.size()==10 && saves==old_saves && sends==old_sends);delete frame;
}
}
namespace overlay_storage {bool ready(){return storage_available;}bool mount(){return storage_available;}void close(){}}
namespace re4 {
bool save_settings_file(const char* path,const Settings& s){if(std::string(path)==overlay_storage::settings_path){++saves;saved_enemy=s.enemy_health_bars;saved_numbers=s.enemy_health_numbers;}return valid_settings(s);}
bool load_settings_file(const char*,Settings& s){if(have_saved){s.gyro_enabled=saved_gyro;s.enemy_health_bars=saved_enemy;s.enemy_health_numbers=saved_numbers;}return have_saved;}
}
void GameLink::initialize(){}void GameLink::close(){}bool GameLink::connect(){return true;}
bool GameLink::apply(const re4::Settings& s){++sends;last_sent_gyro=s.gyro_enabled;last_sent_enemy=s.enemy_health_bars;last_sent_numbers=s.enemy_health_numbers;return re4::valid_settings(s);}
int main(){
    using namespace re4;
    graphics_illustrations();settings={};
    {
        ControlsOverlay overlay;tsl::Overlay& lifecycle=overlay;have_saved=true;
        for(bool enabled:{false,true}){settings={};saved_gyro=enabled;const auto before=sends;
            lifecycle.initServices();assert(settings.gyro_enabled==enabled && last_sent_gyro==enabled && sends==before+1 && saves==0);lifecycle.exitServices();}
        const auto before=sends;storage_available=false;settings={};lifecycle.initServices();
        assert(sends==before && saves==0);lifecycle.exitServices();storage_available=true;have_saved=false;
    }
    settings={};note.clear();MainGui main;
    auto* root=static_cast<tsl::elm::OverlayFrame*>(main.createUI());
    assert(find(root,"Language") && find(root,"Game") && list(root)->children.size()==5);
    auto* language=find(root,"Language");
    for(auto lang:{Language::English,Language::Russian}){
        settings.language=lang;main.update();
        auto* game=find(root,lang==Language::English?"Game":"Игра");assert(game);
        assert(static_cast<tsl::elm::ListItem*>(list(root)->children[2])->label==(lang==Language::English?"Graphics":"Графика"));
        assert(list(root)->children[3]==game);
        assert(!game->click(HidNpadButton_Left) && !game->click(HidNpadButton_Right));
        assert(game->click(HidNpadButton_A) && tsl::navigation.size()==1);
        auto* page=tsl::navigation.back().get();
        auto* content=static_cast<tsl::elm::OverlayFrame*>(page->root);content->layout(0,0,448,720);
        assert(content->title==(lang==Language::English?"Game":"Игра"));
        assert(list(content)->children.size()==2 && !find(content,"Language") && !find(content,"Язык"));
        auto* health=dynamic_cast<tsl::elm::ToggleListItem*>(list(content)->children[0]);assert(health);
        auto* numbers=find(content,lang==Language::English?"Health numbers":"Числа здоровья");assert(numbers);
        assert(list(content)->children[1]==numbers && !settings.enemy_health_numbers);
        assert(numbers->value==(lang==Language::English?"Unavailable":"Недоступно"));
        const auto disabled_saves=saves,disabled_sends=sends;
        for(auto key:{HidNpadButton_A,HidNpadButton_Left,HidNpadButton_Right})assert(!numbers->click(key));
        assert(!settings.enemy_health_numbers && saves==disabled_saves && sends==disabled_sends);
        page->focused=numbers;page->update();tsl::gfx::Renderer disabled_help;content->draw(&disabled_help);
        assert(disabled_help.bitmaps.size()==222);
        assert(disabled_help.bitmaps.front().x==96 && disabled_help.bitmaps.front().y==396);
        assert(disabled_help.bitmaps.back().y+disabled_help.bitmaps.back().h<=647);
        assert(disabled_help.text.back().y<=640); // Keep the real Tesla navigation footer clear.
        assert(disabled_help.text.front().value.find(lang==Language::English?"Enable enemy health bars first":"Сначала включите полоски здоровья врагов")!=std::string::npos);
        assert(health->label==(lang==Language::English?"Enemy health bars":"Полоска здоровья врагов"));
        assert(!health->state && health->value==(lang==Language::English?"Off":"Выкл."));
        page->focused=health;page->update();tsl::gfx::Renderer help;content->draw(&help);
        assert(help.bitmaps.size()==222 && help.rects.size()==1 && !help.text.empty());
        assert(help.text.front().value.find(lang==Language::English?"aiming":"прицел")!=std::string::npos);
        const auto before_saves=saves,before_sends=sends;
        health->changed(true);
        assert(settings.enemy_health_bars && saved_enemy && last_sent_enemy && health->state);
        assert(health->value==(lang==Language::English?"On":"Вкл."));
        assert(saves==before_saves+1 && sends==before_sends+1);
        assert(numbers->value==(lang==Language::English?"Off":"Выкл."));
        assert(!numbers->click(HidNpadButton_Left) && !numbers->click(HidNpadButton_Right));
        assert(numbers->click(HidNpadButton_A));
        assert(settings.enemy_health_numbers && saved_numbers && last_sent_numbers);
        assert(numbers->value==(lang==Language::English?"On":"Вкл."));
        assert(saves==before_saves+2 && sends==before_sends+2);
        page->focused=numbers;page->update();tsl::gfx::Renderer numbers_help;content->draw(&numbers_help);
        assert(numbers_help.text.front().value.find(lang==Language::English?"weapon firepower units":"единицах мощности оружия")!=std::string::npos);
        health->changed(false);
        assert(!settings.enemy_health_bars && !saved_enemy && !last_sent_enemy && !health->state);
        assert(numbers->value==(lang==Language::English?"Unavailable":"Недоступно") && settings.enemy_health_numbers);
        for(auto key:{HidNpadButton_A,HidNpadButton_Left,HidNpadButton_Right})assert(!numbers->click(key));
        assert(saves==before_saves+3 && sends==before_sends+3 && settings.enemy_health_numbers);
        health->changed(true);
        assert(numbers->value==(lang==Language::English?"On":"Вкл."));
        assert(numbers->click(HidNpadButton_A) && !settings.enemy_health_numbers && !saved_numbers && !last_sent_numbers);
        health->changed(false);
        assert(saves==before_saves+6 && sends==before_sends+6);
        tsl::goBack();assert(tsl::navigation.empty());
    }
    settings={};main.update();
    ControlsGui groups;auto* frame=static_cast<tsl::elm::OverlayFrame*>(groups.createUI());frame->layout(0,0,448,720);
    assert(list(frame)->children.size()==unsigned(ControlGroup::Count)+1 && !find(frame,"Language"));
    std::set<unsigned> displayed{unsigned(ControlId::Profile)};
    for(const auto& group:control_groups()){
        auto* item=find(frame,group.label.english);assert(item);
        groups.focused=item;groups.update();tsl::gfx::Renderer root_help;frame->draw(&root_help);assert(!root_help.text.empty());
        assert(!item->click(HidNpadButton_Left) && !item->click(HidNpadButton_Right));
        const auto old_saves=saves,old_sends=sends;
        assert(item->click(HidNpadButton_A) && tsl::navigation.size()==1);
        auto* page=tsl::navigation.back().get();auto* content=static_cast<tsl::elm::OverlayFrame*>(page->root);content->layout(0,0,448,720);
        assert(!find(content,"Language"));
        if(group.id==ControlGroup::Mapping){
            displayed.insert(unsigned(ControlId::ButtonMapping));assert(list(content)->children.size()==22);
            for(unsigned source=0;source<kButtonCount;++source){
                auto* button=static_cast<tsl::elm::ListItem*>(list(content)->children[source+1]);assert(button->label==button_name(source));
                for(unsigned target=0;target<=kButtonCount;++target){
                    assert(button->click(HidNpadButton_A) && tsl::navigation.size()==2);
                    auto* picker=static_cast<tsl::elm::OverlayFrame*>(tsl::navigation.back()->root);
                    auto* choice=static_cast<tsl::elm::ListItem*>(list(picker)->children[target]);
                    assert(choice->label==mapping_target_label(target,Language::English));
                    assert(!choice->click(HidNpadButton_Right));assert(choice->click(HidNpadButton_A));
                    assert(tsl::navigation.size()==1 && settings.button_map[source]==(target==kButtonCount?255:target));
                }
            }
            auto* reset=static_cast<tsl::elm::ListItem*>(list(content)->children.back());assert(!reset->click(HidNpadButton_Right));
            assert(reset->click(HidNpadButton_A) && settings.button_map==identity_button_map());
        }else{
            assert(list(content)->children.size()==control_group_count(group.id));
            for(unsigned i=0;i<control_group_count(group.id);++i){
                const auto id=control_group_item(group.id,i);assert(id!=ControlId::Language && displayed.insert(unsigned(id)).second);
                auto* row=static_cast<tsl::elm::ListItem*>(list(content)->children[i]);
                assert(row->label==control_spec(id).label.english);
                page->focused=row;page->update();tsl::gfx::Renderer help;content->draw(&help);
                assert(help.bitmaps.empty());
                assert(!help.text.empty() && help.text.back().value.size()>20);
                assert(saves==old_saves && sends==old_sends);
                if(group.id!=ControlGroup::Reticle)assert(markers(help)==0);
            }
            if(group.id==ControlGroup::Reticle){
                auto* style=static_cast<tsl::elm::ListItem*>(list(content)->children[0]);
                auto* size=static_cast<tsl::elm::ListItem*>(list(content)->children[1]);page->focused=style;
                for(unsigned type=0;type<3;++type){
                    for(unsigned extent=0;extent<3;++extent){
                        page->update();tsl::gfx::Renderer render;content->draw(&render);
                        assert(markers(render)==(type==0?0u:type==1?1u:4u));
                        assert(size->click(HidNpadButton_Right));
                    }
                    assert(style->click(HidNpadButton_Right));
                }
                assert(settings.reticle==ReticleStyle::Off && settings.reticle_size==ReticleSize::Small);
            }
        }
        tsl::goBack();assert(tsl::navigation.empty());
    }
    assert(displayed.size()==unsigned(ControlId::Count)-1);
    {
        settings=default_controls_profile();
        ControlsPageGui aiming(ControlGroup::Aiming);
        auto* content=static_cast<tsl::elm::OverlayFrame*>(aiming.createUI());
        auto* reload=dynamic_cast<tsl::elm::ToggleListItem*>(find(content,"Move while reloading"));
        assert(reload && !settings.move_while_reloading);
        reload->changed(true);assert(settings.move_while_reloading);
        reload->changed(false);assert(!settings.move_while_reloading);
        delete content;
    }
    for(auto lang:{Language::English,Language::Russian}){
        settings=re6_profile();settings.language=lang;
        for(auto group:{ControlGroup::Camera,ControlGroup::Aiming}){
            ControlsPageGui page(group);
            auto* content=static_cast<tsl::elm::OverlayFrame*>(page.createUI());content->layout(0,0,448,720);
            for(auto id:{ControlId::Camera,ControlId::RightAimX,ControlId::RightAimY}){
                auto* item=find(content,control_spec(id).label.get(lang));if(!item)continue;
                page.focused=item;page.update();tsl::gfx::Renderer render;content->draw(&render);
                assert(render.text.back().value==control_help_text(control_spec(id),settings));
            }
            delete content;
        }
    }
    settings={};
    assert(language->click(HidNpadButton_Right) && settings.language==Language::Russian);
    main.update();groups.update();assert(language->label=="Язык" && language->value=="Русский");
    assert(find(frame,"Гироскоп") && !find(frame,"Язык"));
    auto* gyro=find(frame,"Гироскоп");assert(gyro->click(HidNpadButton_A));
    auto* gyro_page=static_cast<tsl::elm::OverlayFrame*>(tsl::navigation.back()->root);
    assert(find(gyro_page,"Прицеливание гироскопом"));tsl::goBack();
    delete root;delete frame;
    std::puts("actual overlay UI: groups, root-only language, mapping and live reticle preview PASS");
}
