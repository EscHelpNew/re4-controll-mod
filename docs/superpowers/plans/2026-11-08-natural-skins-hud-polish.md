# Natural skins and HUD polish implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Correct Krauser's blade direction, gauge-only spare-ammo visibility,
natural cosmetic proportions, the optional extra attack square and health-panel
character names while preserving the accepted0.26.8 controls/camera/case rotation.

**Architecture:** Keep the native gameplay skeleton and resources intact. Build
donor proportions and attachment transforms in owned frame slots; reuse the
existing character hooks. Add one shared native sprite callback hook for the
attack square and health-word capture, using existing late HUD native glyphs.

**Tech stack:** C++17/20, exlaunch/libnx, authenticated native AArch64 evidence,
actual callback host fixtures and the existing Switch Debug Kit delivery client.

- [x] Reproduce the missing gauge render-layer bit16 gate with the actual HUD
  callback. Patch only ammo_counter.inc and its fixture;39-word native proof.
- [x] Replace stale attack-square overlay with a native0x170 sprite copy beside
  the selected row's last square; native gray/red templates and original fade.
  Preserve upgrade ownership, save/checkpoint and sidecar semantics.
- [x] Prove original vs Krauser knife direction using authenticated assets.
  Prepare its grasp pivot from raw BIN geometry and construct a proper posed
  half-turn. Snapshot per frame and compose only owned blade transforms.
- [x] Replace Leon-length hybrid fitting with strict donor limb FK, retaining
  native torso/pelvis translation residuals. Test all donor rest meshes and
  synthetic poses; preserve current gameplay root/camera/collision data.
- [x] Prepare bounded rigid and weighted held-weapon descriptor snapshots during body Skin,
  before drawing; use actual owner/root-parent bone10/16/17 across both slots.
  Reuse slot consumer/GPU retirement fences. Unsupported paths remain native.
  Keep ordinary weapon size as explicitly confirmed by the user.
- [x] Capture/suppress only the separate transparent native health-word sprite
  and draw the confirmed applied cosmetic name in the same frame's late HUD.
  Prove native queue ordering. Preflight complete native glyph packets first;
  use strictly validated Russian mappings or canonical Latin in the native font.
- [x] Integrate shared290F00 hook, guard manifests and test registrations;
  keep SharedABI23/488 and wire19, with61 hooks in61 slots.
- [x] Run focused regressions, independent review, full build.py --test and
  module/overlay/NRO builds against a frozen source snapshot. Validate relocated
  runtime profile, embedded module/overlay pairing and previous frozen versions.
- [ ] Audit/freeze0.27.0 separately and transfer the exact NRO to the authorized
  console. Confirm blade direction, HUD hiding, names, extra square and natural
  proportions visually; do not claim host fixtures prove console rendering.

User explicitly requested parallel work. Ownership is split among natural rig,
health HUD, square/knife helper agents; root owns shared integration/profile/
version/build/package/review/delivery. CONTRIBUTING requires the root checkout.
No public release or commit is requested for this iteration.

## Added diagnostic mode requirement

The user also requested an explicit Launch mode selector with Normal/Diagnostics values. Default to
normal, with no diagnostic TCP server or automatic diagnostic log writes. Enable
the existing authenticated read-only service only in diagnostic mode; switching
back must stop it and release its resources, including when the UI is hidden.
Persist the frontend preference with invalid/missing values defaulting to normal.
Keep ordinary game controls and the standalone update transport operational.

- [x] Add preference, menu selection and lifecycle regression fixtures.
- [x] Independently review enable/disable, logging gates and persistence.
- [x] Refresh the source snapshot and rerun full tests/app build before freezing.

0.27.0 frozen and transferred: exact NRO SHA256 and new process0.27.0-p94 verified.
Installer action and in-game visual acceptance remain pending. Launch mode defaults
to Normal; automatic diagnostic capture is intentionally not started.
