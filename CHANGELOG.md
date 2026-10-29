# Changelog

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
