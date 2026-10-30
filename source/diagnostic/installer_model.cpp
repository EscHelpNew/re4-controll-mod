#include "re4/installer_ui.hpp"
#include <cstring>
namespace re4 {
const std::array<HomeItem,8>& installer_home_items(){
    static constexpr std::array<HomeItem,8> items{{
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
        {{"Updates","Обновления"},
         {"Download a new app from GitHub Releases.\nWi-Fi is required. The previous app is kept.",
          "Скачать новую версию из GitHub Releases.\nНужен Wi-Fi. Предыдущая версия сохраняется."}},
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
    if(page==InstallerPage::Mapping)return kMappingRowCount;
    if(page==InstallerPage::MappingTargets)return kButtonCount+1;
    return page==InstallerPage::Home?8:page==InstallerPage::Controls?unsigned(ControlId::Count):page==InstallerPage::Updates?2:0;
}
InstallerAction InstallerModel::handle(UiEvent event){
    if(event==UiEvent::Exit)return InstallerAction::Exit;
    if(event==UiEvent::Back){
        if(page==InstallerPage::MappingTargets){page=InstallerPage::Mapping;selected=mapping_source+1;return InstallerAction::None;}
        if(page==InstallerPage::Mapping){page=InstallerPage::Controls;selected=controls_selected;return InstallerAction::None;}
        if(page==InstallerPage::Home)return InstallerAction::Exit;
        const bool cancel=page==InstallerPage::Updates && (update.stage==UpdateStage::Checking
            || update.stage==UpdateStage::Downloading || update.stage==UpdateStage::Installing);
        page=InstallerPage::Home;selected=home_selected;
        return cancel?InstallerAction::CancelUpdate:InstallerAction::None;
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
    if(page==InstallerPage::Updates){
        if(event!=UiEvent::Confirm || update.stage==UpdateStage::Checking || update.stage==UpdateStage::Downloading
            || update.stage==UpdateStage::Installing || update.stage==UpdateStage::Installed)return InstallerAction::None;
        if(selected==0)return InstallerAction::CheckUpdates;
        return update.stage==UpdateStage::Available?InstallerAction::InstallUpdate:InstallerAction::None;
    }
    if(page==InstallerPage::MappingTargets){
        if(event!=UiEvent::Confirm)return InstallerAction::None;
        const bool changed=set_mapping_target(settings,mapping_source,selected);
        page=InstallerPage::Mapping;selected=mapping_source+1;
        return changed?InstallerAction::SaveSettings:InstallerAction::None;
    }
    if(page==InstallerPage::Mapping){
        if(selected>=count)selected=0;
        if(event==UiEvent::Confirm && selected>0 && selected<=kButtonCount){
            mapping_source=selected-1;page=InstallerPage::MappingTargets;
            selected=mapping_target_index(settings,mapping_source);return InstallerAction::None;
        }
        if(event!=UiEvent::Confirm && event!=UiEvent::Left && event!=UiEvent::Right)return InstallerAction::None;
        if(selected==kMappingRowCount-1 && event!=UiEvent::Confirm)return InstallerAction::None;
        return change_mapping(settings,selected,event==UiEvent::Left?-1:1)?InstallerAction::SaveSettings:InstallerAction::None;
    }
    if(page==InstallerPage::Controls){
        if(selected>=count)selected=0;
        const auto& spec=control_specs()[selected];
        if(spec.id==ControlId::ButtonMapping){
            if(event==UiEvent::Confirm){controls_selected=selected;page=InstallerPage::Mapping;selected=0;}
            return InstallerAction::None;
        }
        if(spec.kind==ControlKind::Action && event!=UiEvent::Confirm)return InstallerAction::None;
        const bool arrows=event==UiEvent::Left||event==UiEvent::Right;
        if(!arrows && !(event==UiEvent::Confirm && spec.kind!=ControlKind::Slider))return InstallerAction::None;
        if(change_control(settings,spec.id,event==UiEvent::Left?-1:1))return InstallerAction::SaveSettings;
        return InstallerAction::None;
    }
    if(selected==6 && (event==UiEvent::Confirm||event==UiEvent::Left||event==UiEvent::Right)){
        change_control(settings,ControlId::Language,1);return InstallerAction::SaveSettings;
    }
    if(event!=UiEvent::Confirm)return InstallerAction::None;
    if(selected==0)return InstallerAction::InstallGame;
    if(selected==1)return InstallerAction::InstallOverlay;
    if(selected==2)return InstallerAction::DisableGame;
    if(selected==7)return InstallerAction::Exit;
    home_selected=selected;
    page=selected==3?InstallerPage::Controls:selected==4?InstallerPage::Diagnostics:InstallerPage::Updates;
    selected=0;return InstallerAction::None;
}
const char* InstallerModel::page_title()const{
    if(page==InstallerPage::Mapping)return Bilingual{"BUTTON MAPPING","НАЗНАЧЕНИЕ КНОПОК"}.get(settings.language);
    if(page==InstallerPage::MappingTargets)return Bilingual{"CHOOSE GAME BUTTON","ВЫБОР КНОПКИ ИГРЫ"}.get(settings.language);
    const Bilingual text=page==InstallerPage::Home?Bilingual{"MOD INSTALLER","УСТАНОВКА МОДА"}
        :page==InstallerPage::Controls?Bilingual{"CONTROL SETTINGS","НАСТРОЙКИ УПРАВЛЕНИЯ"}
        :page==InstallerPage::Diagnostics?Bilingual{"CONTROLLER DIAGNOSTICS","ПРОВЕРКА КОНТРОЛЛЕРА"}
        :Bilingual{"APP UPDATES","ОБНОВЛЕНИЯ ПРИЛОЖЕНИЯ"};
    return text.get(settings.language);
}
const char* InstallerModel::status_text()const{
    if(page==InstallerPage::Updates){
        static constexpr Bilingual updates[]={
            {"Check for a newer version","Проверьте наличие новой версии"},
            {"Checking GitHub Releases...","Проверка GitHub Releases..."},
            {"A new version is available","Доступна новая версия"},
            {"You have the latest version","Установлена актуальная версия"},
            {"Downloading update...","Загрузка обновления..."},
            {"Verifying and installing...","Проверка и установка..."},
            {"Update installed. Reopen RE4 Controls.","Обновление установлено. Откройте приложение заново."},
            {"Update unavailable. Check Wi-Fi or try later.","Обновление недоступно. Проверьте Wi-Fi или повторите позже."},
            {"Download cancelled; current app kept","Загрузка отменена; текущая версия сохранена"}
        };
        const auto index=unsigned(update.stage);
        return updates[index<9?index:7].get(settings.language);
    }
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
    if(page==InstallerPage::Mapping)return mapping_row_help(selected,settings.language);
    if(page==InstallerPage::MappingTargets)return mapping_target_help(settings.language);
    if(page==InstallerPage::Home)return installer_home_items()[selected<8?selected:0].help.get(settings.language);
    if(page==InstallerPage::Updates){
        if(update.stage==UpdateStage::Installed)return Bilingual{
            "Reopen this app to start the new version.\nThen use Install / update mod while\nResident Evil4 is closed.",
            "Откройте приложение заново. Затем\nвыберите «Установить / обновить мод»,\nпока Resident Evil4 закрыта."}.get(settings.language);
        return Bilingual{"Check the latest public GitHub release,\nthen choose Download / install.\nSettings are kept. No token is needed.",
            "Проверьте последний публичный релиз,\nзатем выберите «Скачать / установить».\nНастройки сохраняются. Токен не нужен."}.get(settings.language);
    }
    if(page==InstallerPage::Controls)return control_spec(static_cast<ControlId>(selected)).help.get(settings.language);
    return Bilingual{"ZL: gyro preview  A: gyro on/off\nY: calibrate  X: reset preview",
                     "ZL: проверка  A: гироскоп вкл./выкл.\nY: калибровать  X: сбросить прицел"}.get(settings.language);
}
std::string InstallerModel::item_label(unsigned index)const{
    if(page==InstallerPage::Mapping)return mapping_row_label(index,settings.language);
    if(page==InstallerPage::MappingTargets)return mapping_target_label(index,settings.language);
    if(page==InstallerPage::Home)return installer_home_items()[index<8?index:0].label.get(settings.language);
    if(page==InstallerPage::Updates)return (index==0?Bilingual{"Check for updates","Проверить обновления"}
        :Bilingual{"Download / install","Скачать / установить"}).get(settings.language);
    return control_spec(static_cast<ControlId>(index)).label.get(settings.language);
}
std::string InstallerModel::item_value(unsigned index)const{
    if(page==InstallerPage::Mapping)return mapping_row_value(index,settings);
    if(page==InstallerPage::MappingTargets)return index==mapping_target_index(settings,mapping_source)?"*":"";
    if(page==InstallerPage::Controls)return control_value_text(control_spec(static_cast<ControlId>(index)),settings);
    if(page==InstallerPage::Updates)return index==1?update.version:"";
    if(index==6)return settings.language==Language::Russian?"Русский":"English";
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
