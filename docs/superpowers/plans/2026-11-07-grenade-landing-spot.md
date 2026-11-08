# Grenade Landing Spot Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans. Track steps below.

**Goal:** Optional fog-like red marker at an aimed grenade's approximate first floor contact.
**Architecture:** Pure bounded simulation/draw helpers; private native queries after player update; copied displayed-scene snapshots; Game toggle in both menus.
**Tech Stack:** C++17/20, exlaunch/libnx, Tesla, SDL2, Python/Capstone local research.
**Spec:** ../specs/2026-11-07-grenade-landing-spot-design.md

## Global Constraints

- No projectile spawn, RNG consumption, actor/geometry writes, synthetic floor height or physics queries in HUD.
- Restore native collision scratch; bound queries and refresh rate. Unsupported/invalid data hides the spot.
- Local0.21.0, settings wire16 byte106, ABI20. Default off and preserve all Game preferences through profiles.
- Work in root checkout per CONTRIBUTING; preserve published0.20.0 and other mods.

## Review Focus

- First floor after wall/ceiling impact; no floor/expired flight must not show a stale result.
- Collision scratch/reentrancy and live manager bounds; no impact on game physics.
- Display camera/scene coherence, pause/aim release/weapon swap and mixed IPC versions.
- Soft gradient, floor perspective, clipping, finite geometry and render state restoration.
- Profile migration/defaults, third menu row, preview/footer layout and bilingual text.

### Task1: Pure prediction and fog geometry

Files: include/re4/grenade_prediction.hpp, include/re4/grenade_spot_draw.hpp; matching tests.
- [x] Add failing native-step/sweep/first-floor/bounds tests, then pure model.
- [x] Add failing projected gradient/backend/state tests, then fog geometry.
- [x] Run strict host component tests and review interfaces.

### Task2: Setting and Game menus

Files: settings/shared/profiles codecs, game_menu, overlay and installer model, relevant tests/version.
- [x] Test old migrations/defaultOff, roundtrip/invalid bool/reserved bytes and profile preservation.
- [x] Implement wire16/ABI20/0.21.0 and independent grenade_landing_spot flag.
- [x] Add third Game row and guarded paired native update, bilingual labels/help; test actual menus.

### Task3: Native adapter and displayed scene

Files: source/game/grenade_spot.inc, module.cpp, modern_move.inc, profile.hpp, actual adapter tests/build.py.
- [x] Guard researched launch/physics/query sites, validate actor/weapon/manager/finite matrices.
- [x] Test actual callbacks with fake native queries: private vectors, scratch restore, cached refresh, first-floor, scene staging, aim release and exclusion paths.
- [x] Implement after-player-update prediction; publish copied center/normal, pair with displayed camera; HUD consumes no world pointers.
- [x] Review all tasks, run full host tests, module/overlay/NRO builds, public audit, render previews.

### Task4: Delivery

- [x] Prepare local0.21.0 package and matching hashes. Send to console only after working-state/authorization checks; user must install paired game module/overlay. No release publication requested for this feature.

## Research ledger

- Local analysis image SHA256 c4170af8c7990c9bd7da7c1c30a9ca93bc2d325af8929c043f8b94471bd7efb3 matches verified update image.
- Launch2E4510 uses current bones0/10, player+18 basis, aimB27D00, random lateral velocity±15 and inherited bone0 displacement. Gravity20, dtstate+A4; semiimplicit integration.
- Native8330 segment query and85E0 radius25 sphere correction use private vectors but shared scratch1131250/1131260/1131268..1133268. Preserve it around bounded, nonreentrant game-thread batches.
- Floor-like native normal.y>0.7; wall reflection halves speed. Floor material/detonation handling beyond first contact is outside this feature.
- Source research lives ignored under data/analysis/grenade_flight and grenade_collision. No game binaries/disassembly enter public files.

## Expanded scope and verification ledger

- User added pickup confirmation skip, startup-logo button skip and aiming-sway
  removal, all as optional Game rows; four new toggles defaultOff. Wire16 bytes
  106..109, reserve110..123, ABI20. Existing Game preferences survive profiles.
