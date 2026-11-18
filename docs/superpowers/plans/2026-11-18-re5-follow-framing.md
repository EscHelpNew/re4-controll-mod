# RE5 fixed shoulder framing implementation plan

> **For agentic workers:** Use superpowers:executing-plans for root integration. Native qualification and test fixtures have separate delegated ownership. Track steps with checkboxes.

**Goal:** Keep the character at a stable shoulder position during walking and right-stick turns in Modern RE5 controls.

**Architecture:** Preserve the working actor-heading and root-relative component pose. Authenticate one component publication and suppress only native spatial follow retention for that consumption before collision. Native roll, FOV, pose banks and coefficient evolution continue; no persistent post-collision eye overwrite.

**Tech Stack:** C++/devkitA64, actual installed callback fixtures, paired exact main-image guards and private original-instruction replay.

**Spec:** User requires stable framing in both walking and standing camera turns, for both local players. The completed 27 capture contains 516 matching component/PostDesired pairs: component horizontal projection -0.1641..-0.1630, native-follow output -0.2483..-0.0675. This identifies native spatial following after the component as the defect boundary.

## Global constraints

- Next delivered changed build is 0.28.28. Published 0.28.27 bytes remain unchanged.
- Preserve RE4/APP behavior and working RE5 shooting, knife and reload movement.
- No always-on diagnostics, new background worker, IPC layout change or network auto-start.
- Retain native collision and camera transitions; only authenticated ordinary Modern overview may own spatial retention.
- Pair every new consumer contract and hook address with exact latest/legacy guards; update independent overlay identity fixture to the new guard count.

## Review focus

- Stale frame/epoch/generation/release, changed component/default view, scope, first-person, frozen actor and settings changes must leave native follow untouched.
- Tiny spatial changes must not be lost to the native settling threshold; pending native one-shot requests keep native ownership.
- Restore static coefficient bits and the native computed dynamic alpha, without changing FOV/roll smoothing or reimplementing its float arithmetic.
- Both native follow branches, both builds and both player slots must be exercised through installed callbacks, not direct desired-pose copies.
- Exit/reset and native wall clipping must retain their ownership; no old pre-collision pose can be restored afterward.

### Task 1: Qualify native consumer and failing regression

Files: private replay/evidence; tests/re5_module_tests.cpp; tests/re5_runtime_follow_fixture.hpp.

- [x] Capture and compare actual component/PostDesired projection pairs; stop recording after user actions.
- [x] Qualify complete original native follow, threshold/reset, dynamic-alpha evolution and exit for both builds.
- [x] Add a callback-driven native-follow model showing 27 drift; run it and retain expected RED evidence.

### Task 2: Scoped native follow correction

Files: source/re5/camera_follow.inc; source/re5/runtime.inc; source/re5/camera_pose.inc; include/re5/profile.hpp; include/re5/profile_legacy.inc; tests/re5_game_link_tests.cpp.

- [x] Publish/consume one authenticated successful component token, including SamePose; invalidate on a new desired production.
- [x] Temporary zero actor-basis origin retention, capture native arm-alpha bits before its spatial use and restore after native follow; zero only the world branch spatial scalar.
- [x] Request a native one-shot zero threshold only when no native request is pending; restore its saved threshold after exactly one original invocation.
- [x] Add paired exact consumer guards and mappings; keep unsupported images disabled.
- [x] Pass installed callback regressions and existing native/legacy camera/gait tests.

### Task 3: Candidate integration and review

Files: include/re4/version.hpp; current-version assertions/diagnostic allowlist; CHANGELOG.md; docs/re5-controls.md.

- [x] Bump 28 and document physical verification pending.
- [x] Independent review, protected movement/source checks, full host suite, module/overlay/NRO and public audit.
- [ ] Create candidate 28 and verify its version/bytes; only then request local server, send BRIDGE/GAME without RUN, and ask for full RE5 install/restart plus overlay reload.
- [ ] Physical acceptance of stable framing, vertical stick, working combat movement and wall behavior before publication.

## Verification ledger

- Task 1: actual 516-pair capture and original-instruction replay qualify the defect after component publication. Native-alpha variant: both builds, 40 motion/FOV cases plus four reset/exit sequences passed. Installed-callback RED in both builds: frame1 horizontal projection -0.29166667 desired versus -0.29202201 native.
- Task 2: installed callback fixtures pass both builds and both players, 30/60 fps, both follow branches, tiny movement, native alpha/world bookkeeping, 33 exclusions and downstream collision ownership.
- Final review: two P2 findings reproduced RED and fixed GREEN: same-camera failed freshness consumes its lease while foreign cameras preserve it; pure FOV/roll changes retain complete native state and native admission. No changed delivered bytes yet.
- Version metadata verification: initial full run 141/142; game_link_tests exposed an omitted diagnostic version-parser upper bound. Increase to28 with29 still rejected; final run-efc49s5j passed142/142; SDK/module/overlay/NRO, package/release validation and public audit passed.
- Native call-tree qualification finds no recursive Follow path. Simultaneous same-object native Follow is outside the replay; both real player slots have separate nonblocking locks. Hardware framing, wall contact, split-screen and cost remain physically unverified.

- Final candidate: build/releases/v0.28.28/re4-controls.nro, SHA256 723d324954468317aeb74fecc53257edfb888d6613c2a7672f5028b5d31f6c97. Source/ARM builds0, full host142/142, public source/history/release audit0. Local server requested; no28 delivery or physical acceptance yet.
