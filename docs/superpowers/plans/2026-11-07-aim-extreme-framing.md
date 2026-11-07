# Extreme aim framing correction

> **For agentic workers:** Execute inline with failing callback regressions before production changes.

**Goal:** Keep the truthful shoulder-view aim marker visible at maximum upward/downward aiming without narrowing the weapon's native pose range or changing accepted ordinary camera framing.

**Architecture/design:** Native aim input[-1,1] is a secondary animation blend weight, not a proven physical muzzle angle of+/-90degrees. Camera18520 interpolates weapon-specific QFPS framing. Profile0's extreme native view directions are+62.568607/-60.142346degrees; neutral is-9.733629degrees. Read the already selected/blended native output once after Orig. Resolve view elevation as nativeElevation-neutralElevation*(1-abs(command)/(pi/2)), preserving zero look and the full native framing endpoints. Read the current/previous neutral records through bounded read-only memory validation and native254 blending. Use one effective pitch for the pre-collision shoulder arm and final view/up vector, while command/native weapon pitch and projectile/HUD geometry remain unchanged.

**Alternatives:** Artificially limiting weapon input removes native aiming range. Clamping the marker to the screen makes it untruthful. Following the clipped hit point would move the camera with walls/recoil. Direct native framing changes neutral aim. Keeping the entire35degree band before blending can reverse camera pitch with profile2(whose maximum is28degrees); a regression caught that. The selected neutral-anchored native curve is monotonic across tested native profiles, and preserves zero and the endpoints.

**Scope:** Qualified Leon RE6 third-person ordinary weapon aim(routine6), not walking/reload/knife/FPS/scopes/special cameras. Native wall and near-body collision still own final eye. No extra Orig calls, asset changes or ABI changes.

## Tasks and review focus

- [x] Reproduce offscreen endpoint at full command with native up/down profile geometry and realistic projection; show both reticle visibility and native pose range.
- [x] Add helper validation/continuity and actual callback tests for both signs, yaw, distances, collision, selected profile variation, and negative contexts.
- [x] Implement the shared effective view pitch; keep camera token ownership valid when command pitch differs from view pitch.
- [x] Run full tests, module/overlay/NRO build, fresh review, package/public audit; local version0.18.6.
- [x] Transfer to the already authorized console when app is open; do not claim console visual acceptance before the user's check.

## Evidence

- User confirms0.18.5 is good and this bug occurs in ordinary shoulder view.
- Hash-verified native33E674/319020 clamps blend weight and selects up/down animation resources; laser2E8EF0 and shot31DDD0 use actual attachment matrices.
- Native profile0 atAE2AB0: up eye(-527,600,-680),target(-220,4080,1100); neutral eye(-470,1715,-615),target(-200,1345,1525); down eye(-393,2058,-5),target(-179,365,943).18520 returns current selected/interpolated geometry;17F40 does not add pitch while aiming.
- Ruling: replace the first extreme-only comfort-band blend with a neutral-anchored native curve. Profile2's physical camera maximum is below35degrees, so preserving that entire band necessarily reverses the camera before the endpoint. The failing monotonic regression demonstrates this. Current curve preserves the center value; vertical response now follows each weapon's native framing rather than the old unphysical90degree assumption.
- RED/GREEN: original full-command callback failed projection of a native-frame target. Initial comfort-band helper failed profile2's monotonic sweep. Current helper/callbacks pass. Added actual HUD scene/draw promotion and intermediate pose weights0/.4/.75/1, clipped eyes, and consecutive-frame mapping reset.
- Fresh review found the broad native-aim predicate could admit reload phase4. A readable-profile negative callback failed, then passed after restricting correction to routine6 phases1/2. Raise, knife, FPS, nonRE6 and ordinary walking are also excluded by regression. Reviewer found no further important issue.
- All156 native instruction guards match the hash-verified executable. Initial full tests and ARM/overlay/NRO build passed. Final full tests and module/NRO rebuild run after the phase guard change, with the already-built same-version overlay retained.
- Final verification:build.py --test exited0 (`build/aim-extreme-final-tests.log`); final ARM module/NRO build exited0 (`build/aim-extreme-final-build.log`).32/40hooks retained. Package/public release prepared; audit585working files/1174history blobs, no findings. Final NRO SHA256 `d0fd8ecf04c663269961a687ddf3f6092d252eb9c3bd6624a801ecd4c312a825`.
- User explicitly opened RE4 Controls and authorized transfer. Status verified0.18.5-p8D on192.168.1.131; BRIDGE upload hash verified. GAME upload/restart in progress; game mod installation and visual acceptance not yet claimed.
- Delivery completed:GAME restarted as0.18.6-p8E; protocol/title identity and exact final NRO SHA256 verified. User must select RE4/install-update to replace the game module and restart RE4. New extrema behavior awaits their console check.
