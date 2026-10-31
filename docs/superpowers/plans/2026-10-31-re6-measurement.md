# RE6 measurement implementation plan

> **For agentic workers:** Use test-driven implementation, independent-domain delegation and a fresh final code review.

**Goal:** Add the requested four-action RE6 installer page and trustworthy synchronized game telemetry.
**Architecture:** GameTarget selects isolated installer artifacts. A Build-ID-guarded RE6 observer captures bounded records; a verified buffered writer persists them without frame-callback I/O.
**Tech Stack:** C++17/20, libnx/SDL/Tesla, native ARM64 observer, Python analysis/tests.
**Spec:** `docs/superpowers/specs/2026-10-31-re6-measurement-design.md`.

## Global constraints / review focus

- No release/tag, personal tokens/keys, extracted game material or logs in Git.
- All new files remain in this repository; RE4 behavior and files stay isolated.
- L/R selection cannot trigger installation, calibration or old QuickInstall.
- RE6 offers exactly install module/overlay, language and exit in both languages.
- Unknown builds, missing payloads, interrupted writes and lost samples are explicit failures.
- Native addresses and SDK signatures require original-binary proof and callback tests.

### Task1: installer page

Interface: `re4::GameTarget { RE4, RE6 }` in `game_target.hpp`, model `game` field;
events `PreviousGame`, `NextGame`. Renderer accepts optional RE6 bitmap after
the existing initialize arguments. Root wires pad events/build/install dispatch.
- [x] TDD model navigation, language sharing, no install on L/R and four RE6 actions.
- [x] Render violet6 page with EN/RU SDL pixel previews, preserving RE4 screens.

### Task2: identify native measurements

- [ ] Extract small RE6 update ExeFS locally using existing private tooling.
- [ ] Verify title/Build ID, input consumers, player/camera pointers and per-frame order.
- [x] Verify hidden overlay lifetime and choose its writer thread; no native game SDK filesystem/thread injection.
- [ ] Confirm installed console Build ID through own installer inspection.

### Task3: telemetry and installation

- [x] TDD bounded record/ring/cursor/CSV encoding, timestamps, torn reads, invalid groups and real sample gaps.
- [ ] Implement overlay writer lifetime, SD persistence and write-failure reporting.
- [ ] Implement verified RE6 observer/profile and overlay with no control modifications.
- [x] TDD isolated RE6 installer receipts, conflicts, missing payloads and target separation.
- [x] Wire both logos, L/R input and installed-version inspection; reject partial future payload groups.
- [ ] Wire verified native RE6 artifacts into build/RomFS/package once the observer exists.

### Task4: delivery and measurement

- [x] Preparation stage: full host/ARM/SDL tests, independent review and secret audit.
- [ ] Commit preparation source to main, verify CI and deliver installer pages.
- [ ] Library-only console delivery, install/profile validation, real measurement-log capture.
- [ ] Analyze displacement and camera angular response against raw stick/time records; no invented measurements.
