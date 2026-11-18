# RE5 Controls Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add full RE4-style controls and graphics for RE5, including both split-screen players, in a gray RE5 page between RE4 and RE6.

**Architecture:** A separate guarded SDK6 RE5 module owns game-specific hooks. Reuse input math and menu definitions, with separate RE5 persistence and independent runtime state for two players. RE4/RE6 keep their existing wire formats and installation ownership.

**Tech Stack:** C++17, exlaunch/AArch64, libnx/Tesla/SDL2, Python build and host harness, capstone for private native research.

**Spec:** `docs/superpowers/specs/2026-11-11-re5-controls-design.md`

## Global Constraints

- RE5 v196608, title `010018100CD46000`, main Build ID `4E46401BF2FE53A44B69CC07FD951FE6` (padded to 32 bytes).
- Solo and both local split-screen players are required. Online/DLC/Mercenaries support requires additional evidence.
- Gray RE5 between unchanged RE4 and RE6; own config, backups, controls/custom graphics profiles and install receipts.
- All RE4 control options are targets; unverified game effects cannot be exposed as working settings.
- Quality target AF16x, validated AA and distance 1.75; native defaults must be actual RE5 values.
- Native gyro must not add a second contribution. Scopes stay stationary, right stick/gyro aim, left stick zoom.
- Game materials and keys stay in ignored local storage. No guessed game pointers or unguarded hooks.
- New feature version 0.28.0; each changed fix build increments PATCH. No release until artifact checks and an honest support matrix.

## Review Focus

- Controller reconnect or reversed local player assignment must not transfer accumulated motion or calibration between players.
- Entering/leaving split-screen must preserve primary-player settings and reset stale secondary state.
- Sheva's mirrored shoulder must not reverse aim, reticle placement or gyro inversion unexpectedly.
- Switching games with an edited gyro draft must never write another game's configuration.
- Cold boot, failed writes, and replacement of an incompatible module must preserve recoverable settings and existing foreign mod files.

---

### Task 1: Prove the native RE5 adapters

**Files:** private `data/analysis/re5/input-research/`, `data/analysis/re5/graphics-research/`; final public `docs/re5-native.md`, `include/re5/profile.hpp`.

**Interfaces:** Produce exact Build IDs, bounds, instruction guards and documented ABIs for input owner, per-player camera/aim/movement, scope/zoom and graphics consumers. Subsequent code must cite that evidence; private images are not test fixtures or product assets.

- [ ] Trace callers/data consumers from validated NSO images for each adapter; record actor/controller/viewport association and menu/scripted guards.
- [ ] Compare full SDK Build ID and native imports, and identify the one gyro contribution point.
- [ ] Prove sampler descriptor semantics, AA/resolution transitions, culling and shadow ownership independently; record unresolved paths as unresolved, without guessed adapters.
- [ ] Add behavioral identity/guard tests when the profile is written: wrong build, truncated memory or mismatched instruction installs zero hooks.
- [ ] Verify test RED before profile implementation, then GREEN with exact synthetic instruction fixtures; retain private native verification outputs.

### Task 2: RE5 settings, two-player runtime and saved profiles

**Files:** `include/re5/settings.hpp`, `source/re5/settings.cpp`, `source/re5/settings_file.cpp`, `include/re5/player_input.hpp`, `tests/re5_settings_tests.cpp`, `tests/re5_player_input_tests.cpp`, `tools/test_catalog.py`.

**Interfaces:** `re5::Settings` has `std::array<re4::Settings,2> players`, shared `re4::GraphicsOptions graphics` and language; `encode_settings/decode_settings`, `save_settings_file/load_settings_file`; `PlayerInput` owns its own gyro/filter/frame clocks per actor, controller and session. Own RE5 magic/version/CRC envelope; no RE4 config migration. Exact envelope/layout is documented alongside the implementation.

- [ ] Write tests for two distinct player mappings/speeds/biases, shared graphics, malformed enums/nonfinite values/size/version/checksum and unchanged destination on rejection.
- [ ] Run the new native cases; observe missing implementation/behavior fail, then implement and run GREEN.
- [ ] Add real persistence tests for backup recovery and failed save leaving active values intact; ensure built-in profile changes retain Custom.
- [ ] Add independent-input tests for both pad IDs, reassignment/reconnect, stale samples and split-screen transitions; reuse re4 math but remove primary-controller-only assumptions in RE5's own ownership adapter.

### Task 3: Three-game installer and RE5 live diagnostics

**Files:** `include/re4/game_target.hpp`, `include/re4/installer_ui.hpp`, `source/diagnostic/installer_model.cpp`, `source/diagnostic/installer_renderer.cpp`, `source/diagnostic/main.cpp`, `tests/re5_installer_tests.cpp`, `tests/installer_preview.cpp`.

