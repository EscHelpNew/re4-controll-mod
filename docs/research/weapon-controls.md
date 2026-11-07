# Weapon controls, November 2026

These changes address the user's report of restricted downward knife/gun aiming,
pistol-only moving fire, abrupt scoped aiming and zoom. Their clarification is
binding: sniper scoped mode stays stationary, leftY zooms, rightXY/gyro aim.

## Supported executable and native boundaries

The local update-v65536 NSO passed its segment hash checks and matched the
previously analyzed image byte for byte, with the Build ID in profile.hpp.
No original game material is shipped in source or release artifacts.

Native33DF80 bounds the ordinary pitch pair at1265A08 to normalized +/-1. The
mod's shared 35-degree stick comfort bound was also applied to weapon/knife
routes; only those routes now request the full native normalization.

Moving-fire handlers are31CC60(pistol),3351A0(shotgun),3270F0(automatic),
332CC0(rifle),333DF0(launcher),342F90(mine/Wep14),31B9F0(grenade/Wep19).
3212E0 is charged-special Wep54 and is deliberately excluded. Runtime
classification and instruction guards use the supported Switch executable,
not addresses copied from a PC mod.

Readiness is keyed by actor, handler, weapon pointer and item. Native steady1/1
or a verified firing state establishes it. Preserve recovery1/0, pistol/shotgun/
automatic1/4 and rifle1/2 only within that session; initial raising, release,
weapon changes, invalid collision state and unavailable scenes cannot inherit it.
Rifle firing requires phase2/sub0..3; the other ordinary shot tables use0..1.
Reload walking covers every ordinary reload family listed below.

### Grenade movement (0.18.5)

Handler31B9F0 uses phase tableB096D0: raise31BA30, held31BB90,
throw31BC50, exit31BCA0, null reload, special targeting31BD60. All three grenade
items share this handler. Permit stable1/1; permit1/0 and1/4 only after current
actor/weapon/item readiness, plus throw2/0..1. Initial raise0/* and its first1/4
remain stationary. Unverified held1/2,1/3 contain native gait/footstep code and
remain excluded to avoid stacking displacement.

Held31BBBC calls shared aim33DF80. Throw31C824 roots initialization;
31C8BC roots playback before frame5 release31C8C8 and spawn31CC30. Native
projectile2E4510 samples current player bones0/10 and inherits their displacement.
The existing once-only pre-root movement boundary preserves this order and the
native previous positions; no extra projectile/throw hook is necessary.

## Reload movement and steering, 2026-11-06

The original v65536 NSO was loaded with segment hash checks and compared against
the existing full analysis image. Its Build ID matches profile.hpp. Phase4 of
routine6 dispatches to31D020(pistol),335600(shotgun),3275F0(automatic),
3332A0(rifle),343590(mine). Pistol/shotgun accept sub0..3, rifle/mine accept0..1.
Automatic type1 accepts0..4 (jump table A0B3D8); other schemes accept0,1,3,
rejecting2. Shotgun tables A0BE04/A0BE14 both accept0..3. Launcher333DF0 and
throwable31B9F0 have null reload entries. Charged-special3212E0 phase4 is an
aim/charge setup, not an ordinary reload, and stays excluded.

The existing reload toggle is independent of aim/knife movement. Playback does
not require held aim or native317630, including sub0 before weapon+402 is set.
The original native reload controller, timing, ammo updates, upper-body clip,
collision and primary/secondary motion remain owned by the game.

Unified controls use a separate reload look route3, with overview sensitivity,
pitch limits and easing. Its command is consumed once at ModernRoot before the
camera-relative left-stick step, after rechecking live reload/scene conditions.
Neutral left input and unavailable gait assets do not block look. Aim release
does not block look; scope/pause/damage and option-off do. Reload never routes
through33DF80 or enables gyro. Entering/re-entering the route drains stale
velocity; returning to aim uses the existing pitch handoff. Non-unified controls
retain their native camera behavior and camera-relative reload walking.
Reload look identity includes actor, weapon pointer, handler and item. A change
drains velocity; a replacement during the native update cannot consume the old
command at root.

Actual-callback host fixtures cover each family, input schemes, every accepted
subphase, native reload cursor/upper-body preservation, once-only root/pose,
released/held aim, neutral left input, right-stick look, third/first-person
camera direction, zero gain, freeze/resume and damage before root.
Additional regressions cover handler/item/weapon replacement both before update
and before root, plus actual-callback simultaneous steering and translation
with valid gait resources, checking the same-frame heading and once-only step.
Hardware acceptance remains pending: all weapons and reload upgrades, aim release,
walking/strafe/backward while turning, walls/stairs/enemies, pause/damage and
scope entry/exit must be checked on Switch.

The first automatic shot328190 computes geometry before its normal root at
328514. The additional action-entry hook translates first, then the original
code computes the ray, root, pose and collision. Shared once-per-update guards
prevent the later root from translating a second time.

## Scoped control ownership

Native15460 is void(scope). Its active object is controller1131718+530, selected
by controller7B0 with controller54=16 and state5090bit6. Its pitch118 is radians,
positive down, with native lower/upper bounds124/130 (normally +/-70degrees).
Zoom13C is normalized0..1; native FOVC8 is45-36*zoom (45-42*zoom for type1).
The native aiming slowdown is1-.9*zoom.

The adapter uses ordinary aim easing and gyro filtering, subtracts positive-up
pitch from the scope's pitch, and suppresses native stick/digital/zoom input
only for its synchronous original call. Input restoration preserves unrelated
native flag changes. The ordinary aim adapter leaves pending gyro for this
camera owner, so it cannot apply it twice or write the wrong pitch storage.

Zoom is committed at15568, after15490 saves the previous value and before the
native clamp, FOV construction and zoom feedback. The inline callback verifies
savedx19 against a dynamically scoped target and changes only saveds0 and the
owned zoom field. hook_context.hpp follows the actual dispatcher layout
(FP+000,GP+200); the ARM build checks that compiled layout explicitly.

## Verification and hardware acceptance

Host tests execute the actual adapter against bounded engine fixtures, including
initial raising, native recovery/bolt cycling, first-automatic-shot geometry,
stationary scope movement exclusion, pitch/zoom bounds, FOV/zoom feedback,
once-only input/gyro, independent inversion, sensitivity zero, pause/re-entry,
and preservation of unrelated registers and input flags. Easing integrals are
compared at30/60/120Hz. The module checks every original instruction guard
before installing32 hooks within the40-slot pool (including both shoulder follow corrections).

Host fixtures do not reproduce the complete native animation/collision graph.
On Switch, test downward knife attacks and aim; stand/walk/strafe/fire/recover
with every supported gun, especially the first automatic round and bolt cycling;
then scope, zoom both ways with leftY, aim with rightXY and gyro, fire/reload,
leave/re-enter scopes, switch weapons/controllers and pause. Confirm the body
does not move in scopes and the magnification display follows smooth zoom.
