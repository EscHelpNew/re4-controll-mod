#define TESLA_INIT_IMPL
#include <tesla.hpp>
#include <sys/stat.h>
#include "game_link.hpp"
#include "storage.hpp"
#include "re4/settings_file.hpp"
#include "re4/control_menu.hpp"
#include "re4/version.hpp"
#include "re4/reticle_preview.hpp"

namespace {
GameLink link;
re4::Settings settings;
std::string note;
const char* tr(const char* en,const char* ru) { return settings.language==re4::Language::Russian?ru:en; }
void record_connection() {
    if (!overlay_storage::ready()) return;
    mkdir("r4ovl:/switch",0777);mkdir("r4ovl:/switch/re4-controls",0777);
    if (FILE* file=std::fopen("r4ovl:/switch/re4-controls/game-inspection.txt","wb")) {
        std::fprintf(file,"%s\n",link.details.c_str());std::fclose(file);
    }
}
void change() {
    const bool saved=overlay_storage::ready() && re4::save_profiled_settings(overlay_storage::settings_path,settings);
    link.language=settings.language;
    link.apply(settings);
    note=saved?link.message:tr("Could not save settings to SD","Не удалось сохранить настройки на SD");
}
void apply_profile(re4::ProfileDomain domain,re4::ProfileKind next){
    const auto result=overlay_storage::ready()?re4::switch_profile(overlay_storage::settings_path,settings,domain,next):
        re4::ProfileResult::SaveFailed;
    if(result==re4::ProfileResult::Applied){link.language=settings.language;link.apply(settings);}
    note=re4::profile_result_text(result,settings.language);
}
void change_profile(re4::ProfileDomain domain){
    apply_profile(domain,re4::profile_kind(settings,domain)==re4::ProfileKind::Default?
        re4::ProfileKind::Custom:re4::ProfileKind::Default);
}
class DescriptionFrame : public tsl::elm::OverlayFrame {
    const char* description_="Select a setting to see its description.";
    unsigned help_y_=tsl::cfg::FramebufferHeight-152;
    bool preview_=false;
public:
    DescriptionFrame():OverlayFrame("RE4 Controls",std::string(re4::kVersion)+" - Control settings"){}
    void set_help(const char* text){description_=text;}
    void set_preview(bool enabled){preview_=enabled;}
    void layout(u16 x,u16 y,u16 width,u16 height)override{
        OverlayFrame::layout(x,y,width,height);help_y_=y+height-(preview_?310:152);
        if(m_contentElement){
            m_contentElement->setBoundaries(x+35,y+125,width-85,height-73-125-(preview_?268:110));
            m_contentElement->invalidate();
        }
    }
    void draw(tsl::gfx::Renderer* renderer)override{
        OverlayFrame::draw(renderer);
        renderer->drawRect(20,help_y_-21,tsl::cfg::FramebufferWidth-40,1,a(tsl::style::color::ColorDescription));
        if(preview_){
            renderer->drawString(tr("Reticle preview (x2)","Образец прицела (x2)"),false,20,help_y_+78,16,a(tsl::style::color::ColorDescription));
            re4::draw_reticle_preview(settings,20,help_y_+105,tsl::cfg::FramebufferWidth-40,130,
                [&](int x,int y,int w,int h,unsigned rgb){
                    const u16 color=u16((rgb>>20)&15)|u16((rgb>>12)&15)<<4|u16((rgb>>4)&15)<<8|u16((rgb>>28)&15)<<12;
                    renderer->drawRect(x,y,w,h,a(color));
                });
            if(settings.reticle==re4::ReticleStyle::Off)renderer->drawString(tr("Off","Выключен"),false,32,help_y_+130,16,a(tsl::style::color::ColorDescription));
        }
        renderer->drawString(description_,false,20,help_y_,16,a(tsl::style::color::ColorDescription));
    }
};
class ControlsProfilesGui : public tsl::Gui {
    DescriptionFrame* frame_=nullptr;
    std::array<tsl::elm::ListItem*,3> rows_{};
    re4::ProfileKind last_action_=re4::ProfileKind::Default;
public:
    tsl::elm::Element* createUI()override{
        frame_=new DescriptionFrame;auto* list=new tsl::elm::List;
        for(unsigned i=0;i<re4::kControlProfiles.size();++i){
            const auto kind=re4::kControlProfiles[i];
            auto* row=new tsl::elm::ListItem(re4::profile_name(kind,settings.language));
            row->setClickListener([this,kind](u64 keys){
                if(!(keys&HidNpadButton_A))return false;
                last_action_=kind;
                apply_profile(re4::ProfileDomain::Controls,kind);update();return true;
            });
            rows_[i]=row;list->addItem(row);
        }
        frame_->setContent(list);update();return frame_;
    }
    void update()override{
        if(!frame_)return;
        const auto active=re4::profile_kind(settings,re4::ProfileDomain::Controls);
        for(unsigned i=0;i<rows_.size();++i){
            const auto kind=re4::kControlProfiles[i];
            rows_[i]->setValue(active==kind?tr("Active","Активен"):"A");
            if(getFocusedElement()==rows_[i] || (!getFocusedElement() && i==0))
                frame_->set_help(note.empty()||kind!=last_action_?re4::control_profile_help(kind,settings.language):note.c_str());
        }
        frame_->setSubtitle(note.empty()?tr("Choose controls profile","Выберите профиль управления"):note.c_str());
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
        frame_->setSubtitle(std::string(re4::button_name(source_))+" -> "+re4::mapping_row_value(source_+1,settings));
        frame_->set_help(re4::mapping_target_help(settings.language));
        auto* list=new tsl::elm::List;
        for(unsigned target=0;target<=re4::kButtonCount;++target){
            auto* item=new tsl::elm::ListItem(re4::mapping_target_label(target,settings.language),
                target==re4::mapping_target_index(settings,source_)?"*":"");
            item->setClickListener([source=source_,target](u64 keys){
                if(!(keys&HidNpadButton_A))return false;
                if(re4::set_mapping_target(settings,source,target)){note.clear();change();}
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
        frame_->set_help(re4::mapping_row_help(0,settings.language));
        auto* list=new tsl::elm::List;
        for(unsigned row=0;row<re4::kMappingRowCount;++row){
            auto* item=new tsl::elm::ListItem(re4::mapping_row_label(row,settings.language),re4::mapping_row_value(row,settings));
            item->setClickListener([this,row](u64 keys){
                if(row>0 && row<=re4::kButtonCount && (keys&HidNpadButton_A)){
                    tsl::changeTo<MappingTargetsGui>(row-1);return true;
                }
                if(row==re4::kMappingRowCount-1 && !(keys&HidNpadButton_A))return false;
                if(!(keys&(HidNpadButton_A|HidNpadButton_Left|HidNpadButton_Right)))return false;
                if(re4::change_mapping(settings,row,(keys&HidNpadButton_Left)?-1:1)){note.clear();change();}
                update();return true;
            });
            list->addItem(item);rows_.push_back(item);
        }
        frame_->setContent(list);return frame_;
    }
    void update()override{
        for(unsigned row=0;row<rows_.size();++row){
            rows_[row]->setValue(re4::mapping_row_value(row,settings));
            if(getFocusedElement()==rows_[row])frame_->set_help(re4::mapping_row_help(row,settings.language));
        }
    }
};
class GraphicsGui : public tsl::Gui {
    DescriptionFrame* frame_=nullptr;
    std::vector<tsl::elm::ListItem*> rows_;
public:
    tsl::elm::Element* createUI()override{
        rows_.clear();frame_=new DescriptionFrame;
        auto* list=new tsl::elm::List;
        for(const auto& spec:re4::graphics_specs()){
            const auto id=spec.id;
            auto* item=new tsl::elm::ListItem(spec.label.get(settings.language),re4::graphics_value_text(spec,settings));
            item->setClickListener([this,id](u64 keys){
                const auto& entry=re4::graphics_spec(id);
                if(entry.kind==re4::ControlKind::Action && !(keys&HidNpadButton_A))return false;
                const bool arrows=keys&(HidNpadButton_Left|HidNpadButton_Right);
                if(!arrows && !(entry.kind!=re4::ControlKind::Slider && (keys&HidNpadButton_A)))return false;
                if(id==re4::GraphicsId::Profile){change_profile(re4::ProfileDomain::Graphics);update();return true;}
                const bool changed=id==re4::GraphicsId::Reset?re4::reset_graphics(settings):
                    re4::change_graphics(settings,id,(keys&HidNpadButton_Left)?-1:1);
                if(changed){note.clear();change();}
                update();return true;
            });
            list->addItem(item);rows_.push_back(item);
        }
        frame_->setContent(list);frame_->set_help(re4::graphics_spec(re4::GraphicsId::TextureFiltering).help.get(settings.language));
        update();return frame_;
    }
    void update()override{
        frame_->setTitle(tr("Graphics","Графика"));
        frame_->setSubtitle(std::string(re4::kVersion)+tr(" - Manual graphics settings"," - Ручные настройки графики"));
        for(unsigned row=0;row<rows_.size();++row){
            const auto& spec=re4::graphics_specs()[row];
            rows_[row]->setText(spec.label.get(settings.language));
            rows_[row]->setValue(re4::graphics_value_text(spec,settings));
            if(getFocusedElement()==rows_[row])frame_->set_help(spec.id==re4::GraphicsId::Profile && !note.empty()?
                note.c_str():spec.help.get(settings.language));
        }
    }
};
class ControlsPageGui : public tsl::Gui {
    struct Binding {tsl::elm::ListItem* item;const re4::ControlSpec* spec;};
    re4::ControlGroup group_;
    std::vector<Binding> bindings_;
    DescriptionFrame* description_frame_=nullptr;
    std::array<std::uint8_t,re4::kSettingsBytes> rendered_settings_{};
    std::string rendered_note_;
    void refresh(){
        link.language=settings.language;
        description_frame_->setTitle(re4::control_group_spec(group_).label.get(settings.language));
        description_frame_->setSubtitle(note.empty()?re4::kVersion:note);
        for(const auto& binding:bindings_){
            const auto& spec=*binding.spec;
            binding.item->setText(spec.label.get(settings.language));
            if(spec.toggle)static_cast<tsl::elm::ToggleListItem*>(binding.item)->setState(settings.*spec.toggle);
            binding.item->setValue(re4::control_value_text(spec,settings));
        }
        re4::encode_settings(settings,rendered_settings_.data(),rendered_settings_.size());
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
                auto* toggle=new tsl::elm::ToggleListItem(spec.label.get(settings.language),settings.*spec.toggle,"On","Off");
                toggle->setStateChangedListener([this,entry](bool value){
                    settings.*entry->toggle=value;note.clear();refresh();change();
                });
                item=toggle;
            }else{
                item=new tsl::elm::ListItem(spec.label.get(settings.language),re4::control_value_text(spec,settings));
                item->setClickListener([this,entry](u64 keys){
                    if(entry->id==re4::ControlId::Profile){
                        if(!(keys&(HidNpadButton_A|HidNpadButton_Left|HidNpadButton_Right)))return false;
                        note.clear();tsl::changeTo<ControlsProfilesGui>();return true;
                    }
                    if(entry->id==re4::ControlId::ButtonMapping){
                        if(!(keys&HidNpadButton_A))return false;
                        tsl::changeTo<MappingGui>();return true;
                    }
                    if(entry->kind==re4::ControlKind::Action && !(keys&HidNpadButton_A))return false;
                    const bool arrows=keys&(HidNpadButton_Left|HidNpadButton_Right);
                    if(!arrows && !(entry->kind!=re4::ControlKind::Slider && (keys&HidNpadButton_A)))return false;
                    const int direction=(keys&HidNpadButton_Left)?-1:1;
                    if(re4::change_control(settings,entry->id,direction)){note.clear();refresh();change();}
                    return true;
                });
            }
            list->addItem(item);bindings_.push_back({item,entry});
        }
        if(re4::control_group_count(group_))frame->set_help(re4::control_spec(re4::control_group_item(group_,0)).help.get(settings.language));
        frame->setContent(list);refresh();return frame;
    }
    void update()override{
        std::array<std::uint8_t,re4::kSettingsBytes> current{};
        re4::encode_settings(settings,current.data(),current.size());
        if(current!=rendered_settings_ || note!=rendered_note_)refresh();
        if(description_frame_){
            for(const auto& binding:bindings_)if(binding.item==getFocusedElement()){
                description_frame_->set_help(binding.spec->id==re4::ControlId::Profile && !note.empty()?
                    note.c_str():binding.spec->help.get(settings.language));break;
            }
        }
    }
};
class ControlsGui : public tsl::Gui {
    DescriptionFrame* frame_=nullptr;
    std::vector<tsl::elm::ListItem*> groups_;
public:
    tsl::elm::Element* createUI()override{
        groups_.clear();frame_=new DescriptionFrame;auto* list=new tsl::elm::List;
        for(const auto& group:re4::control_groups()){
            auto* item=new tsl::elm::ListItem(group.label.get(settings.language),"A");
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
        frame_->setTitle(tr("Controls","Управление"));frame_->setSubtitle(re4::kVersion);
        for(unsigned i=0;i<groups_.size();++i){
            const auto& group=re4::control_groups()[i];groups_[i]->setText(group.label.get(settings.language));
            if(getFocusedElement()==groups_[i])frame_->set_help(group.help.get(settings.language));
        }
    }
};
class MainGui : public tsl::Gui {
    DescriptionFrame* frame_=nullptr;
    tsl::elm::ListItem *controls_=nullptr,*graphics_=nullptr,*status_=nullptr,*language_=nullptr;
public:
    tsl::elm::Element* createUI()override{
        frame_=new DescriptionFrame;auto* list=new tsl::elm::List;
        status_=new tsl::elm::ListItem("Connect to game","A");
        status_->setClickListener([](u64 keys){
            if(!(keys&HidNpadButton_A))return false;
            link.language=settings.language;const bool connected=link.connect();record_connection();
            if(connected)link.apply(settings);
            note=link.message;return true;
        });
        controls_=new tsl::elm::ListItem("Controls");
        controls_->setClickListener([](u64 keys){
            if(!(keys&HidNpadButton_A))return false;
            note.clear();tsl::changeTo<ControlsGui>();return true;
        });
        graphics_=new tsl::elm::ListItem("Graphics");
        graphics_->setClickListener([](u64 keys){
            if(!(keys&HidNpadButton_A))return false;
            note.clear();tsl::changeTo<GraphicsGui>();return true;
        });
        language_=new tsl::elm::ListItem("Language");
        language_->setClickListener([this](u64 keys){
            if(!(keys&(HidNpadButton_A|HidNpadButton_Left|HidNpadButton_Right)))return false;
            re4::change_control(settings,re4::ControlId::Language,1);note.clear();change();update();return true;
        });
        list->addItem(status_);list->addItem(controls_);list->addItem(graphics_);list->addItem(language_);
        frame_->setContent(list);update();return frame_;
    }
    void update()override{
        frame_->setTitle("RE4 Controls");frame_->setSubtitle(re4::kVersion);
        status_->setText(note.empty()?tr("Connect to game","Подключиться к игре"):note);
        controls_->setText(tr("Controls","Управление"));graphics_->setText(tr("Graphics","Графика"));
        language_->setText(tr("Language","Язык"));language_->setValue(settings.language==re4::Language::Russian?"Русский":"English");
        controls_->setValue(re4::profile_name(re4::profile_kind(settings,re4::ProfileDomain::Controls),settings.language));
        graphics_->setValue(re4::profile_name(re4::profile_kind(settings,re4::ProfileDomain::Graphics),settings.language));
        if(getFocusedElement()==language_){frame_->set_help(re4::control_spec(re4::ControlId::Language).help.get(settings.language));return;}
        frame_->set_help(getFocusedElement()==graphics_?tr(
            "Independent graphics profile and settings.\nFiltering and AA require a game restart.",
            "Отдельный профиль и настройки графики.\nФильтрация и AA: перезапуск игры."):tr(
            "Controls and graphics have separate profiles.\nEach keeps Default and your last Custom.\nA: open. B: return.",
            "Управление и графика: отдельные профили.\nШтатный и последний пользовательский.\nA: открыть. B: назад."));
    }
};
class ControlsOverlay:public tsl::Overlay{
    void initServices()override{
        bool loaded=false;
        if(overlay_storage::mount()){
            mkdir("r4ovl:/config",0777);mkdir("r4ovl:/config/re4-controls",0777);
            loaded=re4::load_settings_file(overlay_storage::settings_path,settings);
        }else note=tr("Could not mount SD card","Не удалось подключить SD");
        link.language=settings.language;link.initialize();
        if(loaded){link.apply(settings);note=link.message;}
    }
    void exitServices()override{link.close();overlay_storage::close();}
    std::unique_ptr<tsl::Gui> loadInitialGui()override{return initially<MainGui>();}
};
}
int main(int argc,char** argv){return tsl::loop<ControlsOverlay>(argc,argv);}
