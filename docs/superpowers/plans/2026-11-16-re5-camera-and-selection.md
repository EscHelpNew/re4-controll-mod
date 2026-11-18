# RE5 camera and two-game selection implementation plan

> **For agentic workers:** Use systematic debugging and existing callback fixtures; independent UI and native scopes run in parallel by the user's explicit request.

**Goal:** Hide RE6 from application selection and implement the previously requested RE5 camera/character turn together.

**Architecture:** Preserve numeric GameTarget values, saved configurations and installed RE6 files; expose RE4 and RE5 tabs only. Replace orbit-only yaw with an authenticated local-player native heading update at the qualified actor world-transform stage; construct camera placement from the same character orientation. Exact latest/legacy identities, local ownership, scope/freeze/action guards and nonblocking snapshots remain required.

**Tech Stack:** Existing C++17 installer/SDL UI, C++20 native ARM64 hooks, actual callback fixtures and opt-in authenticated RE5 diagnostics.

**Spec:** Existing docs/superpowers/specs/2026-11-11-re5-controls-design.md plus user confirmation that camera and character turn together; latest instruction hides RE6 from selection. No new user design decision is required.

## Constraints
- Preserve working RE4 HP, rendering, game hooks and configuration.
- Do not change APP startup/teardown or mix the private lifecycle prototype into this fix.
- Both local players; reject AI/network actors and stale or unqualified input.
- Actor quaternion is at +0x70; never treat it as Euler angles or rotate the actor from the camera worker.
- Qualify setter/getter and execution point against both exact native builds before production changes.
- Version 0.28.21; no automatic restart or SDK RUN; user manually reopens after direct authenticated PUT.

## Tasks
- [x] UI: RE4/RE5 navigation wraps both ways, restores independent settings/player state, leaves draft/update guards intact; render two centered red/gray tabs. Update obsolete RE6-navigation tests while retaining independent backend checks.
- [x] Native diagnosis: prove current orbit-only cause and qualify actor update with original-image evidence, then reproduce the missing body turn in a failing callback test. Audit action movement failures separately rather than treating an unproved hook as working.
- [x] Implement the smallest qualified native heading/camera change. Check neutral input, dt/generation, scope/aim/free-camera off, scene transitions and both players.
- [x] Root: keep RE4 diagnostic version trust in sync; run affected tests then complete host suite, fresh native review, ARM modules/overlays/NRO, package and secret audit.
- [ ] Console: obtain current opt-in monitor snapshot, deliver only after saved/closed game and enabled manual server; full RE5 mod installation and manual game restart, then verify actual camera/body behavior. Host success alone is not console acceptance.

## Qualified native camera pipeline

The actual prior implementation rotates only eye around a fixed target and never turns the actor. The accepted RE4/RE6 semantics therefore require a native actor stage, not a further orbit adjustment. Both exact images qualify the common world-stage join `24DDE0 -> 24DC00` (actor X19), after native ground-tilt quaternion stores and before neck/world/bone processing. Call native Euler getter `D28730 -> D285D0`, scalar yaw setter `D29000 -> D28EA0`, and local-matrix builder `D29060 -> D28F00` there; preserve non-yaw components. The getter uses a verified nontrivial return ABI with hidden X8. Protect 594 added immutable words per image: 580 instruction words including the transitive Euler setter, plus 14 read-only jump-table/literal words; retain all 303 prior guards (897 total). Actual-image mutation checks reject all 1188 independent corruptions, with zero intersections across each image's 248023 DT_RELA entries. Native movement and model rebuilding retain their own scheduling and collision.

UI focused checks and real EN/RU SDL previews passed; root visually reviewed both retained RU frames. RE4/RE6 version21 modules built successfully before RE5 production changes. RE4 diagnostic known-version metadata has its own verified RED/GREEN acceptance for21; the HP/native/render sources remain unchanged. Native camera implementation, independent review and combined full-suite/ARM package checks remain pending.

Corrected revision2 freezes the runtime after two independent callback regressions: ownership loss before the first active anchor now invalidates the accepted input epoch, and a missed camera restore cannot replace its pending native baseline. Actor/anchor state and camera restoration use separate nonblocking locks; the camera acquires pose then anchor. Four focused cases pass on this revision. Full host suite and Switch builds are running; independent reproduced-case rechecks remain pending.

Final integration: revision3 and the independent raw IPC fixture updated to897 words pass the full141/141 suite (`build/tests/run-thi0nnxq/results.json`). All Switch modules, overlays and NRO build successfully; local package/release and source/history/package audit pass. Independent review replays both timing regressions and75 geometry cases successfully. All frozen runtime/profile/UI and17 protected source hashes match. Native gameplay acceptance remains pending; no console21 transfer or restart has been requested.
