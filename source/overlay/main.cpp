#define TESLA_INIT_IMPL
#include <tesla.hpp>
#include <sys/stat.h>
#include "game_link.hpp"
#include "storage.hpp"
#include "re4/settings_file.hpp"
#include "re4/control_menu.hpp"
#include "re4/version.hpp"

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
    const bool saved=overlay_storage::ready() && re4::save_settings_file(overlay_storage::settings_path,settings);
    link.language=settings.language;
    link.apply(settings);
    note=saved?link.message:tr("Could not save settings to SD","Не удалось сохранить настройки на SD");
}
class DescriptionFrame : public tsl::elm::OverlayFrame {
    const char* description_="Select a setting to see its description.";
    unsigned help_y_=tsl::cfg::FramebufferHeight-152;
public:
    DescriptionFrame():OverlayFrame("RE4 Controls",std::string(re4::kVersion)+" - Control settings"){}
    void set_help(const char* text){description_=text;}
    void layout(u16 x,u16 y,u16 width,u16 height)override{
        OverlayFrame::layout(x,y,width,height);help_y_=y+height-152;
        if(m_contentElement){
            m_contentElement->setBoundaries(x+35,y+125,width-85,height-73-125-110);
            m_contentElement->invalidate();
        }
    }
    void draw(tsl::gfx::Renderer* renderer)override{
        OverlayFrame::draw(renderer);
        renderer->drawRect(20,help_y_-21,tsl::cfg::FramebufferWidth-40,1,a(tsl::style::color::ColorDescription));
        renderer->drawString(description_,false,20,help_y_,16,a(tsl::style::color::ColorDescription));
    }
};
class ControlsGui : public tsl::Gui {
    struct Binding {tsl::elm::ListItem* item;const re4::ControlSpec* spec;};
    struct Header {tsl::elm::CategoryHeader* item;re4::Bilingual text;};
    std::vector<Binding> bindings_;
    std::vector<Header> headers_;
    tsl::elm::ListItem* status_=nullptr;
    DescriptionFrame* description_frame_=nullptr;
    unsigned frame_=0;
    void add_header(tsl::elm::List* list,re4::Bilingual text){
        auto* item=new tsl::elm::CategoryHeader(text.get(settings.language));
        list->addItem(item);headers_.push_back({item,text});
    }
    void refresh(){
        link.language=settings.language;
        description_frame_->setTitle(tr("RE4 Controls","RE4 Управление"));
        description_frame_->setSubtitle(std::string(re4::kVersion)+tr(" - Control settings"," - Настройки управления"));
        status_->setText(tr("Connect to game","Подключиться к игре"));
        for(const auto& header:headers_)header.item->setText(header.text.get(settings.language));
        for(const auto& binding:bindings_){
            const auto& spec=*binding.spec;
            binding.item->setText(spec.label.get(settings.language));
            if(spec.toggle)static_cast<tsl::elm::ToggleListItem*>(binding.item)->setState(settings.*spec.toggle);
            binding.item->setValue(re4::control_value_text(spec,settings));
        }
    }
public:
    tsl::elm::Element* createUI()override{
        bindings_.clear();headers_.clear();
        auto* frame=description_frame_=new DescriptionFrame;
        auto* list=new tsl::elm::List;
        status_=new tsl::elm::ListItem(tr("Connect to game","Подключиться к игре"),"A");
        status_->setClickListener([](u64 keys){
            if(!(keys&HidNpadButton_A))return false;
            link.language=settings.language;
            const bool connected=link.connect();record_connection();
            if(connected)link.apply(settings);
            note=link.message;return true;
        });
        list->addItem(status_);
        for(const auto& spec:re4::control_specs()){
            const auto* entry=&spec;
            if(spec.id==re4::ControlId::Language)add_header(list,{"General","Общие"});
            if(spec.id==re4::ControlId::Analog)add_header(list,{"Sticks and aiming","Стики и прицел"});
            if(spec.id==re4::ControlId::Gyro)add_header(list,{"Gyro - firearm aiming only","Гироскоп - только прицеливание"});
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
        frame->setContent(list);refresh();return frame;
    }
    void update()override{
        if(description_frame_){
            if(getFocusedElement()==status_)description_frame_->set_help(tr(
                "A: connect to the mod in running RE4.\nReconnect after restarting the game.\nSettings apply without closing the game.",
                "A: найти мод в запущенной RE4.\nПосле перезапуска игры подключитесь снова.\nНастройки применяются без выхода из игры."));
            for(const auto& binding:bindings_)if(binding.item==getFocusedElement()){
                description_frame_->set_help(binding.spec->help.get(settings.language));break;
            }
        }
        if(++frame_%15==0 && status_ && !note.empty())status_->setText(note);
    }
};
class ControlsOverlay:public tsl::Overlay{
    void initServices()override{
        if(overlay_storage::mount()){
            mkdir("r4ovl:/config",0777);mkdir("r4ovl:/config/re4-controls",0777);
            re4::load_settings_file(overlay_storage::settings_path,settings);
        }else note=tr("Could not mount SD card","Не удалось подключить SD");
        link.language=settings.language;link.initialize();
    }
    void exitServices()override{link.close();overlay_storage::close();}
    std::unique_ptr<tsl::Gui> loadInitialGui()override{return initially<ControlsGui>();}
};
}
int main(int argc,char** argv){return tsl::loop<ControlsOverlay>(argc,argv);}
