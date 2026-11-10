# Separate money receipt file

**Goal:** Follow the user's replacement design: any loaded main-story save can receive the bonus when the separate SD receipt is absent; its presence blocks further grants across loads and restarts.

**Cause:** The0.27.6 campaign/CRC enrollment rejects an existing mid-game save when it is not in the ledger. Actual callback regression `v0277-money-midgame-red.exe` reproduces the rejection.

**Implementation:** Keep the confirmation and bounded game-thread wallet update. Replace campaign IDs, save aliases and title hooks with `config/re4-controls/money-bonus.bin`. Existing valid positive0.27.6 receipts migrate once; unclaimed, unknown or corrupt old histories do not block enrollment. Never write receipt metadata on ordinary game saves. A failed receipt write never adds money. Native load/checkpoint callbacks only discard pending requests, without resetting the receipt.

**Policy:** This is one receipt for the installation, not one per inferred native playthrough. Changing saves or starting New Game does not clear it. The user explicitly requested this simpler file-based policy. Preserve native scene/mode/wallet-limit validation and all existing settings defaults. Bump product version to0.27.7; keep SharedABI24 and wire19.

- [x] Replace persistence and integration; prove mid-game availability, file-presence blocking, restart/load protection, legacy migration, I/O failure and exact amount with actual callback tests.
- [x] Update EN/RU warning/help and documentation to the receipt policy; test confirmation/cancel.
- [x] Independent review before freezing; build paired artifacts and run the one-command4-worker coverage suite with90% gates.
- [ ] Freeze the verified candidate and deliver to the console when the application is ready. Publication is not part of this fix request.

## Verification

Full run `build/tests/run-vck0_b7r`: 113/113 cases, 4 workers, 175 Python tests. C++ executable lines: 91.57%; Python: 96.18%. All 340 frozen inputs stayed unchanged. Independent source/ARM reviews have no remaining critical or important finding. Paired candidate SHA256 `c02fe657e5f3fe60f6b0e92f82c7c4b693f801e56a9360f71411f24a9dbb2ffd`. Console receipt behavior is not yet claimed verified.
