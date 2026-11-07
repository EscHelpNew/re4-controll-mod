# Aimed enemy health bars

> **For agentic workers:** Implement with TDD and actual native/UI adapter fixtures. Independent native analysis and UI work may run in parallel with bounded file ownership.

**Goal:** Add a persistent Game/Игра menu toggle for a translucent proportional health bar above the enemy currently targeted by the player's aim.

**User requirements:** Green above50%HP, yellow at50%through above25%, red at25%or less. The user chose only when aiming at an enemy, not bars on every visible enemy. Menu: Игра → Полоска здоровья врагов. Default off. Width represents current/max health; no invented maximum.

**Architecture:** Read-only verified Switch-v65536 active enemy identity/current/maxHP and native aiming-hit selection. Reuse the game's existing hit trace so walls/props cannot reveal a target behind them. Snapshot a bounded live target and world top anchor during scene submission; promote it with the displayed scene/camera, as with the existing truthful reticle. Draw a compact bar through the established native GX alpha HUD backend. Toggle works independently of controls/graphics profiles. Dead/friendly/invalid/stale/offscreen targets produce no bar.

**UI/settings:** Settings.enemy_health_bars=false. Wire14 keeps128bytes, uses reserved104 for a validated boolean; allv1..13 migrate off. Game setting is preserved by Controls/Graphics resets and profile switches. Add Game page to overlay and installer (append home slot8 before Exit9, preserving existing0..7). Shared ABI18/marker18 prevents mixed old/new menus/modules. No new control-profile preset or general gameplay-options framework.

**Visual choice:** One small horizontal health bar above the selected enemy, translucent fill and dark background. Health thresholds use exact rational integer comparisons. No health text or damage numbers requested.

## Tasks

- [x] Verify native manager/list/HP/max/death/friendly/aim-hit/top-bound data and guards before implementing native reads.
- [x] Add failing settings migration/profile-preservation and bilingual overlay/installer navigation/save tests, then settings and Game menu.
- [x] Add pure snapshot/color/projection/GX-alpha tests, then bounded health bar model and rendering.
- [x] Add actual module regressions for native target selection, target/wall changes, shot damage/death/reuse, scene delay, hide/off/pause/camera behavior and valid pointer bounds.
- [x] Full tests, module/overlay/NRO build, independent review, package/public audit, local version0.19.0.
- [ ] Transfer to user's console when RE4 Controls is open. Actual gameplay acceptance remains pending until tested on Switch.

## Evidence and rulings

- Existing HUD pipeline already stages native laser endpoints to the displayed scene and projects into512x448; alpha blend wrappers exist and can draw bars without altering world geometry.
- Native facts verified against original NSO hashes and analysis image: manager AE6EF8 pool+8/count10/stride14=10C8, active flags mask201==1, rejectbit28, signed HP3C2/max3C4. Spawner1CD200/204 initializes both;1D17F4 stores damage; native enemy accounting uses type118 range10..4F.
- ModelSubmit alone does not prove visibility. Native aim-hit identity should avoid extra physics queries or raw visibility guesses.
- Fresh WeaponLaser Original calls1D1980, clears weapon418, clips against world then selects nearest live body hit. Capture reads that authoritative owner after Original; cached reusebit0 calls never republish. No extra native hook or trace is added.
- The anchor scans up to255 linked parts(count11A/head108/next108/world80), omitting native-hidden bit1 parts, with read-only query cache and cycle/finite guards. Render stage receives packed HP and world anchor/matrices only, not enemy pointers.
- Native RGBA queue24FD80→23A490→2386A0 preserves exact(r,g,b,a)/255; untextured shaders pass vertex colors unchanged. Fillalpha164/background132; integer thresholds handle exact50%/25% correctly.
- RED/GREEN evidence: wire14 expectation failed before codec change; profile-preservation test failed before preserving Game in Controls copies; actual health callback failed before native integration; reload exclusion and hidden-parts cases failed before their guards. UI agent observed missing Game/root count failures before implementing menus.
- Separate UI review and fresh whole-feature review found no important issue. Initial full-suite failure was an obsolete test treating now-valid wire14 byte104 as reserved; updated strict-version test to retain rejection for old formats.
- Scope/throwable coverage: only fresh native target traces are consumed. Separate native scope and throw paths may not emit this trace; no stale target or synthetic extra physics is substituted. Hardware appearance remains unverified.
- Final verification: full build.py --test exited0 (`build/enemy-health-tests.log`), module/overlay/NRO exited0 (`build/enemy-health-build.log`),180nativeguards all match. Fresh review's minor ordering coverage gap was filled with a native-Orig fixture that overwrites/clears target418; targeted actual adapter test passed afterward. Game page EN/RU rendered and visually inspected, with explicit color legend and no unrelated stick preview.
- Package/release0.19.0 prepared; audit592working files/1177history blobs/no findings. NRO SHA256 `cf9ea20cfd7f6d8cd4e8478c88c39044bc1a8c6812cd9e48ffca81446463c9a4`.
- User reiterated defaultOff; new settings and v1..13 migration are Off and verified. User opened RE4 Controls and authorized upload; status0.18.6-p91/title verified on192.168.1.131. Transfer/restart in progress; game module installation and visual acceptance not claimed.
-0.19.0 delivery completed and verified as0.19.0-p92, hashcf9ea20c... . Subsequent user refinements belong to0.19.1, not that already deployed binary.

