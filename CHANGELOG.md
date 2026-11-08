# Changelog

## 0.27.2 (2026-11-08)

- Show the spare-ammo count and cosmetic character name only when the actual native health arc is drawn in the same frame; follow its fade, hierarchy visibility and screen bounds.
- Correct cosmetic laser, ordinary shot, retained muzzle-particle and shell origins using current donor grip displacement and exact live weapon ownership. Preserve native bones, weapon size, spread and direction; detached particles receive one correction.
- Add one complete test command with 2–4 workers, isolated per-case outputs and per-translation-unit coverage profiles. Report executable lines separately from branches and enforce a 90% line threshold independently for own C++ and Python.
- Reject incomplete, stale or excluded production coverage, failed tests and compiler/profile errors. Record input content identities so source changes during a run cannot produce an accepted report.
- Module, overlay and NRO builds pass. The complete four-worker command passes all 104 cases (103 native programs and 175 Python tests), with 91.09% C++ and 96.18% Python executable-line coverage. Console application delivery and restart are verified; the user approved this build for release.

## 0.27.1 (local, unpublished; 2026-11-08)

- Pin the shared IPC block to the first writable data page and reject builds that lose this layout, restoring bounded overlay discovery as initialized character data grows.
- Qualify held weapons by their actual common model methods and resource archives, including the player archive for grenades and the separate case-preview weapon archive.
- Shift the complete camera rig by the donor/Leon neutral head-height difference before native collision; lift short case previews consistently across the body and held objects.
- Position cosmetic health names using the native HUD projection and correction; display Luis/Ramon without surnames. Keep the original sprite when correction sampling is unavailable.
- Preserve ordinary weapon size and opt-in diagnostics. New console acceptance remains pending.

## 0.27.0 (local, unpublished; 2026-11-08)

- Use donor bone lengths for cosmetic bodies and translate owned held-weapon geometry to the new grip positions, keeping weapons at their ordinary size. Gameplay collision and effects retain native positions; short characters may lose offhand contact with long two-handed weapons.
- Reverse Krauser's knife around its posed grip without changing blade size.
- Hide the spare-ammo counter with the native health gauge and show the applied character's name on that panel using native glyphs. Use canonical Latin names when a complete localized glyph mapping cannot be verified.
- Draw the optional extra attack-upgrade square beside the native row, gray before purchase and red afterward, preserving the native fade and existing upgrade ownership.
- Add an explicit launch-mode choice: Normal by default, or opt-in Diagnostics. Normal mode does not start the RE4 diagnostic server or write its automatic logs.
- Preserve the accepted camera, controls, weapon captions and case rotation. Console acceptance of these new changes is pending.

## 0.26.8 (local, unpublished; 2026-11-08)

- Recognize the fully prepared native blade BIN after the game's offset relocation, restoring the ordinary and Krauser attachment paths without accepting raw or partially prepared data.
- Rotate the attache-case character preview horizontally with the right stick, retaining the chosen angle on release.
- Preserve the user-confirmed appearance switching and native weapon caption behavior from 0.26.7.

## 0.26.7 (local, unpublished; 2026-11-08)

- Keep valid donor bodies active when optional blade data is unavailable; hidden blades no longer require copied geometry.
- Use the attache case's native font, localized name records and palette for weapon popups; make the fog's light contribution visible through additive blending.
- Extend bounded read-only blade metadata and sample observations without changing shared ABI or settings.

## 0.26.6 (local, unpublished; 2026-11-08)

- Preserve the native attached blade on replacement bodies with frame-owned geometry; use Krauser's owned blade for his appearance.
- Compensate weapon-name/mist vertices for native viewport adjustment without changing the global HUD, camera or approved layout/timing.

## 0.26.5 (local, unpublished; 2026-11-08)

- Add bounded read-only knife-source/visibility and cached HUD-transform observations to the diagnostic snapshot. No gameplay or rendering correction; cached matrices are explicitly unordered with the popup.

## 0.26.4 (local, unpublished; 2026-11-08)

- Spare ammunition: remove bullet icon and center actual count in the original three-digit field, without leading zeros.

- Fix donor texture reservations by collecting and validating original DDS IDs before the native creator rewrites shared TPL references. Preserve resource, capacity, ownership and render fences.
- Preserve the exact native disabled-scissor sentinel in module/overlay diagnostics and mark it explicitly in JSON. This changes observation only; weapon-popup visibility remains under investigation.

## 0.26.3 (local, unpublished; 2026-11-07)

- Fix the overlay diagnostic port resetting every connection. A single accept failure (for example a client that disconnects right after the handshake) or a listener select failure after sleep/dock/network loss used to stop the server thread permanently while the bound socket stayed open, so the console completed TCP handshakes and reset them until the overlay was reloaded. Accept failures now only drop that client; a failed listener is rebuilt with a one-second retry.
- Root cause of the reset-before-reply: the overlay initialized BSD with `sb_efficiency=1`, i.e. transfer memory for one socket's TCP buffers. The listener consumed it, so the stack could not allocate the accepted socket and reset every client before `accept` returned (no log, no data). Reserve four sockets' buffers (about 160 KiB).
- The diagnostic worker borrows the overlay's existing `dmnt:cht` session instead of opening a second one that could block in sm at the service's session limit.
- Record listener failures in `live-debug.log` as stage 9 (listener select, rebuilt) and stage 10 (accept).

