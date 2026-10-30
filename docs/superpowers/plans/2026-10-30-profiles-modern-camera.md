# Profiles and modern camera implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans for implementation. Steps use checkbox syntax for tracking.

**Goal:** Independent persistent control/graphics profiles and verified optional modern gameplay modes.

**Architecture:** Domain-only profile copying and strict encoded comparisons, sidecar snapshots using current atomic settings IO, shared bilingual UI semantics. Gameplay extensions use verified native hooks independently of profile storage.

**Tech stack:** C++17/20, libnx, exlaunch, Tesla, SDL2, Python unittest, GCC14/16.

**Spec:** `docs/superpowers/specs/2026-10-30-profiles-modern-camera-design.md`.

## Global constraints

- Work in the root repository; no GitHub release while hold is active.
- No personal keys/tokens in tracked files or public artifacts.
- Graphics defaults remain Native, gyro speed0.80 and other speeds1.00.
- Keep previous settings values and the native disabled-mode behavior.

## Review focus

- Profile switching must preserve the other domain and device calibration.
- Loading an old custom configuration must not lose it when Default is selected.
- Corrupt/missing sidecars and write failures must not destroy the active file.
- Recreated UI pages must show the restored domain, not stale labels or values.
- Native scope/cutscene/physics paths must not be affected by experimental modes.

### Task 1: Profile domain and persistence core

Files: new `include/re4/profiles.hpp`, `source/profiles.cpp`, `tests/profiles_tests.cpp`.
Interfaces: `ProfileDomain {Controls, Graphics}`, `ProfileKind {Default, Custom}`;
`copy_profile(Settings&, const Settings&, ProfileDomain)`,
`profile_kind(const Settings&, ProfileDomain)`,
`same_profile(const Settings&,const Settings&,ProfileDomain)`,
`save_profiled_settings(const char*,const Settings&)`,
`load_custom_profile(const char*,ProfileDomain,Settings&)`.

- [x] Add failing tests for independent identity/copy, preference exclusions, canonical equality, switch/restore, migration, corruption and write failure.
- [x] Run the new suite to establish RED, implement the pure domain core and sidecar IO, then establish GREEN.
- [x] Integrate compilation in tests/NRO/overlay without introducing runtime filesystem work in the game module.

### Task 2: Both interfaces

Files: `control_menu.hpp/.cpp`, `installer_ui.hpp`, installer model/renderer/main,
overlay main, existing UI/model tests and preview fixtures.

- [x] Add failing tests for Default/Custom actions independently in Controls and Graphics, restore and missing custom behavior.
- [x] Add profile rows, route saving through profile persistence, move Graphics to an overlay root peer.
- [x] Run native tests and inspect English/Russian profile and root previews.

### Task 3: Optional gameplay modes

Files: native research notes (ignored), settings codec, pure camera/reticle helpers,
game module, build profile guards and corresponding codec/native callback tests.

- [ ] Establish precise camera transforms, head visibility, late HUD ABI and native movement/animation budget before writing hooks.
- [ ] Add RED tests for disabled equivalence, scene/scope gating, true projected reticle, single physics/animation execution, bounded native movement and settings migration.
- [ ] Implement only supported paths; keep unresolved work explicitly pending.
- [ ] Build ARM game module, overlay/NRO; verify all instruction guards and SDK imports.

### Task 4: Review and delivery

- [ ] Run all tests, renderer review, fresh code review and credential audit.
- [ ] Commit reviewed source to main, verify GitHub Actions, package locally only.
- [ ] Transfer through pinned reload library only when console installer is available; request concise hardware acceptance for new native behavior.
