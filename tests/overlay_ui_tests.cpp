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
tsl::elm::List* list(tsl::elm::OverlayFrame* f){return static_cast<tsl::elm::List*>(f->content());}
tsl::elm::ListItem* find(tsl::elm::OverlayFrame* f,const std::string& label){
    for(auto* e:list(f)->children)if(auto* row=dynamic_cast<tsl::elm::ListItem*>(e))if(row->label==label)return row;
    return nullptr;
}
unsigned markers(const tsl::gfx::Renderer& r){unsigned n=0;for(const auto& rect:r.rects)if(rect.color==0x8fff)++n;return n;}
}
namespace overlay_storage {bool ready(){return storage_available;}bool mount(){return storage_available;}void close(){}}
namespace re4 {
bool save_settings_file(const char* path,const Settings& s){if(std::string(path)==overlay_storage::settings_path)++saves;return valid_settings(s);}
bool load_settings_file(const char*,Settings& s){if(have_saved)s.gyro_enabled=saved_gyro;return have_saved;}
}
void GameLink::initialize(){}void GameLink::close(){}bool GameLink::connect(){return true;}
bool GameLink::apply(const re4::Settings& s){++sends;last_sent_gyro=s.gyro_enabled;return re4::valid_settings(s);}
int main(){
    using namespace re4;
    {
        ControlsOverlay overlay;tsl::Overlay& lifecycle=overlay;have_saved=true;
        for(bool enabled:{false,true}){settings={};saved_gyro=enabled;const auto before=sends;
            lifecycle.initServices();assert(settings.gyro_enabled==enabled && last_sent_gyro==enabled && sends==before+1 && saves==0);lifecycle.exitServices();}
        const auto before=sends;storage_available=false;settings={};lifecycle.initServices();
        assert(sends==before && saves==0);lifecycle.exitServices();storage_available=true;have_saved=false;
    }
    settings={};note.clear();MainGui main;
    auto* root=static_cast<tsl::elm::OverlayFrame*>(main.createUI());
    assert(find(root,"Language") && list(root)->children.size()==4);
    auto* language=find(root,"Language");
    ControlsGui groups;auto* frame=static_cast<tsl::elm::OverlayFrame*>(groups.createUI());frame->layout(0,0,448,720);
    assert(list(frame)->children.size()==unsigned(ControlGroup::Count) && !find(frame,"Language"));
    std::set<unsigned> displayed;
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
    assert(language->click(HidNpadButton_Right) && settings.language==Language::Russian);
    main.update();groups.update();assert(language->label=="Язык" && language->value=="Русский");
    assert(find(frame,"Гироскоп") && !find(frame,"Язык"));
    auto* gyro=find(frame,"Гироскоп");assert(gyro->click(HidNpadButton_A));
    auto* gyro_page=static_cast<tsl::elm::OverlayFrame*>(tsl::navigation.back()->root);
    assert(find(gyro_page,"Прицеливание гироскопом"));tsl::goBack();
    delete root;delete frame;
    std::puts("actual overlay UI: groups, root-only language, mapping and live reticle preview PASS");
}