## 0.26.2 (local diagnostic candidate, unpublished; 2026-11-07)

- Capture the first terminal overlay transport failure with its stage, descriptor, return value, errno, native BSD result and elapsed time. Send bounded numeric failure metadata when possible; save a capped local log for automatic support reports.
- Keep one read-only game-module snapshot in the local failure log so authenticated application reports can recover observations when the overlay TCP transport fails.
- Parse and preserve strictly bounded transport fields on the desktop without echoing arbitrary remote text or credentials. The console's pre-authentication disconnect is still under investigation.
- Retain the shared project's optional additional weapon upgrade and case ammunition counter. Both default off; paired module/overlay use settings wire19. Their console acceptance is pending.
- Protect additional-upgrade ownership from failed/corrupt sidecar reads, resolve pending loaded ownership before saving, and retry failed writes without treating the unpersisted cache as durable.

## 0.26.1 (local, unpublished; 2026-11-07)

- Correct the0.26.0 startup rejection that disabled every game feature. Two native knife pointers use loader-relative relocations; validate their exact relocated targets with bounds/overflow checks, retaining strict instruction/build guards.
- Reproduce the failure using a runtime-relocated fixture and cover corrupted/unrelocated pointers, truncated memory and real entrypoint hook rejection.

## 0.26.0 (local, unpublished; 2026-11-07)

- Add an authenticated read-only diagnostic server on TCP28773 in the RE4 overlay. A separate worker keeps serving when the overlay UI is hidden; unload stops it before BSD/SD teardown. Reuse the existing local connection key and expose only bounded schema1 snapshots, with no arbitrary memory/file/write commands.
- Add the host snapshot/watch client, actual module/overlay version reporting, appearance/loading/control/render observations and independent snapshot validity. SharedABI23 appends ordered popup GPU-cache capture plus module version while retaining settings wire18 and previous field offsets.
- Record viewport/scissor after the popup text's native End command without changing renderer state. Preserve zero/negative finite observations separately from rendering usability. Gameplay popup visibility remains unresolved pending console captures.
- Include complete owned donor hands, corrected flash-grenade inventory hand handling, standalone Krauser blade and native-font Attack captions from the pre-network0.25.1 candidate. Keep the user-accepted grenade camera unchanged.


## 0.25.1 (local, unpublished; 2026-11-07)

- Complete authenticated donor bodies with their own native hand pieces and explicit geometry roles. Ramon already has complete hands. Both gameplay and inventory draw owned body chains, removing Leon glove borrowing, the incomplete flash-grenade hand classifier and the hand-suffix admission that reports122.
- Replace Krauser's blade at the exact standalone native knife draw owner, preserving native visibility, effects and weapon actors. Keep the blade out of body/menu chains and retain acknowledged deformation slots.
- Render the inventory Attack prefix with the same live native glyph font, color, baseline and21px size as the numbers. Derive supported Russian glyph indices through cross-letter constraints in four localized item names; retain the licensed prefix fallback for unknown translations.
- Add optional native viewport/scissor readings under the focused D-pad setting. Published events/render counters and visible inventory prefix rule out missing publication and a generic triangle failure; gameplay weapon popup presentation remains under investigation.
- Preserve the grenade camera accepted by the user in0.25.0. Settings/sharedABI and hook count are unchanged. Hardware model/hand/knife and localized-caption validation remains required.


## 0.25.0 (local, unpublished; 2026-11-07)

- Separate gameplay and attache appearance bindings, keep cosmetic ownership through scope/scripted camera states, and invalidate the menu binding before native destruction. Share authenticated resources with disjoint acknowledged deformation storage.
- Correct donor shoulder/head placement in an owned render palette while preserving native wrists, root and legs; cap additional arm spacing changes at12%. Preserve native knives and use Krauser's authenticated own blade with native visibility.
- Report appearance read progress and the precise failing texture-loading stage without changing allocation margins or retry policy. Intermittent console error10 still needs a live stage report.
- Bring the stable grenade lens target slightly closer,70 to68degrees. Enlarge native inventory numbers14 to21px and show `Атака: XX` / `Attack: XX` with a licensed serif prefix, matched to measured numeral ink.
- Instrument D-pad event publication and HUD rendering in ABI22's reserved diagnostic word. Expose the four diagnostic bytes under the focused D-pad option. Missing console weapon-name/mist visibility remains unresolved pending those observations; retain500/2000/500ms animation.


## 0.24.2 (local, unpublished; 2026-11-07)

- Resolve appearance wait109 for the normal six-record native Leon bone-effect table. Preserve the real actor/effect call and reject unknown or malformed owners; retain all other resource, rig and render-lifetime checks.
- Replace grenade prediction-following camera rotation with a smooth native-lens widening toward70degrees during RE6 grenade aim. Apply FOV before the native wall solver, retain stable look input, and let the real projected spot clip at viewport edges instead of disappearing when one edge crosses them.
- Draw weapon names and mist after the native HUD list. Add a real-item end-to-end D-pad transaction/popup test and keep500/2000/500ms timing. Console visibility still needs rechecking.
- Show compact numeric firepower using the selected weapon name's live native font, color and baked outline at60% size. Remove the large custom `Урон:` text; preserve exact instance/upgrade calculation.
- Include0.24.1 TMP recovery and all0.24 features. Wire18/sharedABI22 are unchanged; paired module/overlay installation and console validation are required.

