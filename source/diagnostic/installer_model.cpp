#include "re4/installer_ui.hpp"
#include <cstring>
namespace re4 {
const std::array<HomeItem,7>& installer_home_items(){
    static constexpr std::array<HomeItem,7> items{{
        {{"Install / update mod","Установить / обновить мод"},
         {"Install smooth controls and the overlay.\nClose Resident Evil 4 before installing.",
          "Установить управление и меню оверлея.\nПеред установкой закройте Resident Evil 4."}},
        {{"Install overlay menu","Установить меню оверлея"},
         {"Update only the RE4 Controls menu.\nKeeps the existing Uberhand installation.",
          "Обновить только меню RE4 Controls.\nСуществующий Uberhand сохраняется."}},
        {{"Disable mod","Отключить мод"},
         {"Disable this mod's owned files.\nStart RE4 again to use native controls.",
          "Отключить только файлы нашего мода.\nПосле запуска RE4 управление будет штатным."}},
        {{"Control settings","Настройки управления"},
         {"Stick response, sensitivity, deadzones\nand firearm-only gyro aiming.",
          "Кривые стиков, чувствительность, мёртвые\nзоны и гироскоп при наведении из оружия."}},
        {{"Controller diagnostics","Проверка контроллера"},
         {"Check raw sticks, gyro and calibration.\nHold ZL to preview motion aiming.",
          "Проверить стики, гироскоп и калибровку.\nУдерживайте ZL для проверки прицела."}},
        {{"Language","Язык"},
         {"Choose English or Russian.\nUse LEFT/RIGHT or A to change.",
          "Выберите русский или английский.\nСмена: влево/вправо или A."}},
        {{"Exit","Выход"},
         {"Return to the HOME menu.",
          "Вернуться на главный экран консоли."}}
    }};
    return items;
}
unsigned InstallerModel::item_count()const{
    return page==InstallerPage::Home?7:page==InstallerPage::Controls?unsigned(ControlId::Count):0;
}
InstallerAction InstallerModel::handle(UiEvent event){
    if(event==UiEvent::Exit)return InstallerAction::Exit;
    if(event==UiEvent::Back){
        if(page==InstallerPage::Home)return InstallerAction::Exit;
        page=InstallerPage::Home;selected=home_selected;return InstallerAction::None;
    }
    if(page==InstallerPage::Home){
        if(event==UiEvent::QuickInstall)return InstallerAction::InstallGame;
        if(event==UiEvent::QuickOverlay)return InstallerAction::InstallOverlay;
        if(event==UiEvent::QuickDisable)return InstallerAction::DisableGame;
    }
    const auto count=item_count();
    if(count && (event==UiEvent::Up||event==UiEvent::Down)){
        selected=(selected+(event==UiEvent::Down?1:count-1))%count;return InstallerAction::None;
    }
    if(page==InstallerPage::Diagnostics){
        if(event==UiEvent::Confirm){change_control(settings,ControlId::Gyro,1);return InstallerAction::SaveSettings;}
        if(event==UiEvent::ResetPreview)return InstallerAction::ResetPreview;
        if(event==UiEvent::Calibrate){
            if(diagnostic.sensors)return InstallerAction::Calibrate;
            status=InstallerStatus::NoSensor;
        }
        return InstallerAction::None;
    }
    if(page==InstallerPage::Controls){
        if(selected>=count)selected=0;
        const auto& spec=control_specs()[selected];
        if(spec.kind==ControlKind::Action && event!=UiEvent::Confirm)return InstallerAction::None;
        const bool arrows=event==UiEvent::Left||event==UiEvent::Right;
        if(!arrows && !(event==UiEvent::Confirm && spec.kind!=ControlKind::Slider))return InstallerAction::None;
        if(change_control(settings,spec.id,event==UiEvent::Left?-1:1))return InstallerAction::SaveSettings;
        return InstallerAction::None;
    }
    if(selected==5 && (event==UiEvent::Confirm||event==UiEvent::Left||event==UiEvent::Right)){
        change_control(settings,ControlId::Language,1);return InstallerAction::SaveSettings;
    }
    if(event!=UiEvent::Confirm)return InstallerAction::None;
    if(selected==0)return InstallerAction::InstallGame;
    if(selected==1)return InstallerAction::InstallOverlay;
    if(selected==2)return InstallerAction::DisableGame;
    if(selected==6)return InstallerAction::Exit;
    home_selected=selected;
    page=selected==3?InstallerPage::Controls:InstallerPage::Diagnostics;
    selected=0;return InstallerAction::None;
}
const char* InstallerModel::page_title()const{
    const Bilingual text=page==InstallerPage::Home?Bilingual{"MOD INSTALLER","УСТАНОВКА МОДА"}
        :page==InstallerPage::Controls?Bilingual{"CONTROL SETTINGS","НАСТРОЙКИ УПРАВЛЕНИЯ"}
        :Bilingual{"CONTROLLER DIAGNOSTICS","ПРОВЕРКА КОНТРОЛЛЕРА"};
    return text.get(settings.language);
}
const char* InstallerModel::status_text()const{
    static constexpr Bilingual messages[]={
        {"Ready","Готово"},
        {"Settings saved","Настройки сохранены"},
        {"Mod installed. Start RE4 again.","Мод установлен. Запустите RE4 заново."},
        {"Overlay menu installed","Меню оверлея установлено"},
        {"Mod disabled. Start RE4 again.","Мод отключён. Запустите RE4 заново."},
        {"Close Resident Evil 4 first","Сначала закройте Resident Evil 4"},
        {"Conflict: another mod's files were kept","Конфликт: файлы другого мода сохранены"},
        {"Operation failed. See diagnostics.","Ошибка операции. Откройте проверку."},
        {"Keep the controller still...","Не двигайте контроллер..."},
        {"Calibration saved","Калибровка сохранена"},
        {"No gyro sensor is available","Датчик гироскопа недоступен"}
    };
    const auto index=unsigned(status);
    return messages[index<sizeof(messages)/sizeof(*messages)?index:unsigned(InstallerStatus::Failed)].get(settings.language);
}
const char* InstallerModel::selected_help()const{
    if(page==InstallerPage::Home)return installer_home_items()[selected<7?selected:0].help.get(settings.language);
    if(page==InstallerPage::Controls)return control_spec(static_cast<ControlId>(selected)).help.get(settings.language);
    return Bilingual{"ZL: gyro preview  A: gyro on/off\nY: calibrate  X: reset preview",
                     "ZL: проверка  A: гироскоп вкл./выкл.\nY: калибровать  X: сбросить прицел"}.get(settings.language);
}
std::string InstallerModel::item_label(unsigned index)const{
    if(page==InstallerPage::Home)return installer_home_items()[index<7?index:0].label.get(settings.language);
    return control_spec(static_cast<ControlId>(index)).label.get(settings.language);
}
std::string InstallerModel::item_value(unsigned index)const{
    if(page==InstallerPage::Controls)return control_value_text(control_spec(static_cast<ControlId>(index)),settings);
    if(index==5)return settings.language==Language::Russian?"Русский":"English";
    if(index==3)return curve_name(settings.stick_curve,settings.language);
    return "";
}
void InstallerModel::installation_result(const char* result){
    detail=result?result:"No result";
    if(!result){status=InstallerStatus::Failed;return;}
    if(std::strncmp(result,"Conflict",8)==0)status=InstallerStatus::Conflict;
    else if(std::strncmp(result,"Disabled RE4",12)==0)status=InstallerStatus::Disabled;
    else if(std::strstr(result,"Installed RE4 overlay")==result)status=InstallerStatus::OverlayInstalled;
    else if(std::strncmp(result,"Installed RE4 Controls",22)==0)status=InstallerStatus::Installed;
    else status=InstallerStatus::Failed;
}
bool InstallerModel::allow_installation(bool identified,bool running){
    if(!identified){status=InstallerStatus::Failed;detail="Could not identify the active application";return false;}
    if(running){status=InstallerStatus::GameRunning;return false;}
    return true;
}
}
