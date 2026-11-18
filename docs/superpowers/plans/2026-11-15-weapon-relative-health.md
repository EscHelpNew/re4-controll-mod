# Weapon-relative health implementation plan

> **For agentic workers:** Use superpowers:subagent-driven-development for the independent scopes below. Parent integrates once source ownership is frozen.

**Goal:** Show weapon-relative HP independent of upgrades, and use authoritative boss HP owners in the existing aimed bars.

**Architecture:** Pure fixed1.0 normal-body prediction, atomic scalar publication, unchanged raw bar rendering, exact native boss-owner resolution.

**Tech Stack:** C++17/20, guarded ARM64 native adapters, host callback fixtures, Switch module/overlay/NRO packaging.

**Spec:** [Approved user choices and design](../specs/2026-11-15-weapon-relative-health-design.md).

## Review focus

- An upgrade or extra toggle never changes the reference unit, weapon stamp or displayed enemyHP.
- Headshots/criticals reduce moreHP without feeding a new denominator back into the UI.
- Unsupported/immune states cannot show fabricated finite health numbers.
- Weapon switches cannot leave old normalized numbers attached to fading targets.
- Multipart boss links must prove exact reciprocal ownership, not just matching type or allocation.

## Parallel tasks

### A. Pure reference predictor

- [x] RED regressions for fixed1.0 arithmetic, native rounding, stable upgrades, supported weapon classes and rejected states.
- [x] Implement `include/re4/enemy_health_reference.hpp`, `source/game/enemy_health_reference.inc` and reference tests using authenticated tables and fields.
- [x] Verify the supported matrix and explicitly retain unknown/immune fallback.

API: `enemy_health_current_weapon_stamp()` returns0 when unavailable;
`enemy_health_body_reference(EnemyHealthReader&, uintptr_t canonical_health_owner)` returns
positive rawHP per fixed1.0 unit or0 when unqualified. No current-power argument.

### B. Boss health ownership

- [x] RED Saddler/Salazar body/part versus coreHP fixtures.
- [x] Implement `source/game/enemy_health_target.inc` with
`enemy_health_resolve_owner(reader, aimed, pool, count, stride)` returning the
qualified owner or0. Unchanged ordinary targets resolve to themselves.
- [x] Test exact vtables, reciprocal links, pool membership, hidden/dead/reused instances and phase maxHP.

### C. Scalar UI pipeline

- [x] RED end-to-end fixed-divisor and weapon-switch regressions.
- [x] Append reference_unit32 and reference_weapon64 to internal AimPoint snapshots and fade targets; retain existing seqlock transaction.
- [x] Capture fixed unit per target and a fresh current class stamp for each displayed scene, even without a target; suppress unknown/mismatched numbers, preserve raw bar fade.
- [x] Resolve bossHP owner before current/max reads, retain aimed anchor, combine owner in opaque target key.
- [x] Update English/Russian health-number help without Settings ABI changes.

Source ownership: pipeline worker owns AimPoint publication/read ranges in
`module.cpp`, `enemy_health.inc`, fade header and matching existing tests/help.
Reference and boss workers own only their new includes/headers/tests. Parent
owns build staging, test catalog, version and integration.

## Integration

- [x] Bump0.28.13, stage new includes and register meaningful new tests.
- [x] Run full suite; fresh review; native module/overlay/NRO build.
- [ ] Audit source/history/package, prepare candidate and deliver without SDK RUN. Audit and candidate preparation passed; console receiver readiness is pending.
- [ ] Console-check stableHP through upgrades, stronger body/critical hits, weapon switches and qualified boss phases.
