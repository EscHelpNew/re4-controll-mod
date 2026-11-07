#pragma once
#include "control_menu.hpp"

namespace re4 {
inline constexpr Bilingual kGameMenuLabel{"Game","Игра"};
inline constexpr Bilingual kGameMenuHelp{
    "Game options apply independently of profiles.\nA: open. B: return.",
    "Настройки игры не зависят от профилей.\nA: открыть. B: назад."};
inline constexpr Bilingual kEnemyHealthBarsLabel{"Enemy health bars","Полоска здоровья врагов"};
inline constexpr Bilingual kEnemyHealthBarsHelp{
    "Show the health bar of the enemy you are aiming at.\nOnly the current target is shown.",
    "Полоска здоровья врага под прицелом.\nПоказывается только текущая цель."};
inline constexpr Bilingual kEnemyHealthNumbersLabel{"Health numbers","Числа здоровья"};
inline constexpr Bilingual kEnemyHealthNumbersHelp{
    "Show current HP above the bar in\nweapon firepower units, with one decimal.",
    "Текущее здоровье над полоской в\nединицах мощности оружия.\nОдин знак после точки."};
inline constexpr Bilingual kEnemyHealthNumbersUnavailableHelp{
    "Enable enemy health bars first.",
    "Сначала включите полоски здоровья врагов."};
inline const char* enemy_health_bars_value(const Settings& settings){
    return (settings.enemy_health_bars?Bilingual{"On","Вкл."}:Bilingual{"Off","Выкл."}).get(settings.language);
}
inline const char* enemy_health_numbers_value(const Settings& settings){
    if(!settings.enemy_health_bars)return Bilingual{"Unavailable","Недоступно"}.get(settings.language);
    return (settings.enemy_health_numbers?Bilingual{"On","Вкл."}:Bilingual{"Off","Выкл."}).get(settings.language);
}
inline const char* enemy_health_numbers_help(const Settings& settings){
    return (settings.enemy_health_bars?kEnemyHealthNumbersHelp:kEnemyHealthNumbersUnavailableHelp).get(settings.language);
}
}
