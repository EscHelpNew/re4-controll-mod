# Reload movement implementation plan

> **For agentic workers:** Use superpowers:executing-plans with test-driven-development and a final code review.

**Goal:** Move with the left stick and steer the movement/view with the right stick throughout every ordinary Leon weapon reload.

**Architecture:** Extend the existing reload option using verified native handler/subphase tables. Give unified controls a separate reload look route, consumed at the native root boundary before camera-relative translation, without invoking firearm aim or gyro.

**Tech stack:** C++20, exlaunch, bounded actual-callback host fixtures, local hash-verified NSO analysis.

**Spec:** User request of 2026-11-06; CONTRIBUTING.md and docs/research/weapon-controls.md.

## Global constraints

- Work in the root checkout as CONTRIBUTING.md requires.
- Retain existing settings/profiles, native reload timing, upper-body animation, collision and stationary scopes.
- Verify original Switch code, not PC addresses. Keep game binaries and private data ignored.
- No deployment or publication is requested.

## Review focus

- Automatic type-1 subphase 4 differs from other input schemes.
- Rifle/mine reloads accept only subphases 0/1; launcher has no reload handler.
- Released aim and neutral left input must still allow right-stick look.
- Pause, damage, weapon transitions and option changes must drain stale look input.
- Root/pose callbacks and native reload state must execute once without moving scoped players.

## Tasks

1. **Native reload coverage** — include/re4/modern_move.hpp, source/game/modern_move.inc, tests/modern_move_tests.cpp.
   - [x] Confirm reload tables and subphases against the supported executable.
   - [x] Expand actual callback regressions, observe failure, implement family-specific eligibility and input-scheme collection.
   - [x] Verify reload option independence, released aim, root/pose ordering and rejected states.
2. **Reload steering** — source/game/module.cpp, source/game/modern_move.inc, tests/game_module_tests.cpp.
   - [x] Add failing right-stick/neutral-left/released-aim callback coverage.
   - [x] Add route 3 for reload look, consume once before translation, preserve owner-bound pitch and exclude aim/gyro.
   - [x] Verify setting changes, native transition rechecks, pitch/gains, pause and scope exclusions.
3. **Delivery** — control-menu help, research documentation, local build artifacts.
   - [x] Run python build.py --test and module/overlay/NRO builds; audit public files.
   - [x] Review the final diff, resolve material findings, report local artifacts and hardware-validation limits.

## Evidence ledger

- Native phase4 dispatch and subphase tables confirmed from the hash-verified
  v65536 image, Build ID matching profile.hpp. Launcher has no reload entry;
  charged-special phase4 is aim/charge, so neither is classified as reloading.
- Family callback regression failed on non-pistol movement, then passed after
  family/scheme-specific eligibility. Upper-body/cursor and native root/pose
  ordering checks cover all accepted reload subphases.
- Right-stick callback regression failed on unchanged yaw/pitch, then passed
  with route3 at root. Held/released aim, first/third person, RE6 response,
  neutral left, zero gain, pause/resume and damage are covered.
- Independent review found P2 stale reload velocity across weapon replacement.
  Handler/item/weapon replacement regression failed, then passed after binding
  look to reload identity and rejecting a different identity at root.
- Added combined real-module steering/translation regression with valid gait
  assets; yaw is applied before the same frame's movement and translation is
  not duplicated by an additional root callback.
- Final verification: `python build.py --test` exit0 (62 Python tests and all
  native callback/math/UI suites); `python build.py --module --overlay --nro`
  exit0, including compiled hook ABI and 30/40 hook budget checks.
- Packaged local version0.18.0; release validator checked NRO identity/version,
  bundled module/overlay payload and exact SD ZIP allowlist/digests. NRO SHA256:
  `7231afd7b5ca89a7ea41aa7452a0e77ae0b9238c7992f9259d63236497ced3a0`.
- `python tools/audit_public.py --history --release build/releases/v0.18.0`
  reported no findings across577 working files and1145 history blobs; final
  `git diff --check` passed. Artifacts remain local; no deployment/publication.
- Switch hardware acceptance is still pending, especially reload upgrades,
  walls/stairs/enemies and scope/animation transitions.
