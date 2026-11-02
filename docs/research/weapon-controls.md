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
332CC0(rifle),333DF0(launcher),342F90(mine/Wep14). Native31B9F0 is throwable
Wep19;3212E0 is charged-special Wep54 and is deliberately excluded. Runtime
classification and instruction guards use the supported Switch executable,
not addresses copied from a PC mod.

Readiness is keyed by actor, handler, weapon pointer and item. Native steady1/1
or a verified firing state establishes it. Preserve recovery1/0, pistol/shotgun/
automatic1/4 and rifle1/2 only within that session; initial raising, release,
weapon changes, invalid collision state and unavailable scenes cannot inherit it.
Rifle firing requires phase2/sub0..3; the other ordinary shot tables use0..1.
Reload walking remains limited to the existing verified pistol path.

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
before installing30 hooks within the40-slot pool.

Host fixtures do not reproduce the complete native animation/collision graph.
On Switch, test downward knife attacks and aim; stand/walk/strafe/fire/recover
with every supported gun, especially the first automatic round and bolt cycling;
then scope, zoom both ways with leftY, aim with rightXY and gyro, fire/reload,
leave/re-enter scopes, switch weapons/controllers and pause. Confirm the body
does not move in scopes and the magnification display follows smooth zoom.
