# RE Artwork and RE4 Graphics Presets Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Подготовить локальную 0.17.0 с новым значком RE и пресетами графики RE4 Default / Custom / Quality.

**Architecture:** Расширить существующую модель независимых профилей и общий каталог графического меню. Использовать текущие графические поля и эффекты без изменения настроек/ABI. Значок, документацию и реализацию вести параллельно в непересекающихся файлах; координатор объединяет и проверяет результат.

**Tech Stack:** C++20, Python unittest/Pillow, SDL2, Tesla, devkitA64/libnx, PowerShell.

**Spec:** [RE artwork and RE4 graphics presets](../specs/2026-11-04-re-graphics-presets-design.md).

## Global Constraints

- Локальная версия **0.17.0**, unreleased; без передачи на консоль, публикации, коммита и push.
- Default: `TextureFilter::Native`, `AntiAliasing::Native`, `ShadowMode::Native`, `1.0f`.
- Quality: `TextureFilter::Aniso16`, `AntiAliasing::HandheldSsaa`, `ShadowMode::Native`, `1.75f`.
- `assets/icon-re.png` — новый исходник; `assets/icon-original.png` сохраняется неизменённым.
- Custom — последняя ручная конфигурация, включая канонические Default/Quality; выбор встроенного пресета не заменяет её.
- SSAA: 900p → 720p только handheld; док остаётся штатным.
- Полное применение: HOME → X → Close / Закрыть, затем новый запуск игры; меню/оверлей недостаточны.
- Результаты hardware 0.16.1 не доказывают работу 0.17.0; RE6 и графические hooks не менять.

## Review Focus

- Существующий Custom до обновления восстанавливается после обоих встроенных пресетов.
- Ручной набор, совпадающий с Default/Quality, обновляет Custom; выбор встроенного пресета сохраняет последний Custom.
- Пустой/повреждённый sidecar и ошибка сохранения не теряют активные настройки.
- A/←/→ и EN/RU одинаково доступны в настоящих установщике и оверлее.
- Все потребители значка берут новый исходник, сохраняя оригинал и корректный Switch JPEG.

---

### Task 1: Пресеты, сохранение и оба меню (исполнитель реализации)

**Files:**
- Modify: `include/re4/profiles.hpp`, `source/profiles.cpp` — фабрика Quality, классификация, восстановление Custom и циклический выбор.
- Modify: `source/control_menu.cpp`, `include/re4/installer_ui.hpp`, `source/diagnostic/installer_model.cpp`, `source/diagnostic/main.cpp`, `source/overlay/main.cpp` — три пресета и EN/RU-подсказки полного перезапуска.
- Test: `tests/profiles_tests.cpp`, `tests/control_menu_tests.cpp`, `tests/installer_model_tests.cpp`, `tests/profile_ui_tests.cpp`, `tests/overlay_ui_tests.cpp`.

**Interfaces:**
- Consumes: `Settings`, `ProfileDomain::Graphics`, существующие `save_profiled_settings`, `switch_profile`, `profile_kind`, `load_custom_profile`.
- Produces: `ProfileKind::Quality = 4` только для Graphics; `Settings quality_graphics_profile()`; `ProfileKind next_graphics_profile(ProfileKind current, int direction)`; `bool save_edited_graphics(const char* path, const Settings& settings)` для ручных правок; обе UI используют общий цикл Default → Quality → Custom.

- [x] Добавить падающие host-регрессии: фабрика Quality имеет точные четыре значения из Global Constraints; Default штатный; Quality распознаётся только в Graphics. Обход вперёд/назад даёт все три пресета.
- [x] Добавить сценарии Custom → Default → Quality → Custom с сохранением/повторной загрузкой; сохранить старый sidecar. Ручные наборы, совпадающие с Default и Quality, должны обновлять последний Custom и восстанавливаться из него; UI показывает канонический встроенный пресет. Проверить пустой/повреждённый Custom и отказ записи. Сравнить управление, язык и калибровку до/после.
- [x] Добавить настоящие UI-события A/←/→ для трёх пресетов EN/RU и проверки неизменности RE6; сначала воспроизвести отсутствие Quality на текущем коде.
- [x] Реализовать фабрику, классификацию и общий цикл. Выбор встроенных пресетов сохраняет прежний Custom; ручные правки в обеих UI вызывают `save_edited_graphics`. `load_custom_profile` принимает любой валидный снимок Graphics, включая Default/Quality, и по-прежнему только Custom для Controls. Подключить одинаковые действия и пояснение HOME → X → Close / Закрыть с новым запуском.
- [x] Запустить затронутые native-тесты по командам их компиляции из `build.py`; ожидание — exit 0. Проверить повторную загрузку старого файла без изменения wire/ABI.