## 0.24.1 (local, unpublished; 2026-11-07)

- Recover both unified right-stick and gyro readiness during ordinary TMP firing after a lost aim session. Native steady aim can be transient inside one update; the verified held-fire loop proves raising completed. Keep native safety exclusions and discard stale gyro samples on re-entry.
- In RE6 third-person grenade aim, minimally turn the camera to keep the real predicted explosion-floor disc in view. Preserve native wall-safe eye, throw direction, aim state, physics and visibility checks. Strengthen the red center and retain soft outer edges; update both menu previews.
- Replace generic appearance Pending with localized stage diagnostics and bounded model/bone observations. The reported console appearance blocker is still under investigation; this change locates it without relaxing rig, loader or render-lifetime gates.
- Include all0.24.0 weapon labels, inventory firepower and pickup polish. Settings wire18/sharedABI22 are unchanged.

## 0.24.0 (local, unpublished; 2026-11-07)

- Show weapon/grenade names after successful D-pad equips, tied to the existing D-pad option. Gray bold italic serif text without an outline sits beside the health HUD over a light feathered mist. Text and mist fade in for500ms, stay for2s and fade out for500ms; a new equip immediately replaces the old animation.
- Show the exact hovered firearm's current upgraded firepower below its native name in the attache case, rounded to one decimal place. This readout is independent of the D-pad setting.
- Suppress the native whole-frame pickup fade when skipping confirmation, and restore the short native firearm pickup sound once after successful acquisition. Preserve native grants and inventory-full handling.
- Retain0.23 appearance support and camera continuity. Settings wire18/sharedABI22 are unchanged. Physical-console layout, audio and gameplay acceptance remain pending.

## 0.23.0 (local, unpublished; 2026-11-07)

- Add Game → Character appearance in the app and overlay: Leon default, Ashley, Ada, Luis Sera, normal Krauser, Merchant, human Ramon Salazar and an intact village Ganado. Preserve Leon's actual player type, weapons, inventory and native skeleton/chain. Track requested and applied appearance separately; only an owned draw activates telemetry.
- Read and authenticate the installed donor resources, retarget onto Leon's major joints, and retain native weapon hands. Use owned deformation slots, ordered CPU consumption acknowledgements and post-frame GPU retirement; keep native async loading, descriptor admission and allocation ownership separate.
- Preserve at least128MiB contiguous native heap headroom after a new pack reservation. Unpinned packs follow future native unloads; the seven-pack passive cache is bounded (about196MiB CPU worst case). Unsupported states retain native rendering and report failure.
- Include the0.22.1 D-pad camera continuity fix. Settings wire18/sharedABI22 migrate earlier appearance to Leon and require a paired module/overlay update. Console appearance/gameplay validation remains pending.

## 0.22.1 (local, unpublished; 2026-11-07)

- Preserve camera pitch ownership and free-orbit continuity during D-pad weapon changes. Keep weapon, pose, gyro and stale reticle invalidation; native scope/FOV transitions retain their own behavior. Regression covers both pitch directions, guns/grenades and the next player update.
- Separate hotfix build keeps wire17/shared ABI21 and the seven-row Game menu; unfinished appearance work is excluded.

## 0.22.0 (local, unpublished; 2026-11-07)

- Predict the grenade explosion position after native bounces and fuse timing instead of first floor contact. Include material/rest/water removal handling and native burst clearance; project airborne bursts onto real ground below. The red fog center is slightly redder, with the same soft transparent edges.
- Add optional D-pad weapon switching: Left/Right cycles owned gun instances and launchers; Up/Down cycles distinct available grenade types. Use native equip/rebuild with inventory parameters, menu/QTE/busy guards and a bounded request queue. Default off, independent of profiles.
- Make HP numbers slightly more opaque:128/255 instead of102/255; bar opacity stays102/255 and fade timings stay200/300ms. Menu examples use the same number rendering.
- Settings wire17/shared ABI21 preserve deployed wire16 preferences and migrate D-pad switching off. Update the paired module and overlay after the application.

## 0.21.1 (local, unpublished; 2026-11-07)

- Make enemy HP numbers slightly more opaque: peak alpha128/255, while the bar stays102/255. Preserve the shared200ms appearance and300ms disappearance in gameplay and menu examples.

## 0.21.0 (local, unpublished; 2026-11-07)

