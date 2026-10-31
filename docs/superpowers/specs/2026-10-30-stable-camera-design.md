# Stable camera and smooth unified sticks

The user accepts common walking/aiming controls in0.9, but reports abrupt input,
wandering camera, excessive distance, unrestricted vertical look and a jumping
first-person pistol. BLACK PS2 is a feel reference. Its original manual confirms
move/look separation, not numerical acceleration formulas. Do not claim a port
of its undocumented algorithms.

Keep original face/menu buttons and shared walking/aiming sensitivity. Add a
time-based, monotone velocity response:50ms rise,12ms release, fast reversals;
integrate it analytically so constant-input movement agrees across30/60/120Hz.
Drain all state discontinuities. Limit right-stick pitch to50degrees each way,
without snapping gyro-created positions back inside that band. Gyro retains its
own range and filtering. Set RE9 shoulder distance to1.15; native Default stays1.

## Approaches

1. Filter only input: reduces abrupt turns, but cannot remove camera motion from
   following the animated weapon's clipped ray. Insufficient on its own.
2. Stable camera driven by common player intent, real projected aim marker: stops
   weapon animation driving camera; marker can move slightly off center. Ask the
   user about this conflict with the previous strict-center requirement.
   Chosen: user explicitly prefers steady camera and a truthful offset reticle.
3. Stable camera plus coherent hand/weapon/bullet pose correction: strict center
   needs all actual shot and laser consumers, not a cosmetic laser adjustment.
   Native evidence must establish a safe shared pose boundary before choosing it.

Research confirms laser and bullet calculations use different matrices; changing
only the laser would misrepresent real aim. Use the chosen stable-camera approach.
First person needs testing with motion,
raising, recoil and nearby walls; a head-height eye alone is not a new viewmodel.

Retain release hold, no personal keys/tokens in Git/artifacts, library-only console
deployment, current-repository files and regression tests. Hardware acceptance is
required for feel; automated matrix tests cannot establish comfort.
