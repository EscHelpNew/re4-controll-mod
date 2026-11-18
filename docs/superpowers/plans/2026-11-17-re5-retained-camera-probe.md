# RE5 retained camera observation implementation plan

> **For agentic workers:** Use superpowers:executing-plans for root integration; delegated native and overlay work has separate file ownership. Steps use checkboxes.

**Goal:** Retain exact callback rejection, raw right-Y input and component poses so the next camera correction follows evidence.

**Architecture:** Append an optional5808-byte ABI1 observation after the unchanged384/112/192-byte core blocks. Existing opt-in Monitor request leases authorize nonblocking native observations; the overlay reads a stable authenticated snapshot and transports its complete bytes as base64. A standalone host decoder labels stages and preserves float bits.

**Tech Stack:** C++17/20, devkitA64, Python standard library, existing authenticated Switch diagnostic transport.

**Spec:** This plan records the agreed diagnostic scope: user-enabled monitoring, disabled by default, camera25 and physically working combat movement unchanged. Evidence: `.superpowers/sdd/2026-11-17-re5-camera26/camera-live-165927.jsonl`.

## Global constraints

- Version0.28.26 is diagnostic only; no camera sign, world guard, gait, profile or hook changes.
- Camera wire marker RE5CTL_CAM1, ABI1, bytes5808, offset688. Native identity guard count924 and49 hooks unchanged.
- Existing2-second lease; no new threads, server, always-on logging or blocking locks.
- Core Shared384, Identity112 and Runtime192 remain compatible. Missing optional block must not prevent connection/settings.
- Per-instance overlay buffers; JSON at most16383 bytes plus NUL in the16KiB transport; diagnostic worker stack64KiB.
- Preserve RE4 and APP protected hashes. Local package only; deployment BRIDGE+GAME PUT without RUN, followed by manual relaunch/install.

## Review focus

- Disabled monitoring must perform no new native reads/locks/copies.
- Callback observations must preserve evaluation order, original native results and lock cleanup.
- Changed process/build/sequence, missing optional block and truncated RW extent must fail observation safely.
- Two-player maximum diagnostic output must fit transport and stack bounds.
- Decoder must reject corrupt/truncated/oversized input while retaining exact numeric bits and route attribution.

### Task1: Native capture (native owner)

Files: include/re5/camera_probe.hpp; source/re5/module.cpp, runtime.inc, shoulder.inc and camera_probe.inc; tests/re5_camera_probe_tests.cpp and re5_module_tests.cpp.

- [x] Observe new recorder/actual callback tests fail before adding records.
- [x] Implement stable optional export and one-try-lock retained input/decision records after existing lock release.
- [x] Test Off path, lease expiry, retained nonzeroRY, callback reason/world stages, both players and native versions; confirm gameplay delta is observation only.

### Task2: Overlay reader (overlay owner)

Files: source/re5_controls/game_link.cpp/.hpp, input_probe.inc, camera_probe.inc; tests/re5_game_link_tests.cpp.

- [x] Independently construct raw5808-byte fixture; observe missing-reader failure.
- [x] Add bounded read with identity/sequence revalidation and complete base64 wire envelope.
- [x] Verify absent/corrupt/changing/PID/title/boundary cases and maximum JSON/stack.

### Task3: Root wire tooling

Files: tools/re5_module.py, tools/re5_camera_probe.py, tools/test_catalog.py; tests/test_re5_module.py, tests/test_re5_camera_probe.py.

- [x] Add linker assertion test for offset688/size5808, run RED, then append section.
- [x] Add independent raw-offset decoder fixtures for input, decisions, two players, reasons, matrices, pose, NaN bits and malformed envelopes; run RED.
- [x] Implement decode_camera_probe(raw: bytes) -> dict and decode_snapshot(snapshot: dict) -> dict with strict bounded wire validation, CLI for JSON/JSONL including watch data wrapper; run GREEN.
- [x] Register recorder C++ test in existing catalog.

### Task4: Integration and delivery

Files: include/re4/version.hpp; source/overlay/game_link.cpp known optional observation versions; tests/settings_v7_tests.cpp, tests/game_link_tests.cpp; CHANGELOG.md, docs/re5-controls.md.

- [x] Bump26 and optional RE4 known-version cap; reject future27 observation while core remains compatible.
- [x] Document actual25 movement acceptance and remaining camera failure, diagnostic26 purpose and full installation.
- [x] Obtain independent code review, check protected hashes, run complete host tests and ARM module/overlay/NRO builds.
- [x] Package, make local release and public-file audit; record exact bytes/SHA and pending hardware validation.
- [ ] Request local update only when ready; authenticated BRIDGE+GAME PUT, save each ACK; never RUN. User manually relaunches/installs/restarts game and overlay before further capture.

Integration evidence: `.superpowers/sdd/2026-11-17-re5-camera26/verification26.json`. Host142/142; SDK/package/release/audit0. Native/reader/root freeze sets unchanged. Hardware camera fix pending. Authenticated APP local-update service timed out; requested user to enable it. No PUT or RUN performed.
