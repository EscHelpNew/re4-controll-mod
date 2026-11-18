#define TESLA_INIT_IMPL
#include <tesla.hpp>
#include <sys/stat.h>
#include "game_link.hpp"
#include "live_debug.hpp"
#include "storage.hpp"
#include "re5/profiles.hpp"
#include "re5/control_help.hpp"
#include "re4/control_menu.hpp"
#include "re4/reticle_preview.hpp"
#include "re4/graphics_preview_image.hpp"

namespace {
Re5GameLink link;
re5::Settings settings;
bool settings_persisted=false;
unsigned selected_player=0;
std::string note;
const char* tr(const char* en,const char* ru){return settings.language==re4::Language::Russian?ru:en;}
re4::Settings controls(){auto value=settings.players[selected_player];value.language=settings.language;return value;}
const char* load_error(){return tr("Could not load settings from SD. Defaults are not saved.","Не удалось загрузить настройки с SD. Исходные настройки не сохранены.");}
void send_saved(){
 if(!settings_persisted){note=load_error();return;}
 link.language=settings.language;
 note=link.apply(settings)?link.message:std::string(tr("Saved for next launch. ","Сохранено для следующего запуска. "))+link.message;
}
bool change(const re5::Settings& candidate,re4::ProfileDomain domain=re4::ProfileDomain::Controls,bool edited=true){
 if(!re5_overlay_storage::ready()||!(edited?re5::save_edited_settings(re5_overlay_storage::settings_path,candidate,domain,selected_player)
    :re5::save_settings_file(re5_overlay_storage::settings_path,candidate))){
  note=tr("Could not save settings to SD","Не удалось сохранить настройки на SD");return false;
 }
 settings=candidate;settings_persisted=true;send_saved();return true;
}
void change_controls(re4::Settings candidate,bool edited=true){auto next=settings;candidate.language=next.players[selected_player].language;
 next.players[selected_player]=candidate;change(next,re4::ProfileDomain::Controls,edited);}
void apply_profile(re4::ProfileDomain domain,re4::ProfileKind next){
 auto candidate=settings;
 const auto result=re5_overlay_storage::ready()?re5::switch_profile(re5_overlay_storage::settings_path,candidate,domain,next,selected_player):re4::ProfileResult::SaveFailed;
 if(result==re4::ProfileResult::Applied){settings=candidate;settings_persisted=true;send_saved();}
 else note=re4::profile_result_text(result,settings.language);
}
class DescriptionFrame:public tsl::elm::OverlayFrame{
 std::string description_;
 unsigned help_y_=568;
 bool preview_=false,graphics_preview_=false;
 graphics_preview::Topic graphics_topic_=graphics_preview::Topic::Filtering;
 int graphics_x_=64;
public:
 DescriptionFrame():OverlayFrame(tr("RE5 Controls","Управление RE5"),""){}
 void set_help(const char* text){
  description_=text;
  for(std::size_t pos=0;(pos=description_.find("RE4",pos))!=std::string::npos;pos+=3)description_.replace(pos,3,"RE5");
 }
 void set_preview(bool enabled){preview_=enabled;}
 void set_graphics_preview(bool enabled){graphics_preview_=enabled;}
 void set_graphics_topic(graphics_preview::Topic topic){graphics_topic_=topic;}
 void layout(u16 x,u16 y,u16 width,u16 height)override{
  OverlayFrame::layout(x,y,width,height);help_y_=y+height-(graphics_preview_?367:preview_?310:152);
  graphics_x_=int(x)+(int(width)-320)/2;
  if(m_contentElement){m_contentElement->setBoundaries(x+35,y+125,width-85,height-198-(graphics_preview_?325:preview_?268:110));m_contentElement->invalidate();}
 }
 void draw(tsl::gfx::Renderer* renderer)override{
  OverlayFrame::draw(renderer);
  renderer->drawRect(20,help_y_-21,tsl::cfg::FramebufferWidth-40,1,a(0xF999));
  renderer->drawString(description_.c_str(),false,20,help_y_,16,a(tsl::style::color::ColorDescription));
  if(graphics_preview_){
   const int image_y=int(help_y_)+90;
   graphics_preview::draw_rows(graphics_preview::image(re4::GameTarget::RE5),graphics_x_,image_y,
    [&](int x,int y,int w,int h,const std::uint8_t* rgba){renderer->drawBitmap(x,y,w,h,rgba);});
   const auto r=graphics_preview::focus(re4::GameTarget::RE5,graphics_topic_);
   if(r.width&&r.height){const int x=graphics_x_+int(r.x),y=image_y+int(r.y),w=int(r.width),h=int(r.height);
    renderer->drawRect(x,y,w,2,a(0xF999));renderer->drawRect(x,y+h-2,w,2,a(0xF999));
    renderer->drawRect(x,y,2,h,a(0xF999));renderer->drawRect(x+w-2,y,2,h,a(0xF999));}
   renderer->drawString(graphics_preview::caption(graphics_topic_,settings.language==re4::Language::Russian),false,20,help_y_+282,16,a(tsl::style::color::ColorDescription));
  }else if(preview_){
   renderer->drawString(tr("Reticle preview (x2)","Образец прицела (x2)"),false,20,help_y_+78,16,a(tsl::style::color::ColorDescription));
   re4::draw_reticle_preview(controls(),20,help_y_+105,tsl::cfg::FramebufferWidth-40,130,[&](int x,int y,int w,int h,unsigned rgb){
    const u16 color=u16((rgb>>20)&15)|u16((rgb>>12)&15)<<4|u16((rgb>>4)&15)<<8|u16((rgb>>28)&15)<<12;
    renderer->drawRect(x,y,w,h,a(color));});
  }
 }
};
class MappingTargetsGui : public tsl::Gui {
    unsigned source_;
    DescriptionFrame* frame_=nullptr;
public:
    explicit MappingTargetsGui(unsigned source):source_(source){}
    tsl::elm::Element* createUI()override{
        frame_=new DescriptionFrame;
        frame_->setTitle(tr("Choose game button","Выбор кнопки игры"));
        frame_->setSubtitle(std::string(re4::button_name(source_))+" -> "+re4::mapping_row_value(source_+1,controls()));
        frame_->set_help(re4::mapping_target_help(controls().language));
        auto* list=new tsl::elm::List;
        for(unsigned target=0;target<=re4::kButtonCount;++target){
            auto* item=new tsl::elm::ListItem(re4::mapping_target_label(target,controls().language),
                target==re4::mapping_target_index(controls(),source_)?"*":"");
            item->setClickListener([source=source_,target](u64 keys){
                if(!(keys&HidNpadButton_A))return false;
                auto candidate=controls();if(re4::set_mapping_target(candidate,source,target))change_controls(candidate);
                tsl::goBack();return true;
            });
            list->addItem(item);
        }
        frame_->setContent(list);return frame_;
    }
};
class MappingGui : public tsl::Gui {
    DescriptionFrame* frame_=nullptr;
    std::vector<tsl::elm::ListItem*> rows_;
public:
    tsl::elm::Element* createUI()override{
        rows_.clear();frame_=new DescriptionFrame;
        frame_->setTitle(tr("Button mapping","Назначение кнопок"));
        frame_->setSubtitle(tr("Physical button -> game button","Физическая кнопка -> кнопка игры"));
        frame_->set_help(re4::mapping_row_help(0,controls().language));
        auto* list=new tsl::elm::List;
        for(unsigned row=0;row<re4::kMappingRowCount;++row){
            auto* item=new tsl::elm::ListItem(re4::mapping_row_label(row,controls().language),re4::mapping_row_value(row,controls()));
            item->setClickListener([this,row](u64 keys){
                if(row>0 && row<=re4::kButtonCount && (keys&HidNpadButton_A)){
                    tsl::changeTo<MappingTargetsGui>(row-1);return true;
                }
                if(row==re4::kMappingRowCount-1 && !(keys&HidNpadButton_A))return false;
                if(!(keys&(HidNpadButton_A|HidNpadButton_Left|HidNpadButton_Right)))return false;
                auto candidate=controls();if(re4::change_mapping(candidate,row,(keys&HidNpadButton_Left)?-1:1))change_controls(candidate);
                update();return true;
            });
            list->addItem(item);rows_.push_back(item);
        }
        frame_->setContent(list);return frame_;
    }
    void update()override{
        frame_->setSubtitle(note.empty()?tr("Physical button -> game button","Физическая кнопка -> кнопка игры"):note);
        for(unsigned row=0;row<rows_.size();++row){
            rows_[row]->setValue(re4::mapping_row_value(row,controls()));
            if(getFocusedElement()==rows_[row])frame_->set_help(re4::mapping_row_help(row,controls().language));
        }
    }
};

const char* control_profile_name(){const auto kind=re5::profile_kind(settings,re4::ProfileDomain::Controls,selected_player);
 return kind==re4::ProfileKind::RE6?tr("Modern","Современный"):re4::profile_name(kind,settings.language);}
class ControlsPageGui : public tsl::Gui {
    struct Binding {tsl::elm::ListItem* item;const re4::ControlSpec* spec;};
    re4::ControlGroup group_;
    std::vector<Binding> bindings_;
    DescriptionFrame* description_frame_=nullptr;
    std::array<std::uint8_t,re4::kSettingsBytes> rendered_settings_{};
    std::string rendered_note_;
    void refresh(){
        link.language=controls().language;
        description_frame_->setTitle(re4::control_group_spec(group_).label.get(controls().language));
        description_frame_->setSubtitle(note.empty()?(selected_player?tr("Player 2","Игрок 2"):tr("Player 1","Игрок 1")):note);
        for(const auto& binding:bindings_){
            const auto& spec=*binding.spec;
            binding.item->setText(spec.label.get(controls().language));
            if(spec.toggle)static_cast<tsl::elm::ToggleListItem*>(binding.item)->setState(controls().*spec.toggle);
            binding.item->setValue(re4::control_value_text(spec,controls()));
        }
        re4::encode_settings(controls(),rendered_settings_.data(),rendered_settings_.size());
        rendered_note_=note;
    }
public:
    explicit ControlsPageGui(re4::ControlGroup group):group_(group){}
    tsl::elm::Element* createUI()override{
        bindings_.clear();
        auto* frame=description_frame_=new DescriptionFrame;
        frame->set_preview(group_==re4::ControlGroup::Reticle);
        auto* list=new tsl::elm::List;
        for(unsigned row=0;row<re4::control_group_count(group_);++row){
            const auto& spec=re4::control_spec(re4::control_group_item(group_,row));
            const auto* entry=&spec;
            tsl::elm::ListItem* item;
            if(spec.toggle){
                auto* toggle=new tsl::elm::ToggleListItem(spec.label.get(controls().language),controls().*spec.toggle,"On","Off");
                toggle->setStateChangedListener([this,entry](bool value){
                    auto candidate=controls();candidate.*entry->toggle=value;change_controls(candidate);refresh();
                });
                item=toggle;
            }else{
                item=new tsl::elm::ListItem(spec.label.get(controls().language),re4::control_value_text(spec,controls()));
                item->setClickListener([this,entry](u64 keys){
                    if(entry->id==re4::ControlId::ButtonMapping){
                        if(!(keys&HidNpadButton_A))return false;
                        tsl::changeTo<MappingGui>();return true;
                    }
                    if(entry->kind==re4::ControlKind::Action && !(keys&HidNpadButton_A))return false;
                    const bool arrows=keys&(HidNpadButton_Left|HidNpadButton_Right);
                    if(!arrows && !(entry->kind!=re4::ControlKind::Slider && (keys&HidNpadButton_A)))return false;
                    const int direction=(keys&HidNpadButton_Left)?-1:1;
                    if(entry->id==re4::ControlId::Profile){apply_profile(re4::ProfileDomain::Controls,re4::next_control_profile(re5::profile_kind(settings,re4::ProfileDomain::Controls,selected_player),direction));refresh();return true;}
                    auto candidate=controls();
                    if(entry->id==re4::ControlId::Reset){re4::copy_profile(candidate,re5::native_controls(),re4::ProfileDomain::Controls);change_controls(candidate,false);refresh();}
                    else if(re4::change_control(candidate,entry->id,direction)){change_controls(candidate);refresh();}
                    return true;
                });
            }
            list->addItem(item);bindings_.push_back({item,entry});
        }
        if(re4::control_group_count(group_))frame->set_help(re5::control_help(re4::control_group_item(group_,0),controls()));
        frame->setContent(list);refresh();return frame;
    }
    void update()override{
        std::array<std::uint8_t,re4::kSettingsBytes> current{};
        re4::encode_settings(controls(),current.data(),current.size());
        if(current!=rendered_settings_ || note!=rendered_note_)refresh();
        if(description_frame_){
            for(const auto& binding:bindings_)if(binding.item==getFocusedElement()){
                description_frame_->set_help(binding.spec->id==re4::ControlId::Profile && !note.empty()?
                    note.c_str():re5::control_help(binding.spec->id,controls()));break;
            }
        }
    }
};
class ControlsGui : public tsl::Gui {
    DescriptionFrame* frame_=nullptr;
    std::vector<tsl::elm::ListItem*> groups_;
    tsl::elm::ListItem* profile_=nullptr;
public:
    tsl::elm::Element* createUI()override{
        groups_.clear();frame_=new DescriptionFrame;auto* list=new tsl::elm::List;
        profile_=new tsl::elm::ListItem(re4::control_spec(re4::ControlId::Profile).label.get(controls().language));
        profile_->setClickListener([this](u64 keys){
            const int direction=(keys&HidNpadButton_Left)?-1:(keys&(HidNpadButton_A|HidNpadButton_Right))?1:0;
            if(!direction)return false;
            apply_profile(re4::ProfileDomain::Controls,re4::next_control_profile(re5::profile_kind(settings,re4::ProfileDomain::Controls,selected_player),direction));
            update();return true;
        });
        list->addItem(profile_);
        for(const auto& group:re4::control_groups()){
            auto* item=new tsl::elm::ListItem(group.label.get(controls().language),"A");
            item->setClickListener([id=group.id](u64 keys){
                if(!(keys&HidNpadButton_A))return false;
                note.clear();
                if(id==re4::ControlGroup::Mapping)tsl::changeTo<MappingGui>();
                else tsl::changeTo<ControlsPageGui>(id);
                return true;
            });
            groups_.push_back(item);list->addItem(item);
        }
        frame_->setContent(list);update();return frame_;
    }
    void update()override{
        frame_->setTitle(tr("Controls","Управление"));frame_->setSubtitle((selected_player?tr("Player 2","Игрок 2"):tr("Player 1","Игрок 1")));
        profile_->setText(re4::control_spec(re4::ControlId::Profile).label.get(controls().language));
        profile_->setValue(control_profile_name());
        if(getFocusedElement()==profile_ || !getFocusedElement())frame_->set_help(note.empty()?tr("Default, Modern or your last Custom controls.\nGraphics, language and calibration are preserved.","Исходное, современное или своё управление.\nГрафика, язык и калибровка сохраняются."):note.c_str());
        for(unsigned i=0;i<groups_.size();++i){
            const auto& group=re4::control_groups()[i];groups_[i]->setText(group.label.get(controls().language));
            if(getFocusedElement()==groups_[i])frame_->set_help(group.help.get(controls().language));
        }
    }
};

class GraphicsGui:public tsl::Gui{
 DescriptionFrame* frame_=nullptr;
 std::array<tsl::elm::ListItem*,6> rows_{};
 std::string help_;
 std::string value(unsigned row){
  const auto& g=settings.graphics;
  if(row==0){constexpr const char* values[]={"Native","Bilinear","Trilinear","AF 2x","AF 4x","AF 8x","AF 16x"};return g.filter==re4::TextureFilter::Native?tr("Native","Штатная"):values[unsigned(g.filter)];}
  if(row==1)return g.aa==re5::AntiAliasing::Native?tr("Native","Штатное"):g.aa==re5::AntiAliasing::Off?tr("Off","Выключено"):tr("On (SMAA)","Включено (SMAA)");
  if(row==2){char s[32];std::snprintf(s,sizeof(s),"%.2fx",double(g.draw_distance));return s;}
  if(row==3)return g.shadows==re4::ShadowMode::Native?tr("Native","Штатные"):tr("Off","Выключены");
  if(row==4)return re4::profile_name(re5::profile_kind(settings,re4::ProfileDomain::Graphics,selected_player),settings.language);
  return "A";
 }
public:
 tsl::elm::Element* createUI()override{
  frame_=new DescriptionFrame;frame_->set_graphics_preview(true);auto* list=new tsl::elm::List;
  for(unsigned row=0;row<rows_.size();++row){auto* item=new tsl::elm::ListItem("");rows_[row]=item;list->addItem(item);
   item->setClickListener([this,row](u64 keys){
    if(row==5&&!(keys&HidNpadButton_A))return false;
    const int d=keys&HidNpadButton_Left?-1:keys&(HidNpadButton_Right|HidNpadButton_A)?1:0;
    if(!d||(row==2&&!(keys&(HidNpadButton_Left|HidNpadButton_Right))))return false;
    if(row==4)apply_profile(re4::ProfileDomain::Graphics,re4::next_graphics_profile(re5::profile_kind(settings,re4::ProfileDomain::Graphics,selected_player),d));
    else{auto candidate=settings;auto& g=candidate.graphics;
     if(row==0)g.filter=static_cast<re4::TextureFilter>((int(g.filter)+d+7)%7);
     if(row==1)g.aa=static_cast<re5::AntiAliasing>((int(g.aa)+d+3)%3);
     if(row==2)g.draw_distance=std::clamp(g.draw_distance+d*0.05f,0.5f,2.0f);
     if(row==3)g.shadows=g.shadows==re4::ShadowMode::Native?re4::ShadowMode::Off:re4::ShadowMode::Native;
     if(row==5)g={};
     if(!re5::same_graphics(candidate.graphics,settings.graphics))change(candidate,re4::ProfileDomain::Graphics,row!=5);
    }
    update();return true;
   });
  }
  frame_->setContent(list);update();return frame_;
 }
 void update()override{
  frame_->setTitle(tr("RE5 Graphics","Графика RE5"));frame_->setSubtitle(note);
  unsigned focused=0;
  for(unsigned i=0;i<rows_.size();++i){rows_[i]->setText(re4::graphics_specs()[i].label.get(settings.language));rows_[i]->setValue(value(i));if(getFocusedElement()==rows_[i])focused=i;}
  const char* help=focused==0?tr("Native preserves filtering. AF sharpens angled textures.","Штатная сохраняет фильтрацию. AF улучшает наклонные текстуры."):
   focused==1?tr("Native preserves game AA. Off disables it; On uses SMAA.","Штатное сохраняет сглаживание. Выключено / Включено (SMAA)."):
   focused==2?tr("Object draw distance: 0.50x to 2.00x.","Дальность объектов: от 0.50x до 2.00x."):
   focused==3?tr("Native shadows or Off.","Штатные тени или выключение."):
   focused==4?tr("Default, Quality (AF16 / SMAA / 1.75x), or Custom.","Исходный, Качество (AF16 / SMAA / 1.75x), Свой."):
   tr("Restore native graphics. Both players keep their controls.","Вернуть штатную графику. Управление игроков сохраняется.");
  help_=std::string(help)+tr("\nFull restart: HOME > X > Close.","\nПерезапуск: HOME > X > Закрыть.");frame_->set_help(help_.c_str());
  frame_->set_graphics_topic(graphics_preview::topic_for(re4::GameTarget::RE5,focused));
 }
};
const char* monitor_status(){
 using re5_overlay_debug::State;
 switch(re5_overlay_debug::state()){
  case State::Off:return tr("Off","Выкл.");
  case State::MissingKey:return tr("Missing key","Нет ключа");
  case State::ServicesUnavailable:return tr("Service unavailable","Сервис недоступен");
  case State::SocketsUnavailable:return tr("Network unavailable","Сеть недоступна");
  case State::Starting:return tr("Starting","Запуск");
  case State::Listening:return tr("Listening :28774","Ожидание :28774");
  default:return tr("Failed","Ошибка");
 }
}
class MainGui:public tsl::Gui{
 DescriptionFrame* frame_=nullptr;
 std::array<tsl::elm::ListItem*,6> rows_{};
 std::string help_;
public:
 tsl::elm::Element* createUI()override{
  frame_=new DescriptionFrame;auto* list=new tsl::elm::List;
  for(unsigned i=0;i<rows_.size();++i){auto* row=new tsl::elm::ListItem("");rows_[i]=row;list->addItem(row);
   row->setClickListener([this,i](u64 keys){
    if(!(keys&(HidNpadButton_A|HidNpadButton_Left|HidNpadButton_Right)))return false;
    if((i==1||i==2||i==3||i==5)&&!(keys&HidNpadButton_A))return false;
    if(i==0){selected_player^=1;note.clear();}
    if(i==1){note.clear();tsl::changeTo<ControlsGui>();}
    if(i==2){note.clear();tsl::changeTo<GraphicsGui>();}
    if(i==3){if(!re5_overlay_storage::ready())note=tr("Could not mount SD card","Не удалось подключить SD");
     else if(!settings_persisted)note=load_error();
     else{link.language=settings.language;if(link.connect())send_saved();else note=link.message;}}
    if(i==4){auto candidate=settings;candidate.language=settings.language==re4::Language::English?re4::Language::Russian:re4::Language::English;change(candidate,re4::ProfileDomain::Controls,false);}
    if(i==5){re5_overlay_debug::set_enabled(!re5_overlay_debug::enabled());note.clear();}
    update();return true;
   });
  }
  frame_->setContent(list);update();return frame_;
 }
 void update()override{
  frame_->setTitle(tr("RE5 Controls","Управление RE5"));frame_->setSubtitle(note);
  const char* en[]={"Player","Controls","Graphics","Connect to RE5","Language","Monitor"};
  const char* ru[]={"Игрок","Управление","Графика","Подключиться к RE5","Язык","Мониторинг"};
  for(unsigned i=0;i<rows_.size();++i)rows_[i]->setText(tr(en[i],ru[i]));
  rows_[0]->setValue(selected_player?tr("Player 2","Игрок 2"):tr("Player 1","Игрок 1"));
  rows_[1]->setValue(control_profile_name());
  rows_[2]->setValue(re4::profile_name(re5::profile_kind(settings,re4::ProfileDomain::Graphics,selected_player),settings.language));
  rows_[3]->setValue("A");rows_[4]->setValue(settings.language==re4::Language::Russian?"Русский":"English");
  rows_[5]->setValue(monitor_status());
  if(getFocusedElement()==rows_[0])frame_->set_help(tr("Separate controls and calibration for both local players.","Раздельное управление и калибровка для двух локальных игроков."));
  else if(getFocusedElement()==rows_[2])frame_->set_help(tr("Shared graphics. Full restart: HOME > X > Close.","Общая графика. Перезапуск: HOME > X > Закрыть."));
  else if(getFocusedElement()==rows_[3]){
   help_=tr("Start RE5 with its module. A: send saved settings.","Запустите RE5 с модулем. A: передать сохранённые настройки.");
   if(!link.details.empty()){
    auto details=link.details;
    // The complete 32-byte BuildID needs two lines in this narrow overlay.
    for(std::size_t pos=0;pos<details.size();){
     if(details.find_first_not_of("0123456789ABCDEFabcdef",pos)==pos){++pos;continue;}
     const auto end=details.find_first_not_of("0123456789ABCDEFabcdef",pos);
     const auto count=(end==std::string::npos?details.size():end)-pos;
     if(count==64){details.insert(pos+32,1,'\n');pos+=65;}else pos+=count;
    }
    help_+="\n"+details;
   }
   frame_->set_help(help_.c_str());
  }
  else if(getFocusedElement()==rows_[5])frame_->set_help(tr("Read-only remote monitoring. A: enable / disable.\nRequires the connection key on SD. Port 28774.\nContinues while this overlay is hidden.","Удалённый мониторинг: только чтение. A: вкл. / выкл.\nНужен ключ подключения на SD. Порт 28774.\nРаботает и при скрытом оверлее."));
  else if(getFocusedElement()==rows_[4])frame_->set_help(tr("English or Russian. Saved in the RE5 configuration.","English / Русский. Сохраняется в настройках RE5."));
  else frame_->set_help(tr("Independent player controls; Default, Modern or Custom.\nGyro test and calibration are in the standalone app.","Управление игроков: исходное, современное или своё.\nТест гироскопа и калибровка в отдельном приложении."));
 }
};
class ControlsOverlay:public tsl::Overlay{
public:
 void initServices()override{
  re5_overlay_debug::stop();
  settings={};settings_persisted=false;selected_player=0;note.clear();const bool mounted=re5_overlay_storage::mount();
  if(mounted){mkdir("r5ovl:/config",0777);mkdir("r5ovl:/config/re4-controls",0777);settings_persisted=re5::load_settings_file(re5_overlay_storage::settings_path,settings);}
  link.language=settings.language;link.initialize();re5_overlay_debug::share_link(&link);
  if(mounted){if(!settings_persisted)note=load_error();else if(link.connect())send_saved();else note=link.message;}
  else note=tr("Could not mount SD card","Не удалось подключить SD");
 }
 void exitServices()override{re5_overlay_debug::stop();re5_overlay_debug::share_link(nullptr);link.close();re5_overlay_storage::close();}
 std::unique_ptr<tsl::Gui> loadInitialGui()override{return initially<MainGui>();}
};
}
int main(int argc,char** argv){return tsl::loop<ControlsOverlay>(argc,argv);}
