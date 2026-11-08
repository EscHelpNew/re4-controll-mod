# Natural skins console regression follow-up

The console report for0.27.0 identifies displaced weapons, unavailable cosmetic
switching, incorrect framing of short characters and high health-panel names.
The user additionally requests height-relative camera framing, lifting short
case previews and first names only on the health panel. Keep ordinary weapon size
and the opt-in launch-mode behavior.

- [x] Reproduce the discovery failure:0.27.0 puts the shared block at data+0x34B48,
  beyond the overlay's0x20000 bounded scan.0.26.8 used+0xA148. Actual linker fixture
  fails before pinning the IPC input section first and passes afterward.
- [x] Guard the staged linker revision and verify the final NSO starts writable
  data with SharedABI23. Keep upstream vendor files and the scan bound unchanged.
- [x] Reproduce and fix held resource provenance/native matrix admission using
  authentic native class ordering. Preserve fenced owned snapshots and weapon size.
- [x] Use the donor/Leon neutral head-pivot height difference for camera eye and
  target together, and the opposite displacement for short case previews.
- [x] Match native health-name projection, including captured viewport adjustment;
  retain only Luis/Ramon for the two composed names in both languages.
- [x] Run actual callback fixtures, independent review, full tests and ARM builds;
  freeze a separate0.27.1, preserve0.27.0 and verify payload pairing before transfer.
- [x] Request a new console test only after the candidate is ready. Game may be
  closed during implementation. Automatic diagnostic capture requires user opt-in.

Parallel ownership: root discovery/linker, CameraApply integration and build;
natural_height_research held geometry and owned appearance draw paths;
aim_extremes_native height helper/projection/callback fixtures;
ammo_health_visibility native health-name geometry and first-name selection.

0.27.1 was delivered with verified paired payloads. The user confirmed improved
weapon placement, framing and appearance switching, and correct first-name
placement. Follow-up work for 0.27.2:

- [ ] Follow the actual health-arc draw in the current frame for both cosmetic
  names and spare-ammo count; cached plate/digit records cannot authorize them.
- [ ] Derive the current donor grip displacement from authenticated native bone
  worlds. Apply it to the qualified laser, ordinary shot, muzzle-particle and
  shell producers/consumers without changing native bones or weapon dimensions.
- [ ] Cover current-pose changes, scale/shear, retained particle lifetimes,
  malformed/stale ownership and native fallback with actual callback fixtures.
- [ ] Complete the parallel coverage plan, independent review and new paired
  module/overlay/NRO validation; preserve every frozen earlier release.
