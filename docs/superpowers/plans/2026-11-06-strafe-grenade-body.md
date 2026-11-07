# Strafe framing, grenade movement and body steering

> **For agentic workers:** Execute with superpowers:executing-plans; use failing regressions before implementation.

**Goal:** Keep Leon/Ashley left of the screen during lateral movement, allow grenade-ready locomotion, and turn the walking body slightly toward travel.

**Architecture:** Preserve the existing pre-collision shoulder rig and native physics. Temporarily zero only the qualified normal camera's downstream follow retention. Extend the verified grenade action gates and use the existing native root/pose pass. Overlay a bounded root-bone yaw only during ordinary unified walking, compensating lower-body yaw and restoring owned pose before the next native update.

**Tech Stack:** C++20 exlaunch, native Switch v65536 profile, host callback fixtures, Python build/release tools.

**Spec/design:** This document records the user's three concrete requests. Implementation choices are ours: approximately25degree maximum body steering, native gait speed, no body overlay during weapon actions. Rotating authoritative actor yaw would couple walking body to camera/aim; root-bone steering preserves existing view ownership. Changing the final camera eye after collision would invalidate the wall trace, so fix its follow history instead.

## Constraints and review focus

- Keep changes in this dirty root checkout, preserve unrelated work; no commit/push.
- Preserve native collision output, release events and upper-body action clips.
- Gate changes by exact active camera/player/profile/state, including Ashley, pause, scope and special cameras.
- Restore root-bone pose when stopping, switching character, disabling options or entering aim; preserve later native writes.
- Test moving anchors through world follow, delayed/rotated bases and clipped camera eyes.
- Grenade ready transitions and throw timing need verified native gates and actual adapter tests.
- Host verification does not establish console acceptance.

## Tasks

- [x] Camera: reproduce world-follow lag in actual callbacks, qualify retention fix, verify motion/collision and negative contexts.
- [x] Grenade: verify native states/order, add failing gates and runtime translation/pose tests, extend handler support.
- [x] Body: test bounded steering/root-pose ownership; implement root-bone overlay and lower-body compensation for ordinary walking only.
- [x] Run full tests, build module/overlay/NRO, fresh review, package and public audit; bump local version.
- [x] Deliver build to already authorized console if reachable; report hardware verification limits.

## Evidence ledger

- Camera11E50/12210 calls19B20, then world-follow138B0 after the existing local-X filter. Prior fixtures omitted this stage.
- Grenade31B9F0 phase tableB096D0: held31BB90, throw31BC50. Throw root runs before release event/spawn2E4510, so existing movement boundary is suitable.
- Skeleton2B4D70 composes parent world+18 with bone local+48; bone0 owns torso1 and legs17. Root-bone pose does not overwrite actor position/yaw, but affects bone attachments, so apply only ordinary walking and restore before actions.
- RED: grenade-ready fixture failed at movement count; after handler support, new body-turn assertion failed; both then passed. World-follow moving-anchor projection failed before the scoped retention fix and passed afterward.
- Full build.py --test exited0 (`build/strafe-grenade-tests.log`). ARM module/overlay/NRO build exited0 (`build/strafe-grenade-build.log`),32/40 hooks. All149 native instruction guards match the hash-verified local image.
- Fresh review found no confirmed important issue. Minor limitation: Euler lower-body compensation is approximate when the native root contains pitch/roll; console visual acceptance remains required.
- Additional actual-module fixture retains native grenade root -> release -> shared aim ordering while combining walking and right-stick input, including throw origin after movement.
- Packaged0.18.5 NRO SHA256: `5819954eab536f14a5991d8101518d9dae59fbc6058e9d13d99f032a33f2337f`. Public release audit:583working files/1172history blobs, no findings.
- Delivery status:192.168.1.131 refused the app status service (Win10061). Asked user to close RE4/open RE4 Controls; no transfer or hardware acceptance claimed.
- Follow-up2026-11-07: user reports sway under right-stick turning.0.18.5 had not been transferred by this session. Asked for in-game overlay version before attributing the report to the new binary. Extended actual callback regression to run both follow stages through yaw/pitch sweeps and120 right-stick/reverse/release updates for Leon/Ashley; current0.18.5 code passes within0.02logical pixels. No further production fix inferred from this host result; actual installed module/settings and console behavior remain to be established.
- Delivery completed2026-11-07 after user confirmed previous overlay version and explicitly opened RE4 Controls for upload. Stored both BRIDGE and GAME with pinned reload.py, automatic restart verified app0.18.5-pAE/title identity and SHA256 `5819954eab536f14a5991d8101518d9dae59fbc6058e9d13d99f032a33f2337f` on192.168.1.131. User must select RE4/install-update in the app to replace the game mod; mod installation and in-game visual acceptance are not claimed.
