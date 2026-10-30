# Unified stick control and centered translucent aim

The user rejected separate walking/aiming feel: R must turn/look with one
sensitivity in both modes, L must move; aiming changes framing rather than input
behavior. The reticle must be50% transparent and centered on the actual aim.
Correction: keep original A/B/X/Y and PLUS/MINUS, not just the D-pad.

## Design

- Opt-in `Settings::unified_controls` (RE9 On; legacy default Off), wirev11 bit3
  of byte97; older settings migrate Off. Shared ABI15, same128-byte settings and
 400-byte control block. Existing camera_gain is the one shared R sensitivity;
  right deadzone and curve are also shared. Classic per-mode aim gains remain
  available for legacy control and are explained as such.
- Integrate R once per player update using bounded monotonic time. Intended
  yaw120deg/s, pitch90deg/s at gain1; common normalized pitch uses native2/pi
  conversion. Discard resume/discontinuity/raise motion rather than catching up.
  Preserve native shot/recoil/scope/scene behavior and firearm-only gyro gating.
- R owns body/camera yaw in walking and aim. L normal movement redirects native
  root displacement before physics, retaining native gait/speed/Y and linear
  secondary-motion blending; no post-collision teleport or arbitrary speed.
  Existing moving gun/knife paths keep native-budget translation and pose masks.
- Center effective camera target on the fresh native clipped weapon ray at the
  final normal FDB0/279F70 look-at boundary, before frustum generation. A fixed
  HUD cross must never claim a different shot destination. Scope/menu/stale ray
  frames keep native behavior and hide an unqualified custom center marker.
- Draw alpha128 via verified native GX RGBA/source-alpha blend wrappers. The old
  opaque rect helper ignores packet alpha. Establish and restore the known HUD
  draw state; mirror translucency in reticle previews.
- RE9 factory restores A/B/X/Y, D-pad, PLUS/MINUS native identity. Native X opens
  map, PLUS inventory and MINUS pause. Preserve all unrelated Custom settings.

## Ownership and verification

- Native worker owns module.cpp, modern_move.inc/modern_move.hpp, new unified.hpp,
  unified_tests.cpp, game_module_tests.cpp and modern_move_tests.cpp. Keep actual
  InstallAtOffset calls in already counted module/modern files (pool40 slots).
- Alpha worker owns only reticle_draw.hpp and reticle_draw_tests.cpp; root wires
  codec/UI/profiles/build/docs/previews. Native worker integrates alpha backend.
- Tests must cover shared per-frame R deltas and FPS equivalence, four-direction
  native-budget walk, no L turning/double translation, native collision order,
  secondary blending, all pause/reset/scene/weapon transitions, fresh physical
  shot centering, alpha draw state restoration and original button identities.
- Run full tests, independent review, ARM build, secret audit, main/CI and library
  delivery. User still holds GitHub releases; do not publish a release/tag.

- [x] Inspect native root ABI, camera order, final look-at and alpha wrappers.
- [x] Implement/test native unified control, center qualification and HUD backend.
- [x] Implement/test wire/profile/UI/preview changes and migrations.
- [ ] Full build/review/audit/CI; deploy through the pinned library and validate
  actual walking/aiming/center/wall behavior with the user.

Native pitch animation blending can be nonlinear. Shared controller commands
are testable; exact angular response across all weapons needs hardware checks.
