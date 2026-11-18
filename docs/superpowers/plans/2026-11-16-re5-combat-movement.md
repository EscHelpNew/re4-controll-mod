# RE5 combat movement investigation and implementation plan

> **For agentic workers:** Use systematic debugging and actual native/asset evidence before changing behavior; retain the existing parallel native-flow and adapter scopes.

**Goal:** Make the enabled RE5 movement options work while aiming, attacking with the knife and reloading, for each local player.

**Architecture:** Follow accepted input through the native movement proposal, gait qualification/real animation assets, proposal consumers and lower-body overlay. Preserve native collision/action timing, camera21 and independent player ownership. A synthetic stand-in fixture alone cannot prove a real stationary combat action is eligible.

**Spec:** Original RE5 control-parity request and latest explicit report that all three action-movement options remain ineffective.

## Constraints
- Keep the verified21 camera, two-game menu, RE4HP and APP lifecycle intact.
- No guessed native writes or indiscriminate guard removal; prove field widths, action semantics and the native proposal's final consumer.
- Keep Monitor opt-in and its two-second lease; idle zero counters do not prove a broken hook.
- Upon an actual fix bump22, run meaningful native-backed RED/GREEN callback regressions for all three actions and both profiles, then complete suite, Switch build, review and package audit.
- No SDK RUN or automatic restart. Save/close the game before manual local update; full RE5 module+overlay reinstall/restart required.

## Tasks
- [x] Native flow: prove proposal hook7A6F80 and lower-body hookDD3054 are reached in each action and trace any later zero/discard/overwrite.
- [x] Adapter: qualify every guard against actual stationary combat state and real LMT/MOD assets; reproduce the first demonstrated rejection without weakening ownership/collision guarantees.
- [x] Implement the smallest proven correction, then verify movement vectors, native collision, lower-body animation, scope exclusion and local-player isolation.
- [ ] Integrate revision22 with all checks and deliver for live acceptance when the manual server is ready.

## Proven first rejection and accepted correction

The native animation binder visits tracks forward. Hip solver42 initializes knee2/ankle19 metadata, but the later explicit ankle rotation tracks have solver0 and overwrite the ankle metadata to0. The current topology predicate insists ankle19 survives. Actual idle/walk/back/left/right clips in all five inspected Chris/Sheva weapon resources therefore reject before the native proposal can be modified, while old synthetic fixtures seeded19.

Keep hips42/knees2, logical IDs, parents, bounds, resource/channel qualification and complete fingerprints; accept the two actual native ankle end states0 and19. Add binder-order RED plus positive live callback fixtures for all three actions, both local players and both native profiles. A small default-native GaitRuntimeApi mirrors the existing actor rotation API and lets fixtures replace actual native backend/frame-step boundaries; no test-only setter, wire change or new thread is introduced. The optional IK branch is a separate qualification issue and will not be changed without evidence.

Final revision2 also corrects the pre-existing category guard to use the authenticated native144-record weapon parameter array instead of inventory flags. Actual constructor replay, pointer RELA, record bounds and real firearm/knife categories match both originals. The full141/141 suite passes (`build/tests/run-mq24227e/results.json`); all modules/overlays/NRO, packaging, local release preparation and source/history/package audit pass. Frozen hashes and protected21camera/input/install plus17RE4/APP files match. Candidate22 is ready; physical acceptance and transfer remain pending.
