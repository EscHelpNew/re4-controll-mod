# RE6 shoulder framing implementation plan

> **For agentic workers:** Use superpowers:executing-plans for inline execution and a final fresh review.

**Goal:** Reduce Leon's frame drift in RE6-response third-person views and verify reload-on preset selection.

**Architecture:** A stateless root-relative rig writes QFPS eye0/target before native collision. Existing final direction and native eye1, FOV, roll and non-RE6 paths stay intact.

**Tech Stack:** C++17/20 host tests, ARM exlaunch module, Python build and package tools.

**Spec:** docs/superpowers/specs/2026-10-31-re6-shoulder-design.md

## Global constraints

All files in this repository; no worktrees. No personal keys/tokens/game dumps in Git or packages. No new hook or ABI. Native RE6 gyro remains unchanged. Hardware feel remains unverified until user confirmation.

## Review focus

- Root translations and rotations must not cause screen-relative drift in open space.
- Pitch poles, invalid/singular matrices and extreme inputs must not create partial output.
- Native collision anchor and first-person priority must survive.
- Default/classic/other-character/scoped/reset views must stay isolated.
- Old Custom reload-off settings remain recoverable when selecting RE6.

## Task 1: Rig and adapter

Files: include/re4/view.hpp, source/game/module.cpp, tests/view_tests.cpp, tests/game_module_tests.cpp, tests/profiles_tests.cpp.

- [x] Add geometry tests for a root-relative shoulder eye, basis invariance, yaw/pitch extremes, invalid inputs and retained anchor/FOV/roll.
- [x] Add actual callback tests with changing native eye profiles/basis/root and RE6/Default/FPS/character/reset gates; verify reload-off → RE6 → disk reload.
- [x] Observe RED in the actual callback, implement bounded stateless re6_shoulder_view(native,basis,root,heading,pitch,distance,out), observe GREEN.
- [x] Run full host checks, ARM module and UI/NRO builds sequentially; obtain a fresh review.

## Task 2: Delivery preparation

Files: include/re4/version.hpp, CHANGELOG.md, docs/re6-profile.md, README.md, docs/README.ru.md.

- [x] Bump to0.15.1 and document root framing, collision limits and preset reselect behavior.
- [ ] Package and audit source/history/allowlisted artifacts; commit/push tested main and verify CI.
- [ ] Prepare an OTA release with accurate hardware limits; publish under the user's OTA delivery instruction, verify anonymous manifest/NRO hashes.