- Add optional grenade first-floor landing spot: soft red fog with lower opacity, private bounded trajectory queries and displayed-camera projection. Current pose and the center of native throw randomness provide an approximate landing position; no RNG or projectile is created by the preview. Include a compact live example in both menus.
- Add optional skip of fresh pickup confirmation/information popups. Native item grants, inventory-space checks and rearrangement remain responsible for completing the pickup.
- Add optional button skipping for the native Capcom/CRIWARE startup logo sequence through its own continuation path.
- Add optional removal of native aiming sway, preserving manual stick/gyro input and native pitch conversions.
- Reduce enemy-health appearance to 200 ms while keeping disappearance at 300 ms. Bars and numbers share the same fade.
- All four new Game options default off and remain independent of profiles. Settings wire16 migrates earlier formats with these options off; shared ABI20 requires paired application, overlay and module. Console gameplay acceptance pending.

## 0.20.0 (2026-11-07, includes local 0.18.x and 0.19.0)

- Put Game immediately after Graphics. Enemy health bars and optional Health numbers default off; numbers can be changed only while bars are enabled.
- Show a compact enemy close-up in Game settings in both menus. The example strip and optional 5.0 number appear or disappear immediately with the toggles.
- Use an 18x2 native-pixel colored strip, without a black outline/background, peak alpha 102/255. Fade in/out over 300 ms and crossfade target changes using copied world anchors with the current displayed camera.
- Show optional centered current HP in weapon firepower units (150 raw HP per unit), rounded to one decimal with a point in every interface language, for example 5.0. Use a compact original pixel font, the same opacity and fade as the bar.
- Settings wire15 preserves earlier formats; wire14 keeps the bar preference and starts numbers off. Shared ABI19 rejects mixed module/menu versions. No additional native hook. Console acceptance pending.

## 0.19.0 (local, unpublished; 2026-11-07, enemy health bars)

- Add Game/Игра to the overlay and installer with a persistent Enemy health bars/Полоска здоровья врагов toggle, off by default and independent of control/graphics profiles.
- Show one compact translucent proportional bar above the enemy selected by the game's fresh aiming trace. Green above50%, yellow at50%through above25%, red at25%or below; dead/invalid/allied targets are omitted.
- Reuse the native world-clipped nearest target and verified current/maxHP. Read an animated skeleton-top anchor, snapshot values into the displayed scene, and project using that scene's camera without changing targeting, health, physics or camera behavior.
- Settings wire14 preserves v1..13 with the option off. Shared ABI18 prevents mixed old/new module/menu IPC. No additional native hook is required; console acceptance pending.

## 0.18.6 (local, unpublished; 2026-11-07, extreme aim framing)

- Resolve shoulder-view aim elevation from the game's selected/blended camera profile instead of interpreting full pose weight as a physical90degree angle. Keep neutral look anchored and native upper/lower weapon range intact.
- Use the same resolved pitch for the shoulder arm and final view, preserving fixed character placement and native wall/near-body collision. Weapon rays and the truthful projected HUD marker remain unchanged; scopes/FPS/ordinary overview retain their controls.
- Add real callback regressions for both extrema, yaw, distance, wall clipping and displayed-frame reticle drawing, plus profile blending/memory guards and monotonic angle mapping. Console acceptance pending.

## 0.18.5 (local, unpublished; 2026-11-06, strafe framing and grenades)

- Keep the RE6 shoulder position during lateral travel by bypassing the exact normal camera's second world-space follow for the current clipped rig. Preserve its native near-body correction and restore the follow coefficient after the call.
- Enable aim movement for all three grenade types through verified held/throw/recovery states. Preserve native projectile release and inherited movement; initial raise and unverified native gait substates remain excluded.
- Smoothly steer the ordinary walking/running pelvis toward travel up to25degrees, with compensated leg yaw and owned-pose restoration before actions. Preserve actor/view heading and native blended root matrix.
- Add actual callback regressions for sustained right strafe/reversal/stop, follow exclusions/collision, grenade action order, body steering/restoration and Ashley topology. Console acceptance pending.

## 0.18.4 (local, unpublished; 2026-11-06, legacy installation ownership)

- Repair full updates after a manual own0.16.1 module deployment followed by
  overlay-only installation left zero module ownership in a valid receipt.
  Recognize only the exact known game/file index/size/FNV identity compiled
  into the app; keep rejecting unknown files and malformed/missing receipts.
- Preserve the normal complete preflight, backups and receipt publication.
  The completed full installation records ownership for future updates.
- Include the read-only0.18.3 diagnostic report of the three fixed RE4 artifact
  identities and receipt matches. Raw file contents and credentials are excluded.
- Includes the0.18.2 reload/Ashley/fixed-framing work; gameplay hardware
  acceptance remains pending.
- Full host tests, ARM64 builds, independent review and public package audit passed.

## 0.18.3 (local diagnostic only, unpublished)

- Add read-only installed RE4 artifact/receipt identities to startup support
  reports to identify the conflict. Does not replace installed game files.

## 0.18.2 (local, unpublished; 2026-11-06, fixed shoulder framing)

- Keep Leon/playable Ashley at the existing side of the screen when turning the
  RE6 shoulder camera. Retain the current side/distance and eased right-stick look.
- Correct the native horizontal follow stage after wall collision: it blended
  current and previous local-X coordinates even when the delayed camera basis
  rotated. Seed just its two history values from the current clipped result;
  never reposition the collision result or run camera/wall processing twice.
- Restrict the correction to the exact active shoulder basis/profile call with
  unchanged actor, character, state and settings. Preserve native first-person,
  Default/classic, scripted/scope and unsupported-character paths.
