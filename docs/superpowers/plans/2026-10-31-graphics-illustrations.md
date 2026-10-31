# Graphics menu illustrations implementation plan

The user asks for graphics settings in the standalone mod menu and real game
pictures explaining each graphics option in RE4 and RE6. Continue in this repo;
the earlier design-question opt-out remains in force.

Use the existing SDL installer and libtesla overlays. Add a Graphics action to
the RE6 installer page, editing only the separate RE6 configuration; retain the
existing RE4 Graphics action. Keep native defaults and native RE6 gyro.

Illustrations use genuine original-game screenshots, with an outlined region
showing where a selected option matters. They are labelled as illustrative
scenes, not captures of the selected mod setting. Do not synthesize before/after
results. Record source URLs, copyright and file hashes. Prefer official
Nintendo/Capcom imagery. Preserve images' content; format conversion and UI
cropping/region markers belong to the presentation pipeline.

Provide illustration topics for RE4 filtering, AA, distance and shadows, plus
RE6 filtering and AA. Profile/reset rows show an overview. Scope excludes new
unverified engine hooks, RE6 shadows/distance, custom RE6 gyro and GitHub releases.

- [x] TDD RE6 installer graphics navigation, persistence and RE4/RE6 isolation.
- [x] Obtain real screenshots and implement deterministic bounded RGB565 asset conversion with source attribution.
- [x] TDD shared topic/region metadata and row-streamed preview drawing (no full-frame RGBA heap allocation).
- [x] Add pictures to both SDL graphics pages and both overlays; retain focused descriptions and scrolling.
- [x] Verify English/Russian previews, missing/invalid asset guards, full tests and Switch builds.
- [ ] Independent review, actual-secret audit, main/CI and library-only console update; no GitHub release/tag.

Use320x180 RGB565 illustrations. Overlays convert one scanline to RGBA8888 for
libtesla, bounding extra stack memory to1280bytes. SDL caches textures. Pictures
are illustrative references, independent of the current selected option value.
