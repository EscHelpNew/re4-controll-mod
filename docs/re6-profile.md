# RE6 controls profile

Select **Controls → Profiles and reset → Controls profile → RE6** in the
installer or overlay. Install the matching module and overlay before selecting
it: version0.12.0 uses settings wire12 and shared ABI16.

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
Custom while retaining RE6 response. Selecting Default or RE9 restores their
existing response. Graphics, language, calibration and saved Custom settings
remain scoped to their original domains.

The preset retains the current modern RE4 movement/camera implementation,
native-speed movement while aiming/using the knife, aim-only gyro at0.8, both
gyro inversions,0.05 stick deadzones and the existing shoulder framing. A/B/X/Y
and PLUS/MINUS retain native actions; L3 keeps the existing run alias.

This does not replace RE4's animation, weapon or collision engine. First-person
weapon behavior and camera collision still belong to RE4. Actual comparison of
the new preset in RE4 is required before calling it a complete control match.
