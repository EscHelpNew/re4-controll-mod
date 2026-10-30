# Remapping and free camera implementation plan

> Execute using superpowers:executing-plans, with independent core work delegated under superpowers:dispatching-parallel-agents.

**Goal:** Full game-button remapping and configurable/free camera with compatible settings.
**Spec:** docs/superpowers/specs/2026-10-30-remapping-camera-design.md
**Architecture:** Pure button-map/settings/camera math; thin verified SDK hooks; shared bilingual models for installer/overlay.
**Tech:** C++17/20, SDK5/libnx, SDL2, Python and existing Windows CI.

## Tasks
- [x] RED/GREEN core mappings and128-byte v7 settings; migrate64-byte v1-v6, preserve saved settings, defaults gyro0.80.
  Files: include/re4/button_map.hpp,settings.hpp; source/settings.cpp; core settings/input tests.
- [x] RED/GREEN actual SDK pad hooks and startup-file size migration; shared ABI, all input styles and menus/QTE.
  Files: source/game/module.cpp, include/re4/shared.hpp; tests/game_module_tests.cpp,game_stubs/.
- [x] RED/GREEN return/free-orbit math and actual camera callbacks; add verified18520hook/profile guard.
  Files: include/re4/camera.hpp,profile.hpp; source/game/module.cpp; tests/camera_tests.cpp,game_module_tests.cpp.
- [x] Bilingual mapping page and camera controls in overlay/installer, with focused descriptions and reset/persistence tests.
  Files: source/control_menu.cpp,overlay/main.cpp,diagnostic/installer_model.cpp,installer_renderer.cpp; UI tests/stubs.
- [ ] Review independently, full host suite and ARM build, audit, merge main/push and confirm GitHub Actions.
- [ ] Hardware acceptance of mapping/camera. GitHub release publication is deferred at the user's explicit request; do not publish0.5.0 yet.

Independent review found no concrete P0/P1/P2 issues. Local full checks pass:
26Python tests,22native programs, ARM module/overlay/NRO builds and hook ABI.
All25instruction guards match the actual supported executable;13SDK imports
resolve. New installer EN/RU mapping/camera previews were visually inspected.
Known personal-material scan found no source/history/release-asset matches.
The four0.5.0 assets exist only locally; no GitHub release/tag was created.
