# Localization, response curves and installer implementation plan

> **For agentic workers:** use superpowers:executing-plans inline; one final fresh-context review.

**Goal:** Add the requested bilingual controls, response choices and themed installer.

**Architecture:** Share settings, response math and menu metadata between the
game adapter, overlay and standalone installer. Keep drawing separate from the
portable installer model. Preserve the verified remote-reload and file guards.

**Tech Stack:** C++17/20, libnx/devkitA64, installed SDL2, stb_truetype, Python tests.

**Spec:** docs/superpowers/specs/2026-10-29-localization-curves-ui.md

## Global constraints

- Current root repository; no new hidden worktree.
- English default; both inversions on; deadzones0.05; no walking-speed menu.
- Four monotone bounded curves; native speed maximum1.00.
- No gyro curve shaping or relaxation of firearm-only gating.
- NRO transfer exclusively through Switch Debug Kit.
- The installed Uberhand and foreign SD files remain protected.

## Review focus

- Old settings migrate without misreading new flags or retaining hidden slow-walk presets.
- Live language changes update labels, values, category headers and help consistently.
- Curve selection reaches every stick-controlled game path and never exceeds native speed.
- Installer navigation cannot accidentally trigger install from another page.
- Receiver main-thread polling and graphics/font cleanup work on every exit/failure path.

## Tasks

###1: Settings and response core
- [x] Add failing tests for defaults, wirev4 migration, invalid enums and four curves.
- [x] Implement flags, curve function and native walking maximum.
- [x] Wire settings.curve into game stick processing; bump shared ABI.
- [x] Run native/host suite and cross-build.

###2: Shared bilingual controls and overlay
- [x] Add failing metadata and real-overlay tests for EN/RU switching/no walk item.
- [x] Add include/re4/control_menu.hpp and source/control_menu.cpp.
- [x] Build overlay from metadata, updating translated labels/help immediately.
- [x] Run actual UI/IPC tests and overlay cross-build.

###3: Installer navigation model
- [x] Test page navigation, actions, settings edits and language persistence.
- [x] Add include/re4/installer_ui.hpp and source/diagnostic/installer_model.cpp.
- [x] Keep dangerous filesystem operations in the existing guarded application adapter.
- [x] Run tests.

###4: Theme renderer and application integration
- [x] Add source/diagnostic/installer_renderer.cpp with SDL2 and cached font glyphs.
- [x] Add tests/installer_preview.cpp to render actual EN/RU screenshots on Windows.
- [x] Inspect previews and fix clipping/layout.
- [x] Integrate rendering with existing NRO input/sensors/reload lifecycle.
- [x] Cross-build and verify imports/container assets.

###5: Review, delivery and hardware acceptance
- [x] One fresh-context whole-change review; fix concrete critical/important findings.
- [x] Full tests, build, package and credential/path checks (17 Python, 13 native).
- [x] Commit own source, tests and assets (01002fb).
- [x] Deploy paired NRO through existing library; user confirms themed installer and mod installation.
- [x] Include updated HOME icon package and transfer it through user's DBI (FTP226).
- [x] User confirms in-game EN/RU, four curves and removed walk-speed menu.
- [ ] Resolve stale HOME icon after user-confirmed DBI installation.
