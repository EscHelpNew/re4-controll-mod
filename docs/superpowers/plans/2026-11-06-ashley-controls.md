# Playable Ashley controls and shoulder camera

> **For agentic workers:** Use superpowers:executing-plans with test-driven-development and final independent review.

**Goal:** Give playable Ashley the same left-stick movement, right-stick heading/look and rear shoulder camera as Leon in the RE6/unified controls profile.

**Architecture:** Reuse the verified common player update, movement dispatch and native root redirection. Extend the existing shoulder rig to Ashley's ordinary locomotion only. Bind unified look ownership to actor and character, preserving the game's original collision, motion budget, special actions and scripted cameras.

**Spec:** User request, 2026-11-06; CONTRIBUTING.md; docs/research/view-and-movement.md.

**Tech stack:** C++20/exlaunch, actual-callback host fixtures, hash-verified local NSO/RomFS research.

## Constraints and evidence

- Keep previous local0.18.0 reload changes and work in the root checkout.
- Native311400/common mode dispatch calls30D430 for ordinary locomotion; actor root/yaw are shared. Camera18360 selects Ashley's centered overview profile through character503C=1.
- Local NSO passed segment hashes and equals the existing analysis image; Build ID matches profile.hpp.
- Hash-verified pl01/pl11 skeletons have141 bones and share the exact checked parents of bones0..25. Existing leg-root17 and branches18..25 can be reused during native walking without enabling firearm/knife/reload overlays for Ashley.
- Shoulder camera uses Leon's existing arm, native FOV/roll/collision anchor and the single native wall trace; do not change Default/classic controls or other characters.
- No console deployment or publication requested. Hardware visual acceptance remains separate from host verification.

## Review focus

- Character switches at the same actor address must drain previous look velocity and pitch.
- Ashley strafing/backward/diagonal motion must preserve native displacement and animation timing.
- Only the active player is controlled; accompanying Ashley and other entities are unaffected.
- Pause, damage, interaction, scripted camera and control-option changes preserve native ownership.
- Existing Leon/reload/first-person behavior and unsupported-character camera fallbacks must remain intact.

## Tasks

1. Camera and control ownership — source/game/module.cpp, tests/game_module_tests.cpp.
   - [x] Add actual-callback shoulder camera tests for Ashley; observe centered-camera failure.
   - [x] Add character-switch look regression; observe inherited pitch/velocity failure.
   - [x] Extend shoulder eligibility for Ashley ordinary movement; bind look to actor/character.
2. Movement verification — tests/game_module_tests.cpp, tests/modern_move_tests.cpp, topology comment.
   - [x] Verify Ashley forward/back/run/strafe/diagonal, native root budget, once-only callbacks and leg orientation/restoration using the existing hooks.
   - [x] Cover neutral input, scene/active-player gates and native special-action fallback without broadening Leon-only weapon movement.
3. Delivery — version/docs/package metadata.
   - [x] Record local0.18.1, implementation evidence and enablement instructions.
   - [x] Run full host suite, ARM64 module/overlay/NRO build, independent review and public audit; package local artifacts.

## Evidence ledger

- Confirmed Build ID/segment hashes/full image equality against the supported
  v65536 image. Camera character table6EAB14 entry1 retains index2 and selects
  Ashley's overview; common player initialization311280..311288 selects30D430.
- Verified all covering RomFS SHA256 blocks for pl01/pl11:141 bones, exact
  parent0..25 match. No new native hook or Ashley weapon-state patch needed.
- Ashley shoulder callback test failed on centered native eye, then passed
  with character1 locomotion eligibility. Root-relative position, transformed
  camera basis, yaw, FOV/roll and native collision eye1 are checked.
- Same-address0→1/1→0 character-switch test failed with residual yaw/pitch,
  then passed after character-bound look reset. Live right stick works again
  immediately after the drained switch frame.
- Targeted real-module and movement suites passed. Ashley move cases preserve
  planar native budget7 and vertical4.5, native primary/secondary blending,
  partial-stick motion speed, original speed restoration and one root/pose.
- Ashley native-walk leg-pose/cursor restore passed with141-bone owner count;
  common active-player and camera scene checks retain their original scope.
- Full `python build.py --test` passed (62 Python cases plus all native
  callback/math/UI/renderer suites). ARM64 module/overlay/NRO build passed,
  including compiled hook ABI and unchanged30/40 trampoline budget.
- Independent review found no concrete important defect. Full141-bone skirt/leg
  appearance, wall obstruction and live crawl/interaction/chapter transitions
  remain hardware acceptance items; no hardware success is claimed.
- Local version0.18.1 package passed NRO identity/version, exact archive
  allowlist and bundled payload digest validation. NRO SHA256:
  `7d75cad1f68c13ffa7278fedfd3ea06d5bddf850bd1414e9812c24f10f5da588`.
- Public source/history/release audit:578 working files,1158 history blobs,
  no findings. `git diff --check` passed. No console deployment/publication.
