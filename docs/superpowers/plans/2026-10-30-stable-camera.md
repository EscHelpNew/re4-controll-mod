# Stable camera implementation plan

> **For agentic workers:** Use superpowers:executing-plans for implementation and a fresh final reviewer.

**Goal:** Smooth the accepted common control model and remove animation-driven camera instability.
**Architecture:** Filter intended stick velocity once per player tick; clamp only stick pitch contributions. Keep camera intent independent of animated muzzle feedback, with honest actual-ray display or proven coherent pose correction.
**Tech Stack:** C++17/20, native ARM64 hooks, existing host regression harness.
**Spec:** `docs/superpowers/specs/2026-10-30-stable-camera-design.md`.

## Global constraints

- All files stay in this repository; no GitHub release/tag.
- Original A/B/X/Y and PLUS/MINUS; shared sensitivity walking/aiming.
- Preserve native collision, weapon effects and firearm-only gyro readiness.
- Never change only visual ray while implying the actual bullet follows it.

## Review focus

- Neutral/reversal must brake promptly without orbiting from stored input.
- Pause, controller, weapon and raising transitions must discard old velocity.
- Pitch beyond the stick limit from gyro must return smoothly, without snapping.
- Near-wall clipped endpoints must not swing the camera as aim distance changes.
- First-person weapon and actual shot alignment must remain honest.

### Task1: smooth input and bounds

Files: `include/re4/unified.hpp`, `tests/unified_tests.cpp`.
- [x] Write/run failing step, release, reversal, rate-equivalence and reset tests.
- [x] Add50ms exponential velocity response with analytic delta integration and12ms braking.
- [x] Add stick-only50degree pitch contribution limiting, including outside-band gyro positions.
- [x] Run strict standalone tests; record RED/GREEN.

### Task2: native camera ownership

Files: `source/game/module.cpp`, `tests/game_module_tests.cpp`, native research in ignored data.
- [x] Resolve strict-center versus stable-camera preference; verify native shot/laser consumers.
- [x] Write native callback regressions for stationary input with changing animated ray and FPS transitions.
- [x] Apply common response and pitch bounds at actual consumption; stabilize camera without fake shot centering.
- [x] Run actual-module strict tests, keeping raise/recovery/controller/collision coverage.

### Task3: framing and delivery

Files: `source/profiles.cpp`, profile tests, docs/version.
- [x] Test closer RE9 framing while Default remains native; apply1.15 preset.
- [ ] Full project tests, independent review, ARM module/overlay/NRO build, secret audit.
- [ ] Commit/push main, verify CI, library-only deploy after installer availability.
- [ ] Obtain hardware feel/FPS/aim acceptance; keep unverified limitations explicit.
