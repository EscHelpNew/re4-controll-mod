# RE6 controls profile

Select **Controls → Controls profile → RE6** in the installer or overlay.
The profile is the first row; change it with A or Left/Right. Install the matching
module and overlay before selecting it: local version0.20.0 uses settings wire15
and shared ABI19. Game options are saved independently of this profile.

The preset is an initial adaptation of RE6 response to RE4's proven camera and
movement hooks. It is based on1437 synchronized, fully valid samples recorded
on a Switch, with no dropped records. The original RE6 observer preserves its
game's controls; no extracted game material or private logs are published here.

| Response at gain1.00 | Overview | Aiming / knife |
|---|---:|---:|
| Horizontal angular rate | 171.6°/s | 57.3°/s |
| Vertical angular rate | 90°/s | 53°/s |
| Rise time constant | 50ms | 220ms |
| Braking time constant | 47ms | 47ms |

These are time constants, not a delay before movement. Input begins easing into
motion immediately after native weapon readiness. A47ms braking constant reduces
speed by95% in about141ms. The analytic integral keeps the response consistent
at different update rates; pauses, input ownership changes and weapon raising
do not replay accumulated motion.

Overview yaw and aim yaw are measured sustained rates. Braking fits two isolated
release traces at approximately47ms. The aiming rise is a first-order fit with
practical uncertainty around180–260ms. Its vertical rate is provisional because
the vertical holds were shorter; overview vertical response was not isolated, so
the existing90°/s baseline is retained.

For this preset, **Camera speed** controls overview. **Right-stick aim X/Y**
control aiming and knife look independently. Gains remain0..1; editing saves
Custom while retaining RE6 response. The three profiles are Default, RE6 and
Custom. Default restores the original game's controls with gyro disabled.
Previous RE9 settings remain usable as Custom. Graphics, language, calibration
and saved Custom settings remain scoped to their original domains.

The preset retains the current modern RE4 movement/camera implementation,
native-speed movement while aiming/using the knife/reloading, aim-only gyro at0.8, both
gyro inversions,0.05 stick deadzones and the existing shoulder framing. A/B/X/Y
and PLUS/MINUS retain native actions; L3 keeps the existing run alias.

Right-stick vertical look is limited to ±35°; gyro keeps its independent native
range and can return gradually into the stick band without snapping. Filtered
gyro angles are interpolated onto the game frame clock with10ms presentation
delay, avoiding unequal steps from asynchronous sensor batches. Native firearm
raising is seeded with the retained pitch for the verified generic mode0 path.

Reload walking covers all ordinary Leon reload families with their verified
native subphase tables. Existing v12 settings keep reload
walking off; reselect RE6 to apply the updated preset, or enable the new toggle.

From0.15.1, RE6-response third-person views use a shoulder arm anchored to Leon's
root position and the owned look direction. The neutral side and distance remain
the same, but walking profiles and the native delayed camera basis cannot drag
the desired eye around. Native wall collision still runs afterward and can move
the composition near obstacles. Body and weapon animations are retained; this is
not a frozen character pose. From local0.18.1, playable Ashley uses the same
shoulder arm during ordinary walking, backing up and running, with left-stick
movement and right-stick heading/look. Her native animation speeds and movement
budget are retained. The shared checked leg hierarchy supports the same strafe
correction. Switching Ashley/Leon drains old look easing and resets owned pitch,
including when the engine reuses the player address.

Local0.18.2 also corrects the native horizontal follow stage after camera wall
clipping. Its old local-X history belonged to a differently rotated camera
basis, so it could pull the character across the screen despite the stable
shoulder arm. Only the exact active shoulder call seeds the eye/target X history
with its already clipped result. The existing side, distance and eased look
remain; no extra camera or collision pass runs. Near walls the original clipped
eye still takes priority over exact screen placement.

Default/classic controls, accompanying Ashley, other characters, scripted
actions and first person retain their existing camera paths. Console comfort,
Ashley framing and interaction/wall transitions remain to be checked.

This does not replace RE4's animation, weapon or collision engine. First-person
weapon behavior and camera collision still belong to RE4. Actual comparison of
the new preset in RE4 is required before calling it a complete control match.
