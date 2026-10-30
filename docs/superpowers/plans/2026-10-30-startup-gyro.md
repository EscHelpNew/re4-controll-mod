# Restore persisted gyro at startup

> Execute inline with superpowers:executing-plans and test-driven-development.

**Goal:** Saved gyro enable and sensitivity work after restarting RE4 without toggling a menu item.
**Architecture:** Keep the existing settings file and IPC format. Read settings with bounded startup retries, give live IPC priority, and apply saved settings when opening the overlay. Add startup diagnostics to a versioned shared block.
**Tech:** C++17/game SDK5, libnx overlay, Python tests.

## Evidence and constraints
- The game marks its only SD attempt complete before mounting succeeds.
- The overlay loads saved values for display but only sends them on user input.
- The profile has only debug FS permission (bit62); explicit SD access is bit21 in Atmosphere's access-control definitions.
- No per-frame filesystem polling, no sleeps, no default enable forced over saved Off.
- A late startup read must never replace newer live settings.
- Preserve own-file installation receipts and all user credentials outside Git/releases.

## Tasks
- [x] RED/GREEN actual game adapter tests: saved On/Off, backup, delayed mount, bounded exhaustion, live precedence, sensor activation.
- [x] RED/GREEN actual overlay startup: send saved settings without a toggle, no save on open, no defaults on SD failure.
- [x] Test the game's ordinary SD access permission and generated NPDM flags.
- [x] Fix the CI forwarder path guard for resolved Windows short/long aliases, with a regression.
- [ ] Full host suite and ARM builds, audit, commit to main; deploy 0.4.2 through the pinned library.
- [ ] Hardware acceptance after a full RE4 restart. User requested publication first and will update from the release.

26 Python tests and 19 native test programs pass, including regressions that failed before the fix. Module, overlay and NRO cross-builds pass. Release assets were audited against known local credentials with no findings. Console acceptance of the cold-start fix remains pending.

Permission reference: https://github.com/Atmosphere-NX/Atmosphere/blob/master/libraries/libstratosphere/include/stratosphere/fssrv/impl/fssrv_access_control_bits.hpp (SdCard bit21; debug bit62). The generated NPDM was independently inspected with hactool and has 0x4000000000200000.
