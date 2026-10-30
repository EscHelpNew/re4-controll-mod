# Native-default graphics controls

User requests a Graphics menu for AA, texture filtering, object draw distance and
shadows. User chose manual settings with original game behavior by default.
GitHub release publication remains explicitly on hold. Current controls/camera/
mapping defaults and existing saved choices must survive this update.

## Verified approaches and decisions

The supported ARM executable was inspected and compared with its hash-verified
NSO. Do not infer a game feature merely from generic NVN API imports.

1. Filtering: hook sampler constructor64C3A0 and copy its synchronous20-byte
   descriptor. Apply Native/Bilinear/Trilinear/AF2/4/8/16 only to original linear,
   mipmapped, noncomparison, nonreduction samplers. Preserve nearest/special
   samplers and all unrelated fields. Latch the option for the process and label
   restart required. Live native-cache variants are rejected: both caches have
   fixed128entries and throw on overflow.
2. Shadows: Native/Off. Scope native display bits02048000 around BOTH void
   scene submission4EB700 and queued drawing4ECFB0. Call native logic even Off
   so counters reset. Restore only controlled bits, preserve unrelated mutations,
   and avoid restoring through a replaced game-state pointer. Shadow resolution
   has not been established and is not exposed.
3. Object distance: hook global-frustum update4F5C90. Temporarily scale native
   float+18 by0.50..2.00, call original to calculate planes/cache+24, restore+18.
   Native1.00 is unchanged. Skip nonfinite/transition far<=near, clamp to valid
   current projection far and near+1. This changes fog-limited loaded-geometry
   culling, not streaming, fog appearance or native model LOD.
4. AA: handheld1.25x SSAA using native720p display index2 and900p back index1.
   Native resolution mapper24FAE0 otherwise returns2/3 handheld or0/1 docked.
   Check SDK operation mode as well as dimensions, preserving docked720p configurations.
   Existing native final blit scales independent texture rectangles with linear
   filtering (NVN copy flag1).900p handheld back targets are bounded by stock dock
   allocations; no new dimensions/pool limits. Docked behavior remains native.
   Latch the AA choice for the process and require restart. Only modify outputs of
   the native mapper; native initialization/display-event callers retain their
   request24FA80 and deferred render queue/GPU finish. Never issue resolution
   requests or invoke resize240820 from the menu/main thread, avoiding a race
   with physical dock/display events.

## Architecture

Pure graphics policy helpers and tests; settings wirev8 retains128bytes and adds
fields in previously reserved bytes88..95. Decode v1-v7 with native graphics
defaults; preserve all controls. Render-worker callbacks consume one atomically
published graphics snapshot rather than mutable main-thread Settings.

Only startup/main-thread code reads SD. Filter latching must never freeze an
unloaded default as a configured choice: track startup settings readiness and
report effective modes in inspection diagnostics and restart requirements in menu help. Native startup and
Native graphics must have no renderer writes or requests.

Add a dedicated bilingual Graphics page to installer and overlay. Include help
for scope/restart/handheld constraints, restore graphics-only defaults and save
on actual changes. Add versioned shared diagnostics for call/effect counters.
Retain physical overlay recovery and existing module install safeguards.

## Validation and evidence

Portable tests: all sampler types/flags/LOD preservation, latching, packed snapshot,
shadow-mask restoration, non-drifting frustum scaling, exact native passthrough,
handheld/dock mapping, process latching and native resize ownership, strict
wirev8 validation and v1-v7 migration. Actual callbacks must test native consumer
ordering and all gates. Verify every added instruction guard against the original
image, ARM imports/ABI, EN/RU rendered pages, complete local/CI suites and known
personal-material audits. Hardware visual/FPS acceptance is pending.

Private evidence: data/analysis/graphics_nvn/findings.md,
data/analysis/graphics_scene/findings.md,
data/analysis/graphics_resolution/FEASIBILITY.txt.
Numeric NVN copy-flag corroboration (reconstructed MIT header):
https://code.botw.link/uking/uking/lib/NintendoSDK/include/nvn/nvn.h.html#1100
