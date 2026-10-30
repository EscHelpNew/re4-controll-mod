# Changelog

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
