#include "re4/control_menu.hpp"
#include <algorithm>
#include <cstdio>
namespace re4 {
namespace {
constexpr ControlSpec selector(ControlId id,Bilingual label,Bilingual help){
    return {id,ControlKind::Selector,label,help};
}
constexpr ControlSpec toggle(ControlId id,Bilingual label,Bilingual help,bool Settings::*field){
    return {id,ControlKind::Toggle,label,help,nullptr,field};
}
constexpr ControlSpec slider(ControlId id,Bilingual label,Bilingual help,float Settings::*field,float step,float maximum){
    return {id,ControlKind::Slider,label,help,field,nullptr,step,0,maximum};
}
}
const std::array<ControlSpec,unsigned(ControlId::Count)>& control_specs(){
    static constexpr std::array<ControlSpec,unsigned(ControlId::Count)> items{
        selector(ControlId::Language,{"Language","Язык"},
            {"English or Russian for both interfaces.\nUse LEFT/RIGHT or A to switch.\nThe choice is saved on the SD card.",
             "Русский или английский для обоих меню.\nВлево/вправо или A: сменить язык.\nВыбор сохраняется на карте памяти."}),
        selector(ControlId::Curve,{"Stick response","Кривая стиков"},
            {"Linear: proportional. Exp: precise centre.\nLog: a faster response near the centre.\nHybrid: a smooth exp-to-log transition.",
             "Линейная: равномерный отклик. Эксп.:\nточный центр. Лог.: быстрый отклик.\nСмешанная: плавный переход эксп. -> лог."}),
        toggle(ControlId::Analog,{"Smooth sticks","Плавное управление"},
            {"Turn, walk, look and aim proportionally\nto stick deflection. Native speed is the\nmaximum. Gyro has its own switch.",
             "Плавные повороты, ходьба, камера и прицел\nпо величине отклонения стика. Максимум —\nштатная скорость. Гироскоп отдельно."},&Settings::analog_enabled),
        slider(ControlId::Turn,{"Turn speed","Скорость поворота"},
            {"Character turning with the left stick.\n0: no turning; 1: native maximum.\nThe selected curve shapes the response.",
             "Поворот персонажа левым стиком.\n0: без поворота; 1: штатный максимум.\nОтклик зависит от выбранной кривой."},&Settings::turn_gain,0.1f,1),
        slider(ControlId::Camera,{"Camera sensitivity","Чувствительность камеры"},
            {"Right-stick look outside firearm aiming.\nA lower value gives a gentler response.\n1 keeps the native full look range.",
             "Обзор правым стиком вне прицеливания.\nМеньше значение — мягче отклик камеры.\n1: полный штатный диапазон обзора."},&Settings::camera_gain,0.1f,1),
        slider(ControlId::CameraReturn,{"Camera return speed","Скорость возврата камеры"},
            {"Return to centre after releasing the stick.\n1: native return; 0: hold the look angle.\nFree camera has no automatic return.",
             "Возврат камеры при отпускании стика.\n1: штатный; 0: удерживать угол обзора.\nСвободная камера не возвращается сама."},&Settings::camera_return_speed,0.1f,1),
        toggle(ControlId::FreeCamera,{"Free camera 360","Свободная камера 360"},
            {"Continuous right-stick orbit while walking.\nCamera sensitivity sets the orbit speed.\nNative collision stays active; no auto return.",
             "Непрерывный обзор правым стиком при ходьбе.\nЧувствительность камеры задаёт скорость.\nШтатные столкновения; без автовозврата."},&Settings::free_camera),
        {ControlId::ButtonMapping,ControlKind::Action,{"Button mapping","Назначение кнопок"},
            {"Map physical buttons in menus, QTE and play.\nOverlay controls remain physical for recovery.\nHOME/Capture are system buttons outside RE4.",
             "Кнопки в игре, меню и QTE.\nОверлей использует физические кнопки.\nHOME/Capture — системные кнопки вне RE4."}},
        selector(ControlId::Profile,{"Controls profile","Профиль управления"},
            {"Default or your last Custom controls.\nEditing controls saves Custom automatically.\nGraphics and calibration are preserved.",
             "По умолчанию / пользовательский.\nИзменения сохраняются в пользовательский.\nГрафика и калибровка сохраняются."}),
        slider(ControlId::AimX,{"Left-stick aim X","Прицел левым стиком X"},
            {"Horizontal aiming from the stick.\n0: disabled; 1: native maximum.\nGyro sensitivity is independent.",
             "Прицел влево и вправо от стика.\n0: выключено; 1: штатный максимум.\nЧувствительность гироскопа отдельно."},&Settings::aim_gain_x,0.1f,1),
        slider(ControlId::AimY,{"Left-stick aim Y","Прицел левым стиком Y"},
            {"Vertical aiming from the stick.\n0: disabled; 1: native maximum.\nDoes not limit vertical gyro aiming.",
             "Прицел вверх и вниз от стика.\n0: выключено; 1: штатный максимум.\nНе ограничивает вертикальный гироскоп."},&Settings::aim_gain_y,0.1f,1),
        slider(ControlId::RightAimX,{"Right-stick aim X","Прицел правым стиком X"},
            {"Horizontal right-stick aiming speed.\nIndependent from normal camera sensitivity.\nUses the right deadzone and chosen curve.",
             "Горизонтальный прицел правым стиком.\nОтдельно от чувствительности камеры.\nУчитывает правую мёртвую зону и кривую."},&Settings::right_aim_gain_x,0.05f,1),
        slider(ControlId::RightAimY,{"Right-stick aim Y","Прицел правым стиком Y"},
            {"Vertical right-stick aiming speed.\n0: disabled; 1: native maximum.\nGyro still adds motion independently.",
             "Вертикальный прицел правым стиком.\n0: выключено; 1: штатный максимум.\nГироскоп дополняет движение независимо."},&Settings::right_aim_gain_y,0.05f,1),
        slider(ControlId::LeftDeadzone,{"Left deadzone","Мёртвая зона левого стика"},
            {"An idle area around the stick centre.\nIncrease it if the left stick drifts.\nDefault: 0.05.",
             "Область около центра без реакции.\nУвеличьте при дрейфе левого стика.\nПо умолчанию: 0.05."},&Settings::left_deadzone,0.01f,0.95f),
        slider(ControlId::RightDeadzone,{"Right deadzone","Мёртвая зона правого стика"},
            {"An idle area around the right stick.\nIncrease it if the camera drifts.\nDefault: 0.05. Gyro is unaffected.",
             "Область около центра правого стика.\nУвеличьте, если камера двигается сама.\nПо умолчанию: 0.05. Гироскоп отдельно."},&Settings::right_deadzone,0.01f,0.95f),
        toggle(ControlId::Gyro,{"Gyro aiming","Прицеливание гироскопом"},
            {"Adds controller motion to stick input\nonly while aiming a firearm.\nWalking and normal camera are unaffected.",
             "Наклон дополняет управление стиком\nтолько при наведении из оружия.\nПри ходьбе гироскоп не действует."},&Settings::gyro_enabled),
        slider(ControlId::GyroSpeed,{"Gyro aiming speed","Скорость прицела гироскопом"},
            {"Motion aiming speed for both axes.\nMultiplies the separate X/Y sensitivities.\nTwo readings are averaged10ms apart.",
             "Скорость прицела от движения контроллера.\nУмножает чувствительности X и Y.\nСреднее двух замеров через10мс."},&Settings::gyro_speed,0.1f,10),
        slider(ControlId::GyroDeadzone,{"Gyro deadzone (deg/s)","Мёртвая зона гироскопа (°/с)"},
            {"Ignore small angular motion near zero.\nMeasured in degrees per second.\nDefault: 0.50; stick zones are separate.",
             "Убирает мелкие движения и дрожание.\nВ градусах в секунду; по умолчанию0.50.\nМёртвые зоны стиков задаются отдельно."},&Settings::gyro_deadzone_dps,0.1f,20),
        slider(ControlId::GyroX,{"Gyro sensitivity X","Гироскоп по горизонтали"},
            {"Response to horizontal controller motion.\n0: disabled; higher: more sensitive.\nDoes not change native stick/walk speeds.",
             "Отклик на поворот контроллера.\n0: ось выключена; больше — чувствительнее.\nНе меняет скорость ходьбы и стиков."},&Settings::gyro_gain_x,0.1f,10),
        slider(ControlId::GyroY,{"Gyro sensitivity Y","Гироскоп по вертикали"},
            {"Response to tilting up and down.\nIndependent from horizontal gyro gain.\nKeeps the native vertical aiming range.",
             "Отклик на наклон вверх и вниз.\nНезависим от горизонтальной оси.\nДоступен штатный диапазон прицеливания."},&Settings::gyro_gain_y,0.1f,10),
        toggle(ControlId::InvertX,{"Invert gyro X","Инверсия гироскопа X"},
            {"Reverses horizontal gyro direction.\nDoes not reverse stick input.\nEnabled by default.",
             "Меняет направление горизонтального\nгироскопа, не затрагивая стик.\nПо умолчанию включено."},&Settings::invert_x),
        toggle(ControlId::InvertY,{"Invert gyro Y","Инверсия гироскопа Y"},
            {"Reverses vertical gyro direction.\nDoes not reverse stick input.\nEnabled by default.",
             "Меняет направление вертикального\nгироскопа, не затрагивая стик.\nПо умолчанию включено."},&Settings::invert_y),
        {ControlId::Reset,ControlKind::Action,{"Reset settings","Сбросить настройки"},
            {"Restore defaults: English, linear curve,\n0.05 deadzones, native stick speeds,\ngyro off and both gyro inversions on.",
             "Вернуть настройки: английский, линейная\nкривая, мёртвые зоны 0.05, гироскоп выкл.,\nобе инверсии вкл., штатные скорости."}}
    };
    return items;
}
const ControlSpec& control_spec(ControlId id){
    const auto index=unsigned(id);
    return control_specs()[index<unsigned(ControlId::Count)?index:0];
}
const char* curve_name(StickCurve curve,Language language){
    static constexpr const char* en[]={"Linear","Exponential","Logarithmic","Exp -> log"};
    static constexpr const char* ru[]={"Линейная","Экспонента","Логарифм","Эксп. -> лог."};
    const auto index=unsigned(curve)<4?unsigned(curve):0;
    return language==Language::Russian?ru[index]:en[index];
}
std::string control_value_text(const ControlSpec& item,const Settings& s){
    if(item.value){char text[24];std::snprintf(text,sizeof(text),"%.2f",s.*item.value);return text;}
    if(item.toggle)return s.language==Language::Russian?(s.*item.toggle?"Вкл":"Выкл"):(s.*item.toggle?"On":"Off");
    if(item.id==ControlId::Language)return s.language==Language::Russian?"Русский":"English";
    if(item.id==ControlId::Curve)return curve_name(s.stick_curve,s.language);
    if(item.id==ControlId::Profile)return profile_name(profile_kind(s,ProfileDomain::Controls),s.language);
    return "A";
}
bool change_control(Settings& s,ControlId id,int direction){
    if(unsigned(id)>=unsigned(ControlId::Count)||!direction)return false;
    const auto& item=control_spec(id);
    if(item.value){
        const float previous=s.*item.value;
        s.*item.value=std::clamp(previous+(direction>0?item.step:-item.step),item.minimum,item.maximum);
        return previous!=s.*item.value;
    }
    if(item.toggle){s.*item.toggle=!(s.*item.toggle);return true;}
    if(id==ControlId::Language){s.language=s.language==Language::English?Language::Russian:Language::English;return true;}
    if(id==ControlId::Curve){s.stick_curve=static_cast<StickCurve>((unsigned(s.stick_curve)+(direction>0?1:3))%4);return true;}
    if(id==ControlId::Reset){s={};return true;}
    return false;
}
const std::array<GraphicsSpec,unsigned(GraphicsId::Count)>& graphics_specs(){
    static constexpr std::array<GraphicsSpec,unsigned(GraphicsId::Count)> items{{
        {GraphicsId::TextureFiltering,ControlKind::Selector,{"Texture filtering","Фильтрация текстур"},
            {"Normal mipmapped textures only.\nBilinear / trilinear / AF 2x-16x.\nSpecial filters stay native.\nRequires a game restart.",
             "Обычные текстуры с mip-уровнями.\nБилинейная / трилинейная / AF 2x-16x.\nОсобые фильтры игры сохраняются.\nТребуется перезапуск игры."}},
        {GraphicsId::AntiAliasing,ControlKind::Selector,{"Anti-aliasing","Сглаживание"},
            {"Handheld SSAA: 900p render to 720p.\n1.25x, native buffers/downsample.\nHandheld only; dock stays native.\nHigher GPU cost. Restart required.",
             "SSAA: рендер 900p, вывод 720p.\n1.25x, штатные буферы и уменьшение.\nТолько портативно; док без изменений.\nВыше нагрузка GPU. Перезапуск игры."}},
        {GraphicsId::DrawDistance,ControlKind::Slider,{"Object draw distance","Дальность объектов"},
            {"Loaded-room culling: 0.50x-2.00x.\nLimited by the room's native fog.\nNo extra rooms, streaming or LOD.\nFog stays unchanged. Native: 1.00x.",
             "Отсечение в комнате: 0.50x-2.00x.\nПредел задаёт штатный туман комнаты.\nБез новых комнат, загрузки или LOD.\nТуман не меняется. Штатное: 1.00x."}},
        {GraphicsId::Shadows,ControlKind::Selector,{"Shadows","Тени"},
            {"Rendered shadows: Native or Off.\nCast, self and foot shadows.\nBaked texture lighting stays unchanged.",
             "Отрисовываемые тени: штатные/выкл.\nТени сцены, самотени и тени под ногами.\nОсвещение в текстурах сохраняется."}},
        {GraphicsId::Profile,ControlKind::Selector,{"Graphics profile","Профиль графики"},
            {"Default or your last Custom graphics.\nEditing graphics saves Custom automatically.\nControls and calibration are preserved.\nFiltering and AA: restart the game.",
             "По умолчанию / пользовательский.\nИзменения сохраняются в пользовательский.\nУправление и калибровка сохраняются.\nФильтрация и AA: перезапуск игры."}},
        {GraphicsId::Reset,ControlKind::Action,{"Reset graphics","Сбросить графику"},
            {"A: restore native graphics only.\nFiltering, AA, shadows: Native.\nObject distance: 1.00x.\nControls and mappings are preserved.",
             "A: вернуть только штатную графику.\nФильтрация, сглаживание, тени: штатные.\nДальность объектов: 1.00x.\nУправление и назначения сохраняются."}}
    }};
    return items;
}
const GraphicsSpec& graphics_spec(GraphicsId id){
    return graphics_specs()[unsigned(id)<unsigned(GraphicsId::Count)?unsigned(id):0];
}
std::string graphics_value_text(const GraphicsSpec& item,const Settings& s){
    const auto native=Bilingual{"Native","Штатное"}.get(s.language);
    if(item.id==GraphicsId::TextureFiltering){
        static constexpr Bilingual names[]={{"Native","Штатное"},{"Bilinear","Билинейная"},{"Trilinear","Трилинейная"},
            {"AF 2x","AF 2x"},{"AF 4x","AF 4x"},{"AF 8x","AF 8x"},{"AF 16x","AF 16x"}};
        const unsigned index=unsigned(s.texture_filtering);return names[index<7?index:0].get(s.language);
    }
    if(item.id==GraphicsId::AntiAliasing)return s.anti_aliasing==AntiAliasing::HandheldSsaa?"Handheld SSAA":native;
    if(item.id==GraphicsId::Shadows)return s.shadow_mode==ShadowMode::Off?Bilingual{"Off","Выкл"}.get(s.language):native;
    if(item.id==GraphicsId::DrawDistance){char text[24];std::snprintf(text,sizeof(text),"%.2fx",s.draw_distance_scale);return text;}
    if(item.id==GraphicsId::Profile)return profile_name(profile_kind(s,ProfileDomain::Graphics),s.language);
    return "A";
}
bool change_graphics(Settings& s,GraphicsId id,int direction){
    if(!direction)return false;
    if(id==GraphicsId::TextureFiltering){s.texture_filtering=static_cast<TextureFilter>((unsigned(s.texture_filtering)+(direction>0?1:6))%7);return true;}
    if(id==GraphicsId::AntiAliasing){s.anti_aliasing=s.anti_aliasing==AntiAliasing::Native?AntiAliasing::HandheldSsaa:AntiAliasing::Native;return true;}
    if(id==GraphicsId::Shadows){s.shadow_mode=s.shadow_mode==ShadowMode::Native?ShadowMode::Off:ShadowMode::Native;return true;}
    if(id==GraphicsId::DrawDistance){const auto previous=s.draw_distance_scale;s.draw_distance_scale=std::clamp(previous+(direction>0?0.25f:-0.25f),0.5f,2.f);return previous!=s.draw_distance_scale;}
    return false;
}
bool reset_graphics(Settings& s){
    const bool changed=s.texture_filtering!=TextureFilter::Native || s.anti_aliasing!=AntiAliasing::Native
        || s.shadow_mode!=ShadowMode::Native || s.draw_distance_scale!=1.f;
    s.texture_filtering=TextureFilter::Native;s.anti_aliasing=AntiAliasing::Native;s.shadow_mode=ShadowMode::Native;s.draw_distance_scale=1.f;
    return changed;
}
const char* profile_name(ProfileKind kind,Language language){
    return (kind==ProfileKind::Default?Bilingual{"Default","По умолчанию"}:
        Bilingual{"Custom","Пользовательский"}).get(language);
}
const char* profile_result_text(ProfileResult result,Language language){
    if(result==ProfileResult::MissingCustom)return Bilingual{
        "Custom profile is empty. Edit a setting first.",
        "Пользовательский профиль пуст. Измените настройку."}.get(language);
    if(result==ProfileResult::SaveFailed)return Bilingual{
        "Could not save profile to SD.","Не удалось сохранить профиль на SD."}.get(language);
    return Bilingual{"Profile applied","Профиль применён"}.get(language);
}
const char* mapping_target_label(unsigned target,Language language){
    if(target>=kButtonCount)return Bilingual{"Unbound","Не назначено"}.get(language);
    return button_name(target);
}
unsigned mapping_target_index(const Settings& s,unsigned source){
    return source<kButtonCount && s.button_map[source]<kButtonCount?s.button_map[source]:kButtonCount;
}
std::string mapping_row_label(unsigned row,Language language){
    if(row==0)return Bilingual{"Enable mapping","Включить назначения"}.get(language);
    if(row<=kButtonCount)return button_name(row-1);
    return Bilingual{"Reset mappings","Сбросить назначения"}.get(language);
}
std::string mapping_row_value(unsigned row,const Settings& s){
    if(row==0)return Bilingual{s.remap_enabled?"On":"Off",s.remap_enabled?"Вкл":"Выкл"}.get(s.language);
    if(row<=kButtonCount)return mapping_target_label(mapping_target_index(s,row-1),s.language);
    return "A";
}
const char* mapping_target_help(Language language){
    return Bilingual{"A: use this game button; B: cancel.\nDuplicates, swaps and cycles are allowed.\nOverlay controls always stay physical.",
        "A: назначить кнопку; B: отменить.\nДопустимы повторы, обмен и циклы.\nОверлей всегда использует физические кнопки."}.get(language);
}
const char* mapping_row_help(unsigned row,Language language){
    if(row==0)return Bilingual{"Enable mappings in play, menus and QTE.\nOverlay buttons stay physical for recovery.\nHOME/Capture belong to the system, not RE4.",
        "Назначения для игры, меню и QTE.\nКнопки оверлея остаются физическими.\nHOME/Capture — системные кнопки вне RE4."}.get(language);
    if(row>kButtonCount)return Bilingual{"A: enable mapping and restore every button.\nCamera, gyro and other settings are kept.\nArrows do not reset mappings.",
        "A: включить и вернуть исходные кнопки.\nКамера, гироскоп и прочие настройки сохранятся.\nСтрелки не сбрасывают назначения."}.get(language);
    return Bilingual{"Physical source -> game button. A: choose.\nLEFT/RIGHT: cycle targets, including Unbound.\nOverlay stays physical; HOME/Capture excluded.",
        "Физическая кнопка -> кнопка игры. A: выбрать.\nВлево/вправо: сменить, «Не назначено».\nОверлей физический; HOME/Capture вне игры."}.get(language);
}
bool set_mapping_target(Settings& s,unsigned source,unsigned target){
    if(source>=kButtonCount || target>kButtonCount)return false;
    const auto value=target==kButtonCount?kUnboundButton:std::uint8_t(target);
    if(s.button_map[source]==value)return false;
    s.button_map[source]=value;return true;
}
bool change_mapping(Settings& s,unsigned row,int direction){
    if(!direction || row>=kMappingRowCount)return false;
    if(row==0){s.remap_enabled=!s.remap_enabled;return true;}
    if(row>kButtonCount){
        const bool changed=!s.remap_enabled || s.button_map!=identity_button_map();
        s.remap_enabled=true;s.button_map=identity_button_map();return changed;
    }
    const unsigned next=(mapping_target_index(s,row-1)+(direction>0?1:kButtonCount))%(kButtonCount+1);
    return set_mapping_target(s,row-1,next);
}
}
