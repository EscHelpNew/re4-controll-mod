# Button remapping and free camera

User requests: default sensitivities/speeds1.00 except gyro aiming speed0.80;
remap every game-visible controller button throughout the game including menus
and QTE; adjustable camera return speed and a right-stick360-degree free camera
without automatic return. Keep stick deadzones0.05 and gyro deadzone0.50deg/s.
Proceed in the current root repository, with tests and GitHub Actions. Preserve
the existing English/Russian interfaces, additive firearm gyro and native speed caps.

## Approach

Use game-local SDK pad hooks for a physical-button-to-game-button mapping. This
preserves the physical overlay controls and permits recovery even if a user
disables important game buttons. System HOME/Capture do not reach game HID state.
Keep the target options to a single button or unbound; permit duplicates, swaps
and cycles by transforming the original simultaneous state exactly once.
Expose the20physical game buttons: ABXY, stick presses, shoulders/triggers,
PLUS/MINUS, D-pad, left/right Joy-Con SL/SR. Unknown/virtual-stick bits survive.
No global controller or sysmodule patches. Default mapping is identity.

Add a dedicated bilingual Button mapping page to both installer and overlay.
Overlay A opens the target list; arrows also cycle targets. Include mapping
enable and restore-default-mapping controls. Opening/hovering does not save.

The current64-byte v1-v6 settings file is full. Extend current v7 to128bytes,
with an exact-length checksum, reserved-byte validation and migration from
all supported old versions. Preserve saved values; new/default/reset gyro master
speed is0.80. Increase the shared ABI and update module+overlay together.

## Camera

Native19B20 updates pitch/yaw float offsets and calls18520 to consume them for
camera vectors before collision handling. Add a guarded, narrowly scoped18520
trampoline. Native mode at return speed1 is unchanged. When both shaped input
bytes are neutral, use old offset*pow(0.9,return_speed), range0..1. Zero holds
the native bounded look; a separate Free camera toggle integrates stick input
at up to120deg/s yaw and1.5normalized pitch units/s, scaled by camera sensitivity.
These rates are our defaults, not measured native speed limits. Clamp pitch to
[-1,1], wrap yaw and use monotonic time with no accumulation across pauses.

Only affect the known active camera, ordinary player movement, unpaused input,
native non-aim path and non-reset frames. Exclude aiming, scripted scenes and
camera changes; clear continuity on exclusions/resume gaps/external state resets.
Preserve native collision and output smoothing. Native aim entry consumes camera
yaw to turn toward the view; allow that native finite-angle handoff, and test
the180degree boundary and cleared offsets. Hardware acceptance of aim entry from
the front remains required before claiming all camera transitions verified.

## Alternatives and tradeoffs

Per-action engine binding would need many unrelated context-specific hooks and
could miss menus/QTE. SDK-state mapping covers all those contexts consistently.
Changing render output after camera calculation would miss same-frame consumers
and collision handling; adjust the existing offset fields before18520 instead.
A render-only orbit would avoid extending aim-entry turns, but would make aiming
snap away from the visible direction. Preserve the native aim-to-view mechanism.

## Validation

Core mapping tests cover identity, all20buttons, unknown bits, unbound, duplicate
targets, simultaneous swaps/cycles and migration/corrupt files. Actual pad-hook
tests cover every SDK style and menu/QTE states. Camera tests exercise the real
callbacks before native camera consumers, alternative input routes, zero stock
changes, return factors, continuous yaw, pitch limits, frame-rate independence,
reset/pause/aim gates, native aim-entry clearing and inversion. UI tests cover
target selection, EN/RU, persistence/reset and physical recovery controls. Full
host tests, ARM builds, hook ABI and source/history/release audits must pass.
