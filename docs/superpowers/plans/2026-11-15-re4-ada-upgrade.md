# RE4 Ada controls and purchased Firepower colour

> **For agentic workers:** Use superpowers:subagent-driven-development for the three independent scopes below; parent integrates and verifies the complete candidate.

**Goal:** Restore the existing anchored camera and case rotation for actual playable Ada, and make the entire Firepower bar red after buying the mod's extra upgrade.

**Architecture:** Extend explicit native-qualified character admission for camera and add an independent identity check for native Ada case models. Keep cosmetic donor identity and unqualified gait guards unchanged. Apply purchased colour to temporary native sprite copies at their existing draw boundary, preserving alpha, visibility and geometry.

**Tech Stack:** C++ game hooks, native ARM64 evidence, host callback fixtures, Python packaging.

**Spec:** Existing [case rotation design](../specs/2026-11-08-case-character-rotation-design.md), [shoulder camera design](../specs/2026-10-31-re6-shoulder-design.md), and the user's corrections captured in this goal.

## Constraints and review focus

- Actual Ada is native character2; cosmetic Ada on Leon does not establish native Ada support.
- Camera admission retains active actor, native scene, scope and event exclusions.
- Case admission retains exact native pool/global/vtable/archive ownership; unknown characters/costumes/layouts remain native.
- A native filled-colour template may be non-red: purchased colour must be explicit, preserving current fade and every original sprite byte.
- Only the purchased weapon's Firepower row changes colour. Cancelled purchases, other rows, unowned/low-level duplicate weapons and disabled settings retain native behaviour.
- Bump to0.28.12; no SDK RUN or automatic console restart. Host checks do not establish console acceptance.

## Parallel work

### 1. Purchased Firepower row

- [x] Reproduce non-red native palette and purchased full-row requirement with callback regressions.
- [x] Apply explicit red at native sprite draw boundary; preserve geometry/fade and original sprite storage.
- [x] Run upgrade purchase, damage, persistence and row-colour tests; report hardware limits.

Files: `source/game/extended_upgrade.inc`, matching extended-upgrade tests.

### 2. Actual Ada shoulder camera

- [x] Reproduce actual character2 shoulder drift with native camera fixtures.
- [x] Admit qualified Ada in existing shoulder allowlist; keep combat gait restrictions.
- [x] Verify heading, strafe, pitch, collision and all exclusions.

Files: CameraApply admission in `source/game/module.cpp`, camera cases in `tests/game_module_tests.cpp`.

### 3. Actual Ada case rotation

- [x] Qualify Ada native preview and authenticated81/100-bone model variants.
- [x] Reproduce stopped right-stick rotation, implement independent preview identity, retain cosmetic Leon-only checks.
- [x] Test Ada costumes and negative identity/input/time cases.

Files: `source/game/case_rotation.inc`, bounded include in `character_appearance.inc`, `tests/case_rotation_module_tests.cpp`. Parent adds build staging.

## Integration

- [x] Inspect all scoped diffs, run full host suite, build modules/overlays/NRO.
- [x] Obtain fresh review, audit source/history/package, prepare local0.28.12.
- [x] Deliver only when console ready; verify actual Ada camera, case rotation and purchased red row on Switch. User confirmed these three items in0.28.12; enemy-health/damage diagnosis is tracked separately.
