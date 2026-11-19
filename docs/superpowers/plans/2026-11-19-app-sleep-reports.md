# Application sleep evidence implementation plan

> **For agentic workers:** Use superpowers:subagent-driven-development or superpowers:executing-plans for the steps below.

**Goal:** Make existing crash-report download include bounded Atmosphere evidence without changing sleep behavior.

**Architecture:** Combine the existing AppJournal callback with a read-only fixed-directory system-report snapshot. Both callbacks share the existing manually enabled authenticated receiver and1MiB buffer; no startup I/O or new worker is added.

**Tech Stack:** C++17, libnx, host filesystem fixtures, Python SDK build.

**Spec:** docs/superpowers/specs/2026-11-19-app-sleep-reports-design.md

## Constraints and review focus

- Five retained own files,64KiB each; five selected system files, text64KiB each; aggregate below1MiB.
- No changes to focus/sleep policy, game hooks, or network default Off.
- Check tiny output capacities, missing/large directories, symlinks/reparse points, private names and latest-file selection across directories.
- Never claim these changes resolve the physical hard hang.

## Task1: Fixed system-report reader

Files: include/re4/system_reports.hpp, source/diagnostic/system_reports.cpp, tests/system_reports_tests.cpp.
- [x] Demonstrate missing system evidence with an actual fixture failure (RED).
- [x] Implement append_system_reports(out,capacity,sd_root), bounded/read-only.
- [x] Verify newest-five global selection, content/truncation, tiny capacities, unsafe paths and unchanged input files (GREEN).

## Task2: Real CRASH integration

Files: include/re4/app_reports.hpp, source/diagnostic/main.cpp, tests/app_reports_tests.cpp, tools/test_catalog.py, build.py.
- [x] Show current callback omits an actual Atmosphere fixture.
- [x] Compose AppJournal then system snapshot and wire manual server callback.
- [x] Verify combined callback bounds, missing system logs and existing own journal rotation.

## Task3: Candidate verification

- [x] Independent read-only review and resolve material findings.
- [x] Bump0.28.30, full host suite and module/overlay/NRO build; package/audit.
- [ ] Keep source/deployment evidence separate; do not publish or claim hardware acceptance.
- [ ] Once the console is available, download system evidence through the local server; analyze it before a sleep fix.

Independent review found the receiver384KiB allocation insufficient for the new combined callback. An actual localhost CRASH test reproduced ERR report for700KiB, then passed after server and callback shared the1MiB capacity; oversize rejection/redaction/cancellation remain covered. No unresolved important review findings. Initial144/144 suite passed before final wire-capacity and small-output-marker changes; a fresh full suite and NRO build are running for the final source.

Final verification:144/144 host cases passed (`build/tests/run-6q9oh8wj/results.json`). Modules/overlays, final NRO, package/release preparation and public tree/history/archive audit exited0. Physical sleep/wake remains unresolved. Deployment has not occurred: after user enabled the server, ICMP replied but28285 refused twice; awaiting restart of the manual server.
