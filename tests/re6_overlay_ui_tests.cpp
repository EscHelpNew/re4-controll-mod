#define OVERLAY_TEST_TRACK_ALLOCATIONS
#define main re6_overlay_entry
#include "../source/re6_controls/main.cpp"
#undef main
#include <cassert>
#include <cstdlib>
#include <set>
#undef assert
#define assert(c) do { if (!(c)) { std::fprintf(stderr,"RE6 graphics UI line %d: %s\n",__LINE__,#c); std::exit(1); } } while(false)
namespace {
unsigned saves=0,sends=0,connects=0,initializes=0,closes=0,storage_closes=0;
bool storage_available=true,have_saved=false,save_success=true,connect_success=true;
re6::Settings persisted,sent;
std::vector<std::string> events;
tsl::elm::List* list(tsl::elm::OverlayFrame* f){return static_cast<tsl::elm::List*>(f->content());}
tsl::elm::ListItem* find(tsl::elm::OverlayFrame* f,const std::string& text){for(auto* e:list(f)->children)if(auto* r=dynamic_cast<tsl::elm::ListItem*>(e))if(r->label==text)return r;return nullptr;}
std::string help(tsl::Gui& gui,tsl::elm::OverlayFrame* f,tsl::elm::ListItem* row){
    gui.focused=row;gui.update();f->layout(0,0,448,720);tsl::gfx::Renderer r;f->draw(&r);assert(!r.text.empty());
    if(!r.bitmaps.empty()){
        assert(r.text.front().x==20 && r.text.front().y==353 && r.text.back().y==635);
        return r.text.front().value;
    }
    assert(r.text.back().x==20 && r.text.back().y==568);assert(!r.rects.empty() && r.rects.back().y==547 && r.rects.back().color==0xF93B);
    return r.text.back().value;
}
void graphics_illustrations(){
    settings={};note.clear();GraphicsGui gui;
    auto* f=static_cast<tsl::elm::OverlayFrame*>(gui.createUI());f->layout(0,0,448,720);
    assert(list(f)->y+list(f)->height<=322);
    const int regions[][4]={{284,137,34,31},{252,12,62,80},{0,0,0,0}};
    std::set<std::string> captions;const auto old_saves=saves,old_sends=sends;
    for(auto language:{re6::Language::English,re6::Language::Russian}){
        settings.language=language;
        for(unsigned row=0;row<3;++row){
            gui.focused=list(f)->children[row];gui.update();tsl::gfx::Renderer render;
            overlay_test_allocations::largest=0;overlay_test_allocations::track=true;
            f->draw(&render);overlay_test_allocations::track=false;
            assert(overlay_test_allocations::largest<65536);
            assert(render.bitmaps.size()==180 && render.text.size()==2);
            assert(render.text.front().x==20 && render.text.front().y==353 && render.text.back().y==635);
            captions.insert(render.text.back().value);unsigned varied=0;
            for(unsigned line=0;line<180;++line){const auto& b=render.bitmaps[line];
                assert(b.x==64 && b.y==443+int(line) && b.w==320 && b.h==1);
                assert(b.source==render.bitmaps.front().source);varied+=b.distinct;
            }
            assert(varied>10000 && render.rects.size()==(row<2?5u:1u));
            if(row<2){const auto& outline=render.rects[1];
                assert(outline.x==64+regions[row][0] && outline.y==443+regions[row][1]);
                assert(outline.w==regions[row][2] && outline.h==2 && outline.color==0xF93B);
                for(unsigned i=1;i<render.rects.size();++i){const auto& r=render.rects[i];
                    assert(r.x>=64 && r.y>=443 && r.x+r.w<=384 && r.y+r.h<=623);
                }
            }
        }
    }
    assert(captions.size()==6 && saves==old_saves && sends==old_sends);delete f;settings={};
}
void navigation_and_languages(){
    settings={};note.clear();MainGui gui;auto* f=static_cast<tsl::elm::OverlayFrame*>(gui.createUI());f->layout(0,0,448,720);
    assert(f->title=="RE6 Graphics" && list(f)->children.size()==3);
    auto* graphics=find(f,"Graphics");auto* connect=find(f,"Connect to RE6");auto* language=find(f,"Language");assert(graphics && connect && language);
    const auto old_saves=saves,old_sends=sends;std::set<std::string> descriptions;
    for(auto* row:{graphics,connect,language})descriptions.insert(help(gui,f,row));
    assert(descriptions.size()==3 && saves==old_saves && sends==old_sends);
    assert(help(gui,f,graphics).find("restart")!=std::string::npos);
    assert(!graphics->click(HidNpadButton_Left) && graphics->click(HidNpadButton_A));
    auto* page=tsl::navigation.back().get();auto* group=static_cast<tsl::elm::OverlayFrame*>(page->root);group->layout(0,0,448,720);
    assert(list(group)->children.size()==3 && !find(group,"Language"));
    auto* filter=find(group,"Texture filtering");auto* aa=find(group,"Anti-aliasing");auto* reset=find(group,"Reset graphics");assert(filter && aa && reset);
    assert(help(*page,group,filter)!=help(*page,group,reset));
    assert(help(*page,group,aa).find("live")!=std::string::npos);assert(saves==old_saves && sends==old_sends);tsl::goBack();
    events.clear();assert(language->click(HidNpadButton_Right) && settings.language==re6::Language::Russian);
    assert(events==std::vector<std::string>({"save","apply"}));
    assert(f->title=="Графика RE6" && find(f,"Графика") && find(f,"Подключиться к RE6"));
    assert(language->label=="Язык" && language->value=="Русский" && help(gui,f,language).find("Русский")!=std::string::npos);
    assert(graphics->click(HidNpadButton_A));page=tsl::navigation.back().get();group=static_cast<tsl::elm::OverlayFrame*>(page->root);
    filter=find(group,"Фильтрация текстур");aa=find(group,"Сглаживание");reset=find(group,"Сброс графики");assert(filter && aa && reset && !find(group,"Язык"));
    assert(filter->value=="Штатная" && help(*page,group,filter).find("Штатная")==0);
    assert(aa->value=="Штатное" && help(*page,group,aa).find("Сглаживание")==0);
    assert(help(*page,group,reset).find("Вернуть")==0);tsl::goBack();
    assert(language->click(HidNpadButton_Left) && settings.language==re6::Language::English);
    assert(language->click(HidNpadButton_A) && settings.language==re6::Language::Russian);
    assert(language->click(HidNpadButton_A) && settings.language==re6::Language::English);
    const auto before_sends=sends,before_connects=connects,before_saves=saves;
    assert(connect->click(HidNpadButton_A) && sends==before_sends+1 && connects==before_connects+1 && saves==before_saves);
    connect_success=false;assert(connect->click(HidNpadButton_A) && sends==before_sends+1 && f->subtitle=="Start RE6");connect_success=true;delete f;
}
void filtering_reset_and_save_failure(){
    settings={};note.clear();GraphicsGui gui;auto* f=static_cast<tsl::elm::OverlayFrame*>(gui.createUI());
    auto* filter=find(f,"Texture filtering");auto* aa=find(f,"Anti-aliasing");auto* reset=find(f,"Reset graphics");assert(filter && aa && reset);
    const char* values[]={"Native","AF 2x","AF 4x","AF 8x","AF 16x"};
    for(unsigned i=0;i<5;++i){assert(unsigned(settings.texture_filtering)==i && filter->value==values[i]);events.clear();assert(filter->click(HidNpadButton_A));assert(events==std::vector<std::string>({"save","apply"}));}
    assert(settings.texture_filtering==re6::TextureFilter::Native);
    assert(filter->click(HidNpadButton_Left) && settings.texture_filtering==re6::TextureFilter::Af16x);
    const char* aa_values[]={"Native","Off","FXAA","FXAA3","FXAA3HQ"};
    for(unsigned i=0;i<5;++i){assert(unsigned(settings.anti_aliasing)==i && aa->value==aa_values[i]);events.clear();assert(aa->click(HidNpadButton_A));assert(events==std::vector<std::string>({"save","apply"}));assert(settings.texture_filtering==re6::TextureFilter::Af16x);}
    assert(settings.anti_aliasing==re6::AntiAliasing::Native);
    assert(aa->click(HidNpadButton_Left) && settings.anti_aliasing==re6::AntiAliasing::Fxaa3High);
    assert(filter->click(HidNpadButton_Left) && settings.texture_filtering==re6::TextureFilter::Af8x && settings.anti_aliasing==re6::AntiAliasing::Fxaa3High);
    settings.language=re6::Language::Russian;assert(!reset->click(HidNpadButton_Right));
    assert(reset->click(HidNpadButton_A) && settings.texture_filtering==re6::TextureFilter::Native && settings.anti_aliasing==re6::AntiAliasing::Native && settings.language==re6::Language::Russian);
    assert(aa->click(HidNpadButton_Right) && aa->value=="Выкл.");
    assert(reset->click(HidNpadButton_A) && settings.anti_aliasing==re6::AntiAliasing::Native && settings.language==re6::Language::Russian);
    assert(filter->value=="Штатная" && aa->value=="Штатное");save_success=false;const auto old_sends=sends;
    assert(filter->click(HidNpadButton_Right) && settings.texture_filtering==re6::TextureFilter::Native && sends==old_sends);
    assert(aa->click(HidNpadButton_Right) && settings.anti_aliasing==re6::AntiAliasing::Native && sends==old_sends);
    assert(f->subtitle.find("SD")!=std::string::npos);save_success=true;delete f;
}
void startup_and_mount_failure(){
    GraphicsOverlay overlay;tsl::Overlay& lifecycle=overlay;have_saved=true;
    for(unsigned i=0;i<5;++i){persisted={};persisted.texture_filtering=static_cast<re6::TextureFilter>(i);persisted.anti_aliasing=static_cast<re6::AntiAliasing>(4-i);settings={};const auto old_sends=sends,old_connects=connects,old_saves=saves;
        lifecycle.initServices();assert(settings.texture_filtering==persisted.texture_filtering && sent.texture_filtering==persisted.texture_filtering && settings.anti_aliasing==persisted.anti_aliasing && sent.anti_aliasing==persisted.anti_aliasing);
        assert(sends==old_sends+1 && connects==old_connects+1 && saves==old_saves);lifecycle.exitServices();}
    have_saved=false;settings.texture_filtering=re6::TextureFilter::Af16x;settings.anti_aliasing=re6::AntiAliasing::Fxaa;const auto old_sends=sends;lifecycle.initServices();
    assert(sends==old_sends+1 && sent.texture_filtering==re6::TextureFilter::Native && sent.anti_aliasing==re6::AntiAliasing::Native && sent.language==re6::Language::English);lifecycle.exitServices();
    storage_available=false;const auto before_sends=sends,before_saves=saves;lifecycle.initServices();
    assert(sends==before_sends && saves==before_saves && note.find("SD")!=std::string::npos);
    MainGui gui;auto* f=static_cast<tsl::elm::OverlayFrame*>(gui.createUI());assert(find(f,"Connect to RE6")->click(HidNpadButton_A));
    assert(find(f,"Language")->click(HidNpadButton_Right) && settings.language==re6::Language::English);
    assert(sends==before_sends && saves==before_saves);lifecycle.exitServices();storage_available=true;
    assert(initializes==7 && closes==7 && storage_closes==7);delete f;
}
}
namespace re6_overlay_storage{bool ready(){return storage_available;}bool mount(){return storage_available;}void close(){++storage_closes;}}
namespace re6{
bool save_settings_file(const char* p,const Settings& s){assert(std::string(p)=="r6ovl:/config/re4-controls/re6-settings.bin");++saves;events.push_back("save");if(save_success)persisted=s;return save_success;}
bool load_settings_file(const char* p,Settings& s){assert(std::string(p)=="r6ovl:/config/re4-controls/re6-settings.bin");if(have_saved)s=persisted;return have_saved;}
}
void Re6GameLink::initialize(){++initializes;}void Re6GameLink::close(){++closes;}
bool Re6GameLink::connect(){++connects;message=connect_success?"Connected to RE6":"Start RE6";return connect_success;}
bool Re6GameLink::apply(const re6::Settings& s){++sends;sent=s;events.push_back("apply");message="Applied";return true;}
int main(){graphics_illustrations();navigation_and_languages();filtering_reset_and_save_failure();startup_and_mount_failure();std::puts("RE6 graphics UI:root/groups, Cyrillic help, all filter/AA modes, transactional saves and native startup PASS");}