### Task 2: Новый значок и локальная версия (исполнитель графики/упаковки, параллельно Task 1)

**Files:**
- Create: `assets/icon-re.png`.
- Modify: `build.py`, `tools/prepare_icon.py` только при необходимости, `tests/test_icon.py`, `include/re4/version.hpp`.
- Preserve: `assets/icon-original.png`.

**Interfaces:**
- Consumes: оригинальный красный прицел и штатные `prepare(source, destination)` / `prepare_bitmap(source, destination)`.
- Produces: новый исходник для всех путей app/HOME/installer, версия `0.17.0`; настройку пресетов не трогает.

- [x] Сохранить SHA-256 оригинала; подготовить RE вместо 4 с прежним красным прицелом и визуально проверить.
- [x] Обновить регрессии значка: оригинал неизменён, новый исходник существует, выход JPEG RGB 256×256 и ≤128 KiB, запрет перезаписи исходника сохраняется.
- [x] Все вызовы подготовки app icon, forwarder icon и installer logo в `build.py` перевести на новый исходник; обновить локальную версию в `include/re4/version.hpp`.
- [x] Запустить `.venv\Scripts\python.exe -m unittest discover -s tests -p test_icon.py -v`; ожидание — PASS/exit 0. Просмотреть новый PNG и полученный JPEG, проверить все потребители поиском `icon-original`/`icon-re`.

### Task 3: Пользовательская документация (исполнитель документации, параллельно Tasks 1–2)

**Files:**
- Modify: `README.md`, `docs/README.ru.md`, `docs/testing.md`, `CHANGELOG.md`.
- Create: текущие design и implementation plan.

**Interfaces:**
- Consumes: значения и ограничения Global Constraints; результаты реальных проверок координатора.
- Produces: одинаковые описания EN/RU, новая unreleased-запись, ссылка README на `assets/icon-re.png` и аппаратный чек-лист.

- [x] Описать три пресета и сохранение Custom, handheld/dock, полный перезапуск HOME → X → Close; отделить hardware 0.16.1 от непроверенной 0.17.0.
- [x] Добавить требования регрессий и открытые аппаратные проверки в `docs/testing.md`, не объявляя требования уже пройденными тестами.
- [x] Проверить команды по `build.py`, относительные ссылки по дереву итоговой сборки и `git diff --check`; полную сборку исполнитель документации не запускает.

### Task 4: Интеграция и независимая проверка (координатор и проверяющий без правок)

**Files:** Все изменения Tasks 1–3; проверяющий не редактирует файлы исполнителей.

**Interfaces:**
- Consumes: готовые непересекающиеся изменения, логи проверок, итоговый diff.
- Produces: проверяемая локальная сборка и отчёт об уровнях проверки без публикации.

- [x] Объединить изменения; проверить соответствие новых интерфейсов плана реальному коду и каждый пункт Review Focus.
- [x] Из корня проекта выполнить `.venv\Scripts\python.exe build.py --test`; ожидание — все host-тесты завершаются с exit 0, EN/RU-предпросмотры читаемы.
- [x] Выполнить `.venv\Scripts\python.exe build.py --module --overlay --nro`; ожидание — ARM64-модуль, оверлеи и NRO собираются с exit 0, метаданные NRO содержат 0.17.0, иконка новая.
- [x] Независимо просмотреть итоговый diff и выполнить `git diff --check`. Зафиксировать команды/коды завершения, файлы локальной сборки и SHA-256 NRO; перечислить непроверенную консольную приёмку по `docs/testing.md`.

## Result

Local implementation and checks completed. See [validation report](../../validation/2026-11-04-re-graphics-presets.md). Hardware acceptance remains pending.
