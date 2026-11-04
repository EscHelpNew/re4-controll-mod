# Gyro debug and configured deadzones ? local validation 0.17.0

Date: 2026-11-04. Base commit `bfe9042cc2166beac3a330b01e1c3b7120e2a104` plus the existing 0.17.0 artwork/preset and current gyro working-tree changes. The version remains unreleased 0.17.0. No console deployment, commit, push or GitHub publication was performed.

## Final behavior

- Controller diagnostics display left/right configured deadzones numerically and as dim red axial hatching on each of four bars.
- Press ZL in Diagnostics, or choose Debug mode in the application's Gyro settings, to enter the same static-scene test. Releasing ZL does not leave it.
- Eight list rows: seven current gyro settings and Save and apply. No point-reset menu entry.
- In GyroTest: Y centers the point at zero angular position (810,350), clearing the filter/interpolation queue; X calibrates; B cancels the draft. The ordinary Diagnostics X/Y bindings remain unchanged.
- Changes and calibration affect a draft used by live preview. Saving validates and persists it, then returns to the exact origin/selection only on success. Save errors leave the test and active configuration intact. The existing profile sidecars remain individually recoverable rather than one multi-file transaction.
- Raw rotations/second and bounded accumulated preview radians remain in the lower-left. The red point uses the existing game GyroFilter and GyroFrame with current gains, speed, inversion, deadzone and bias. The static scene's pixel projection is illustrative rather than a reconstruction of game-world FOV.
- The image is a separately attributed ImageGen edit of the existing official RE4 frame, removing the baked laser and dot. Original RE4/RE6 JPEGs and thumbnail identity schema remain unchanged.

## Checks actually executed

1. Full root `build.py --test`: exit 0, 62 Python tests plus the full native/actual callback/UI/SDL suite. This run preceded the final X/Y and list adjustment; log `build/tests-0.17.0-gyro.log`.
2. After the final adjustment, root recompiled and ran the affected installer model and real SDL renderer tests with project compiler flags: exit 0, log `build/tests-0.17.0-gyro-final.log`. These verify actual x_shortcut/y_shortcut and handle actions inside/outside GyroTest, no configuration mutation on Y, save/cancel/errors, eight rows, EN/RU rendering, transparency, dim zone geometry, point movement/bounds/NaN and calibration/no-sensor status.
3. Preview math synthetic comparisons use fixed 200Hz input over four seconds at 30/60/120Hz frame cadence, comparing GyroFilter/GyroFrame results and exercising gain/bias/deadzone/inversion, zero gain, disabled input, repeated/older samples, NaN, interval/wall gaps, center resets and immediate reversal at bounds. These establish behavior, not speed or real HID acceptance.
4. Final `build.py --nro`: exit 0; ARM64 application linked and fresh background generation ran. Log `build/build-0.17.0-gyro-final.log`. Game module and overlay sources are unchanged by this gyro task; their already-built 0.17.0 binaries from the earlier successful module/overlay build are bundled, not claimed as newly rebuilt here.
5. Root viewed final `build/installer-gyro-test-ru-apply.png` and Diagnostics frame; EN/RU first/apply/no-sensor/calibration frames and pixel probes were checked by the visual implementer. Final review found no substantive defects. Hardware font/input behavior remains unverified.
6. `tools/package.py`, `tools/release.py` and the source/artifact secret audit passed. SD ZIP launch files equal the final NRO byte-for-byte; its own forwarder matches the unique prepared HOME package. NRO application identity/version validation passed. Prior preset-only package c03144b9 is retained under `build/previous/v0.17.0-presets-c03144b9/`.
7. Final diff reviewed; `git diff --check` passed. No generated header or private input is added to source control.

## Artifact identities

- `build/re4-controls.nro`: `3ea14d10d67008f013e1e0e8c0abc28e8f69f16a582beb08131860fc0d15ce66`
- `build/re4-controls.elf`: `d7edaa71e2176f369d08220b9163954a43da2e768e581e3c19070d46593b01ef`
- `build/graphics/gyro_test_background_image.hpp`: `fd80ae8c0578ff1788e3003f29275dce1874835836b9171a1a1046c76b78675c`
- `include/re4/gyro_preview.hpp`: `06f6c4015b5c5c10091166246cf9e462866c9664044a2c152f47ffa17fb11bee`
- `assets/graphics/gyro-test-background.png`: `bb548643949ea97a0570ef2619f9036d16a97d4f90ce106a5e3630369527c127`

## Limits and remaining checks

No Switch execution or hardware acceptance of this build is claimed. Check real HID batching, handheld/Joy-Con/Pro reconnects, motion direction/comfort, live adjustment, X calibration, Y reset, both entry points, persistence after reopening and real SD write failure. Host fixtures do not simulate every sensor/OS timing or storage failure; existing sidecar transactions are not globally atomic. A dedicated SDL capture of a save failure and every diagonal raw-input combination was not added; model save-failure tests, per-axis band probes and existing input tests cover their respective components, not full hardware UI acceptance.
