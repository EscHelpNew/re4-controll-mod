# Gyro transition and native aiming smoothness

> **For agentic workers:** Use superpowers:executing-plans to implement inline.

**Goal:** Discard motion during weapon raising and remove gyro-only stepping while retaining additive stick input.

**Architecture:** Qualify each gyro session using verified native weapon states, recheck at consumption, and drain the readiness-edge sensor FIFO. Preserve the requested two measurements 10ms apart, but integrate their average on every fresh sensor sample. Keep the existing native pitch and manual-sway adapter: research proved that writing only the target loses gyro input in the alternate native interpolation branch.

**Tech Stack:** C++20 game adapter, C++17 input filter, actual-module host fixture, devkitA64.

**Spec:** User reports immediate downward motion during raising causes a snap, and gyro animation is less smooth than the right stick. Prefer ignoring preparation motion; keep existing settings and physical direction. RE9 is an experiential reference, not a claim of engine/animation parity.

## Constraints and review focus

- Stay in the current repository. Never publish credentials or create a GitHub release while the hold is active.
- Unknown native weapon state tables retain their previous behavior; readiness claims require disassembly evidence.
- Fire/recovery must retain an armed session; release, reload, actor/weapon/controller change and pause must discard it.
- Never replay preparation samples, including a batch straddling the readiness transition.
- Native pose easing and manual-input sway ownership must work together; gyro and right stick remain additive.
- Sensor grouping, repeated samples, gaps, variable intervals and malformed samples must not cause bursts or drift.

## Tasks

- [x] Trace native raise/aim/shot transitions and compare gyro with native stick processing.
- [x] Reproduce preparation motion and 10ms output stepping with failing tests.
- [ ] Add native readiness latch, FIFO drain, context invalidation and consumption recheck; extend lifecycle tests.
- [ ] Make the 10ms two-endpoint average continuous at sensor cadence; test timing and malformed input.
- [x] Trace native pitch/sway ordering before changing pose injection. Reject target-only injection because native33E608 overwrites that value; ordinary steady aim uses factor0. Keep the working additive adapter.
- [ ] Run the complete suite, ARM build and independent review; document remaining hardware acceptance.
- [ ] Commit source to main, verify CI, audit credentials and transfer through tools/reload.py when the installer is reachable. No release.