- Health appearance changed to200ms; disappearance stays300ms. Pure and actual
  callback timing tests passed with shared number/bar alpha.
- Pickup4A0948 clears only active native primary-channel bit0 at+1C/+24 and
  writes Yes only for a fresh selectable popup on known controllerB00648. Native
  grant/full-inventory flow remains in4A0470. Callback exact-write/invalid-state
  tests passed; independent review found no actionable issue.
- Logos4E3960 checks live owner1272A18, title stage4/substates0,1,2,10/frame105..320
  and fresh native accept edge. It requests native state30 cleanup. All eight
  locale timelines were independently checked; tests preserve other stages,
  movies, held/repeated nonaccept directions and native continuation.
- Sway option passes native bit0 at33EC50, preserving Orig, manual input, gyro
  and pitch unit conversions. Actual callback RED/GREEN and exclusion tests passed.
- Grenade room identity is native uint16 state+5020, proved by two resource
  builders and room initialization. Cache/capture/submit/display/HUD all check it.
  Same-room teardown uses existing inactive-scene resets.
- Ruling: refuse active collision meshes above8192triangles. This bounds the
  native scratch to1025visited bytes and limits save/restore to0x419bytes,
  excluding adjacent camera globals. Invalid/missing geometry hides the marker.
- Fresh model review found missing near/far clipping. Fixed using the proven
  native projection interval -clip.w<=clip.z<=0; near/far/crossing tests pass.
- Actual grenade adapter tests cover corrected origin, walls/visibility,
  bounded meshes, scratch restoration on success/early returns,100ms refresh,
  room-stage races, all three ordinary grenade IDs, defaultOff and unsafe states.
- Fresh native/QOL review verified all296distinct profile guards against the
  verified local image and reported no remaining actionable finding. Hook pool
  uses34/40slots. ARM64 module/overlay/NRO build passed(v021-build.log).
- Full-suite passes were interrupted by obsolete hook-count expectations and a
  standalone movement fixture missing the new post-update dependency stub;
  both fixture issues were corrected. Final full-suite rerun is in progress.
- Local0.21.0 artifacts prepared and audited, NROSHA256
  2e1799d9f17b053515287803992ceb7f00d82ea87123f7182ac4747097cd9ccf.
  User confirmed the console app is open for transfer; await final test completion.

- Final full python build.py --test exited0 (build/v021-tests.log); ARM64 module/overlay/NRO exited0 (build/v021-build.log). Diffcheck exit0; source/history/localrelease audit613workingfiles/no findings. Final NRO2e1799d9f17b053515287803992ceb7f00d82ea87123f7182ac4747097cd9ccf matches prepared artifacts and BRIDGE upload. Console GAME restart verified as0.21.0-p9A with exactSHA256, titleverified andcompatiblebridge. No0.21commit/publication performed.
- Delivery complete:0.21.0-p9A, protocol2, exacthash2e1799d9f17b053515287803992ceb7f00d82ea87123f7182ac4747097cd9ccf. User must install/update the paired RE4 module/overlay, then restart the game; console gameplay acceptance remains pending.

## 0.22.0 follow-up delivery

- Replaced first-floor prediction with native bounce/fuse/material/water burst
  prediction; airborne bursts project onto real ground below. Center is redder.
- Added optional D-pad instance/type cycling and increased HP number opacity.
  Settings wire17/sharedABI21 migrate deployed 0.21.0 without enabling D-pad.
- Full host suite and module/overlay/NRO builds passed; final water material2
  fail-closed correction passed actual grenade callback tests and rebuilt module
  and NRO. Public source/history/release audit reported no findings.
- Transferred frozen artifacts on 2026-11-07 after user confirmed app open.
  BRIDGE storage and automatic GAME restart both verified exact SHA256
  a4726d92307bb0f643b5798d250cd4a341455cbaed300c4ce847c4893510efaa,
  app0.22.0-pA1, protocol2, compatiblebridge and verifiedtitle.
- User was told to install/update paired game module and restart RE4. Cosmetic
  appearance is separate unfinished work; no skin feature exists in this build.
