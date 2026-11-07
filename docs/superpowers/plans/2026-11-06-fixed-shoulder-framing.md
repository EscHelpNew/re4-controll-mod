# Fixed shoulder framing implementation plan

> **For agentic workers:** Use superpowers:executing-plans, test-driven-development and final independent review.

**Goal:** Keep Leon and playable Ashley in the existing side of the frame while turning the RE6 shoulder camera, with small natural camera movement and native wall protection.

**Architecture:** Keep the existing root-relative rig and eased stick look. Correct the native local-X follow stage that runs after collision: it blends current and previous eye/target X in a rotating delayed basis, breaking the rig's otherwise constant screen anchor. At the verified collision-return boundary, seed only its two X-history values from the already clipped current result for the exact active shoulder call. Do not move the collision result or replay camera/collision updates.

**Spec:** User's request of 2026-11-06 to fix the character in the corner and reduce camera movement around them; CONTRIBUTING.md and existing view/movement constraints.

**Tech stack:** C++20/exlaunch, actual-module callback tests, local hash-verified native disassembly.

## Constraints and native evidence

- Preserve all previous uncommitted reload/Ashley changes; work in root checkout.
- Preserve current shoulder side, distance, actor/weapon animations, input easing, game FOV/roll, collision anchor and one native wall pass.
- Native18990(x0 unused, basis x1, profile x2, result x3) returns32-byte eye/target/roll/FOV in the local captured basis.19CDC calls it once.
-19D0C..19D44 blends only eyeX and targetX with history1132A30/+C using floats6E8A2C=.3999999761581421 and6E8A30=.6000000238418579, then19D70/19D84 transforms both points into world space. Old local-axis history is not valid after the camera basis rotates.
- Add a guarded trampoline at18990; retain40-slot capacity and guard the native ABI/call/filter/cache instructions before installation.
- First-person, Default/classic, unsupported characters, scopes, scripts, pause and unqualified camera calls retain native ownership.
- Hardware acceptance of feel, obstruction behavior and full character animations remains pending; no deployment/publication requested.

## Review focus

- Full rotations/reversals, delayed bases and pitch must keep the screen anchor in free space.
- A clipped camera result must remain exactly clipped; no stale eye can move it through a wall.
- Cache writes require the exact current basis/profile call and completed shoulder ownership.
- Actor/character/scene/profile changes during native collision must cancel history ownership.
- All other camera modes, foreign calls, invalid results and hook-budget/ABI failures fall back safely.

## Tasks

- [x] Reproduce downstream framing drift in the actual camera fixture, including native collision and the verified local-X filter; observe a failing projected screen-anchor test.
- [x] Add the guarded collision-result adapter, exact call ownership and finite validation; verify rotations/pitch/collision and unchanged fallbacks.
- [x] Record local0.18.2, update documentation, run full tests and ARM64 module/overlay/NRO builds, obtain independent review, audit and package local artifacts.

## Evidence ledger

- Projected torso regression failed with the old local-X history, then passed
  after the guarded collision-return adapter. Free-space projection stays
  within.02 logical pixels through turns/reversals/pitch and delayed translated/
  rotated native bases for Leon and playable Ashley.
- Native wall fractions1/.5/.1 retain the clipped eye even with old history
  at+5000/-5000.23 callback gates verify one original call, identical collision
  output, and no unrelated cache writes or ineligible-path ownership.
- All157 literal instruction/data guards match the segment-hash-verified NSO;
  existing mutation tests cover rejection of every guarded site.
- Independent review confirmed native ABI/result/cache layout and reported no
  important issue. Real obstruction geometry, live poses and camera comfort
  remain hardware acceptance items.
- ARM64 module/overlay/NRO build passed with compiled hook ABI and31/40 slots.
  Local0.18.2 NRO SHA256:
  `914ff81f09a899c47dcf1f22c5880171da2e87fad5a3d61f39283b5a98140306`.
- Full `python build.py --test` passed:62 Python tests plus all native
  callback/math/installer/renderer suites, including the new projection,
  collision ownership, instruction guard and hook-pool checks.
- Release validator passed NRO identity/version and exact archive allowlist/
  payload digests. Source/history/release audit:579 working files,1161 history
  blobs, no findings. `git diff --check` passed. Artifacts remain local; no
  deployment/publication or hardware success is claimed.
