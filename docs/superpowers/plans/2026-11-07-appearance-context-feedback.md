# Appearance contexts and console feedback

User scope: preserve the selected cosmetic model in scope, scripted actions and
the attache preview; restore knives, including Krauser's own blade; correct
shoulder/head proportions; move grenade framing slightly closer; enlarge the
inventory power caption 1.5 times and label it `Атака: XX`; investigate the still
missing D-pad weapon inscription and mist.

## Design

Native gameplay skeletons, equipment and animation remain the owners of play.
Owned render palettes correct donor neck/head and clavicle placement relative
to the native chest. Wrist matrices remain exact native weapon anchors. Elbow
corrections taper toward those anchors; a per-arm bound limits additional length
change to 12%. This corrects the reported shoulder/head distortion without a
full donor-height conversion or changing collision and weapon grips.

Authenticated donor resources and pins are independent of rendering contexts.
Gameplay and attache have separate identities, native part headers, retarget
maps and three acknowledged deformation slots. Their backing memory is disjoint
and reuses bounded existing bank storage. Camera/input eligibility must not
retire otherwise valid model resources. The menu's shorter preview object is
validated through its native pool and never read as a gameplay actor. Its
destruction hook invalidates bindings before native destruction; resources remain
until all outstanding consumers acknowledge them.

Native attachment suffixes retain the validated knife and hands. Krauser's
authenticated blade is an owned additional part, with visibility following the
native knife's visibility and alpha. Unknown suffix layouts fail closed.

Loading error10 retains its actual failing stage and bounded observation;113 is
file-read progress, displayed as a percentage. No speculative retry or memory
margin reduction is part of this change.

Grenade lens target changes from70 to68 degrees with the existing continuous
transition and native collision. Prediction does not steer the camera.

The inventory caption retains native numerical glyphs, scaled from14 to21px.
The Cyrillic prefix uses the already licensed owned serif face because there is
no proven localized native atlas mapping. No guessed glyph indices or native
message-channel writes are allowed. Both parts share color and visible scale.

Popup ordering changes have not resolved the hardware report. Instrument the
actual queued/equipped/published/rendered boundaries in the previously reserved
ABI22 diagnostic word. Expose it when the D-pad setting is focused. A passing
host renderer fixture does not establish console visibility; another unproven
ordering adjustment is not a fix.

## Implementation and verification

1. Characterize loading, binding invalidation, dropped knife, shape distortion,
   caption sizing and popup stages with pure and actual callback fixtures.
2. Integrate independent context bindings, safe native attachments and bounded
   render-only proportion correction. Add authenticated native hook guards.
3. Integrate loading stage/progress UI, closer grenade lens and caption layout.
4. Add persistent popup event/HUD diagnostics without changing its timing.
5. Run `build.py --test`, ARM module, both overlays and NRO; verify every guarded
   instruction against the supported private executable; audit public source
   and release artifacts; review resource lifetimes and callback exclusions.
6. Freeze a new version separately from all existing releases. Hardware tests
   must check menu first frame, scope, ladders, knives, several skin changes,
   grenade extremes, caption style and the popup diagnostic after D-pad equip.

No commit, push or public release is requested for this iteration.
