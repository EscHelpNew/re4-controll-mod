# On-demand application gyro lifecycle implementation plan

> **For agentic workers:** Use superpowers:executing-plans or superpowers:subagent-driven-development for these steps.

**Goal:** Remove unused SixAxis activity from ordinary application menus, preserving controller diagnostics and gyro testing.

**Architecture:** Extract the actual existing Sensors SDK adapter into an application-only header. Gate activation by InstallerPage (Diagnostics or GyroTest), stop handles and invalidate selection cache when leaving those pages. The main loop calls this same adapter and retains preview/calibration rules. No sleep policy, renderer, server or game-hook changes.

**Tech Stack:** C++17, libnx HID, actual SDK-adapter host fixtures.

## Evidence and limits

Current main.cpp unconditionally starts sensors and calls read16 on Home/LocalUpdate and all other pages, although only Diagnostics/GyroTest consume these data. The installed SDK's16-state reader can retry indefinitely on a discontinuous ring. No captured stuck instruction proves this is the cause of the console-wide hard wake failure. This candidate is one isolation hypothesis, not a confirmed sleep fix. User redirected investigation to the application/server/overlay; CFW configuration is unchanged.

Server stop/join/socket order was audited and stress-tested; current overlay title guards precede process attachment. No causal server/overlay deadlock has yet been reproduced.

## Tasks

- [x] Add tests before implementation: ordinary pages never get/start/read sensors, Diagnostics/GyroTest do; leaving stops each started handle once; reentering with identical pad restarts; existing P1/P2/Handheld/JoyDual and partial-failure behavior remains.
- [x] Extract actual SDK adapter into source/diagnostic/app_sensors.hpp; implement page gate with cache invalidation.
- [x] Wire main loop to the adapter; cancellation on leaving a diagnostic page remains Ready rather than a false sensor failure.
- [x] Independent review, full host suite, modules/overlays/NRO, package/audit, version0.28.31 before delivery.
- [x] Preserve existing application reports and optional server behavior. No new background logging or workers.
- [x] Deliver only to a manually enabled user server, without RUN. Physical wake confirmation is a separate required check; failure returns investigation to other components.

Verification: original SDK adapter reproduced four failing lifecycle requirements (8/12); gated adapter passed12/12. Independent review found no blocking issue. Full host suite145/145 (`build/tests/run-yavyaat4/results.json`), modules/overlays/NRO, package/release preparation and public audit passed. NRO SHA2560654168e6b0268e4471bff7dcec88431b7d2a40f8fae185e3a7288fadc751a8a. Physical wake is not yet verified.

Physical follow-up: both0.28.31 NRO files were stored/hash-verified without RUN. User manually reopened31, started/stopped Local Update, slept10seconds using POWER and confirmed the console woke and HOME worked. Short check passed. Prolonged charging/automatic sleep and a universal root-cause claim remain unverified.

User follow-up: the hangs were attributed by the user to the system update, and the prolonged check was deferred. Preserve this as user-reported context rather than a proven app-side root cause. Retain the independently tested gyro ownership correction; no additional speculative app/server/overlay sleep changes are planned from the current evidence.
