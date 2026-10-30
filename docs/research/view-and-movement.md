# View and movement implementation notes

These notes describe the build-specific design for the original Switch port.
Host callback tests substitute engine calls; they cannot establish visual quality,
physical collision behavior or weapon compatibility on real hardware.

## References and scope

Capcom's developers describe choosing between first- and third-person views in
[this Requiem interview](https://blog.playstation.com/2026/01/29/resident-evil-requiem-interview-chainsaws-reinvented-zombies-fear-balancing-and-more/).
That choice is the behavior reference. This mod uses RE4's own camera, weapons
and animations; it does not contain Requiem assets or replace its animation system.

The MIT-licensed [boshjerns RE4 SDK](https://github.com/boshjerns/re4-boshjerns-sdk)
provides useful independent PC-side research and rig documentation. The Switch
bone hierarchy was checked against the user's extracted character assets. PC
addresses, speed constants and hook implementations must not be assumed portable.
No original game assets, keys or raw extracted binaries belong in this repository.

## First-person camera

`19B20` is the native player-camera update. Its `17F40` call supplies the current
48-byte basis; calling that function a second time would change native state.
`18520` supplies two local eye candidates, a target, roll and field of view.

The optional view moves both real eye vectors to a body-local eye anchor through
the inverse of that captured basis. It preserves the native eye-to-target vector,
roll and FOV and lets the original collision and smoothing run afterward.
The initial local height1700 follows native neutral-camera profiles. It is not
an asserted bone index or a replacement first-person arms model.

Third-person-only distance changes would not create a first-person view. Native
scope and scripted cameras retain ownership of their own output. Hiding the
player belongs in its main-view model submission; globally hiding it would also
remove its shadow and affect other passes.

## Screen reticle

The native laser calculation `2E8750` computes a clipped weapon endpoint. A fresh
calculation for the active weapon is required; the model's cached endpoint can
remain unchanged for very short rays. Original ray and collision calculations
must still execute when adding a marker.

The frame draws a previously submitted scene before updating the next one.
Consequently, a freshly simulated endpoint is not automatically the point that
belongs to the image on screen. Stage the point with scene submission and promote
it with the corresponding scene draw. Preserve its actor/weapon identity and
project it with the displayed main camera, excluding temporary special passes.

The native late screen list uses a512-by448 logical orthographic viewport.
`view.hpp` projects row-major native view/projection matrices to that coordinate
space and checks all conversions before building unsigned rectangle coordinates.
Offscreen, behind-camera, stale, invalid or hidden-HUD points are rejected.

## Native movement and leg animation

The player frame `311400` runs state and animation first, then entity collision,
map collision and final skeleton synchronization. The native translation helper
`81D0` moves actor and bone positions without erasing their previous positions.
A new displacement must enter that frame before root/skeleton evaluation and
weapon hit geometry, followed by the game's single existing collision pass.
Post-frame teleportation or a second animation/collision pass is not equivalent.

Walking speed can be derived from the active character's native walking resource
and private motion state. It must not use an invented world-units-per-second
constant. Native root displacement, frame factor, model scale and loop wrapping
determine the budget; normalized diagonal input must not exceed it.

The verified Switch rig has upper-body and leg branches under the same root.
Bone17 parents the legs18–25, while the torso/arms belong to a different branch.
The native evaluator supports excluding tracks through its private bone map.
This permits a private walking cycle on the legs without replacing the existing
primary/secondary aim blend. A changed leg pose must be restored before the next
native evaluation so static tracks cannot accumulate rotations or survive mode
switches. Previous world positions must remain intact for collision and attacks.

Native bone keyframe cursors also belong to the original animation. Private gait
evaluation temporarily saves, clears and restores both cursor banks at each
selected bone's `1B4..1D7`, before native matrix/IK work resumes. A cursor from an
aim track can otherwise exceed the number of keys in a walking track. The
regression uses different cursor sentinels for native and private evaluation,
including invalid-pose rollback and untouched upper-body caches.

First hardware checks must cover firing and knife timing, walls, stairs, mode
transitions and return to native behavior. Any unverified path stays explicitly
outside the compatibility claim.
