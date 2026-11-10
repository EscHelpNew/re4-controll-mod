#define TESLA_INIT_IMPL
#include <tesla.hpp>
#include <sys/stat.h>
#include <chrono>
#include "game_link.hpp"
#include "storage.hpp"
#include "live_debug.hpp"
#include "re4/settings_file.hpp"
#include "re4/control_menu.hpp"
#include "re4/game_menu.hpp"
#include "re4/version.hpp"
#include "re4/reticle_preview.hpp"
#include "re4/graphics_preview_image.hpp"
#include "re4/enemy_health_preview.hpp"
#include "re4/grenade_spot_preview.hpp"
#include "re4/character_loading.hpp"

namespace {
GameLink link;
re4::Settings settings=re4::default_controls_profile();
re4::LaunchMode launch_mode=re4::LaunchMode::Normal;
const char* launch_mode_path=re4::kOverlayLaunchModePath;
std::string note;
const char* tr(const char* en,const char* ru) { return settings.language==re4::Language::Russian?ru:en; }
void record_connection() {
    if (!re4::automatic_diagnostics(launch_mode)||!overlay_storage::ready()) return;
    mkdir("r4ovl:/switch",0777);mkdir("r4ovl:/switch/re4-controls",0777);
    if (FILE* file=std::fopen("r4ovl:/switch/re4-controls/game-inspection.txt","wb")) {
        std::fprintf(file,"%s\n",link.details.c_str());std::fclose(file);
    }
}
void change_launch_mode(re4::LaunchMode next){
    launch_mode=next;overlay_debug::set_mode(next);
    const bool saved=overlay_storage::ready()&&re4::save_launch_mode(launch_mode_path,next);
    note=saved?"":tr("Could not save launch mode to SD","Не удалось сохранить режим запуска на SD");
}
void change(bool graphics_edited=false) {
    const bool saved=overlay_storage::ready() && (graphics_edited?
        re4::save_edited_graphics(overlay_storage::settings_path,settings):
        re4::save_profiled_settings(overlay_storage::settings_path,settings));
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
class DescriptionFrame : public tsl::elm::OverlayFrame {
    const char* description_="Select a setting to see its description.";
    unsigned help_y_=tsl::cfg::FramebufferHeight-152;
    bool preview_=false;
    bool graphics_preview_=false;
    bool health_preview_=false;
    bool health_preview_visible_=true;
    bool grenade_preview_=false;
    graphics_preview::Topic graphics_topic_=graphics_preview::Topic::Filtering;
    int graphics_x_=64;
public:
    DescriptionFrame():OverlayFrame("RE4 Controls",std::string(re4::kVersion)+" - Control settings"){}
    void set_help(const char* text){description_=text;}
    void set_preview(bool enabled){preview_=enabled;}
    void set_graphics_preview(bool enabled){graphics_preview_=enabled;}
    void set_health_preview(bool enabled){health_preview_=enabled;}
    void set_health_preview_visible(bool visible){health_preview_visible_=visible;}
    void set_grenade_preview(bool visible){grenade_preview_=visible;}
    void set_graphics_topic(graphics_preview::Topic topic){graphics_topic_=topic;}
    void layout(u16 x,u16 y,u16 width,u16 height)override{
        OverlayFrame::layout(x,y,width,height);help_y_=y+height-(health_preview_?404:graphics_preview_?367:preview_?310:152);
        graphics_x_=int(x)+(int(width)-320)/2;
        if(m_contentElement){
            m_contentElement->setBoundaries(x+35,y+125,width-85,height-73-125-(health_preview_?362:graphics_preview_?325:preview_?268:110));
            m_contentElement->invalidate();
        }
    }
    void draw(tsl::gfx::Renderer* renderer)override{
        OverlayFrame::draw(renderer);
        renderer->drawRect(20,help_y_-21,tsl::cfg::FramebufferWidth-40,1,a(tsl::style::color::ColorDescription));
        if(grenade_preview_){
            renderer->drawString(description_,false,20,help_y_,16,a(tsl::style::color::ColorDescription));
            re4::grenade_preview::draw_rows(settings,graphics_x_+32,int(help_y_)+80,
                [&](int x,int y,int w,int h,const std::uint8_t* rgba){renderer->drawBitmap(x,y,w,h,rgba);});
            renderer->drawString(tr("Enlarged example","Увеличенный пример"),false,20,help_y_+280,16,a(tsl::style::color::ColorDescription));
            return;
        }
        if(health_preview_ && health_preview_visible_){
            renderer->drawString(description_,false,20,help_y_,16,a(tsl::style::color::ColorDescription));
            re4::health_preview::draw_rows(settings,graphics_x_+(320-re4::health_preview::width)/2,int(help_y_)+80,
                [&](int x,int y,int w,int h,const std::uint8_t* rgba){renderer->drawBitmap(x,y,w,h,rgba);});
            renderer->drawString(tr("Enlarged example","Увеличенный пример"),false,20,help_y_+320,16,a(tsl::style::color::ColorDescription));
            return;
        }
        if(graphics_preview_){
            renderer->drawString(description_,false,20,help_y_,16,a(tsl::style::color::ColorDescription));
            const int image_y=int(help_y_)+90;
            graphics_preview::draw_rows(graphics_preview::image(re4::GameTarget::RE4),graphics_x_,image_y,
                [&](int x,int y,int w,int h,const std::uint8_t* rgba){renderer->drawBitmap(x,y,w,h,rgba);});
            const auto roi=graphics_preview::focus(re4::GameTarget::RE4,graphics_topic_);
            if(roi.width && roi.height){
                const int x=graphics_x_+int(roi.x),y=image_y+int(roi.y),w=int(roi.width),h=int(roi.height);
                renderer->drawRect(x,y,w,2,a(0xF93B));renderer->drawRect(x,y+h-2,w,2,a(0xF93B));
                renderer->drawRect(x,y,2,h,a(0xF93B));renderer->drawRect(x+w-2,y,2,h,a(0xF93B));
            }
            renderer->drawString(graphics_preview::caption(graphics_topic_,settings.language==re4::Language::Russian),
                false,20,help_y_+282,16,a(tsl::style::color::ColorDescription));
            return;
        }
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
        rows_.clear();frame_=new DescriptionFrame;frame_->set_graphics_preview(true);
        auto* list=new tsl::elm::List;
        for(const auto& spec:re4::graphics_specs()){
            const auto id=spec.id;
            auto* item=new tsl::elm::ListItem(spec.label.get(settings.language),re4::graphics_value_text(spec,settings));
            item->setClickListener([this,id](u64 keys){
                const auto& entry=re4::graphics_spec(id);
                if(entry.kind==re4::ControlKind::Action && !(keys&HidNpadButton_A))return false;
                const bool arrows=keys&(HidNpadButton_Left|HidNpadButton_Right);
                if(!arrows && !(entry.kind!=re4::ControlKind::Slider && (keys&HidNpadButton_A)))return false;
                if(id==re4::GraphicsId::Profile){
                    apply_profile(re4::ProfileDomain::Graphics,re4::next_graphics_profile(
                        re4::profile_kind(settings,re4::ProfileDomain::Graphics),(keys&HidNpadButton_Left)?-1:1));
                    update();return true;
                }
                const bool changed=id==re4::GraphicsId::Reset?re4::reset_graphics(settings):
                    re4::change_graphics(settings,id,(keys&HidNpadButton_Left)?-1:1);
                if(changed){note.clear();change(id!=re4::GraphicsId::Reset);}
                update();return true;
            });
            list->addItem(item);rows_.push_back(item);
        }
        frame_->setContent(list);frame_->set_help(re4::graphics_spec(re4::GraphicsId::TextureFiltering).help.get(settings.language));
        update();return frame_;
    }
    void update()override{
        frame_->setTitle(tr("Graphics","Графика"));
        frame_->setSubtitle(std::string(re4::kVersion)+tr(" - Graphics profiles"," - Профили графики"));
        for(unsigned row=0;row<rows_.size();++row){
            const auto& spec=re4::graphics_specs()[row];
            rows_[row]->setText(spec.label.get(settings.language));
            rows_[row]->setValue(re4::graphics_value_text(spec,settings));
            if(getFocusedElement()==rows_[row]){
                frame_->set_help(spec.id==re4::GraphicsId::Profile && !note.empty()?note.c_str():spec.help.get(settings.language));
                frame_->set_graphics_topic(graphics_preview::topic_for(re4::GameTarget::RE4,row));
            }
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
        if(re4::control_group_count(group_))frame->set_help(re4::control_help_text(re4::control_spec(re4::control_group_item(group_,0)),settings));
        frame->setContent(list);refresh();return frame;
    }
    void update()override{
        std::array<std::uint8_t,re4::kSettingsBytes> current{};
        re4::encode_settings(settings,current.data(),current.size());
        if(current!=rendered_settings_ || note!=rendered_note_)refresh();
        if(description_frame_){
            for(const auto& binding:bindings_)if(binding.item==getFocusedElement()){
                description_frame_->set_help(binding.spec->id==re4::ControlId::Profile && !note.empty()?
                    note.c_str():re4::control_help_text(*binding.spec,settings));break;
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
        profile_=new tsl::elm::ListItem(re4::control_spec(re4::ControlId::Profile).label.get(settings.language));
        profile_->setClickListener([this](u64 keys){
            const int direction=(keys&HidNpadButton_Left)?-1:(keys&(HidNpadButton_A|HidNpadButton_Right))?1:0;
            if(!direction)return false;
            apply_profile(re4::ProfileDomain::Controls,re4::next_control_profile(re4::profile_kind(settings,re4::ProfileDomain::Controls),direction));
            update();return true;
        });
        list->addItem(profile_);
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
        profile_->setText(re4::control_spec(re4::ControlId::Profile).label.get(settings.language));
        profile_->setValue(re4::profile_name(re4::profile_kind(settings,re4::ProfileDomain::Controls),settings.language));
        if(getFocusedElement()==profile_ || !getFocusedElement())frame_->set_help(note.empty()?re4::control_help_text(re4::control_spec(re4::ControlId::Profile),settings):note.c_str());
        for(unsigned i=0;i<groups_.size();++i){
            const auto& group=re4::control_groups()[i];groups_[i]->setText(group.label.get(settings.language));
            if(getFocusedElement()==groups_[i])frame_->set_help(group.help.get(settings.language));
        }
    }
};
class CharacterAppearanceGui : public tsl::Gui {
    DescriptionFrame* frame_=nullptr;
    std::array<tsl::elm::ListItem*,8> choices_{};
    std::string help_;
public:
    tsl::elm::Element* createUI()override{
        frame_=new DescriptionFrame;auto* list=new tsl::elm::List;
        for(unsigned i=0;i<choices_.size();++i){
            auto* item=new tsl::elm::ListItem(re4::character_appearance_name(static_cast<re4::CharacterAppearance>(i),settings.language));
            item->setClickListener([this,i](u64 keys){
                if(!(keys&HidNpadButton_A))return false;
                if(unsigned(settings.character_appearance)!=i){
                    settings.character_appearance=static_cast<re4::CharacterAppearance>(i);note.clear();change();
                }
                update();return true;
            });
            choices_[i]=item;list->addItem(item);
        }
        frame_->setContent(list);update();return frame_;
    }
    void update()override{
        frame_->setTitle(re4::kCharacterAppearanceLabel.get(settings.language));frame_->setSubtitle(re4::kVersion);
        link.refresh_appearance();
        for(unsigned i=0;i<choices_.size();++i){
            choices_[i]->setText(re4::character_appearance_name(static_cast<re4::CharacterAppearance>(i),settings.language));
            choices_[i]->setValue(i==unsigned(settings.character_appearance)?re4::kAppearanceRequested.get(settings.language):
                link.appearance.valid&&i==unsigned(link.appearance.applied)?re4::kAppearanceApplied.get(settings.language):"");
        }
        help_=re4::kCharacterAppearanceHelp.get(settings.language);
        help_+="\n";help_+=re4::kAppearanceRequested.get(settings.language);help_+=": ";
        help_+=re4::character_appearance_name(settings.character_appearance,settings.language);
        help_+="\n";help_+=re4::kAppearanceApplied.get(settings.language);help_+=": ";
        if(!link.appearance.valid)help_+=tr("Unavailable","Недоступно");
        else{
            help_+=re4::character_appearance_name(link.appearance.applied,settings.language);help_+=" / ";
            help_+=re4::character_appearance_status(link.appearance.status,settings.language);
            if(link.appearance.error&&(link.appearance.status==re4::CharacterAppearanceStatus::Pending
                ||link.appearance.status==re4::CharacterAppearanceStatus::Failed)){
                help_+="\n";help_+=re4::character_appearance_reason(link.appearance.error,settings.language);
                help_+=" (";help_+=std::to_string(unsigned(re4::character_appearance_reason(link.appearance.error)));
                const auto stage=re4::character_loading_stage(link.appearance.error);
                if(stage!=re4::CharacterLoadingStage::None){
                    help_+=" / ";help_+=std::to_string(unsigned(stage));
                    if(stage==re4::CharacterLoadingStage::Headroom){
                        help_+=" / ";help_+=std::to_string(re4::character_loading_observed(link.appearance.error));help_+=" MiB";
                    }
                }else{
                    const auto observed=re4::character_appearance_observed(link.appearance.error);
                    if(observed>=0){help_+=" / ";help_+=std::to_string(observed);
                        if(re4::character_appearance_reason(link.appearance.error)==re4::CharacterAppearanceError::ReadingAsset)help_+="%";
                    }
                }
                help_+=")";
            }
        }
        frame_->set_help(help_.c_str());
    }
};
class MoneyBonusGui : public tsl::Gui {
    DescriptionFrame* frame_=nullptr;
    tsl::elm::ListItem* confirm_=nullptr;
    u64 refresh_tick_=0;
public:
    tsl::elm::Element* createUI()override{
        frame_=new DescriptionFrame;frame_->setTitle(re4::money_action_label(settings.language));
        frame_->setSubtitle(re4::kVersion);
        auto* list=new tsl::elm::List;
        auto* cancel=new tsl::elm::ListItem(tr("Cancel","Отмена"));
        cancel->setClickListener([](u64 keys){if(!(keys&HidNpadButton_A))return false;tsl::goBack();return true;});
        list->addItem(cancel);
        confirm_=new tsl::elm::ListItem(re4::money_action_label(settings.language));
        confirm_->setClickListener([this](u64 keys){
            if(!(keys&HidNpadButton_A))return false;
            link.language=settings.language;
            if(!link.request_money())return false;
            update();return true;
        });
        list->addItem(confirm_);frame_->setContent(list);update();return frame_;
    }
    void update()override{
        const auto now=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        if(!refresh_tick_||u64(now)-refresh_tick_>=250){link.refresh_money();refresh_tick_=u64(now);}
        confirm_->setValue(re4::money_status_text(link.money,settings.language));
        frame_->set_help(link.money.result==re4::MoneyResult::Granted?tr(
            "Added 200 000 pesetas.\nSave your game to keep the money.\nThe receipt blocks another bonus.",
            "Добавлено 200 000 песет.\nСохраните игру, чтобы сохранить деньги.\nПовторно получить бонус нельзя."):tr(
            "Adds 200 000 pesetas and makes the game easier.\nOne-time bonus; the receipt is saved on SD.\nSave afterwards: unsaved money can be lost.",
            "Добавит 200 000 песет и облегчит игру.\nРазовый бонус; отметка сохраняется на SD.\nСохраните игру: несохранённые деньги теряются."));
    }
};
class GameGui : public tsl::Gui {
    DescriptionFrame* frame_=nullptr;
    tsl::elm::ToggleListItem* health_=nullptr;
    tsl::elm::ListItem* numbers_=nullptr;
    tsl::elm::ListItem* appearance_=nullptr;
    tsl::elm::ListItem* money_=nullptr;
    std::chrono::steady_clock::time_point money_refreshed_{};
    std::array<tsl::elm::ListItem*,7> options_{};
    std::string help_;
public:
    tsl::elm::Element* createUI()override{
        frame_=new DescriptionFrame;frame_->set_health_preview(true);auto* list=new tsl::elm::List;
        health_=new tsl::elm::ToggleListItem(re4::kEnemyHealthBarsLabel.get(settings.language),settings.enemy_health_bars,"On","Off");
        health_->setStateChangedListener([this](bool value){
            settings.enemy_health_bars=value;note.clear();change();update();
        });
        list->addItem(health_);
        numbers_=new tsl::elm::ListItem(re4::kEnemyHealthNumbersLabel.get(settings.language));
        numbers_->setClickListener([this](u64 keys){
            if(!(keys&HidNpadButton_A) || !settings.enemy_health_bars)return false;
            settings.enemy_health_numbers=!settings.enemy_health_numbers;note.clear();change();update();return true;
        });
        list->addItem(numbers_);
        const re4::Bilingual labels[]={re4::kGrenadeLandingSpotLabel,re4::kSkipPickupConfirmationLabel,re4::kSkipStartupLogosLabel,re4::kRemoveAimingSwayLabel,re4::kDpadWeaponSwitchingLabel,re4::kExtendedWeaponUpgradeLabel,re4::kCaseAmmoCounterLabel};
        bool re4::Settings::* members[]={&re4::Settings::grenade_landing_spot,&re4::Settings::skip_pickup_confirmation,&re4::Settings::skip_startup_logos,&re4::Settings::disable_aim_sway,&re4::Settings::dpad_weapon_switching,&re4::Settings::extended_weapon_upgrade,&re4::Settings::case_ammo_counter};
        for(unsigned i=0;i<options_.size();++i){
            auto* item=new tsl::elm::ListItem(labels[i].get(settings.language));
            item->setClickListener([this,member=members[i]](u64 keys){
                if(!(keys&HidNpadButton_A))return false;
                settings.*member=!(settings.*member);note.clear();change();update();return true;
            });
            options_[i]=item;list->addItem(item);
        }
        money_=new tsl::elm::ListItem(re4::money_action_label(settings.language));
        money_->setClickListener([](u64 keys){
            if(!(keys&HidNpadButton_A))return false;
            link.language=settings.language;
            if(!link.refresh_money()||link.money.pending()||link.money.availability!=re4::MoneyAvailability::Ready)return false;
            tsl::changeTo<MoneyBonusGui>();return true;
        });
        list->addItem(money_);
        appearance_=new tsl::elm::ListItem(re4::kCharacterAppearanceLabel.get(settings.language));
        appearance_->setClickListener([](u64 keys){
            if(!(keys&HidNpadButton_A))return false;
            note.clear();tsl::changeTo<CharacterAppearanceGui>();return true;
        });
        list->addItem(appearance_);
        frame_->setContent(list);update();return frame_;
    }
    void update()override{
        frame_->setTitle(re4::kGameMenuLabel.get(settings.language));
        frame_->setSubtitle(re4::kVersion);
        frame_->set_help(getFocusedElement()==numbers_?re4::enemy_health_numbers_help(settings):re4::kEnemyHealthBarsHelp.get(settings.language));
        bool option_focused=false;
        for(auto* option:options_)option_focused=option_focused||getFocusedElement()==option;
        frame_->set_health_preview_visible(getFocusedElement()!=appearance_ && getFocusedElement()!=money_ && !option_focused);
        frame_->set_grenade_preview(getFocusedElement()==options_[0]);
        const re4::Bilingual labels[]={re4::kGrenadeLandingSpotLabel,re4::kSkipPickupConfirmationLabel,re4::kSkipStartupLogosLabel,re4::kRemoveAimingSwayLabel,re4::kDpadWeaponSwitchingLabel,re4::kExtendedWeaponUpgradeLabel,re4::kCaseAmmoCounterLabel};
        const re4::Bilingual help[]={re4::kGrenadeLandingSpotHelp,re4::kSkipPickupConfirmationHelp,re4::kSkipStartupLogosHelp,re4::kRemoveAimingSwayHelp,re4::kDpadWeaponSwitchingHelp,re4::kExtendedWeaponUpgradeHelp,re4::kCaseAmmoCounterHelp};
        const bool values[]={settings.grenade_landing_spot,settings.skip_pickup_confirmation,settings.skip_startup_logos,settings.disable_aim_sway,settings.dpad_weapon_switching,settings.extended_weapon_upgrade,settings.case_ammo_counter};
        for(unsigned i=0;i<options_.size();++i){
            options_[i]->setText(labels[i].get(settings.language));
            options_[i]->setValue(re4::game_option_value(values[i],settings.language));
            if(getFocusedElement()==options_[i])frame_->set_help(help[i].get(settings.language));
        }
        if(getFocusedElement()==options_[4]&&settings.dpad_weapon_switching&&link.refresh_popup()){
            const auto diagnostic=link.popup_diagnostic;
            help_=re4::kDpadWeaponSwitchingHelp.get(settings.language);
            help_+=tr("\nPopup: ","\nПодпись: ");
            help_+=std::to_string(diagnostic&255);help_+=" / ";help_+=std::to_string((diagnostic>>8)&255);
            help_+=tr("; events/frames: ","; смен/кадров: ");
            help_+=std::to_string((diagnostic>>16)&255);help_+=" / ";help_+=std::to_string(diagnostic>>24);
            if(link.popup_bounds_valid){
                char bounds[128];
                std::snprintf(bounds,sizeof(bounds),"\nV:%.1f,%.1f,%.1f,%.1f\nS:%d,%d,%d,%d",
                    double(link.popup_viewport[0]),double(link.popup_viewport[1]),
                    double(link.popup_viewport[2]),double(link.popup_viewport[3]),
                    int(link.popup_scissor[0]),int(link.popup_scissor[1]),
                    int(link.popup_scissor[2]),int(link.popup_scissor[3]));
                help_+=bounds;
            }
            frame_->set_help(help_.c_str());
        }
        health_->setText(re4::kEnemyHealthBarsLabel.get(settings.language));
        health_->setState(settings.enemy_health_bars);
        health_->setValue(re4::enemy_health_bars_value(settings));
        numbers_->setText(re4::kEnemyHealthNumbersLabel.get(settings.language));
        numbers_->setValue(re4::enemy_health_numbers_value(settings));
        appearance_->setText(re4::kCharacterAppearanceLabel.get(settings.language));
        appearance_->setValue(re4::character_appearance_name(settings.character_appearance,settings.language));
        if(getFocusedElement()==appearance_)frame_->set_help(re4::kCharacterAppearanceHelp.get(settings.language));
        money_->setText(re4::money_action_label(settings.language));
        if(getFocusedElement()==money_){
            const auto now=std::chrono::steady_clock::now();
            if(now-money_refreshed_>=std::chrono::milliseconds(250)){link.refresh_money();money_refreshed_=now;}
            money_->setValue(re4::money_status_text(link.money,settings.language));
            frame_->set_help(tr("Confirmation required. Receipt saved in a separate SD file.\nLoading another save does not reset it.",
                "С подтверждением. Отметка — в отдельном файле на SD.\nЗагрузка другого сохранения её не сбрасывает."));
        }
    }
};
class MainGui : public tsl::Gui {
    DescriptionFrame* frame_=nullptr;
    tsl::elm::ListItem *controls_=nullptr,*graphics_=nullptr,*game_=nullptr,*status_=nullptr,*language_=nullptr,*diagnostics_=nullptr;
    std::string diagnostics_help_;
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
        game_=new tsl::elm::ListItem("Game");
        game_->setClickListener([](u64 keys){
            if(!(keys&HidNpadButton_A))return false;
            note.clear();tsl::changeTo<GameGui>();return true;
        });
        language_=new tsl::elm::ListItem("Language");
        language_->setClickListener([this](u64 keys){
            if(!(keys&(HidNpadButton_A|HidNpadButton_Left|HidNpadButton_Right)))return false;
            re4::change_control(settings,re4::ControlId::Language,1);note.clear();change();update();return true;
        });
        diagnostics_=new tsl::elm::ListItem("Launch mode");
        diagnostics_->setClickListener([this](u64 keys){
            if(!(keys&(HidNpadButton_A|HidNpadButton_Left|HidNpadButton_Right)))return false;
            change_launch_mode(re4::automatic_diagnostics(launch_mode)?re4::LaunchMode::Normal:re4::LaunchMode::Diagnostics);
            update();return true;
        });
        list->addItem(status_);list->addItem(controls_);list->addItem(graphics_);list->addItem(game_);list->addItem(language_);list->addItem(diagnostics_);
        frame_->setContent(list);update();return frame_;
    }
    void update()override{
        frame_->setTitle("RE4 Controls");frame_->setSubtitle(re4::kVersion);
        diagnostics_->setText(tr("Launch mode","Режим запуска"));
        const auto diagnostic_state=overlay_debug::state();
        diagnostics_->setValue(re4::automatic_diagnostics(launch_mode)?tr("Diagnostics","Диагностика"):tr("Normal","Обычный"));
        game_->setText(re4::kGameMenuLabel.get(settings.language));
        status_->setText(note.empty()?tr("Connect to game","Подключиться к игре"):note);
        controls_->setText(tr("Controls","Управление"));graphics_->setText(tr("Graphics","Графика"));
        language_->setText(tr("Language","Язык"));language_->setValue(settings.language==re4::Language::Russian?"Русский":"English");
        controls_->setValue(re4::profile_name(re4::profile_kind(settings,re4::ProfileDomain::Controls),settings.language));
        graphics_->setValue(re4::profile_name(re4::profile_kind(settings,re4::ProfileDomain::Graphics),settings.language));
        if(getFocusedElement()==diagnostics_){
            if(!re4::automatic_diagnostics(launch_mode)){
                frame_->set_help(tr("Normal: no automatic diagnostic logs or server.\nControls and appearance remain available.",
                    "Обычный: без автоматических логов и сервера диагностики.\nУправление и внешний вид доступны."));
            }else{
                diagnostics_help_=tr("Read-only diagnostics and logs.\nContinues while this overlay is hidden.\n",
                    "Чтение диагностики и запись логов.\nРаботает и при скрытом оверлее.\n");
                diagnostics_help_+=diagnostic_state==overlay_debug::State::Listening?
                    std::string(tr("Port: ","Порт: "))+std::to_string(overlay_debug::port()):
                    diagnostic_state==overlay_debug::State::Starting?tr("Starting","Запуск"):
                    diagnostic_state==overlay_debug::State::MissingKey?tr("No key","Нет ключа"):tr("Unavailable","Недоступен");
                frame_->set_help(diagnostics_help_.c_str());
            }
            return;
        }
        if(getFocusedElement()==game_){frame_->set_help(re4::kGameMenuHelp.get(settings.language));return;}
        if(getFocusedElement()==language_){frame_->set_help(re4::control_spec(re4::ControlId::Language).help.get(settings.language));return;}
        frame_->set_help(getFocusedElement()==graphics_?tr(
            "Full restart: HOME > X > Close, then start.\nMenu / overlay alone is not enough.",
            "Полный перезапуск: HOME > X > Закрыть.\nОдного меню / оверлея недостаточно."):tr(
            "Controls: Default, RE6 or your last Custom.\nGraphics have a separate saved profile.\nA: open. B: return.",
            "Управление: исходное, RE6 или своё.\nУ графики отдельный сохранённый профиль.\nA: открыть. B: назад."));
    }
};
class ControlsOverlay:public tsl::Overlay{
    void initServices()override{
        launch_mode=re4::LaunchMode::Normal;
        bool loaded=false;
        if(overlay_storage::mount()){
            mkdir("r4ovl:/config",0777);mkdir("r4ovl:/config/re4-controls",0777);
            loaded=re4::load_settings_file(overlay_storage::settings_path,settings);
            re4::load_launch_mode(launch_mode_path,launch_mode);
        }else note=tr("Could not mount SD card","Не удалось подключить SD");
        link.language=settings.language;link.initialize();
        if(loaded){link.apply(settings);note=link.message;}
        overlay_debug::share_link(&link);overlay_debug::set_mode(launch_mode);
    }
    void exitServices()override{overlay_debug::stop();launch_mode=re4::LaunchMode::Normal;overlay_debug::share_link(nullptr);link.close();overlay_storage::close();}
    std::unique_ptr<tsl::Gui> loadInitialGui()override{return initially<MainGui>();}
};
}
int main(int argc,char** argv){return tsl::loop<ControlsOverlay>(argc,argv);}
