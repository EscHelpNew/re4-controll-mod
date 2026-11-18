# Installed RE5 Build Support Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Read-only domain researchers may use superpowers:dispatching-parallel-agents. Steps use checkbox syntax for tracking.

**Goal:** Support the user's installed C517ECBB RE5 build while preserving Russian translation and all existing latest-build controls.

**Architecture:** Select an immutable runtime build after the entire image BuildID and every protected instruction match. Translate canonical main offsets through a compile-time checked profile table; keep SDK guards and verified invariant object/global layouts. If dmnt returns a zero BuildID, authorize only after a valid known GNU note, immutable native identity proof after complete startup fingerprinting, and stable process identity; keep raw/effective identities distinct in monitoring.

**Tech Stack:** C++20/libnx/exlaunch/libTesla, Python local build and audit tools, private Capstone binary analysis.

**Spec:** Existing RE5 controls requirements and explicit user choice to support installed version while preserving translation. Private validated evidence: .superpowers/sdd/2026-11-13-re5-old-profile/context.md and domain reports.

## Global Constraints

- Keep changes in the primary checkout; do not modify original game/translation archives or console files beyond the user's installed mod.
- Latest4E46401B and installedC517ECBB use distinct main GNU note offsets and all299 protected instruction words; exact shared SDK guard remains required.
- Unknown, partial, conflicting or changed identity cannot enable hooks or game writes.
- Both local players and saved configuration/UI/translation remain compatible. Existing shared IPCABI1 stays unchanged.
- A changed delivered build must use0.28.5 or higher. Never claim global hangs fixed from host tests.

## Review Focus

- A zero metadata BuildID with wrong GNU note or one bad instruction must fail closed.
- A process/title change during fingerprinting must discard the result before any write.
- Every native main-relative code offset must map for both builds; no default-to-latest address on the legacy build.
- Latest profile still installs identical hooks; SDK identity and single-session monitoring lifetime unchanged.
- Zero raw metadata and recovered effective identity must remain distinguishable; diagnostics disabled by default.

### Task1: Exact native profiles and runtime offsets

**Files:** include/re5/profile.hpp, new legacy profile/address data, source/re5/module.cpp/runtime.inc/reticle.inc, include/re5/native_input.hpp/native_graphics.hpp; tests/re5_profile_tests.cpp, tests/re5_module_tests.cpp, private actual-image driver.

- [ ] Run a private regression against the actual old image and observe current identity rejection.
- [ ] Add generated numeric legacy guard data from independently cross-checked domain mappings; validate against both private real images.
- [ ] Add Build detection/view and compile-time checked canonical-offset translation, select only after exact main and SDK verification.
- [ ] Route every hook/call/vtable/global through its validated mapping and cover old/latest hooks and wrong instruction/SDK rejection with host callback fixtures.

### Task2: Verified zero-ID recovery in the overlay

**Files:** source/re5_controls/game_link.hpp/.cpp and tests/re5_game_link_tests.cpp.

- [ ] Add failing tests for known zero-ID GNU note + full fingerprint recovery, unknown/conflicting/corrupt notes, one corrupt guard, bad bounds, read failures and identity changes before apply.
- [ ] Reuse the owner's dmnt session. Bound every read to raw main extents, verify exact known GNU note and the native module identity proof published only after its complete pre-hook fingerprint, then revalidate PID/title.
- [ ] Keep raw BuildID unchanged; report effective BuildID and identity source separately. Apply must independently revalidate the same identity with no fallback for a nonzero conflicting metadata BID.

### Task3: Integrate, verify and deliver

- [ ] Bump version0.28.5 and documentation; keep one permissions profile only after byte-level old/latest payload equivalence proof.
- [ ] Run actual image validation, focused callbacks/GameLink tests, required full host suite, all ARM modules/overlays/NRO builds and public/artifact secret audits.
- [ ] Obtain fresh scoped review, resolve findings and rebuild if required.
- [ ] On console readiness, preserve available reports, upload exact reviewed NRO into both slots and verify its version/hash. User installs full RE5 mod, restarts game, enables monitoring; verify actual legacy profile, IPC frames and settings application, then ask physical controls acceptance.

Ruling: native hooks overwrite protected live instructions. The overlay must validate the immutable native112-byte identity record at SharedControl+384 rather than compare patched hook words to original instructions. Native startup retains the complete299-word main and30-word SDK checks.
