# OTA and public release implementation plan

> Execute inline with superpowers:executing-plans; review with the gyro/HOME change.

**Goal:** Let future users update the installed app from GitHub Releases.
**Spec:** docs/superpowers/specs/2026-10-29-ota-release-design.md
**Architecture:** Strict portable release manifest/transaction core, HTTPS transport and worker, EN/RU UI.
**Tech:** C++17/libnx/libcurl, existing protocol2 validators, Python release tooling, GitHub Actions.

## Tasks
- [x] RED/GREEN: manifest, versions, file size/hash/NRO validation, paired publish/rollback.
  Files: include/re4/update.hpp, source/update.cpp, tests/update_tests.cpp.
- [x] Add HTTPS worker and Updates page; test navigation/cancellation/status/permissions failures.
  Files: source/diagnostic/update_worker.cpp, installer_model.cpp, installer_renderer.cpp, main.cpp.
- [x] Add release manifest/package generator tests, README EN/RU, changelog, CI and issue templates.
  Files: tools/release.py, tests/test_release.py, .github/, docs/.
- [x] Full suite/cross-build and fresh whole-change review; right-stick aim qualified on hardware (0.4.0).
- [ ] Commit final 0.4.1 source and release tooling.
- [ ] Merge tested changes into main (explicit user instruction), push main and publish requested allowlisted release from main; verify remote hashes/downloads.
- [ ] Exercise in-app update against the published release; private-repo failures remain clearly reported until public.

## Review focus
- HTTPS remains verified through redirects; no keys/credentials in public artifacts.
- Cancel, short downloads, bad hashes and partial publication preserve the old launchable app.
- Main-thread polling and worker cleanup remain compatible with the pinned reload library.
- No silent downgrade, duplicate action during download or write outside the own SD directory.
- Public docs distinguish tests and actual console acceptance from untested contexts.

Anonymous OTA hardware acceptance is deferred at the user's request: publish the release while keeping the repository private. Transport, validation, installation and rollback have automated coverage.