**Interfaces:** RE5 game identity is separate from navigation order; preserve existing RE6 enum identity. `active_controls()` selects RE4 or RE5 selected local player; `active_graphics()` selects own game graphics. RE5 player selector has Player 1 / Player 2. The common language and updater remain shared.

- [ ] Test L/R cycling RE4→RE5→RE6 and inverse with separate selection/status; mutate RE5 controls and prove RE4/other player unchanged.
- [ ] Implement gray theme and a gray numeral 5 using the established layout; retain RE4's original4 and RE6's existing icon.
- [ ] Reuse applicable control groups/mapping/profile menus with RE5-specific help, save routing and per-player selector.
- [ ] Extend diagnostics and gyro draft/save/cancel to the selected player's settings; Y centers, X calibrates; draft never crosses a game/player switch. Test both inversions against native axis basis.
- [ ] Add RE5 graphics presets and correct restart guidance only after Task1 proves their native paths; render and inspect RE5 SDL screenshots.

### Task 4: Guarded RE5 game module

**Files:** `source/re5/module.cpp`, `include/re5/profile.hpp`, `include/re5/shared.hpp`, focused `include/re5/` native adapter headers, `tools/re5_module.py`, `profiles/010018100CD46000/npdm.json`, `tests/re5_module_tests.cpp`, `tests/test_re5_build.py`, `build.py`.

**Interfaces:** Strict RE5 shared ABI publishes settings and per-player status; module latches settings on the verified game thread. SDK6 staging mirrors verified RE6 compatibility without altering third-party source. NPDM starts from the verified RE5 permissions and grants only module-required aliases/SD access.

- [ ] Test real callback adapters with a memory/SDK boundary fake: wrong actor/state/build must preserve native behavior; both local controllers mutate only their actor/camera.
- [ ] Implement stick sensitivity/curves/deadzones/mapping and exactly-once gyro using the proven native points, including reset on reconnect/session changes.
- [ ] Implement aim/knife/reload walking via native collision/animation handlers; retain stationary scopes with smooth zoom. Verify representative weapon families, special-action exclusions, death/pause/cutscene gates.
- [ ] Implement free camera/return/distance/first person and weapon-ray reticle in each viewport; Sheva basis is explicit and tested.
- [ ] Implement proven filtering/AA/distance/shadows with restoration of owned values only; test native defaults and preset1.75.
- [ ] Build ARM module and check undefined SDK imports, trampoline capacity, exact guards, and context ABI.

### Task 5: RE5 overlay and independent installation

**Files:** `source/re5_controls/{main.cpp,game_link.cpp,storage.cpp}`, corresponding headers, `source/install.cpp`, `include/re4/install.hpp`, `tools/payload.py`, `tools/package.py`, `build.py`, installer main, new RE5 overlay/storage/install tests and existing packaging tests.

**Interfaces:** `Re5GameLink` validates title, full build and shared ABI before publication; one settings writer at a time with sequence/ack. Install bundle includes own RE5 subsdk9/main.npdm/overlay and receipt; existing files require known ownership before replacement/removal.

- [ ] Test actual overlay events and settings publication for either local player; disk-write failure cannot report applied success, unknown/RE4/RE6 module cannot receive RE5 bytes.
- [ ] Implement gray RE5 overlay using shared controls descriptions and RE5-specific graphics help.
- [ ] Test installation/disable transaction and unknown foreign files, closed-game gate, incomplete payload, rollback and both existing games unchanged.
- [ ] Wire full module/overlay/NRO builds and package validation; audit that no game assets/keys enter product artifacts.

### Task 6: Complete validation and console acceptance

**Files:** `include/re4/version.hpp`, README EN/RU, CHANGELOG, `docs/re5-controls.md`, `docs/releases/v0.28.0.md`, validation evidence.

- [ ] Run `python build.py --test --jobs 4 --coverage`; all cases pass and both project coverage floors hold.
- [ ] Run `python build.py --module --overlay --nro`; verify all three paired modules/overlays and installer identify0.28.0.
- [ ] Inspect real SDL previews and native evidence; fresh whole-branch review checks all five Review Focus conditions.
- [ ] Prepare installable candidate; use previously authorized SDK network deployment if console is available and identity can be checked.
- [ ] Request concrete hardware checks for both players, Chris/Sheva, weapon families, scopes and graphics; distinguish host/ARM proof from console acceptance.
- [ ] Only publish a release with accurate support and pending-validation notes; do not label unresolved target functionality completed.
