#include "../source/re5_controls/live_debug.hpp"
#define main re5_overlay_entry
#include "../source/re5_controls/main.cpp"
#undef main
#include <cstdio>
#include <cstdlib>
#undef assert
#define assert(c) do{if(!(c)){std::fprintf(stderr,"RE5 UI line %d: %s\n",__LINE__,#c);std::exit(1);}}while(false)
namespace {
bool mounted=true,save_ok=true,online=true,loaded=false,send_ok=true;
unsigned saves=0,sends=0,closes=0,profile_calls=0,profile_player=99,edited_saves=0;
re4::ProfileDomain profile_domain=re4::ProfileDomain::Graphics;
re5::Settings persisted,sent;
std::vector<std::string> events;
auto* rows(tsl::elm::OverlayFrame* f){return static_cast<tsl::elm::List*>(f->content());}
tsl::elm::ListItem* find(tsl::elm::OverlayFrame* f,const std::string& text){
 for(auto* e:rows(f)->children)if(auto* r=dynamic_cast<tsl::elm::ListItem*>(e))if(r->label==text)return r;
 return nullptr;
}
bool monitor_enabled=false,monitor_fail=false;
const Re5GameLink* monitor_owner=nullptr;bool ui_link_initialized=false;
unsigned monitor_starts=0,monitor_stops=0;
void monitor_opt_in_and_lifecycle(){
 ControlsOverlay overlay;tsl::Overlay& lifecycle=overlay;
 loaded=true;persisted={};settings={};monitor_enabled=false;monitor_starts=0;
 lifecycle.initServices();assert(!monitor_enabled&&!monitor_starts&&monitor_owner==&link);
 const auto old_sends=sends;
 MainGui gui;auto* f=static_cast<tsl::elm::OverlayFrame*>(gui.createUI());
 assert(rows(f)->children.size()==6);
 auto* monitor=find(f,"Monitor");assert(monitor&&monitor->value=="Off");
 assert(rows(f)->children[4]==find(f,"Language")&&rows(f)->children[5]==monitor);
 assert(!monitor->click(HidNpadButton_Right)&&!monitor_starts);
 monitor_fail=true;assert(monitor->click(HidNpadButton_A));
 assert(monitor_starts==1&&sends==old_sends&&monitor->value=="Failed");
 assert(monitor->click(HidNpadButton_A)&&!monitor_enabled&&monitor->value=="Off");
 monitor_fail=false;assert(monitor->click(HidNpadButton_A));
 assert(monitor_enabled&&monitor_starts==2&&sends==old_sends&&monitor->value=="Listening :28774");
 gui.focused=monitor;gui.update();f->layout(0,0,448,720);tsl::gfx::Renderer renderer;f->draw(&renderer);
 assert(renderer.text[0].value.find("Read-only")!=std::string::npos);
 link.details="BuildID 4E46401BF2FE53A44B69CC07FD951FE600000000000000000000000000000000";
 gui.focused=find(f,"Connect to RE5");gui.update();tsl::gfx::Renderer details;f->draw(&details);
 assert(details.text[0].value.find("4E46401BF2FE53A44B69CC07FD951FE6\n00000000000000000000000000000000")!=std::string::npos);
 events.clear();lifecycle.exitServices();assert(!monitor_enabled&&(events==std::vector<std::string>{"monitor-stop","monitor-unshare","link-close","storage-close"}));
 lifecycle.initServices();assert(!monitor_enabled&&monitor_starts==2);lifecycle.exitServices();
 delete f;link.details.clear();closes=0;
}
void events_and_players(){
 settings={};selected_player=0;note.clear();MainGui gui;auto* f=static_cast<tsl::elm::OverlayFrame*>(gui.createUI());
 assert(f->title=="RE5 Controls"&&rows(f)->children.size()==6);
 assert(find(f,"Player")->click(HidNpadButton_Right)&&selected_player==1);
 assert(find(f,"Controls")->click(HidNpadButton_A));auto* controls=tsl::navigation.back().get();
 auto* c=static_cast<tsl::elm::OverlayFrame*>(controls->root);
 assert(rows(c)->children.size()==re4::control_groups().size()+1);tsl::goBack();
 ControlsPageGui aim(re4::ControlGroup::Gyro);auto* a=static_cast<tsl::elm::OverlayFrame*>(aim.createUI());
 const auto& spec=re4::control_spec(re4::ControlId::GyroX);auto* speed=find(a,spec.label.english);assert(speed);
 const auto before=settings.players[0].gyro_gain_x;events.clear();assert(speed->click(HidNpadButton_Right));
 assert(settings.players[1].gyro_gain_x!=before&&settings.players[0].gyro_gain_x==before);
 assert((events==std::vector<std::string>{"save","apply"})&&sent.players[1].gyro_gain_x==settings.players[1].gyro_gain_x);
 const auto active=settings.players[1].gyro_gain_x;save_ok=false;const auto old=sends;
 assert(speed->click(HidNpadButton_Right)&&settings.players[1].gyro_gain_x==active&&sends==old&&note.find("SD")!=std::string::npos);
 save_ok=true;online=false;assert(speed->click(HidNpadButton_Right)&&note.find("next launch")!=std::string::npos);online=true;
 auto* language=find(f,"Language");assert(language->click(HidNpadButton_A)&&settings.language==re4::Language::Russian);
 assert(f->title=="Управление RE5"&&find(f,"Игрок")&&find(f,"Управление"));
 delete a;delete f;
}
void mapping_toggles_and_failures(){
 settings={};selected_player=0;note.clear();
 ControlsPageGui gyro(re4::ControlGroup::Gyro);auto* g=static_cast<tsl::elm::OverlayFrame*>(gyro.createUI());
 auto* toggle=dynamic_cast<tsl::elm::ToggleListItem*>(find(g,re4::control_spec(re4::ControlId::Gyro).label.english));assert(toggle);
 toggle->changed(true);assert(settings.players[0].gyro_enabled&&!settings.players[1].gyro_enabled);
 save_ok=false;const auto old=sends;toggle->changed(false);assert(settings.players[0].gyro_enabled&&toggle->state&&sends==old);save_ok=true;
 MappingGui mapping;auto* m=static_cast<tsl::elm::OverlayFrame*>(mapping.createUI());
 auto* enabled=find(m,re4::mapping_row_label(0,re4::Language::English));assert(enabled&&enabled->click(HidNpadButton_A));
 assert(settings.players[0].remap_enabled&&!settings.players[1].remap_enabled);
 auto* source=find(m,re4::mapping_row_label(1,re4::Language::English));assert(source&&source->click(HidNpadButton_A));
 auto* target=static_cast<tsl::elm::OverlayFrame*>(tsl::navigation.back()->root);
 auto* disable=find(target,re4::mapping_target_label(re4::kButtonCount,re4::Language::English));assert(disable&&disable->click(HidNpadButton_A));
 assert(re4::mapping_target_index(settings.players[0],0)==re4::kButtonCount&&re4::mapping_target_index(settings.players[1],0)==0);
 save_ok=false;assert(enabled->click(HidNpadButton_A));assert(m->subtitle.find("SD")!=std::string::npos&&settings.players[0].remap_enabled);save_ok=true;
 ControlsPageGui reticle(re4::ControlGroup::Reticle);auto* r=static_cast<tsl::elm::OverlayFrame*>(reticle.createUI());r->layout(0,0,448,720);tsl::gfx::Renderer render;r->draw(&render);assert(render.text.size()>=2);
 delete g;delete m;delete r;
}
void reset_keeps_custom_snapshot(){
 settings={};selected_player=1;settings.players[1].gyro_gain_x=2;settings.players[1].gyro_bias_x=0.125f;
 settings.players[0].gyro_speed=2.5f;settings.graphics.aa=re5::AntiAliasing::Smaa;
 ControlsPageGui reset(re4::ControlGroup::Profiles);auto* f=static_cast<tsl::elm::OverlayFrame*>(reset.createUI());
 const auto before=edited_saves;auto* action=find(f,re4::control_spec(re4::ControlId::Reset).label.english);assert(action&&action->click(HidNpadButton_A));
 assert(edited_saves==before&&settings.players[1].gyro_gain_x==1&&settings.players[1].gyro_bias_x==0.125f
  &&settings.players[0].gyro_speed==2.5f&&settings.graphics.aa==re5::AntiAliasing::Smaa);delete f;
}
void descriptions_and_profile_route(){
 settings={};selected_player=1;ControlsGui controls_gui;auto* c=static_cast<tsl::elm::OverlayFrame*>(controls_gui.createUI());
 auto* profile=find(c,re4::control_spec(re4::ControlId::Profile).label.english);assert(profile);
 const auto old_sends=sends;assert(profile->click(HidNpadButton_Left));
 assert(profile_calls==1&&profile_player==1&&profile_domain==re4::ProfileDomain::Controls&&sends==old_sends&&!note.empty());
 delete c;
 for(auto language:{re4::Language::English,re4::Language::Russian}){
  settings.language=language;MappingGui mapping;auto* m=static_cast<tsl::elm::OverlayFrame*>(mapping.createUI());
  mapping.focused=rows(m)->children[0];mapping.update();m->layout(0,0,448,720);tsl::gfx::Renderer render;m->draw(&render);
  assert(render.text[0].value.find("RE4")==std::string::npos&&render.text[0].value.find("RE5")!=std::string::npos);delete m;
  for(const auto& group:re4::control_groups())if(group.id!=re4::ControlGroup::Mapping){
   ControlsPageGui page(group.id);auto* f=static_cast<tsl::elm::OverlayFrame*>(page.createUI());
   assert(rows(f)->children.size()==re4::control_group_count(group.id));
   for(unsigned row=0;row<rows(f)->children.size();++row){
    page.focused=rows(f)->children[row];page.update();f->layout(0,0,448,720);tsl::gfx::Renderer r;f->draw(&r);assert(!r.text[0].value.empty());
    const auto& help=r.text[0].value;
    assert(help.find("RE4")==std::string::npos&&help.find("Leon")==std::string::npos&&help.find("Леон")==std::string::npos);
    if(re4::control_group_item(group.id,row)==re4::ControlId::Turn)
     assert(help.find(language==re4::Language::English?"Classic controls only":"Только классическое управление")!=std::string::npos);
   }
   delete f;
  }
 }
}
void graphics_and_preview(){
 settings={};selected_player=0;GraphicsGui gui;auto* f=static_cast<tsl::elm::OverlayFrame*>(gui.createUI());
 auto* aa=find(f,"Anti-aliasing");assert(aa&&aa->value=="Native");
 assert(aa->click(HidNpadButton_Right)&&settings.graphics.aa==re5::AntiAliasing::Off&&aa->value=="Off");
 assert(aa->click(HidNpadButton_Right)&&settings.graphics.aa==re5::AntiAliasing::Smaa&&aa->value=="On (SMAA)");
 assert(aa->click(HidNpadButton_Right)&&settings.graphics.aa==re5::AntiAliasing::Native);
 for(unsigned i=0;i<4;++i){gui.focused=rows(f)->children[i];gui.update();f->layout(0,0,448,720);tsl::gfx::Renderer r;f->draw(&r);
  assert(r.bitmaps.size()==180&&r.rects.size()==5&&r.rects[0].color==0xF999&&r.text[0].value.find("HOME > X > Close")!=std::string::npos);
 }
 assert(settings.players[0].texture_filtering==re4::TextureFilter::Native&&settings.players[1].anti_aliasing==re4::AntiAliasing::Native);
 delete f;
}
void startup_persistence_status(){
 ControlsOverlay o;tsl::Overlay& lifecycle=o;
 // Missing files and unreadable/invalid files all return false at this boundary.
 // Startup must preserve them and expose defaults without publishing unsaved bytes.
 for(unsigned failure=0;failure<2;++failure){
  loaded=false;mounted=true;online=true;send_ok=false;save_ok=failure==0;
  const auto old_sends=sends,old_saves=saves;lifecycle.initServices();
  assert(sends==old_sends&&saves==old_saves&&note.find("load")!=std::string::npos&&note.find("not saved")!=std::string::npos);
  MainGui gui;auto* f=static_cast<tsl::elm::OverlayFrame*>(gui.createUI());
  auto* connect=find(f,"Connect to RE5");assert(connect&&connect->click(HidNpadButton_A));
  assert(sends==old_sends&&note.find("not saved")!=std::string::npos&&note.find("Saved for next launch")==std::string::npos);
  // Failed save must not establish persistence; a successful edit must establish it.
  save_ok=false;assert(find(f,"Language")->click(HidNpadButton_A));
  assert(sends==old_sends&&settings.language==re4::Language::English);
  assert(connect->click(HidNpadButton_A)&&sends==old_sends&&note.find("Saved for next launch")==std::string::npos);
  save_ok=true;assert(find(f,"Language")->click(HidNpadButton_A));
  assert(sends==old_sends+1&&settings.language==re4::Language::Russian&&note.find("следующего запуска")!=std::string::npos);
  assert(connect->click(HidNpadButton_A)&&sends==old_sends+2&&note.find("следующего запуска")!=std::string::npos);
  delete f;lifecycle.exitServices();
 }
 loaded=true;persisted={};send_ok=false;lifecycle.initServices();
 assert(note.find("Saved for next launch")!=std::string::npos);
 MainGui gui;auto* f=static_cast<tsl::elm::OverlayFrame*>(gui.createUI());
 assert(find(f,"Connect to RE5")->click(HidNpadButton_A)&&note.find("Saved for next launch")!=std::string::npos);
 delete f;lifecycle.exitServices();send_ok=true;save_ok=true;
}
void startup(){
 ControlsOverlay o;tsl::Overlay& lifecycle=o;loaded=true;persisted={};persisted.players[1].gyro_speed=3.0f;
 lifecycle.initServices();assert(settings.players[1].gyro_speed==3&&sent.players[1].gyro_speed==3);lifecycle.exitServices();
 mounted=false;const auto old=sends;lifecycle.initServices();assert(sends==old&&note.find("SD")!=std::string::npos);
 MainGui gui;auto* f=static_cast<tsl::elm::OverlayFrame*>(gui.createUI());assert(find(f,"Connect to RE5")->click(HidNpadButton_A));
 assert(sends==old);lifecycle.exitServices();delete f;mounted=true;assert(closes==2);
}
}
namespace re5_overlay_storage{bool ready(){return mounted;}bool mount(){return mounted;}void close(){++closes;events.push_back("storage-close");}}
namespace re5{
bool save_settings_file(const char* p,const Settings& s){assert(std::string(p)==re5_overlay_storage::settings_path);++saves;events.push_back("save");if(save_ok)persisted=s;return save_ok;}
bool load_settings_file(const char*,Settings& s){if(loaded)s=persisted;return loaded;}
bool save_edited_settings(const char* p,const Settings& s,re4::ProfileDomain,unsigned){++edited_saves;return save_settings_file(p,s);}
re4::ProfileResult switch_profile(const char* path,Settings&,re4::ProfileDomain domain,re4::ProfileKind,unsigned player){assert(std::string(path)==re5_overlay_storage::settings_path);++profile_calls;profile_player=player;profile_domain=domain;return re4::ProfileResult::MissingCustom;}
re4::ProfileKind profile_kind(const Settings&,re4::ProfileDomain,unsigned){return re4::ProfileKind::Default;}
}
namespace re4{bool load_settings_file(const char*,Settings&){std::abort();}bool save_settings_file(const char*,const Settings&){std::abort();}}
void Re5GameLink::initialize(){ui_link_initialized=true;}void Re5GameLink::close(){assert(!monitor_owner&&!monitor_enabled);ui_link_initialized=false;events.push_back("link-close");}
bool Re5GameLink::connect(){message=online?"Connected":"Start RE5";return online;}
bool Re5GameLink::apply(const re5::Settings& s){++sends;sent=s;events.push_back("apply");message=online&&send_ok?"Settings sent to RE5":"Could not send settings";return online&&send_ok;}
namespace re5_overlay_debug {
void share_link(const Re5GameLink* owner){assert(!monitor_enabled);if(owner)assert(ui_link_initialized&&owner==&link);monitor_owner=owner;events.push_back(owner?"monitor-share":"monitor-unshare");}
void stop(){monitor_enabled=false;++monitor_stops;events.push_back("monitor-stop");}
void set_enabled(bool enabled,const char*){if(!enabled){stop();return;}assert(monitor_owner==&link);monitor_enabled=true;++monitor_starts;}
bool enabled(){return monitor_enabled;}
State state(){return !monitor_enabled?State::Off:monitor_fail?State::Failed:State::Listening;}
std::uint16_t port(){return state()==State::Listening?28774:0;}
}
int main(){monitor_opt_in_and_lifecycle();events_and_players();mapping_toggles_and_failures();reset_keeps_custom_snapshot();descriptions_and_profile_route();graphics_and_preview();startup();startup_persistence_status();std::puts("RE5 overlay events: player isolation, transactional saves, offline status, AA, gray previews and lifecycle PASS");}
