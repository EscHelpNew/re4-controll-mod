#define OVERLAY_TEST_TRACK_ALLOCATIONS
#define main re4_overlay_entry
#include "../source/overlay/main.cpp"
#undef main
#include <cassert>
#include <set>
#include <chrono>
#include <filesystem>
#include <fstream>
#undef assert
#define assert(c) do{if(!(c)){std::fprintf(stderr,"overlay UI line %d: %s\n",__LINE__,#c);std::exit(1);}}while(false)
namespace {
unsigned saves=0,sends=0;
unsigned diagnostic_starts=0,diagnostic_stops=0;
re4::LaunchMode diagnostic_mode=re4::LaunchMode::Normal;
std::array<bool,7> saved_game{},sent_game{};
bool storage_available=true,have_saved=false,saved_gyro=true,last_sent_gyro=false;
bool saved_enemy=false,last_sent_enemy=false,saved_numbers=false,last_sent_numbers=false;
re4::CharacterAppearance saved_appearance=re4::CharacterAppearance::Leon,sent_appearance=re4::CharacterAppearance::Leon;
re4::CharacterAppearanceSnapshot game_appearance;
unsigned money_requests=0;
bool money_connected=true;
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
void launch_mode_lifecycle(){
    const auto root=std::filesystem::temp_directory_path()/
        ("re4-launch-mode-ui-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    assert(std::filesystem::create_directory(root));const auto path=(root/"launch-mode.bin").string();
    const auto previous_path=launch_mode_path;launch_mode_path=path.c_str();
    ControlsOverlay overlay;tsl::Overlay& lifecycle=overlay;
    const auto old_sends=sends,old_saves=saves;
    lifecycle.initServices();assert(launch_mode==re4::LaunchMode::Normal&&diagnostic_starts==0);
    assert(std::filesystem::is_empty(root));
    MainGui gui;auto* frame=static_cast<tsl::elm::OverlayFrame*>(gui.createUI());
    auto* row=find(frame,"Launch mode");assert(row&&row->value=="Normal");
    assert(!row->click(0));
    assert(row->click(HidNpadButton_A)&&row->value=="Diagnostics"&&diagnostic_starts==1);
    re4::LaunchMode saved=re4::LaunchMode::Normal;
    assert(re4::load_launch_mode(path.c_str(),saved)&&saved==re4::LaunchMode::Diagnostics);
    assert(sends==old_sends&&saves==old_saves); // No Settings wire update or unrelated preference write.
    delete frame;assert(overlay_debug::state()==overlay_debug::State::Listening&&diagnostic_stops==0); // UI hidden.
    lifecycle.exitServices();assert(diagnostic_stops==1);
    lifecycle.initServices();assert(launch_mode==re4::LaunchMode::Diagnostics&&diagnostic_starts==2);
    MainGui next;frame=static_cast<tsl::elm::OverlayFrame*>(next.createUI());row=find(frame,"Launch mode");
    settings.language=re4::Language::Russian;next.update();
    assert(row->label=="Режим запуска"&&row->value=="Диагностика");
    assert(row->click(HidNpadButton_Left)&&row->value=="Обычный"&&diagnostic_stops==2);
    assert(re4::load_launch_mode(path.c_str(),saved)&&saved==re4::LaunchMode::Normal);
    storage_available=false;
    assert(row->click(HidNpadButton_Right)&&overlay_debug::state()==overlay_debug::State::Listening&&!note.empty());
    assert(re4::load_launch_mode(path.c_str(),saved)&&saved==re4::LaunchMode::Normal);
    assert(row->click(HidNpadButton_A)&&overlay_debug::state()==overlay_debug::State::Off&&!note.empty());
    storage_available=true;delete frame;lifecycle.exitServices();const auto stops=diagnostic_stops;
    lifecycle.exitServices();assert(diagnostic_stops==stops);
    {std::ofstream file(path,std::ios::binary|std::ios::trunc);file<<"invalid";}
    lifecycle.initServices();assert(launch_mode==re4::LaunchMode::Normal&&diagnostic_starts==3);lifecycle.exitServices();
    std::filesystem::remove(path);lifecycle.initServices();assert(launch_mode==re4::LaunchMode::Normal);lifecycle.exitServices();
    for(const auto& file:std::filesystem::directory_iterator(root))std::filesystem::remove(file.path());
    std::filesystem::remove(root);launch_mode_path=previous_path;settings={};note.clear();
}
}
namespace overlay_storage {bool ready(){return storage_available;}bool mount(){return storage_available;}void close(){}}
namespace overlay_debug {
void start(const char*){if(re4::automatic_diagnostics(diagnostic_mode))++diagnostic_starts;}
void share_link(const GameLink*){}
void stop(){if(re4::automatic_diagnostics(diagnostic_mode))++diagnostic_stops;diagnostic_mode=re4::LaunchMode::Normal;}
void set_mode(re4::LaunchMode value,const char* path){if(re4::automatic_diagnostics(value)){diagnostic_mode=value;start(path);}else stop();}
re4::LaunchMode mode(){return diagnostic_mode;}
State state(){return re4::automatic_diagnostics(diagnostic_mode)?State::Listening:State::Off;}
std::uint16_t port(){return re4::automatic_diagnostics(diagnostic_mode)?28773:0;}
}
namespace re4 {
bool save_settings_file(const char* path,const Settings& s){if(std::string(path)==overlay_storage::settings_path){++saves;saved_appearance=s.character_appearance;saved_enemy=s.enemy_health_bars;saved_numbers=s.enemy_health_numbers;saved_game={s.grenade_landing_spot,s.skip_pickup_confirmation,s.skip_startup_logos,s.disable_aim_sway,s.dpad_weapon_switching,s.extended_weapon_upgrade,s.case_ammo_counter};}return valid_settings(s);}
bool load_settings_file(const char*,Settings& s){if(have_saved){s.gyro_enabled=saved_gyro;s.enemy_health_bars=saved_enemy;s.enemy_health_numbers=saved_numbers;}return have_saved;}
}
void GameLink::initialize(){}void GameLink::close(){}bool GameLink::connect(){return true;}
bool GameLink::apply(const re4::Settings& s){++sends;sent_appearance=s.character_appearance;last_sent_gyro=s.gyro_enabled;last_sent_enemy=s.enemy_health_bars;last_sent_numbers=s.enemy_health_numbers;sent_game={s.grenade_landing_spot,s.skip_pickup_confirmation,s.skip_startup_logos,s.disable_aim_sway,s.dpad_weapon_switching,s.extended_weapon_upgrade,s.case_ammo_counter};return re4::valid_settings(s);}
bool GameLink::refresh_appearance(){appearance=game_appearance;return appearance.valid;}
bool GameLink::refresh_popup(){popup_diagnostic=0;return false;}
bool GameLink::refresh_money(){return money_connected;}
bool GameLink::request_money(){if(!money_connected||money.availability!=re4::MoneyAvailability::Ready||money.pending())return false;++money_requests;++money.request;return true;}
int main(){
    using namespace re4;
    {
        settings={};note.clear();link.money={0,0,MoneyResult::None,MoneyAvailability::Ready};
        GameGui page;auto* frame=static_cast<tsl::elm::OverlayFrame*>(page.createUI());
        auto* action=find(frame,"Receive 200 000 pesetas");assert(action);
        const auto old_saves=saves,old_sends=sends;
        assert(!action->click(HidNpadButton_Left));
        assert(action->click(HidNpadButton_A)&&money_requests==0);
        auto* warning=static_cast<tsl::elm::OverlayFrame*>(tsl::navigation.back()->root);
        tsl::gfx::Renderer render;warning->layout(0,0,448,720);warning->draw(&render);
        assert(!render.text.empty());
        bool mentions_easier=false,mentions_receipt=false;
        for(const auto& text:render.text){mentions_easier|=text.value.find("easier")!=std::string::npos;mentions_receipt|=text.value.find("receipt")!=std::string::npos;}
        assert(mentions_easier&&mentions_receipt);
        assert(find(warning,"Cancel")->click(HidNpadButton_A)&&tsl::navigation.empty()&&money_requests==0);
        assert(action->click(HidNpadButton_A));warning=static_cast<tsl::elm::OverlayFrame*>(tsl::navigation.back()->root);
        auto* confirm=find(warning,"Receive 200 000 pesetas");assert(confirm->click(HidNpadButton_A)&&money_requests==1);
        assert(!confirm->click(HidNpadButton_A)&&money_requests==1);
        link.money.ack=link.money.request;link.money.result=MoneyResult::Granted;link.money.availability=MoneyAvailability::Claimed;
        tsl::navigation.back()->update();assert(!confirm->click(HidNpadButton_A));
        tsl::goBack();assert(!action->click(HidNpadButton_A)&&money_requests==1);
        assert(saves==old_saves&&sends==old_sends&&!settings.extended_weapon_upgrade);
        money_connected=false;assert(!action->click(HidNpadButton_A));money_connected=true;delete frame;
    }
    launch_mode_lifecycle();
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
    assert(find(root,"Language") && find(root,"Game") && list(root)->children.size()==6);
    assert(find(root,"Launch mode")&&find(root,"Launch mode")->value=="Normal");
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
        assert(list(content)->children.size()==11 && !find(content,"Language") && !find(content,"Язык"));
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
        assert(list(content)->height==160 && list(content)->y==125);
        for(unsigned row=2;row<9;++row){
            auto* option=static_cast<tsl::elm::ListItem*>(list(content)->children[row]);
            const Bilingual labels[]={{"Grenade explosion spot","Пятно взрыва гранаты"},
                {"Skip pickup confirmation","Без подтверждения подбора"},{"Skip startup logos","Пропуск стартовых заставок"},
                {"Remove aiming sway","Убрать покачивание прицела"},
                {"D-pad weapon switching","D-pad смена оружия"},
                {"Extended weapon upgrade","Расширенная прокачка оружия"},
                {"Case ammo counter","Патроны в чемодане"}};
            const auto member=row==2?&Settings::grenade_landing_spot:row==3?&Settings::skip_pickup_confirmation:row==4?&Settings::skip_startup_logos:row==5?&Settings::disable_aim_sway:row==6?&Settings::dpad_weapon_switching:row==7?&Settings::extended_weapon_upgrade:&Settings::case_ammo_counter;
            assert(!(settings.*member));
            page->focused=option;page->update();content->layout(0,0,448,720);
            tsl::gfx::Renderer help;content->draw(&help);
            assert(!help.text.empty() && help.text.front().y<640);
            assert(row==2?help.bitmaps.size()==171:help.bitmaps.empty());
            if(row==2)assert(help.bitmaps.back().y+help.bitmaps.back().h<647 && help.text.back().y<640);
            assert(option->label==labels[row-2].get(lang));
            assert(!option->click(HidNpadButton_Left) && !option->click(HidNpadButton_Right));
            const auto old_saves=saves,old_sends=sends;
            assert(option->click(HidNpadButton_A) && settings.*member);
            assert(saves==old_saves+1 && sends==old_sends+1 && !settings.enemy_health_bars && saved_game[row-2] && sent_game[row-2]);
            assert(option->click(HidNpadButton_A) && !(settings.*member));
        }
        auto* appearance_row=find(content,lang==Language::English?"Character appearance":"Внешность персонажа");assert(appearance_row);
        assert(!appearance_row->click(HidNpadButton_Left)&&!appearance_row->click(HidNpadButton_Right));
        assert(appearance_row->click(HidNpadButton_A)&&tsl::navigation.size()==2);
        auto* appearance_page=tsl::navigation.back().get();auto* appearance_frame=static_cast<tsl::elm::OverlayFrame*>(appearance_page->root);
        appearance_frame->layout(0,0,448,720);assert(list(appearance_frame)->children.size()==8);
        const char* names_en[]={"Leon","Ashley","Ada","Luis Sera","Krauser","Merchant","Ramon Salazar","Village Ganado"};
        const char* names_ru[]={"Леон","Эшли","Ада","Луис Сера","Краузер","Торговец","Рамон Салазар","Деревенский ганадо"};
        for(unsigned choice=0;choice<8;++choice){
            auto* row=static_cast<tsl::elm::ListItem*>(list(appearance_frame)->children[choice]);
            assert(row->label==(lang==Language::English?names_en[choice]:names_ru[choice]));
            assert(!row->click(HidNpadButton_Left)&&!row->click(HidNpadButton_Right));
            const bool changed=unsigned(settings.character_appearance)!=choice;const auto before_saves=saves,before_sends=sends;
            assert(row->click(HidNpadButton_A));
            assert(unsigned(settings.character_appearance)==choice&&saves==before_saves+changed&&sends==before_sends+changed);
            if(changed)assert(unsigned(saved_appearance)==choice&&unsigned(sent_appearance)==choice);
            assert(row->value==(lang==Language::English?"Requested":"Выбрано"));
            appearance_page->update();tsl::gfx::Renderer visual;appearance_frame->draw(&visual);
            assert(visual.bitmaps.empty()&&visual.text.front().value.find(lang==Language::English?"Leon weapons":"оружие и управление Леона")!=std::string::npos);
            assert(visual.text.front().value.find(lang==Language::English?"Active":"Активна")==std::string::npos);
        }
        for(auto status:{CharacterAppearanceStatus::Pending,CharacterAppearanceStatus::Failed,CharacterAppearanceStatus::Active}){
            game_appearance={status==CharacterAppearanceStatus::Active?CharacterAppearance::VillageGanado:CharacterAppearance::Leon,status,5,2,true};
            appearance_page->update();tsl::gfx::Renderer visual;appearance_frame->draw(&visual);
            const auto& text=visual.text.front().value;
            assert(text.find(status==CharacterAppearanceStatus::Pending?(lang==Language::English?"Pending":"Ожидание"):
                status==CharacterAppearanceStatus::Failed?(lang==Language::English?"Failed":"Ошибка"):(lang==Language::English?"Active":"Активна"))!=std::string::npos);
            assert(text.find(status==CharacterAppearanceStatus::Active?(lang==Language::English?"Village Ganado":"Деревенский ганадо"):(lang==Language::English?"Applied: Leon":"Применено: Леон"))!=std::string::npos);
        }
        for(unsigned reason=100;reason<=127;++reason){
            game_appearance={CharacterAppearance::Leon,CharacterAppearanceStatus::Pending,reason,2,true};
            appearance_page->update();tsl::gfx::Renderer visual;appearance_frame->draw(&visual);
            const auto& text=visual.text.front().value;
            assert(text.find(std::to_string(reason))!=std::string::npos);
            if(reason==100)assert(text.find(lang==Language::English?"Game graphics":"Графика игры")!=std::string::npos);
        }
        game_appearance={CharacterAppearance::Leon,CharacterAppearanceStatus::Pending,
            unsigned(character_appearance_diagnostic(CharacterAppearanceError::WaitingBones,26)),2,true};
        appearance_page->update();tsl::gfx::Renderer diagnostic;appearance_frame->draw(&diagnostic);
        assert(diagnostic.text.front().value.find("(105 / 26)")!=std::string::npos);
        game_appearance.error=character_loading_diagnostic(CharacterLoadingStage::Headroom,127);
        game_appearance.status=CharacterAppearanceStatus::Failed;
        appearance_page->update();tsl::gfx::Renderer failed;appearance_frame->draw(&failed);
        assert(failed.text.front().value.find("(10 / 13 / 127 MiB)")!=std::string::npos);
        game_appearance.error=unsigned(character_appearance_diagnostic(CharacterAppearanceError::ReadingAsset,59));
        game_appearance.status=CharacterAppearanceStatus::Pending;
        appearance_page->update();tsl::gfx::Renderer progress;appearance_frame->draw(&progress);
        assert(progress.text.front().value.find("(113 / 59%)")!=std::string::npos);
        game_appearance={};appearance_page->update();
        tsl::goBack();assert(tsl::navigation.size()==1);page->update();
        assert(appearance_row->value==(lang==Language::English?"Village Ganado":"Деревенский ганадо"));
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