- Includes local0.18.0 reload movement and0.18.1 Ashley controls. Hardware
  acceptance of framing comfort, animations and camera obstruction is pending.
- Full host tests, ARM64 build, independent review and public artifact audit passed.

## 0.18.1 (local, unpublished; 2026-11-06, playable Ashley)

- Give playable Ashley the existing RE6-profile rear shoulder camera instead of
  her centered native overview. Reuse Leon's side, distance and look direction,
  preserving the original camera collision anchor, wall trace, FOV and roll.
- Retain unified left-stick movement and right-stick heading/look during her
  normal walk/back/run states, using her native animations and motion budget.
  The checked Ashley leg hierarchy supports the existing strafe pose correction.
- Drain look easing and reset owned pitch when switching Ashley/Leon at a reused
  player address. Keep accompanying Ashley, scripted actions, Default/classic
  controls and other character camera paths unchanged.
- Includes local0.18.0 reload movement/steering. Full host tests, ARM64 build,
  independent review and public artifact audit passed. Switch hardware
  acceptance of Ashley framing, transitions and collision remains pending.

## 0.18.0 (local, unpublished; 2026-11-06, reload movement and steering)

- Extend optional reload walking to all ordinary Leon reload families: pistols,
  shotguns, automatic weapons, rifles outside scopes and the mine thrower.
  Validate each native subphase table, including both automatic control schemes.
- Process right-stick view/heading changes during reload in unified controls,
  including released aim and neutral left input. Left-stick movement follows
  the updated heading; native reload timing, ammo and upper-body animation stay
  owned by the game. The existing RE6 profile enables both required settings.
- Keep scopes stationary and drain look velocity on entry, pause/resume and
  option changes. Reload does not enable gyro or invoke firearm aiming.
- Full host tests and the ARM64 module/overlay/NRO build passed; public source,
  history and packaged artifacts passed the audit. Per-weapon Switch hardware
  acceptance remains pending.

## 0.17.2 (2026-11-04, gyro preview horizontal direction)

- Correct the app's gyro preview X projection and its asymmetric angular bounds
  to match game aiming: positive native yaw projects left. The game overlay's
  direction was already correct; only the app preview was reversed.
- Preserve game gyro behavior, inversion flags, gains, wire/settings formats
  and saved values. No settings migration or reset is needed.
- Include all changes since published 0.16.1; local 0.17.0 and 0.17.1 were not
  published. Final host tests and the ARM64 build for 0.17.2 passed; physical
  direction validation and broader hardware acceptance remain pending.

## 0.17.1 (local, unpublished; HOME exit and RE4 page artwork)

- Restore the original 4 inside the RE4 installer page; keep the RE6 page's 6
  and the global NRO/HOME icon's RE.
- Route B from the main menu and + exits from the verified own HOME launcher through
  managed Horizon exit before UI cleanup, including launches without a debug
  token. Keep the UI and error code visible if the request fails; retain normal
  hbmenu return for other launchers. The user confirmed basic HOME exit in this
  local build after deployment; this does not validate all exit paths or gyro/graphics.
- Include the features developed in local 0.17.0 below; 0.17.0 was not published.
- Increase PATCH for every fix build; never reuse a version for changed fix binaries.

## 0.17.0 (local, unpublished; RE artwork, RE4 graphics presets and gyro test)

- Replace the application icon's 4 with RE while retaining the red crosshair;
  preserve the original artwork and use the new asset for app/HOME packaging.
- Add independent RE4 Default/Custom/Quality graphics presets in the installer
  and overlay. Default restores Native filtering, AA and shadows, distance 1.00x.
- Quality selects AF16x, existing Handheld SSAA, distance 1.75x and Native shadows.
  SSAA renders 900p to 720p in handheld mode only; docked rendering remains native.
- Preserve the last manual Custom graphics configuration across built-in preset
  transitions, saves and reloads, independently of controls/language/calibration.
- Explain that full graphics application requires HOME → X → Close and relaunch;
  returning to a game menu or reopening the overlay is insufficient.
- Show configured left/right stick deadzones numerically on all four diagnostics
  axes, with muted red hatching for the axial zero intervals.
- Open the app's Gyro test with one ZL press from diagnostics or Debug mode in
  Control settings → Gyro. Use a fixed classic Leon scene without a baked laser
  or dot, and a live red preview dot driven by the game's gyro filter/integration.
- Preview seven current gyro settings and calibration in a draft; X calibrates,
  Y returns the dot to (810, 350), zeroes preview angles and clears pending motion
  without changing settings; B cancels. The eight rows are seven settings and
  Save and apply, which validates and persists before returning
  to the exact origin; a failed save leaves the test open and active settings intact.
- Keep diagnostics shortcuts outside Gyro test: Y calibrates and X resets preview.
- Label raw motion in rotations/second and accumulated preview angles in radians;
  the background projection is illustrative, with no game-FOV or performance claim.
- Host tests and the ARM64 build passed. Hardware acceptance of 0.17.0, including
  the new presets, icon, Quality image quality/performance and gyro test, remains
  pending. These local results do not validate the final 0.17.1 binaries.
  The user's 0.16.1 confirmation does not cover these changes.

