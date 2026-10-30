# RE9 control profile and shoulder-camera framing

> **For agentic workers:** Use superpowers:executing-plans or scoped parallel workers with TDD and independent review.

**Goal:** Add a selectable built-in RE9 controls profile informed by actual controller screenshots, and reduce Leon's oversized head in third-person framing.

**Architecture:** Keep Default and recoverable Custom profiles; add a canonical RE9 recipe and an explicit three-choice controls profile picker. Add third_person_distance (native default1, range1..2) to settings wirev10 at100..103, migratingv1-v9 to1. Apply the distance to native camera output before existing collision/smoothing, without changing FOV, bones or first-person/scoped views. Graphics profiles remain independent.

**Tech stack:** Existing C++ profiles, SDL installer, Tesla overlay, guarded ARM native adapter and actual-module host tests.

## Evidence and product choices

- Visually inspected RE9 Switch diagram and in-game PS5 controller screenshot saved under ignored data/analysis/re9; primary RE4 control diagram from Capcom inspected too.
- RE9 references: https://www.spottis.com/img/public/resident-evil-requiem-controls-playstation.jpg and https://img.gamewith.jp/img/ec96ddc7bf46f4b390c10f9f731e09a2.png . Their controller positions establish sticks, aim/attack, melee, L3 run, inventory, map and pause.
- RE4 default controls: https://game.capcom.com/manual/bio4/img/manual/en/switch/5.jpg . Use the default RE4 layout; do not guess mappings for alternate native types.
- RE9 recipe: analog/gyro On, gyro speed0.8, other sensitivities1, stick DZ0.05, gyro DZ0.5, both gyro inversionsOn; free360 camera, return0, third-person distance1.35, first-personOff, moving aim/knifeOn, crosshairSmall, left-stick aim gains0/right1.
- Button sources -> native RE4 targets: A->B (cancel/quickturn), B->Y (interact/confirm), X->PLUS (inventory), Y->B (reload while aiming), L3->B (hold run), PLUS->MINUS (pause), MINUS->X (map). Other buttons retain identity. This uses existing remapping and keeps its native menu/QTE prompt limitation visible.
- This is an approximation using existing actions: no new crouch/parry/quick-heal, weapon shortcuts or full camera-relative walking outside combat. R retains Ashley orders. No claim that RE9 internal curves, animation or timing were copied.
- User specifically says head size/obstruction is the problem, not camera jitter. Use a distance/framing change rather than speculative head-bob filtering.

## Global constraints

- Work in this repository; no personal secrets in source/assets/history. GitHub release hold remains active.
- Preserve other profile domain, language and calibration. Selecting built-ins cannot replace the saved Custom snapshot.
- Gameplay defaults retain original framing1 and all existing behavior. New framing must bypass scopes, scene/reset locks and first person; native collision receives adjusted output.
- Core/profile worker owns profiles.hpp/profiles.cpp/profiles_tests.cpp. Camera worker owns view.hpp/module.cpp/view_tests.cpp/game_module_tests.cpp. Root owns settings codec, UI/catalogue/tests/docs/build. Coordinate interfaces before edits.

## Tasks and review focus

- [x] Search and visually inspect real RE9 and RE4 diagrams; record differences and honest limitations.
- [x] Write RED codec/migration tests; implement field, wirev10 and compatibility marker; preserve all existing settings.
- [x] Write RED profile tests; add RE9 factory/kind/apply and protect Custom snapshots through built-in switching and restart.
- [x] Write RED view/native callback tests; implement third-person distance before collision, exact native bypass1, rotated/scaled bases, walls/reset/scopes/FPS isolation, finite-value rejection.
- [x] Add explicit profile pickers and bilingual descriptions to both UIs; verify empty Custom, Back, save failure and no hover writes. Add camera slider to Camera group.
- [ ] Run complete tests, inspect rendered UI, independent review, ARM build and secret audit; commit/push main and verify CI.
- [ ] Deploy both own slots only through pinned library when installer is open; request hardware framing/layout checks. Do not create a GitHub release/tag.
