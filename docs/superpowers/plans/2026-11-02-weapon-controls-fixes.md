# Weapon controls fixes implementation plan

> **For agentic workers:** Use superpowers:executing-plans task by task with test-driven-development and a final independent review.

**Goal:** Restore the native knife/firearm downward range, enable correct moving fire for verified weapon families, and give sniper scopes proportional stick/gyro aiming and smooth zoom.

**Architecture:** Preserve original attack, animation, collision and camera pipelines. Separate overview pitch limits from weapon limits; validate each native weapon state instead of treating every handler as the pistol. Adapt scoped input before native camera matrix construction, retaining the scope's own pitch/FOV storage.

**Tech Stack:** C++20, exlaunch ARM64 hooks, actual-module host fixtures, Python build tools and local Capstone analysis.

**Spec:** User's November 2 request in this session; existing native-control invariants in CONTRIBUTING.md and docs/research/view-and-movement.md.

## Global constraints

- Only the supported Title ID and full Build ID; guard original instructions for every new native boundary.
- Keep Default behavior and user settings compatibility; no game assets, credentials or private logs in changes.
- Original attack timing and collision execute once. Do not bypass reload, raising, scripted scenes or pause gates.
- Changes end in the root RE4 checkout as required by CONTRIBUTING.md; the temporary candidate is solely for preparation and verification under the current filesystem permissions.
- Do not publish, push or deploy a mod to the console without an instruction to do so.

## Review focus

- Up/down limits at weapon entry and weapon changes: no snap from overview or a previous scoped session.
- Different shot/recovery tables: moving fire must retain the original weapon controller and hit origin.
- Scope entry, exit, reload and pause: discard stale stick/gyro/zoom velocity.
- Native control types, inversion and sensitivity zero: no duplicate input or reversed axes.
- Zoom and frame rate: continuous bounded movement, native magnification endpoints, consistent 30/60 Hz behavior.

## Tasks

1. **Weapon pitch** — tests/game_module_tests.cpp, tests/unified_tests.cpp, include/re4/unified.hpp, source/game/module.cpp.
   - [x] Add failing actual-callback tests for knife and firearm stick motion beyond 35 degrees in both directions, bounded by the original normalized pitch endpoints.
   - [x] Restore full weapon pitch while preserving the 35-degree overview range and independent gyro behavior; run targeted callback/math tests.
2. **Moving weapon families** — include/re4/modern_move.hpp, source/game/modern_move.inc, tests/modern_move_tests.cpp and actual callback tests.
   - [x] Verify native handler tables and root/pose/attack ordering against the local supported executable.
   - [x] Add failing family/state tests, implement verified movement eligibility, and check shot/recovery/reload exclusions and input restoration.
3. **Scoped aim and zoom** — focused scope helper/adapter, source/game/module.cpp, include/re4/profile.hpp, callback/math tests and hook-budget checks.
   - [x] Record native scope ABI, pitch/zoom storage, input routing and camera construction boundaries.
   - [x] Add failing proportional aiming, additive gyro, smooth bounded zoom, entry/exit/disabled/inversion and frame-rate tests.
   - [x] Implement guarded scope adaptation before original camera construction; run targeted tests.
4. **Integration and delivery** — version, changelog, test/acceptance notes and local artifacts.
   - [x] Run the full host suite, ARM module/overlay/NRO build and source audit.
   - [x] Obtain independent review, resolve findings with regressions, then transfer the exact verified changes to the root RE4 checkout and verify again.
   - [x] Provide the local installer/package and explicit hardware acceptance limits.

## Evidence ledger

- Original source at c1dfc5e is clean. The active chat checkout is BLACK, not RE4; RE4 was found at F:/projects/NS/re4-controll-mod.
- unified_stick_pitch_delta hardcodes +/-0.610865238 radians for every route. Native 33DF80 clamps weapon target pitch to +/-1, matching the existing full gyro normalization.
- modern_move_frame marks only handler31CC60 as generic_firearm, and modern_move_allowed requires that flag for all firearm movement.
- Scope camera update15460 owns separate pitch at+118 and zoom at+13C, rather than the ordinary aim pitch pair. Native scope state/controller differs from the ordinary camera; analysis is in progress.
