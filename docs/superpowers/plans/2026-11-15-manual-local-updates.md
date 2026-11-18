# Manual local updates implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Start local updates only on explicit selection and download bounded own application reports.

**Architecture:** A stoppable application-owned receiver reuses authenticated SDK framing and atomic file helpers without resident lifetime or remote launches. Main lazily owns GitHub networking or the local receiver, and records bounded service-stage observations.

**Tech Stack:** C++17/libnx, existing C reload core, SDL model/renderer, Python host client.

**Spec:** docs/superpowers/specs/2026-11-15-manual-local-updates-design.md

## Constraints

Five reports x64KiB (total320KiB); no credentials or per-frame logs. ServerOff every launch. No remote RUN or automatic restart. No teardown with live receiver worker. Root checkout and existing unrelated changes preserved. New delivery version0.28.14.

## Tasks

- [x] Runtime worker: write failing real protocol/storage/lifecycle tests; implement LocalUpdateServer/header; verify targeted tests.
- [x] UI: write failing navigation/localization/state tests, implement Updates third item and LocalUpdate page across all games.
- [x] Reports: write failing retention/bounds/failure tests; implement AppJournal and bounded report callback.
- [x] Integration: test idle startup snapshot and larger bounded authenticated report download, observe RED, wire main with explicit server start/stop and lazy GitHub worker; record service boundaries. Register sources/tests and bump version.
- [x] Run full host suite, all native module/overlay/NRO builds and package audit; independent code review; resolve important findings.
- [ ] Transfer only when console ready; manual app/module installation; collect actual reports and verify startup/exit/sleep. Keep HP and RE5 unresolved findings explicit.
