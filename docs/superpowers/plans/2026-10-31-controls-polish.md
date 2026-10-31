# Controls menu and motion polish plan

**Goal:** three inline profiles, smoother gyro/camera entry, bounded right-stick
vertical look, and guarded movement during ordinary reloads.

**User requirements:** Default restores original controls without gyro; RE6 is
the measured response preset; Custom stores manual settings. Profile is first
in Controls and changes with A/Left/Right. Remove RE9. Fix the visible weapon
raise snap and gyro animation jitter. Allow walking during reload.

**Evidence and design:** current RE4 report has1495 valid samples in34.427s,
without losses. Existing gyro callback reproduces6/7-sample displacement pulses
on a uniform30Hz frame clock; use bounded cumulative-angle interpolation with
10ms presentation delay while retaining the existing10ms sensor endpoint mean.
Five-second averaging would add unacceptable aiming latency. Entry native pitch
handoff is investigated separately from sample cadence. Native reload proof is
restricted to ordinary Leon/generic handler31CC60, routine6 phase4 subphase0..3,
independent of held aim; no blanket admission of other weapon handlers.

- [x] TDD three-profile recipes, native Default, legacy RE9 as Custom, inline UI.
- [x] Reproduce gyro sample/frame timing mismatch; TDD portable interpolator.
- [x] Add explicit sensor emitted duration and integrate frame interpolator with session/gain resets.
- [x] Reproduce and fix native aim-entry handoff continuity with callback tests.
- [x] Bound right-stick vertical angle to35degrees without snapping gyro-created angles.
- [x] Add a reload movement toggle, wire13/ABI17, and narrow native reload locomotion/camera guards.
- [ ] Full tests/ARM/SDL, fresh review, credential audit, main/CI and own-app library delivery.
- [ ] Console acceptance and new comparison journal if needed.

Keep all files in this repo, retain native weapon/shot logic and collision/root
budget, preserve personal credentials and private logs outside Git. No GitHub
release or tag. Tests first; no new offset without verified original code.