## 0.16.1 (2026-11-02, native RE6 anti-aliasing)

- Remove the manual RE6 AA render hook after hardware reports of artifacts
  outside the original FXAA3 mode. RE6 now retains its native rendering path.
- Show fixed `FXAA3 (native)` in the RE6 overlay and installer; A/Left/Right
  cannot select Off, FXAA or FXAA3HQ or write settings from that status row.
- Migrate all valid legacy RE6 AA choices to Native, retaining texture filtering
  and language. New saves/live settings also encode Native, so an older installed
  module receives the native setting when the new menu connects.
- Keep native RE6 gyro, filtering, recording and all RE4 0.16.0 controls changes.
- User confirmed 0.16.1 working on Switch after deployment, and separately
  confirmed RE4 AF16x after a full game restart.

## 0.16.0 (2026-11-02, weapon movement and scoped controls)

- Restore the full native firearm/knife stick pitch range while keeping the
  35-degree overview limit. Gyro range remains independent.
- Enable moving aim/fire for verified pistol, shotgun, automatic, rifle,
  launcher and mine handlers. Retain readiness through native shot recovery
  and all four rifle bolt-cycle states; throwable and charged-special paths
  are excluded. Existing ordinary pistol reload movement is unchanged.
- Move before the first automatic round's native geometry, with one original
  root/pose/collision pass and no duplicate translation.
- Add eased right-stick and additive gyro scoped aim, retaining native pitch
  bounds, inversion, sway and zoom-dependent slowdown. LeftY smoothly changes
  zoom; the character remains stationary inside sniper scopes.
- Commit zoom after the native old-value snapshot and before clamping/FOV so
  the original magnification label and zoom feedback observe each change.
- Add actual callback, family/state, register-preservation and frame-rate
  regressions. These changes still require console gameplay acceptance.

## 0.15.1 (2026-10-31, steady RE6 shoulder framing)

- Anchor the RE6-response third-person shoulder eye to Leon's root position and
  owned look direction before native wall collision. Native profile blends and
  delayed camera-basis translation no longer move the desired framing.
- Retain the existing side, neutral shoulder dimensions, distance slider, FOV,
  collision anchor and truthful projected reticle. First person, other characters,
  Default/classic controls and special cameras keep their existing paths.
- Verify that choosing RE6 from an older reload-off configuration enables and
  saves reload walking, which is already on in the built-in preset. Custom reload
  choices remain recoverable; old files are not silently overwritten.
- Add geometric and actual callback regressions for profile drift, movement,
  turns, pitch poles, invalid matrices and view isolation. Camera comfort and
  wall behavior still require console confirmation.

## 0.15.0 (2026-10-31, illustrated graphics menus)

- Add a Graphics page to the standalone installer's RE6 tab, with filtering,
  AA and reset backed by the separate RE6 settings file.
- Add official original-game screenshot illustrations to both games' installer
  graphics pages and overlays. A focused outline identifies textures, edges,
  distant objects or shadows; reset/profile rows show the whole example scene.
- Preserve English/Russian help, reticle preview and separate game settings.
- Render overlay pictures one scanline at a time with a1280-byte conversion
  buffer. Source images, attribution and reproducible bounded conversion are
  included in the repository; pictures are illustrative, not measured mode comparisons.

## 0.14.0 (unreleased, separate RE6 graphics overlay)

- Add a separate bilingual RE6 Graphics overlay with Native defaults, focused
  help, AF2x–16x filtering and Off/FXAA/FXAA3/FXAA3HQ anti-aliasing choices.
- Preserve native RE6 controls and gyro; the requested custom gyro work was
  cancelled after the existing native motion setting was found.
- Clone eligible native sampler descriptors, keeping point/comparison modes and
  other flags intact. Filtering changes take effect after restarting RE6.
- Apply AA transiently to the verified primary render resource; preserve its
  previous native mode and leave SMAA/resource allocation unchanged.
- Add separate64-byte settings,128-byte live protocol, safe startup publication
  and saved startup settings without opening the overlay.
- Install both graphics and measurement menus; migrate owned three-file RE6
  receipts while preserving foreign files and the separate RE4 installation.
- Do not expose unverified shadow or draw-distance controls.

## 0.13.0 (unreleased, controls and gyro polish)

- Put the controls profile first, cycling with A or Left/Right between Default,
  RE6 and Custom. Default restores original controls without gyro; legacy RE9
  settings remain Custom. Preserve separate graphics and calibration settings.
- Present filtered gyro displacement on the game frame clock with10ms delay,
  removing uneven6/7-sample batch steps and flushing old motion on session,
  controller, pause and gain changes.
- Seed the verified generic firearm's original raising animation with the
  retained camera pitch, avoiding the late pitch handoff in native mode0.
- Limit right-stick vertical look to ±35° without restricting gyro's native
  range or snapping angles created by gyro.
- Add walking during ordinary generic Leon reloads, enabled in RE6. Preserve
  native collision, root movement budget and weapon animation/readiness.
- Migrate settings1..12 to wire13 with reload movement off; use shared ABI17
  with the same400-byte layout. Update module and overlay together.
