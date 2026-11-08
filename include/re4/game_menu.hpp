#pragma once
#include "control_menu.hpp"

namespace re4 {
inline constexpr Bilingual kCharacterAppearanceLabel{"Character appearance","Внешность персонажа"};
inline constexpr Bilingual kCharacterAppearanceHelp{
    "Visual appearance; Leon weapons and controls.\nApplies during gameplay.",
    "Только внешность; оружие и управление Леона.\nПрименяется во время игры."};
inline constexpr Bilingual kAppearanceRequested{"Requested","Выбрано"};
inline constexpr Bilingual kAppearanceApplied{"Applied","Применено"};
inline const char* character_appearance_name(CharacterAppearance appearance,Language language){
    constexpr Bilingual names[]={{"Leon","Леон"},{"Ashley","Эшли"},{"Ada","Ада"},{"Luis Sera","Луис Сера"},
        {"Krauser","Краузер"},{"Merchant","Торговец"},{"Ramon Salazar","Рамон Салазар"},{"Village Ganado","Деревенский ганадо"}};
    return valid_character_appearance(appearance)?names[unsigned(appearance)].get(language):"";
}
inline const char* character_appearance_status(CharacterAppearanceStatus status,Language language){
    constexpr Bilingual labels[]={{"Native","Штатная"},{"Pending","Ожидание"},{"Active","Активна"},{"Failed","Ошибка"}};
    return unsigned(status)<4?labels[unsigned(status)].get(language):"";
}
inline const char* character_appearance_reason(std::uint32_t reason,Language language){
    reason&=0xFFFFu; // Pending may include a bounded observed model byte.
    constexpr Bilingual waiting[]={
        {"Game graphics are not ready","Графика игры ещё не готова"},
        {"Waiting for Leon","Ожидание Леона"},
        {"Waiting for gameplay","Ожидание игрового процесса"},
        {"Waiting for the game world","Ожидание игрового мира"},
        {"Waiting for a compatible Leon model","Ожидание совместимой модели Леона"},
        {"Waiting for a compatible Leon rig","Ожидание совместимой анимации Леона"},
        {"Waiting for Leon's model","Ожидание загрузки модели Леона"},
        {"An alternate Leon model is active","Активна дополнительная модель Леона"},
        {"Current model materials are unsupported","Материалы текущей модели не поддерживаются"},
        {"Waiting for the model effect to finish","Ожидание завершения эффекта модели"},
        {"Waiting for playable Leon","Ожидание управляемого Леона"},
        {"Waiting for game files","Ожидание файлов игры"},
        {"Waiting for the previous appearance to finish","Ожидание освобождения прошлого облика"},
        {"Reading appearance files","Чтение файлов внешности"},
        {"Waiting for the appearance loader","Ожидание загрузки внешности"},
        {"The game file loader is busy","Загрузчик файлов игры занят"},
        {"Loading appearance textures","Загрузка текстур внешности"},
        {"Waiting for texture loading to finish","Ожидание завершения загрузки текстур"},
        {"Preparing appearance textures","Подготовка текстур внешности"},
        {"Reserving appearance textures","Подготовка памяти для текстур"},
        {"Waiting for the model animation","Ожидание анимации модели"},
        {"Waiting for the first appearance frame","Ожидание первого кадра внешности"},
        {"Waiting for weapon hands","Ожидание модели рук на оружии"},
        {"Waiting for a free frame","Ожидание свободного кадра"},
        {"Waiting for the renderer","Ожидание отрисовки"},
        {"Waiting for the native animation","Ожидание штатной анимации"},
        {"Waiting for the previous frame","Ожидание обработки прошлого кадра"},
        {"Waiting for compatible model bindings","Ожидание совместимых связей модели"}
    };
    if(reason>=100&&reason<=127)return waiting[reason-100].get(language);
    constexpr Bilingual errors[]={
        {"",""},{"Game files are unavailable","Файлы игры недоступны"},
        {"Could not open the appearance file","Не удалось открыть файл внешности"},
        {"Unexpected appearance file size","Неподдерживаемый размер файла внешности"},
        {"Could not read the appearance file","Не удалось прочитать файл внешности"},
        {"Appearance file is incompatible","Файл внешности несовместим"},
        {"The current model is unsupported","Текущая модель не поддерживается"},
        {"Could not initialize the appearance","Не удалось подготовить внешность"},
        {"Appearance rendering is unavailable","Отрисовка внешности недоступна"},
        {"Could not animate the appearance","Не удалось применить анимацию внешности"},
        {"Appearance textures are unavailable","Текстуры внешности недоступны"},
        {"Game graphics changed during loading","Графика игры изменилась при загрузке"}
    };
    if(reason<sizeof(errors)/sizeof(errors[0]))return errors[reason].get(language);
    return Bilingual{"Details unavailable","Подробности недоступны"}.get(language);
}
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
inline constexpr Bilingual kGrenadeLandingSpotLabel{"Grenade explosion spot","Пятно взрыва гранаты"};
inline constexpr Bilingual kGrenadeLandingSpotHelp{
    "Approximate explosion position after bounces.\nA soft red fog spot appears while aiming a grenade.",
    "Примерное место взрыва с учётом отскоков.\nПри прицеливании гранатой видно мягкое красное пятно."};
inline constexpr Bilingual kSkipPickupConfirmationLabel{"Skip pickup confirmation","Без подтверждения подбора"};
inline constexpr Bilingual kSkipPickupConfirmationHelp{
    "Pick up items and treasure without confirmation.\nThe full inventory flow is retained.",
    "Подбор предметов и сокровищ без подтверждения.\nПри полном инвентаре сохраняется обычный порядок."};
inline constexpr Bilingual kSkipStartupLogosLabel{"Skip startup logos","Пропуск стартовых заставок"};
inline constexpr Bilingual kSkipStartupLogosHelp{
    "Press a button to skip the Capcom/CRIWARE logos.\nNo automatic skip; other cutscenes are unchanged.",
    "Кнопка пропускает заставки Capcom/CRIWARE.\nБез автопропуска; остальные сцены без изменений."};
inline constexpr Bilingual kRemoveAimingSwayLabel{"Remove aiming sway","Убрать покачивание прицела"};
inline constexpr Bilingual kRemoveAimingSwayHelp{
    "Reduce native aiming bobbing.\nManual stick and gyro aiming are retained.",
    "Уменьшает штатное покачивание прицела.\nРучное управление стиком и гироскопом сохраняется."};
inline constexpr Bilingual kDpadWeaponSwitchingLabel{"D-pad weapon switching","D-pad смена оружия"};
inline constexpr Bilingual kDpadWeaponSwitchingHelp{
    "Right/Left: next/previous attache weapon.\nUp/Down: next/previous available grenade.\nGameplay only; menus retain normal controls.",
    "Вправо/влево: следующее/предыдущее оружие.\nВверх/вниз: следующая/предыдущая граната.\nТолько в игре; в меню обычное управление."};
inline constexpr Bilingual kExtendedWeaponUpgradeLabel{"Extended weapon upgrade","Расширенная прокачка оружия"};
inline constexpr Bilingual kExtendedWeaponUpgradeHelp{
    "Adds one more Firepower level at the merchant.\nCosts the same as the last Firepower upgrade.\nStored by the mod; game saves stay unchanged.",
    "Добавляет ещё один уровень атаки у торговца.\nСтоит как последнее улучшение атаки.\nХранится модом; сохранения игры не меняются."};
inline constexpr Bilingual kCaseAmmoCounterLabel{"Case ammo counter","Патроны в чемодане"};
inline constexpr Bilingual kCaseAmmoCounterHelp{
    "Shows spare rounds for the equipped gun\nabove the name plate of the ammo gauge.\nRed when the case has none left.",
    "Запас патронов к текущему оружию\nнад табличкой с именем у счётчика.\nКрасный, когда в чемодане пусто."};
inline const char* game_option_value(bool enabled,Language language){
    return (enabled?Bilingual{"On","Вкл."}:Bilingual{"Off","Выкл."}).get(language);
}
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
