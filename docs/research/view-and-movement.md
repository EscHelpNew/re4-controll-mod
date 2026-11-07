# View and movement implementation notes

These notes describe the build-specific design for the original Switch port.
Host callback tests substitute engine calls; they cannot establish visual quality,
physical collision behavior or weapon compatibility on real hardware.

## Fixed shoulder screen framing, 2026-11-06

The user reported that rotating the camera pulls the character from side to
side despite the root-relative shoulder rig. The earlier fixture ended at
CameraApply18520, before native wall collision and the following camera update.
The rig itself already keeps the torso in fixed camera coordinates; adding
another input smoother cannot correct a later eye-position filter.

The hash-verified v65536 image identifies the remaining stage. Native19CDC
calls18990 with captured basis in x1, QFPS profile in x2 and32-byte local
eye/target/roll/FOV result in x3 (x0 is unused).18990 runs the wall traces and
returns the selected eye in that local basis.19D0C..19D44 then mixes only eyeX
and targetX with1132A30/+C history: current*.3999999761581421 plus
previous*.6000000238418579 (constants6E8A2C/6E8A30).19D70/19D84 transforms
those points to world space. When the delayed basis rotates, previous localX
does not represent the previous eye in the new basis. It moves the character's
screen projection even in free space, after otherwise correct shoulder framing.

Local0.18.2 adds a guarded18990 trampoline. It runs the original once, then
seeds just those two X-history floats from the already clipped result for the
exact basis/profile pointers produced by this active shoulder call. It checks
completed current-frame ownership, actor state/character, heading/pitch,
distance/mode/settings and finite output again after native collision. The
collision result is never edited and no camera or wall pass is replayed.
Native input easing, shoulder side/distance, profile eye1, FOV/roll, animations
and final LookAt/frustum ordering remain. At walls the clipped eye takes
priority over exact screen placement. First-person, Default/classic, foreign
calls, scripts/scopes and unsupported characters do not seed this history.

The expanded actual-module fixture models the native downstream collision
result, local-X filter and world transform before calling real FinalLookAt.
The projected torso test failed with the old rotating-basis history, then
passed within.02 logical pixels for Leon/Ashley through rotations/reversals,
pitch and independently translated/rotated delayed bases. Wall fractions1/.5/.1
retain the original clipped eye even with wildly different old history.
Additional callback gates verify one original call, byte-identical collision
output and preservation of all history values except the two owned X fields.
The new ABI, call/filter/cache instructions and constants are guarded; the hook
budget is31/40. Hardware acceptance still needs camera turns in open space and
near walls, different poses/animations, scope/script transitions and comfort.

## Playable Ashley, 2026-11-06

The original v65536 NSO passed segment hashes and matched the full existing
analysis image/Build ID. Native player311400 dispatches ordinary mode0 through
30D430 and the shared walking/back/run table B067C0. Character503C=1 selects
Ashley's distinct centered overview at18360: character jump table6EAB14 entry1
lands183B0 with profile index2, while Leon entry0 chooses index0 at183AC.
This is compatible with the common player/root hooks; no new native entry is
needed to enable unified controls.

The mod had an explicit character0-only gate on its root-relative RE6 shoulder
rig. Local0.18.1 allows character1 only during ordinary routines0..4, retaining
native handling for scripted/interaction/damage modes. The rig uses exactly the
same arm/height, distance setting, owned heading/pitch and world-space conversion
as Leon, before the existing native18990 wall trace. Native collision eye1,
FOV and roll are preserved; final LookAt/frustum construction is unchanged.
Default/classic controls, other characters and first-person paths are unchanged.

The selected pl01/pl11 assets were read from local RomFS with all covering
16KiB blocks checked against the existing SHA256 block hashes. Both contain141
bones and the same parents0..25 as the existing topology whitelist, including
leg root17 and branches18..25. The existing native-walk lower-body direction
correction and cursor/pose restoration therefore apply without broadening the
Leon-only firearm/knife/reload movement gate. The original native animation and
its root displacement budget remain authoritative.

Owned look now records the character alongside the player address. Switching
Ashley/Leon at the same address resets easing and pitch before sampling the new
frame, avoiding a residual turn or inherited view tilt. Actual module callback
tests first reproduced the centered camera and stale character-switch look,
then passed after the fix. Ashley coverage includes rear shoulder position,
moving/transformed camera bases, yaw, forward/back/run/strafe/diagonal and partial
stick movement, zero input, input restoration, once-only native root/pose calls,
leg orientation/restoration, pause/scripted/invalid camera gates and unchanged
other-character camera fallback. Hardware acceptance remains pending for Ashley
chapter gameplay, narrow passages, obstacles, crouch/crawl/interaction/cutscene
transitions and character switching. Host fixtures do not establish visual feel.

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

