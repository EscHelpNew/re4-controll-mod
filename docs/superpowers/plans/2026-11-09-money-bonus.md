# One-time money bonus implementation plan

> **For agentic workers:** Use superpowers:executing-plans to implement the tasks below. The user's requested execution remains in this session.

**Goal:** Separate Game action granting exactly 200,000 pesetas after confirmation, once per playthrough across restarts.

**Architecture:** The overlay submits a fixed request to the module; only the native game thread changes the wallet. A separate checksummed SD ledger stores campaign claims and every observed native save identity. Authenticated title launch creates campaigns for fresh story and cleared-save New Game Plus. Checkpoint restoration never resets claims.

**Tech stack:** Existing C++ module, dmnt shared control, Uberhand overlay, native nn::fs, host C++/Python tests.

## Requirements and decisions

- Separate action, independent of extended weapon upgrade. Explicit Cancel/Receive warning says this adds 200,000 and makes the game easier; save the game afterwards.
- Claim is durable before money is changed. An interruption before a normal game save can lose unsaved money; never promise transactional native autosaving.
- First existing save can be enrolled when the ledger is empty. After enrollment, unknown old/imported saves fail closed: RE4 has no campaign GUID. No guessed matching by slot, save counter or last active campaign.
- Preserve all observed `(slot, CRC)` aliases; refuse when capacity is full instead of evicting claim history. New genuine title starts create a new campaign.
- Dedicated alternating checksummed ledger generations, flushed writes. Corruption/I/O failure blocks grants. Never alter the native save file or extended-upgrade sidecar.
- No grants at title, other game modes, invalid wallet, insufficient headroom, or before a recognized loaded/new campaign. Exactly +200,000, no saturation.
- Bump product to 0.27.6 and shared ABI for the new request/ack/status fields; settings schema remains 19.

## Review focus

Unknown save branches cannot unlock an already consumed claim; checkpoint/death and overlay reopen cannot repeat requests; failed writes must not change money; title/NG+ edge must not repeat per frame; pending commands must not cross a load/new campaign.

## Tasks

1. [x] Test the real Game menu confirmation, cancellation, pending/consumed action and settings independence; add UI and bounded shared command reader/writer.
2. [x] Test ledger round trips, corruption, campaign/alias limits, claim persistence, different save slots and unknown histories; implement dedicated codec.
3. [x] Test actual module callbacks with native memory/FS boundaries: +200,000, replay/restart/load/checkpoints, cap/mode checks and I/O faults. Implement authenticated title/save/load integration and wallet mutation on PadNormalize.
4. [x] Fresh independent review, resolve findings, build paired artifacts, run `.venv/Scripts/python.exe build.py --test --jobs 4 --coverage` and check executable-line coverage >=90% separately for C++ and Python.
5. [x] Update concise release notes and freeze/audit the four release assets. Commit/push and publication are verified separately in the release record. No game money is granted remotely as a test.

## Verified result

Full run `build/tests/run-er1zrw2p`:113/113 cases with4 workers, including175 Python tests. Executable-line coverage: C++91.60%, Python96.18%. All340 frozen source inputs remained unchanged. Independent source and linked ARM reviews have no remaining critical/important finding. Paired0.27.6 candidate SHA256 `222128ddb5608225d94fbabbe17d351d3c3891b6f651985594dead55ccdfe7c6`. Console money acceptance and battery measurement are not claimed.
