# RE5 input route diagnosis implementation plan

> **For agentic workers:** Use superpowers:executing-plans for this single dependent diagnostic change.

**Goal:** Explain why an active RE5 module with applied controls does not affect gameplay.

**Architecture:** Extend only the existing authenticated, explicitly enabled Monitor snapshot. After a supported connected image and exact main extent are validated, invoke the existing native player resolver through a bounded remote MemoryReader. Return each actor route result and the last eight scalar reads. No new native hooks, recording loops, services or game writes.

**Tech Stack:** C++20, existing dmnt client and host adapter fixtures.

**Spec:** User requests optional diagnostics, Off by default, preservation of installed RE5/translation and patch version increments. Observed 0.28.6 has native_verified legacy identity, applied controls and responsive recovery startup/exit, but gameplay controls are ineffective with attached Joy-Con.

## Global constraints
- Keep native ownership/build/IPC protections unchanged.
- Maximum four actor slots, 24 scalar reads per resolver, eight retained observations, no arbitrary network-selected addresses.
- Stop reading if process/title changes; failed reads are diagnostic data, not authorization.
- Every changed delivered image is version 0.28.7.
- Remain in root checkout as CONTRIBUTING.md requires; existing unrelated edits remain untouched.

## Review focus
- Unsupported identity and malformed main bounds: no route reads.
- Process changes during reads: stop and discard observations.
- Handheld routes: resolve real native helper without changing its guards.
- Unreadable objects: bounded errors, no writes.
- Maximum output: complete JSON within existing 16 KiB server limit and 64 KiB worker stack.

### Task 1: Read-only input route snapshot
- [x] Add host fixtures for an exact legacy image, handheld route, ownership rejection, inaccessible memory, process change, budget and small output buffer.
- [x] Run the focused test and confirm missing input_routes fails.
- [x] Add source/re5_controls/input_probe.inc and call from diagnostic_snapshot only after connected image/extent validation; reuse re5::resolve_player unchanged.
- [x] Run focused tests to green, then existing full host checks.
- [x] Bump app version and assertion; build overlays/NRO, audit candidate, obtain scoped review.
- [x] Deliver after user readiness; user requested direct RE Controls PUT without RUN, followed by manual relaunch and overlay installation. Live route inspected and reproduced the native style rejection. Diagnosis is not a controls-fix claim.

### Task 2: Correct the native pad style namespace (0.28.8)

Console0.28.7 rejects the local human at the physical pad style field: handheld index8 / physical ID32 / native game style0x10. The mod erroneously treats that field as SDK style bits. The native game converter independently maps game1->SDK1 (full), game2->SDK4 (paired Joy-Con), game16->SDK2 (handheld); individual game4/8 are unsupported here.

- [x] Update native-input and overlay fixtures to observed native handheld16/dual2; confirm RED.
- [x] Translate native style bits into SDK1/2/4 in native_input_detail::pad without weakening identity/ownership guards.
- [x] Add rejection checks for unsupported single/unknown styles, plus actual handheld SDK observer + local-input callback regression.
- [ ] Verify focused and full host checks; version0.28.8, target builds, scoped review and archive audit.
- [ ] Store both SD NRO slots without RUN after user readiness; manually install full RE5 mod, restart, verify routes and gameplay. Do not claim hardware effectiveness until user confirms.

### Task3: Camera feedback and opt-in runtime diagnosis (0.28.9)

- [x] Confirm on console that distance1 removes shifted frames independently of free-camera toggle; retain evidence of valid attached-controller routing.
- [x] Audit native desired/history reads and add protected pre-desired hook2406BC/legacy2404DC; all303 instructions match both actual images.
- [x] Test and implement bounded one-use native eye/up/target restoration before native history capture, including rejected-input cleanup and no changes at neutral distance1.
- [x] Add default-off192-byte runtime probe after unchanged shared/identity blocks; nonblocking counters capture only in explicit two-second request windows.
- [x] Add authenticated snapshot discovery/proof/size/header/process guards and defer the sole four-byte request until output capacity is validated.
- [x] Finish full135 tests, final target builds, scoped reviews and source/archive audit.
- [ ] Deliver exact0.28.9 via direct PUT without RUN after user readiness; require full RE5 mod update/restart, camera-distance acceptance and native counter snapshot before any further control-gate change.