## World follow and lateral framing (0.18.5)

The local-X correction alone does not remove lateral motion lag. Native camera
controller11E50 copies19B20's clipped world input, transitions it through13770
and calls138B0 at12040 with follow1131F50/input1131998. Follow history100/10C
uses alpha=pow(coefficientFC,frameFactorA4), normally coefficient0.8. The native
compensation vector state4FE8 enters the old eye/target before blending. Cached
near-body output130 can first replace history when12C is set. A sustained right
strafe therefore reduces shoulder separation even though the collision rig is
correct. The former fixture stopped before this stage.

For one current qualified shoulder calculation, CameraFollow temporarily sets
FC to zero and calls the original exactly once, then restores FC. Positive
finite frame factor is already required by unified_view_ready. Native reset,
cached correction handling, roll/FOV and the near-body correction13A6C..13B68
still execute. Active transitions, wrong objects, scope, stale/mutated player,
look/settings or nonfinite input leave native follow untouched. The token lives
only inside CameraCalculate and is published by a successful exact wall trace.
No final camera eye is rewritten after collision.

The actual callback regression now includes this world follow, native alpha,
compensation and cache. It projects a moving torso through sustained right
strafe, reversal and stop for Leon/Ashley and fractional frame factors. It also
checks clipped-eye/near-body correction and coefficient restoration/exclusions.

## Ordinary body steering (0.18.5)

Native2B4D70 composes actor -> bone0 -> torso1/legs17; bone0's bind position is
the pelvis. Before final world/IK, ordinary unified locomotion1..3 rotates the
existing root0 local3x3 matrix toward the backward-folded leg heading, at most
25degrees with smoothing over4.5native frame-factor units. Translation and any native blended matrix are
preserved. Leg17 subtracts the applied body angle, retaining travel heading.
Actor yaw and root displacement remain unchanged. Transition blend309 excludes
the body overlay. A separate single-bone ownership ledger restores its local
matrix before the next native player update, including weapon-action entry;
later native matrix replacements win. Sampling remains leg-only.

Bone attachments/colliders follow the model, so this overlay is restricted to
ordinary locomotion. Host tests check bounded steering, no accumulation,
translation/blend preservation, leg compensation, Ashley and restoration. Wall,
stairs and action-transition appearance require console acceptance.

## Extreme aim elevation (0.18.6)

Native normalized aim is an animation/profile weight, not a physical muzzle
elevation. Profile0's up/neutral/down eye-to-target elevations are about
62.57/-9.73/-60.14degrees, while the previous shoulder rig and final look used
the full input command as90degrees. A truthful point visible along the native
aiming frame could therefore leave the mod's vertical frustum at either limit.
The actual-callback regression reproduced this with a50degree projection.

CameraApply now reads the native output once after18520, and the neutral record
at selected profile+2C through a bounded read-only memory query. During native
profile transition, neutral eye/target are blended previous*camera254 +
current*(1-camera254), before computing their angle. The effective elevation
is nativeElevation-neutralElevation*(1-abs(command)/(pi/2)). Neutral remains
zero and full command reaches the native profile's actual framing endpoints.
No guessed common muzzle limit, asset edit or second original call is used.

The same effective pitch drives the shoulder arm before collision and the final
LookAt/up vector. The command pitch remains the ownership token and native pose
input. Scene-staged HUD projection and native projectile directions are unchanged.
Qualified ordinary RE6 shoulder weapon aim alone uses this mapping; missing
readable profile data retains the existing view. Walking/reload/knife/FPS/scopes
remain outside the correction. Prior fixed-character and native collision paths
still apply.

Tests cover both extrema, yaw/distance, clipped eyes and actual displayed-frame
reticle drawing; normalized weapon input remains+/-1. Profile neutral blending,
memory permissions/bounds, invalid angles and monotonic profile2 elevation are
checked. An initially attempted35degree unchanged band was rejected because its
upper endpoint exceeds profile2's28degree maximum and reverses the camera.
The final neutral-anchored curve follows the native framing instead.

User accepted0.18.5's camera and movement on Switch. This new extrema correction
still requires console verification with normal weapons, wall/stair proximity,
weapon switches, firing/recovery and leaving/re-entering aim.
