#pragma once
#include "re4/control_menu.hpp"

namespace re5 {
// Game-specific descriptions shared by the standalone app and RE5 overlay.
inline const char* control_help(re4::ControlId id,const re4::Settings& settings){
    using re4::ControlId;using re4::Bilingual;
    if(id==ControlId::Turn)return Bilingual{
        "Classic controls only: left-stick turn speed.\nUnified movement and right-stick look/aim\nare unaffected. 0: no classic turning.",
        "Только классическое управление: поворот.\nЕдиные стики: движение, обзор и прицел\nне меняются. 0: без классических поворотов."}.get(settings.language);
    if(id==ControlId::ThirdPersonDistance)return Bilingual{
        "1.00: original shoulder view. Larger values\nshow more space around the character.\nNative wall collision stays active.\nNot used in first person or scopes.",
        "1.00: штатный вид из-за плеча. Увеличьте,\nчтобы персонаж меньше закрывал обзор.\nШтатные столкновения со стенами сохранены.\nНе влияет на вид от глаз и оптику."}.get(settings.language);
    if(id==ControlId::Profile)return Bilingual{
        "Default, Modern or your last Custom controls.\nA / LEFT / RIGHT: change profile.\nGraphics and calibration are preserved.",
        "Исходное, Современное или своё управление.\nA / влево / вправо: сменить профиль.\nГрафика и калибровка сохраняются."}.get(settings.language);
    if(id==ControlId::ButtonMapping)return Bilingual{
        "Map physical buttons in menus, QTE and play.\nOverlay controls stay physical for recovery.\nHOME/Capture belong to the system.",
        "Кнопки в игре, меню и QTE.\nОверлей использует физические кнопки.\nHOME/Capture — системные кнопки."}.get(settings.language);
    return re4::control_help_text(re4::control_spec(id),settings);
}
}
