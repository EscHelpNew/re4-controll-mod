# Independent profiles and optional modern camera

The user requests independent control and graphics profiles, a separate graphics
menu, an optional first-person view inspired by Requiem, movement while aiming
and using the knife, and a configurable reticle. Their explicit release hold
continues: prepare and test locally, do not publish a GitHub release.

## Profiles

Each domain offers Default and Custom. Editing a domain automatically makes it
Custom. Selecting Default preserves the most recent Custom snapshot; selecting
Custom restores that domain alone. Language and hardware gyro calibration are
preferences, preserved across both profile changes. Controls include mappings,
camera and every future gameplay setting. Graphics include only the four current
graphics fields. Existing settings migrate without changing their values.

Use canonical settings encoding for comparisons, never structure padding.
Store validated custom snapshots beside the active settings file as
`.controls-custom` and `.graphics-custom`, using the existing incoming/previous
atomic file writer. Write necessary snapshots before replacing the active file;
on failure return false and preserve the active file. This is per-file recovery,
not a claim of a multi-file atomic transaction. A missing custom snapshot gives
a clear message and changes nothing. Profiles need no game-wire or ABI change.

The overlay opens a root menu with Controls and Graphics as peers. Both sections
contain their own profile row and descriptions in English/Russian. The standalone
installer provides the same domain selection and persistence semantics.

## Gameplay research and implementation boundary

All new gameplay modes are independently switchable and off by default. Existing
third-person controls, gyro, scopes, scenes and native graphics remain unchanged
when disabled. First-person view is a separate persistent toggle, with return to
third person. It must preserve native camera collision and weapon aim geometry.
A reticle during weapon aiming should project the real weapon endpoint rather
than suggest a false centered hit location.

Prefer native camera-output and late HUD hooks to graphics injection. Native
output has eye vectors, target, roll and FOV; first-person requires actual eye
position and visibility handling, not merely a shorter third-person distance.
Native player model suppression must not remove its shadow in other passes.

Movement during aim/knife must run before the original animation and attack
geometry evaluation and then use the game's one existing collision pass. Do not
teleport after physics, double-advance animation, repurpose aim blending, or
invent a native speed budget. Static research currently identifies translation
and collision order, but animation layering, speed and special-state gates are
not yet established. Continue investigation; document any unfinished feature
honestly instead of offering a nonfunctional setting.

## Alternatives and chosen approach

Embedding profile snapshots in the fixed game IPC block needlessly couples UI
storage to real-time hooks; sidecars avoid that dependency. A single all-settings
profile conflicts with the user's explicit separation. Whole-player first-person
hiding in the main view is a possible prototype, while claiming new first-person
arms/animations without assets is not. The native paths are the preferred approach.

## Verification

Add tests before code for independent profile identity, switch/restore, existing
custom migration, corruption/recovery, failed writes, unchanged preferences and
actual UI events. Run existing tests, renderer previews, ARM builds and guarded
binary verification before delivery. New camera/movement/reticle hooks require
callback tests plus explicit hardware acceptance; static tests cannot establish
animation/collision quality. Scan source, Git history and deliverable archives
for personal credentials before committing or transmitting a public artifact.
