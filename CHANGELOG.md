# Changelog

## 0.8.0 (unreleased)

- Add a separate RE9-inspired controls profile and explicit Default / RE9 / Custom selection in both interfaces.
- Keep the last Custom controls when applying built-in profiles; graphics, language and calibration remain independent.
- Add third-person camera distance1.00–2.00, default1.00; RE9 uses1.35 to give Leon more room in the frame.
- Extend the native shoulder eye before wall collision while keeping its anchor, view direction, FOV and roll; first person and scopes retain their own views.
- Combine free360 camera, moving aim/knife, gyro and a crosshair with RE9-inspired physical buttons using RE4's default control layout.
- Document shared native-button and unsupported-action limitations rather than promising RE9 mechanics.
- Migrate old settings into wirev10 without changing their framing; use shared ABI14 with the same400-byte block.

## 0.7.3 (unreleased)

- Fix the native knife-held input mask used by movement and analog knife aiming.
- Test real held input0x800 in stance and strike, including gait/translation before hit processing and native collision.
- Reject unrelated high input bits and preserve knife-off, release and firearm behavior.

## 0.7.2 (unreleased)

- Discard gyro motion during native weapon raising, including the first ready FIFO batch.
- Keep gyro active through firing/recovery; invalidate old input on reload, weapon changes and paused contexts.
- Recheck readiness at aim consumption without arming from a mid-frame transition.
- Average readings10ms apart continuously at sensor cadence, avoiding alternating zero/10ms bursts.
- Preserve additive sticks, gyro direction, soft deadzone and the native manual-input sway baseline.

## 0.7.1 (unreleased)

- Group control settings into seven submenus in installer and overlay.
- Keep language selection on the main menu only.
- Show a live target-picture reticle preview for shape and size, including Off.
- Verify every preview shape against actual native HUD rectangles and rendered pixels.
- Include the startup hook-pool capacity and boundary fix confirmed on hardware.

## 0.7.0 (unreleased)

- Fix startup abort at hook22: enlarge the staged trampoline pool to40 slots,
  correct its last-slot bound and assert the actual install count at build time.

- Add independent Default/Custom profiles for controls and graphics, with automatic
  Custom snapshots, migration and recovery through the existing settings codec.
- Preserve the other domain, language and gyro calibration when switching profiles.
- Open Controls and Graphics as peers in the overlay root menu.
- Test actual profile UI events, restore and failed writes; fix persistent feedback.
- Add an optional first-person camera with continuous look and native scope/scene ownership.
- Add an optional dot/crosshair reticle using native HUD primitives and the displayed weapon ray.
- Add optional native-budget movement and masked leg animation during Leon's ordinary aiming/knife states.
- Keep native animation cursor banks isolated during private gait sampling and preserve IK/state restoration.
- Route the right stick to aiming independently of movement input and exclude knife stance from gyro aiming.
- Migrate settings v1-v8 into v9 with all new modes Off, shared ABI13 and diagnostic counters.

## 0.6.0 (unreleased)

- Add a bilingual Graphics menu with exact Native defaults and graphics-only reset.
- Add restart-latched bilinear/trilinear/AF2x-16x texture filtering without extra sampler-cache variants.
- Add restart-latched handheld1.25x SSAA (900p to720p) through the game's native allocation/scaler path; dock stays native.
- Add loaded-object culling distance0.50x-2.00x with native fog, projection and streaming unchanged.
- Add rendered-shadow Native/Off with native pass cleanup and flag restoration.
- Preserve controls/mapping/camera when migrating settings v1-v7 into v8.
- Publish render settings atomically, load saved settings after successful ROM mount, and expose graphics diagnostics in shared ABI12.

## 0.5.0

- Add complete 20-button game mapping to installer and overlay, including menus/QTE.
- Support duplicate targets, swaps, cycles, unbound buttons and mapping-only reset.
- Add camera return speed and continuous right-stick 360-degree free camera.
- Preserve native camera collision/smoothing and exclude aiming, pause/reset and scripted camera paths.
- Set new/reset gyro aiming speed to 0.80; other sensitivity/speed defaults remain 1.00.
- Migrate 64-byte settings v1-v6 into strict 128-byte v7 without resetting saved controls.
- Expand core, actual game callback, bilingual UI and rendering tests; bump shared ABI to 11.

## 0.4.2

- Restore saved gyro enable/sensitivity after restarting RE4 without toggling the overlay.
- Grant explicit SD access and retry startup reads at most eight times, one second apart.
- Apply saved settings when opening the overlay; live changes take priority over delayed reads.
- Add startup source/error diagnostics and regression coverage for saved Off, backup recovery and sensor activation.
- Fix the forwarder build path guard for canonical Windows short/long directory names in CI.

## 0.4.1

- Prepared the first public release, documentation and source/artifact audits.
- Build the NRO RomFS from a fixed allowlist, removing stale local payload files.
- Keep normal app and SD package builds independent of personal console keys.

## 0.4.0

- Added automatic own HOME shortcut installation and icon refresh.
- Added gyro aiming speed, a configurable angular deadzone and10ms two-sample averaging.
- Fixed wireless Joy-Con sensor selection and recovery after transient sensor failures.
- Process stick aiming before native reticle, animation and sway updates.
- Added separate right-stick aiming X/Y settings; reconnect reapplies saved settings.
- Added a bilingual Updates page for GitHub Releases with verified downloads and rollback.
- Added first-launch app image installation and protection against accidental downgrades.
- Expanded tests for actual game callbacks, installer transactions, OTA and public packaging.

## 0.3.0

- Added English/Russian menus, four stick response curves and a themed graphical installer.
- Removed the walking-speed slider; retained proportional movement at the native maximum.
- Set stick deadzones to0.05 and both gyro inversions to on by default.

## 0.2.2

- Confirmed proportional movement/camera/aim and additive firearm-only gyro on console.
- Fixed vertical gyro range, added focused-setting descriptions and native stick speed caps.