## User refinements for0.20.0 (supersedes unbuilt0.19.1)

- Game must be immediately after Graphics in both menus. Installer Game now6, Updates7, Language8, Exit9; earlier keep0..7 design is superseded by this explicit instruction. Overlay already has that order. Installer navigation RED/GREEN verified.
- Strip becomes18x2native pixels (halfwidth, one-third colored height), with no black outline/background.
- Fade in/out about300ms. Pure bounded four-target fade model implemented and tested inenemy_health_fade.hpp; native integration is complete with actual callback regressions. Preserve latest displayed camera while fading copied last-known world anchors; no enemy dereference in HUD. Fade state belongs to render thread; use atomic settings-generation notification instead of game-thread mutation.
- User asked for preview before next delivery. Rendered actualbar/fadecode over existing official1280x720scene into build/health-bar-preview.png/gif (UI mockup, not console capture). User approved general appearance, then requested slightly less transparency: final peakalpha102(~40% opacity) instead of preview82(~32%).
- Native integration requires an opaque target identity propagated atomically with HP/anchor, current displayed-camera snapshots even after aim release, and tests for0/150/300ms opacity plus target loss/reacquisition. DefaultOff remains unchanged. Version0.20.0 includes these refinements and optional numbers; not yet deployed.

- Optional numbers use canonical150rawHP per weapon firepower unit, proved against native damage/stat lookup and guarded at startup. Integer rounding to tenths; fixed decimal point in both languages per user feedback. Numbers defaultOff, unavailable while barsOff; latent preference survives toggles/profiles.
- The first numeric mockup showed false horizontal gaps because adjacent fractional raster rows overwrote coverage in the preview compositor. Combining coverage fixes it; native glyph quads are continuous. Updated1280x720mockup and4xdetail show5.0 at alpha102 using actual backend geometry. These are not console captures.
- Fresh independent review found no confirmed issue in native snapshots/fades, codec, UI gates or health units. Full tests caught an input fixture allocating only to the buildID while new guarded data extends farther; fixture now allocates through the maximum guard.
- Final0.20.0 verification: full build.py --test exit0 (build/enemy-health-020-tests.log); ARM64 module/overlay/NRO build exit0 (build/enemy-health-020-build.log),32/40hooks; package/release prepared. Audit597working files/1208history blobs/no findings. NRO SHA2566f02cd50bae7a35754e2d58e1c99430755bf0a07750151b1197cd9db9b185e2a. Not uploaded; user is reviewing appearance.

## Approved interactive menu preview

- User approved the numeric appearance, requested the same close-up in Game settings with instant toggles, and explicitly requested upload to the already-open console app.
- Reuse the unchanged attributed RE4 source crop(415,105,640,300), generated256x222RGB565. Composite actual bar/number backend geometry per settings; both menus use the same bounded opaque-row helper, no new game hooks/settings/ABI.
- SDL caches its texture by effective toggle state, enlarged to344x298. Overlay centers the256x222example above its navigation footer. A review caught the initial277pximage overlapping the real Tesla footer; reduced it and added explicit footer bounds checks.
- Validation: actual overlay draw/navigation test; SDL renderer pixel differences confined to strip/number rectangles, with latent numbers hidden while barsOff; full host checks and ARM64 build before packaging and console transfer.

- Menu preview final validation: full build.py --test exit0(build/enemy-health-preview-tests.log); full ARM64 module/overlay/NRO build exit0, final RE4 overlay/NRO rebuild exit0(build/enemy-health-preview-final-build.log). SDLscreens visually inspected. Fresh review confirms footerclear(imageend617,caption636,footer647). Package/release0.20.0prepared, audit598working files/1212history blobs/no findings. FinalNROSHA2563d626fd2c5e515aac607c4334197d1174d40384b8b333a44cc89e603ca3993b2. Transfer/restart in progress; module installation not claimed.
- Delivery verified on192.168.1.131:0.20.0-p96, protocol2, title/bridge compatible and exact SHA2563d626fd2c5e515aac607c4334197d1174d40384b8b333a44cc89e603ca3993b2. User must choose RE4 install/update to replace paired game module/overlay, then restart game.