- Baseline RE4 recording:1495 valid samples/34.43s, zero drops; sustained yaw
  approximately171°/s overview and56°/s aim, close to the RE6 reference.

## 0.12.0 (unreleased, RE6 controls profile)

- Add a separate RE6 controls preset to the installer and overlay.
- Use reference-based yaw rates of171.6degrees/s for overview and57.3 for aiming,
  a slower220ms aiming rise and47ms braking. Overview rise remains50ms; aiming
  pitch is provisionally53degrees/s, overview pitch90degrees/s.
- Give the RE6 response independent overview and right-stick aiming gains,
  including immediate zero-gain cancellation and safe route/profile transitions.
- Preserve existing Default/RE9 behavior and scoped Custom snapshots.
- Migrate settings1..11 to wire12 with classic response; use shared ABI16 without
  changing the400-byte layout. Install the new module and overlay together.
- RE6 capture on console is now verified:1437 valid samples without lost records.
  Matching the new preset inside RE4 remains a hardware validation step.

## 0.11.2 (unreleased, capture buffer discovery fix)

- Identify the public telemetry ring by its committed slot sequences, rather
  than accepting the producer's private copy of its header. The wrong address
  caused capture to stop with zero records and an incomplete `.csv.part` file.
- Add whole-producer-memory regression tests, including wrapped buffers and
  compatibility with the already-installed 0.11.0 game modules.
- The fix requires the updated telemetry overlay; native RE6 behavior and the
  existing game module remain unchanged.

## 0.11.1 (unreleased, capture diagnostics)

- Persist the telemetry overlay's target/build identity, capture phase, IPC result
  and recorder failure, including failures before a game journal is found.
- Generate bounded own-app support reports with installed-game metadata and
  diagnostic file inventories; download them through the existing authenticated
  read-only service. Credential files are excluded.
- Keep RE6 metadata at the start of the legacy truncated inspection report.
- This build gathers the failed-capture evidence; it does not claim the
  underlying console capture error has been diagnosed or fixed.

## 0.11.0 (unreleased, measurement instrumentation)

- Add a Build-ID/SDK/instruction-guarded read-only RE6 observer for update v131072.
- Capture exact original input and separate poll/frame timestamps in both RE4 and RE6.
- Add a bilingual telemetry overlay with tail-only Start, finite Stop snapshot,
  buffered SD persistence, drop counters and explicit capture failures.
- Keep recording while hidden; join and flush the writer before overlay unload.
- Download completed CSVs through the own installer with local authentication,
  bounded fixed-folder access and streamed SHA256 validation.
- Add native-time motion analysis and a repeatable measurement protocol.
- Actual console recordings and control-feel fitting are pending; no measured
  RE6 response or transferred calibration is claimed by this build.

## 0.10.0 (unreleased, RE6 measurement preparation)

- Add L/R game pages with a violet RE6 emblem and four RE6 home actions.
- Share language while preserving each page's selection and RE4 settings.
- Isolate RE6 installation paths and receipts; reject missing or incomplete payloads.
- Remove redundant home/description headings and use consistent16px panel spacing.
- Prepare bounded telemetry records and log encoding; RE6 native hooks await verified complete game files.

## 0.9.1 (unreleased)

- Ease unified stick velocity into motion, brake promptly and integrate by time for consistent frame-rate response.
- Limit stick-driven vertical look to50degrees each way without snapping gyro angles back into that band.
- Keep camera orientation and profile pitch independent of animated weapon sway and clipped laser endpoints.
- Project the translucent reticle onto actual displayed laser aim; it may move off-center while the view remains steady.
- Bring the RE9 shoulder camera closer at1.15; native Default remains1.00.
- Keep native shot/laser calculations and first-person weapon animations; this is not a replacement first-person viewmodel.

## 0.9.0 (unreleased)

- Add opt-in unified sticks, enabled by the RE9 profile: left-stick movement and right-stick turn/look with one sensitivity during walking and aiming.
- Restore original A/B/X/Y and PLUS/MINUS actions in the RE9 preset; retain the existing D-pad mapping.
- Keep native motion and collision budgets while redirecting movement, and drain look input across inactive contexts.
- Center supported firearm aiming on the fresh native weapon ray before generating the camera frustum.
- Draw the custom reticle at half opacity, including both live previews.
- Migrate settings to wirev11/shared ABI15; older saved profiles retain classic controls until explicitly changed.

## 0.8.0 (unreleased)

- Add a separate RE9-inspired controls profile and explicit Default / RE9 / Custom selection in both interfaces.
- Keep the last Custom controls when applying built-in profiles; graphics, language and calibration remain independent.
- Add third-person camera distance1.00–2.00, default1.00; RE9 uses1.35 to give Leon more room in the frame.
- Extend the native shoulder eye before wall collision while keeping its anchor, view direction, FOV and roll; first person and scopes retain their own views.
- Combine free360 camera, moving aim/knife, gyro and a crosshair with RE9-inspired physical buttons using RE4's default control layout.
- Document shared native-button and unsupported-action limitations rather than promising RE9 mechanics.
- Migrate old settings into wirev10 without changing their framing; use shared ABI14 with the same400-byte block.

