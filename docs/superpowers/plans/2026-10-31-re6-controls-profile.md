# RE6 controls profile implementation plan

> For agentic workers: use tests-first implementation, independent-domain delegation and fresh final review.

**Goal:** selectable RE6 profile in RE4 with faster overview and slower aiming.
**Architecture:** settings wire12 selects measured-response timing/rates in the existing guarded adapter; both UIs use one preset catalogue.
**Tech stack:** C++17/20, libnx/Tesla/SDL, Python build/test/audit.
**Spec:** `docs/superpowers/specs/2026-10-31-re6-controls-profile-design.md`.

## Constraints and review focus

- No changes to original RE6 controls, no new native hooks or unproved offsets.
- Preserve Default/RE9 behavior and ABXY/PLUS/MINUS native mappings.
- Profile changes, gains0, weapon raising, controller switch and pauses cannot replay old momentum.
- Settings1..11 migrate safely; RE6 is controls-only and Custom retains its response flag.
- Derivatives and fitting use native timestamps; do not claim an exact RE6 camera-engine port.
- Main only, all tests in CI, no personal credentials and no release/tag.

## Tasks

- [x] Core: failing tests for flag wire12/migration, ABI16, RE6 preset identity and Custom preservation; implement and verify.
- [x] UI: failing tests for four-row catalogue, RE6 action and bilingual per-mode gain help; implement and render/verify.
- [x] Runtime: failing tests for separate measured rates, rise/brake timing, mode transitions, frame-rate integration and gain0; implement with existing camera hooks.
- [x] Verify measured response constants from existing journal; document fit limits.
- [x] Run full native/Python/SDL and ARM builds; fresh review; source/history/package secret audit.
- [ ] Commit/push main, verify CI; library-only own-app delivery and concrete install/test instructions.
- [ ] Hardware RE4 acceptance and optional synchronized comparison log.
