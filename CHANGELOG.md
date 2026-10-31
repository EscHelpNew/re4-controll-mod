# Changelog

## 0.13.0 (unreleased, controls and gyro polish)

- Put the controls profile first, cycling with A or Left/Right between Default,
  RE6 and Custom. Default restores original controls without gyro; legacy RE9
  settings remain Custom. Preserve separate graphics and calibration settings.
- Present filtered gyro displacement on the game frame clock with10ms delay,
  removing uneven6/7-sample batch steps and flushing old motion on session,
  controller, pause and gain changes.
- Seed the verified generic firearm's original raising animation with the
  retained camera pitch, avoiding the late pitch handoff in native mode0.
- Limit right-stick vertical look to ±35° without restricting gyro's native
  range or snapping angles created by gyro.
- Add walking during ordinary generic Leon reloads, enabled in RE6. Preserve
  native collision, root movement budget and weapon animation/readiness.
- Migrate settings1..12 to wire13 with reload movement off; use shared ABI17
  with the same400-byte layout. Update module and overlay together.
- Baseline RE4 recording:1495 valid samples/34.43s, zero drops; sustained yaw
  approximately171°/s overview and56°/s aim, close to the RE6 reference.

## 0.12.0 (unreleased, RE6 controls profile)

- Add a separate RE6 controls preset to the installer and overlay.
- Use reference-based yaw rates of171.6degrees/s for overview and57.3 for aiming,
  a slower220ms aiming rise and47ms braking. Overview rise remains50ms; aiming
  pitch is provisionally53degrees/s, overview pitch90degrees/s.
- Give the RE6 response independent overview and right-stick aiming gains,
  including immediate zero-gain cancellation and safe route/profile transitions.
- Preserve existing Default/RE9 behavior and scoped Custom snapshots.
- Migrate settings1..11 to wire12 with classic response; use shared ABI16 without
  changing the400-byte layout. Install the new module and overlay together.
- RE6 capture on console is now verified:1437 valid samples without lost records.
  Matching the new preset inside RE4 remains a hardware validation step.

## 0.11.2 (unreleased, capture buffer discovery fix)

- Identify the public telemetry ring by its committed slot sequences, rather
  than accepting the producer's private copy of its header. The wrong address
  caused capture to stop with zero records and an incomplete `.csv.part` file.
- Add whole-producer-memory regression tests, including wrapped buffers and
  compatibility with the already-installed 0.11.0 game modules.
- The fix requires the updated telemetry overlay; native RE6 behavior and the
  existing game module remain unchanged.

## 0.11.1 (unreleased, capture diagnostics)

- Persist the telemetry overlay's target/build identity, capture phase, IPC result
  and recorder failure, including failures before a game journal is found.
- Generate bounded own-app support reports with installed-game metadata and
  diagnostic file inventories; download them through the existing authenticated
  read-only service. Credential files are excluded.
- Keep RE6 metadata at the start of the legacy truncated inspection report.
- This build gathers the failed-capture evidence; it does not claim the
  underlying console capture error has been diagnosed or fixed.

## 0.11.0 (unreleased, measurement instrumentation)

- Add a Build-ID/SDK/instruction-guarded read-only RE6 observer for update v131072.
- Capture exact original input and separate poll/frame timestamps in both RE4 and RE6.
- Add a bilingual telemetry overlay with tail-only Start, finite Stop snapshot,
  buffered SD persistence, drop counters and explicit capture failures.
- Keep recording while hidden; join and flush the writer before overlay unload.
- Download completed CSVs through the own installer with local authentication,
  bounded fixed-folder access and streamed SHA256 validation.
- Add native-time motion analysis and a repeatable measurement protocol.
- Actual console recordings and control-feel fitting are pending; no measured
  RE6 response or transferred calibration is claimed by this build.

## 0.10.0 (unreleased, RE6 measurement preparation)

- Add L/R game pages with a violet RE6 emblem and four RE6 home actions.
- Share language while preserving each page's selection and RE4 settings.
- Isolate RE6 installation paths and receipts; reject missing or incomplete payloads.
- Remove redundant home/description headings and use consistent16px panel spacing.
- Prepare bounded telemetry records and log encoding; RE6 native hooks await verified complete game files.

## 0.9.1 (unreleased)

- Ease unified stick velocity into motion, brake promptly and integrate by time for consistent frame-rate response.
- Limit stick-driven vertical look to50degrees each way without snapping gyro angles back into that band.
- Keep camera orientation and profile pitch independent of animated weapon sway and clipped laser endpoints.
- Project the translucent reticle onto actual displayed laser aim; it may move off-center while the view remains steady.
- Bring the RE9 shoulder camera closer at1.15; native Default remains1.00.
- Keep native shot/laser calculations and first-person weapon animations; this is not a replacement first-person viewmodel.

## 0.9.0 (unreleased)

- Add opt-in unified sticks, enabled by the RE9 profile: left-stick movement and right-stick turn/look with one sensitivity during walking and aiming.
- Restore original A/B/X/Y and PLUS/MINUS actions in the RE9 preset; retain the existing D-pad mapping.
- Keep native motion and collision budgets while redirecting movement, and drain look input across inactive contexts.
- Center supported firearm aiming on the fresh native weapon ray before generating the camera frustum.
- Draw the custom reticle at half opacity, including both live previews.
- Migrate settings to wirev11/shared ABI15; older saved profiles retain classic controls until explicitly changed.

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
