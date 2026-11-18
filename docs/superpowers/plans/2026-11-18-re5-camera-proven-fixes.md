# RE5 camera27 proven-failure corrections implementation plan

> **For agentic workers:** Use superpowers:executing-plans for root integration; native implementation has explicit separate file ownership. Track steps with checkboxes.

**Goal:** Correct the observed180-degree overview reversal and translation-triggered camera dropouts while retaining working combat movement.

**Architecture:** Keep the existing component-before-desired/collision owner and input pitch pipeline. Correct only the actor-forward sign and the definition of body rotation commitment: exact stable complete current world reads, exact expected orientation/homogeneous words, current finite native translation for anchoring. No new hooks or input algorithms.

**Tech Stack:** C++17/20, devkitA64, existing native callback tests, bounded opt-in camera ABI1 observation.

**Spec:** User requires camera and character turning together, held character composition, useful right-Y, all working shooting/knife/reload movement preserved for both local players. Evidence: `.superpowers/sdd/2026-11-17-re5-camera26/camera26-live-long-20261118T025145Z.jsonl` (completed210snapshots). Earlier idle capture independently proves positive native direction and negative custom direction.

## Global constraints

- Working gait files and runtime movement code remain byte-identical to25; RE4/APP17protected sources unchanged.
- Core IPC384/112/192 plus optional5808@688, ABI1, guards924/hooks49 unchanged.
- Ordinary actor ownership, unit scale, parent/worldmode, generation/frame/epoch/release and double-read stability guards remain.
- No extra pitch/inversion/smoothing algorithm changes: rawRY and accepted pitch are present; successful component elevation matches requested pitch, post-desired tracks through native smoothing.
- Actual19 rejection records show only translation words12/13/14 differing with matching frame/epoch/generation. Some differences exceed one ULP due native movement/ground resolution. Exact orientation words0..11 and homogeneous15 must remain committed; both current world reads must still match all16words.
- Version0.28.27 local candidate only. Camera hardware acceptance pending until physically checked.

## Review focus

- Stale or changing orientation must still reject even if translation is allowed.
- Torn current reads, wrong owner/mode/generation/epoch/release must still reject.
- Nonfinite/out-of-range matrices/anchors must not reach writes.
- Both players/builds, neutral/vertical input, moving collision translation and frame composition must be exercised independently of implementation.
- Failed-camera paths must not corrupt working gait or native desired/history/collision ownership.

### Task1: Native regression and minimal corrections (native owner)

Files: source/re5/shoulder.inc; tests/re5_module_tests.cpp. Optional small pure helper only if clarity warrants it; runtime movement/profile untouched.

- [x] Add actualcallback regression using native optical+actorWorldZ frame; show current negative sign fails, then pass positive actorWorldZ.
- [x] Add independent hardware translation cases (ULP and ground correction) showing current full expected/current comparison fails.
- [x] Accept current finite translation while checking exact committed orientation/homogeneous fields and complete double-read stability; retain other guards and record reasons.
- [x] Verify native callback root anchoring and independent optical/vertical/composition assertions for both players/builds; preserve rejection tests using actual orientation changes.
- [x] Run focused native/legacy/camera/probe/gait tests and freeze owned files for independent review.

### Task2: Metadata, evidence and integration (root)

Files: include/re4/version.hpp; tests/settings_v7_tests.cpp; source/overlay/game_link.cpp optional known-version cap; tests/game_link_tests.cpp; CHANGELOG.md; docs/re5-controls.md.

- [x] Save a bounded derived evidence report with exact nonzeroRY, component/post pitch and rejected matrix indices; no claim that absent live frames were recorded.
- [x] Bump27, allow known diagnostic27, retain future28 negative coverage.
- [x] Document precise fixes, working movement preservation and hardware validation limits.
- [x] Obtain fresh independent review; check protected hashes, full host tests and ARM module/overlay/NRO builds.
- [x] Package/release locally and public-file audit, record exact sizes/SHA and build proof.
- [ ] Request user Local Update only after ready artifact; PUT BRIDGE+GAME, never RUN; manual fullRE5install/restart/overlayreload, then physical acceptance.

Root verification: host142/142 (build/tests/run-gnpj3nct/results.json), SDK/module/overlay/NRO, package/release/audit all0; exact source freeze and ARM384/112/192/5808 layout verified. Artifact verification27.json. APP local-update ConnectionRefused; user requested to enable server. No27transfer or hardware acceptance yet.
