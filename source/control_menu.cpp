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
}
