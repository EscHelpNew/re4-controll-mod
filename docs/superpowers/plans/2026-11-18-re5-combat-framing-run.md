# RE5 combat framing and lateral run implementation plan

> **For agentic workers:** Use superpowers:executing-plans for root integration. Native camera, locomotion and callback tests have explicit separate ownership.

**Goal:** Hold Modern shoulder composition through firearm/knife/reload states and make lateral left-stick running move in the requested direction.

**Architecture:** One retained anatomical screen reference covers ordinary overview and supported combat. Preserve native combat optics, lens, upper pose and projectile inputs; constrain common eye/target translation at the existing final Desired hook before collision, using authenticated single-use component/follow provenance. Ordinary locomotion feeds native forward/back polar admission and redirects only its already-produced horizontal step budget; combat gait remains unchanged.

**Spec:** User reports28 is better in overview but character slides along the bottom during aim/fire/reload/knife; pure lateral run also fails. Actual28 capture contains49snapshots across actions05/07/08/1B/1C/1F/23/24. Paired component/PostDesired combat translation shifts reach25worldunits. Native analysis proves combat pitch-key geometry is not a fixed composition and native run steering is overridden by Modern right-owned body heading.

## Global constraints

- Next changed delivered build: 0.28.29; never change delivered28 bytes.
- Preserve scope/first-person/defaults and already working native combat movement, gyro, aim limits, projectile endpoints, collision and lower-body combat gait.
- No new worker, automatic network server/journal or IPC-layout change. Existing52hooks only; authenticate all new native contracts in both profiles.
- Ordinary left input keeps raw ControlFrame axes. Native buttons/action admission and speed remain native; forward/side sectors use native forward walk/run, back-dominant sectors use native backward admission to retain backward gait/speed.
- New composition uses stable actor-root plus cached anatomical height, current native direction/up/FOV/aspect. Preserve direction/up/lens; translate eye and target together once before collision, never restore a pre-wall pose afterward.

## Review focus

- A pure lateral native turn with zero step must become native forward/run admission; redirection alone is insufficient. No additive invented walk stride or diagonal speed gain.
- The native polar proxy must be paired with an authenticated same-frame redirect marker; stale/busy/missing route cases must not accidentally redirect combat or unrelated movement.
- Combat framing cannot require overview heading/look epoch, and reload's native input freeze is bypassed only for the explicitly qualified enabled reload state.
- Native partner camera component197, transitions, unsupported actors and changed owners must retain native ownership; retained references cannot cross an owner or generation.
- PostDesired framing preserves native quake/recoil orientation and lens, updates no actor/projectile state, and one-use provenance prevents a second correction after collision.

### Task1: Native qualification and tests

- [x] Record actual28 camera/input states and qualify native run action selection, root/optical combat construction, final lens/aspect, and relative-bank lifecycle in both original images.
- [x] Add real callback RED cases: combat native pose changes with constant wanted anatomical projection; actual follow/Desired/collision order; pure lateral native action admission followed by redirected step.

### Task2: Camera owner

Files: source/re5/camera_follow.inc; new source/re5/camera_framing.inc; include/re5/camera_pose.hpp if a pure optical helper is needed; shoulder.inc only to export its already-calculated anchor. Root wires runtime.inc CameraDistance/CameraRig changes from the camera owner's exact instructions.

- [x] Cache one root-relative anatomical height and screen reference from qualified overview, or first supported combat when no overview exists.
- [x] Combat distance uses anatomical-anchor dilation of both eye and target, with unchanged native direction/up/FOV. Publish a combat-qualified single-use lease even at distance1/SamePose.
- [x] Preserve native follow admission for pure lens/roll; admit actual spatial work. Record successful follow provenance and constrain common eye/target translation at final Desired before collision using actual native lens/aspect/up.
- [x] Preserve all existing28 one-use/failure/foreign-camera exclusions and native coefficient evolution; test defaults/scopes/FP/partner/script/transition/owner changes.

### Task3: Ordinary locomotion owner

Files: new source/re5/locomotion.inc (helper definitions); new tests/re5_runtime_locomotion_fixture.hpp. Root adds ControlFrame proxy marker, polar hook call, marker publication and proposal hook call in runtime.inc. Do not alter include/re5/gait.hpp or its fixture.

- [x] At owned ordinary Modern polar callback, use native forward angle0 for front/side and native backward angle180 degrees for back-dominant input, keep magnitude, neutral native right polar, retain actual raw frame axes/buttons.
- [x] Set/publish an explicit proxy-applied frame marker. At native proposal, revalidate owner/mode/frame and redirect proposal-minus-native-previous horizontal length along the raw camera-relative vector. Preserve Y/quaternion/actor position, speed, native animation and collision.
- [x] Exercise both adapters together, both builds/players, real recorded lateral/diagonal vectors, physical run admission, direction reversal, right-owned yaw and combat/default/exclusion cases.

### Task4: Root integration

- [x] Add paired exact native guards and independent new-count wire fixture; bump29/version assertions/diagnostic allowlist and current release notes.
- [x] Fresh independent review, full host suite, module/overlay/NRO, package validation and public audit.
- [ ] Prepare verified29 artifact, request Local update, PUT BRIDGE/GAME only. Manual app reopen/full RE5 install/game restart/overlay reload; physical combat-framing and lateral-run acceptance before publication.

## Verification ledger

- Actual capture completed49snapshots/83attempts across ordinary/run/firearm/reload/knife states. Raw PC-only record stopped after user HOME; no ongoing console journal.
- Camera RED on installed callbacks: action1B/distance1 final NDC -0.4074700/-0.2090480 versus retained -0.3960819/-0.2011845.
- Locomotion RED on both builds: pure-left input(-32739,-1338) remains a native turn sector before the forward/back proxy. Proposed step also remains forward before redirection. Coupled tests now reach the independent camera RED.
- Paired original-image guard merge:2529 old rows +578 new rows =3107 per profile, all words independently verified. Hook count52; SDK/core IPC layouts unchanged.
- Native replay independently passed both builds for translated follow-bank exit and final pre-collision composition/history ownership. No pre-wall pose restore is used by the new path.

- Final review fixes: nested/concurrent rejected input now suppresses outer scope and sequences per-player input epochs at entry and rejected exit; stale authorizations cannot republish or redirect. RED36 ordinary-effect checks per profile -> GREEN.
- Final camera completion is claimed atomically at the first Rig instruction before every fallible gate, with unique serial and camera identity. Rejected/busy first Rig cannot modify a clipped eye on a later call. Current settings and partner/skip/transition gates precede mutations. Camera/live-combat review RED -> GREEN for both players/builds.
- Preserve live combat gait on an ordinary-to-combat transition: treated as Important because the existing requested movement must not lose its first frame. Six RED movement cases -> GREEN.
- Host access violation was a fixture lifetime defect: freed combat action metadata remained in the fake MemoryInfo map. Ordinary fixture now supplies safe action ownership; normal latest/legacy runs pass outside a debugger.
- Fixture corrections: scoped generic polar math intentionally has no ordinary-input epoch, and the partner flag belongs to component+197, not camera+197. Real native-prefix epoch and partner exclusions remain tested.
- Final full host run-k3zp3307:142/142; SDK/module/overlay/NRO0. Gait core, actor heading and original overview geometry match28. No physical29 acceptance or delivery yet.

- Candidate29 prepared and package validated; SHA256962029f9009b58f96739146d900f690ae99518bb199958078cafd3a238777819. Source/history/archive audit0. Local server requested; publication awaits hardware acceptance.