## 0.7.3 (unreleased)

- Fix the native knife-held input mask used by movement and analog knife aiming.
- Test real held input0x800 in stance and strike, including gait/translation before hit processing and native collision.
- Reject unrelated high input bits and preserve knife-off, release and firearm behavior.

## 0.7.2 (unreleased)

- Discard gyro motion during native weapon raising, including the first ready FIFO batch.
- Keep gyro active through firing/recovery; invalidate old input on reload, weapon changes and paused contexts.
- Recheck readiness at aim consumption without arming from a mid-frame transition.
- Average readings10ms apart continuously at sensor cadence, avoiding alternating zero/10ms bursts.
- Preserve additive sticks, gyro direction, soft deadzone and the native manual-input sway baseline.

## 0.7.1 (unreleased)

- Group control settings into seven submenus in installer and overlay.
- Keep language selection on the main menu only.
- Show a live target-picture reticle preview for shape and size, including Off.
- Verify every preview shape against actual native HUD rectangles and rendered pixels.
- Include the startup hook-pool capacity and boundary fix confirmed on hardware.

## 0.7.0 (unreleased)

- Fix startup abort at hook22: enlarge the staged trampoline pool to40 slots,
  correct its last-slot bound and assert the actual install count at build time.

- Add independent Default/Custom profiles for controls and graphics, with automatic
  Custom snapshots, migration and recovery through the existing settings codec.
- Preserve the other domain, language and gyro calibration when switching profiles.
- Open Controls and Graphics as peers in the overlay root menu.
- Test actual profile UI events, restore and failed writes; fix persistent feedback.
- Add an optional first-person camera with continuous look and native scope/scene ownership.
- Add an optional dot/crosshair reticle using native HUD primitives and the displayed weapon ray.
- Add optional native-budget movement and masked leg animation during Leon's ordinary aiming/knife states.
- Keep native animation cursor banks isolated during private gait sampling and preserve IK/state restoration.
- Route the right stick to aiming independently of movement input and exclude knife stance from gyro aiming.
- Migrate settings v1-v8 into v9 with all new modes Off, shared ABI13 and diagnostic counters.

## 0.6.0 (unreleased)

- Add a bilingual Graphics menu with exact Native defaults and graphics-only reset.
- Add restart-latched bilinear/trilinear/AF2x-16x texture filtering without extra sampler-cache variants.
- Add restart-latched handheld1.25x SSAA (900p to720p) through the game's native allocation/scaler path; dock stays native.
- Add loaded-object culling distance0.50x-2.00x with native fog, projection and streaming unchanged.
- Add rendered-shadow Native/Off with native pass cleanup and flag restoration.
- Preserve controls/mapping/camera when migrating settings v1-v7 into v8.
- Publish render settings atomically, load saved settings after successful ROM mount, and expose graphics diagnostics in shared ABI12.

## 0.5.0

- Add complete 20-button game mapping to installer and overlay, including menus/QTE.
- Support duplicate targets, swaps, cycles, unbound buttons and mapping-only reset.
- Add camera return speed and continuous right-stick 360-degree free camera.
- Preserve native camera collision/smoothing and exclude aiming, pause/reset and scripted camera paths.
- Set new/reset gyro aiming speed to 0.80; other sensitivity/speed defaults remain 1.00.
- Migrate 64-byte settings v1-v6 into strict 128-byte v7 without resetting saved controls.
- Expand core, actual game callback, bilingual UI and rendering tests; bump shared ABI to 11.

## 0.4.2

- Restore saved gyro enable/sensitivity after restarting RE4 without toggling the overlay.
- Grant explicit SD access and retry startup reads at most eight times, one second apart.
- Apply saved settings when opening the overlay; live changes take priority over delayed reads.
- Add startup source/error diagnostics and regression coverage for saved Off, backup recovery and sensor activation.
- Fix the forwarder build path guard for canonical Windows short/long directory names in CI.

## 0.4.1

- Prepared the first public release, documentation and source/artifact audits.
- Build the NRO RomFS from a fixed allowlist, removing stale local payload files.
- Keep normal app and SD package builds independent of personal console keys.

## 0.4.0

- Added automatic own HOME shortcut installation and icon refresh.
- Added gyro aiming speed, a configurable angular deadzone and10ms two-sample averaging.
- Fixed wireless Joy-Con sensor selection and recovery after transient sensor failures.
- Process stick aiming before native reticle, animation and sway updates.
- Added separate right-stick aiming X/Y settings; reconnect reapplies saved settings.
- Added a bilingual Updates page for GitHub Releases with verified downloads and rollback.
- Added first-launch app image installation and protection against accidental downgrades.
- Expanded tests for actual game callbacks, installer transactions, OTA and public packaging.

## 0.3.0

- Added English/Russian menus, four stick response curves and a themed graphical installer.
- Removed the walking-speed slider; retained proportional movement at the native maximum.
- Set stick deadzones to0.05 and both gyro inversions to on by default.

## 0.2.2

- Confirmed proportional movement/camera/aim and additive firearm-only gyro on console.
- Fixed vertical gyro range, added focused-setting descriptions and native stick speed caps.
