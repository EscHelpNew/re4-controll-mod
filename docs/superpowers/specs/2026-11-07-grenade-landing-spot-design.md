# Grenade first-floor landing spot

User-approved behavior: while aiming any of the three ordinary grenades, show
a red translucent spot on the first floor contact, rather than the eventual
explosion after floor bounces. The user approved the example, then requested
lower opacity and soft fading edges like fog. Add an on/off option in Game.

Use the existing native projectile launch/integration and private world queries,
never spawn a projectile, consume RNG, modify physics or fabricate a flat floor.
The deterministic center omits native lateral random velocity and cannot know
future release-animation motion: describe this as an approximate landing spot.
Consider wall/ceiling bounces before first floor contact; no hit means no spot.

Decisions: Game option `Grenade landing spot`, default off, independent of profiles.
Native camera projection makes a world-space circular spot follow floor
perspective. Red(232,48,40), peak alpha76/255, radial soft falloff to zero; no hard
outline. Radius200 world units. Hide immediately after aim release, throw, pause,
actor/weapon/room changes, unavailable scene/camera, invalid collision or expiry.

Approach: a pure bounded ballistic model and projected gradient geometry, plus
a build-guarded native adapter. Compute on the game/player-update thread with
private vectors and save/restore collision scratch; render copied snapshots only.
Reuse existing scene/HUD hooks. A screen-fixed ellipse or ray to actor-floorY
would not meet the requested floor location; mutating native projectile actors
would change gameplay. Do not do either.

Settings wire16 uses bytes106..109, old formats migrate off; shared ABI20 requires
matching module/overlay/application. Local version0.21.0, no commit/publication
unless requested. Host evidence does not establish physical-console accuracy.

## Additional explicit user requests

- Add default-off Game toggles for native pickup confirmation skipping,
  Capcom/CRIWARE startup logo button skipping, and removal of native aiming sway.
  Preserve normal grants, inventory-space rearrangement, story movies and manual
  stick/gyro behavior. Profiles preserve all six Game preferences.
- Enemy-health appearance is200ms; disappearance remains300ms. Numeric HP and
  the bar use the same fade.
- Pickup stays inside4A0470's transaction; only fresh known primary popup flags
  and its Yes response change at4A0948. No speculative inventory grant.
- Logo skip enters native title state30 only in its proven startup-logo window,
  with a fresh native accept edge; original continuation owns fade/resource work.
- Sway removal uses native33EC50 bit0 manual-baseline branch, preserving both
  pitch conversions and the existing gyro manual-input route.
