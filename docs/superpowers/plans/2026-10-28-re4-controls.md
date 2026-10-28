# RE4 controls implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Patch the installed Switch RE4 to provide proportional stick turning, adjustable gyro aiming and an overlay, with a reloadable diagnostic NRO.

**Architecture:** Identify the actual executable before selecting hooks and injection framework. Share a small, tested input/config core between the game component, overlay and diagnostic NRO; use the user's Switch Debug Kit for NRO replacement and restart.

**Tech Stack:** Windows Python, devkitA64/libnx, C/C++, Switch Debug Kit protocol 2; extraction and injection tools selected from verified upstream projects.

**Spec:** `docs/superpowers/specs/2026-10-28-re4-controls-design.md`, approved by the user on 2026-10-28.

## Global Constraints

- Target Title ID `010099A00BC1E000`; validate full NSO Build ID and original instructions.
- Preserve original game containers, localization and normal controls outside patched contexts.
- No game assets, cryptographic keys, local tokens or extracted executables in Git/distribution.
- NRO reload is a verified new process; a settings change must not restart RE4.
- Never assign RE4's Title ID to the diagnostic NRO or its forwarder.
- Hardware validation is required for claims of functioning game controls and reload.

## Review Focus

- Unknown Build ID: no writes to executable code.
- Repeated/stale sensor samples and dropped frame timing: no duplicate gyro motion.
- Pause, inventory, QTE and cutscenes: no unintended analogue or gyro actions.
- Interrupted settings/NRO transfer: keep a usable previous version.
- Controller reconnect or missing gyro: stop motion and report actual sensor availability.

## Execution

This is a binary-research project. Task 1 determines the architecture and available
functions; record findings and concrete hook signatures in `docs/research/re4-binary.md`
before writing the game adapter. Do not invent hook offsets to complete a plan.
The user said to proceed; execute inline without a second plan approval round.

### Task 1: Obtain and identify ExeFS

**Files:** `.tools/` and `data/extracted/` (ignored), `tools/inspect_nso.py`,
`tests/test_inspect_nso.py`, `docs/research/re4-binary.md`.

**Interfaces:** `inspect_nso(path: pathlib.Path) -> dict` produces Build ID,
segment layout, module information and architecture evidence for subsequent tasks.

- [ ] Search official upstream extraction documentation; prefer extracting the small update only.
- [ ] Install tooling locally, extract ExeFS using local keys without logging secrets.
- [ ] Write a failing test for NSO header parsing and malformed/truncated input.
- [ ] Implement the inspector and run `python -m unittest discover -s tests -v`.
- [ ] Record real executable identity, relevant symbols, candidate input functions and loader constraints.
- [ ] Commit public tools, tests and findings only.

### Task 2: Implement proportional input and settings

**Files:** `include/re4/input.hpp`, `source/input.cpp`, `include/re4/settings.hpp`,
`source/settings.cpp`, `tests/input_tests.cpp`, `tests/settings_tests.cpp`, `build.py`.

**Interfaces:** `shape_axis(float raw, float deadzone, float gain) -> float`;
versioned `Settings` with independent stick and gyro gains; sample-driven gyro
integrator returning a yaw/pitch delta only for fresh valid samples while aiming.

- [ ] Write failing native tests for deadzone, monotonicity, saturation, symmetry,
      frame-rate parity, stale samples, invalid floats and damaged configuration.
- [ ] Implement the core with fixed-size data and no game-specific addresses.
- [ ] Build/run native tests; cross-compile using devkitA64.
- [ ] Commit verified core and build entry point.

### Task 3: Connect the actual game functions

**Files:** `source/game/`, `include/re4/game/`, `profiles/`, `docs/research/re4-binary.md`.

**Interfaces:** Consume Task 1's verified functions/layout and Task 2's `Settings`;
provide active profile status, validated game/aim/pause state and safe input deltas.

- [ ] Specify exact hook signatures and instruction checks from binary evidence.
- [ ] Write failing profile validation tests for known/unknown IDs and changed bytes.
- [ ] Implement the supported loader and adapter; fail closed for unsupported builds.
- [ ] Cross-compile the game component and verify original-instruction matching offline.
- [ ] Validate proportional turning and context gating in RE4 on hardware when available.
- [ ] Commit the adapter and original patch code, never extracted game bytes in full.

### Task 4: Diagnostic NRO and remote replacement

**Files:** `source/diagnostic/`, `third_party/switch-debug-kit/`, `config/project.example.json`,
`tools/reload.py`, `docs/development.md`.

**Interfaces:** Display actual pad/sensor state and Task 2's processed values;
compile the six receiver files and retain protocol 2 host/receiver compatibility.

- [ ] Copy a pinned subset of the user's kit with LICENSE/provenance; keep local config ignored.
- [ ] Integrate early bridge, configure, main-thread poll and finish around libnx lifecycle.
- [ ] Build NRO; run upstream local checks plus malformed update/reconnect coverage.
- [ ] Prepare own forwarder with matching NACP/ProgramId after collision check.
- [ ] Bootstrap on confirmed console, then verify changed digest and new PID after replacement.
- [ ] Commit distributable source and instructions.

### Task 5: Overlay and release package

**Files:** `source/overlay/`, `tools/package.py`, `README.md`, `docs/installation.md`.

**Interfaces:** Read/write Task 2's versioned settings; game adapter applies valid
snapshots without restart. Overlay reports whether the supported RE4 process is active.

- [ ] Add overlay dependencies with license/provenance and implement settings UI.
- [ ] Test corrupt settings, out-of-range values and interrupted atomic publication.
- [ ] Build overlay/package; ensure no keys, game files or development tokens are included.
- [ ] Hardware-check update application, input focus, gyro axes, reconnect and clean removal.
- [ ] Run the complete applicable checks and request one fresh whole-branch review.
- [ ] Fix material review findings, commit and report actual hardware coverage and remaining limits.
