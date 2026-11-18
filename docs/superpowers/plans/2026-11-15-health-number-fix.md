# RE4 health-number formula correction plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans.

**Goal:** Correct normal-body HP formulas while preserving the previous working numeric display.

**Architecture:** Retain read-only reference calculation and scalar capture; restore the old HUD draw decision and use legacy150 when no reference is known. Keep the copied per-target unit through fade. No extra diagnostic UI/services.

**Tech Stack:** Existing C++17/20 native module/HUD adapters.

**Spec:** docs/superpowers/specs/2026-11-15-health-number-fix-design.md

## Constraints

Indication only; actual enemyHP/damage unchanged. Upgrades do not inflate displayed enemyHP. Number style/geometry/opacity/fade unchanged. Unknown reference preserves old units. Cancelled HP-report feature must leave no production delta. Version0.28.15, root checkout, unrelated work preserved.

## Tasks

- [x] Native proof and RED/GREEN ordinary subtype/link/phase formulas; independent review and1142word/55pointer guard merge.
- [x] Actual capture/scene/HUD ordinary matrix, enemy-byte invariance.
- [x] RED/GREEN restore number fallback and fade, with old numeric pixels verified.
- [x] Remove cancelled diagnostic probe/button/accessor deltas and build dependencies; retain private evidence.
- [x] Independent focused review, full suite, rebuild affected module, overlays/NRO and package/audit.
- [ ] Console transfer/manual module install, verify same Handgun/Professional digits and upgrade-invariant enemyHP.
