# Parallel tests and measured coverage

User request: extend tests to90+% and run the complete suite with one script and
2–4 parallel workers. The existing game fixes remain part of the ongoing task.

The supported command is `python build.py --test --jobs 4 --coverage`. Keep
`--test` compatible; accept2,3or4 workers, default4. Each native case has its own
working directory, temporary files, objects, profiles and logs. Prepare assets
once; include all101 original native executables plus Python discovery. Never
share an instrumented C object between concurrently running executables.

Coverage policy: require at least90% physical executable source lines separately
for own C++ and Python. Report branches separately; do not use coverage.py's
combined line/branch percentage for the line gate. Include root build.py, all
tools Python, and all own source/include C++ files. Exclude only third-party,
generated and test code. Keep uncovered platform code in the denominator.

Host GCC14 profiles execute the real callback fixtures. Matching ARM GCC16/gcov
compile the real target translation units into private zero-count baseline
objects; no instrumented product is linked or deployed. Union the source lines
from both configurations, merge template instances by source line, and preserve
raw per-target branch/function records. Compiler dependency artifacts account
for included declarations/compile-time data without runtime counters. Missing
sources, missing evidence, failed compiles, stale profiles or a failed test must
fail the complete command.

- [x] Measure initial Python baseline across all23 original own files:1038/1709
  executable lines(60.74%); branch counts remain separate.
- [x] Implement and test an isolated2–4 worker runner; full normal run102/102
  top-level cases passes, with exact101 native compile-recipe parity.
- [x] Prove compiler/gcov matching, all31 target translation units and all136
  initial C++ files accounted for; implement native collector and threshold gate.
- [x] Collect the first complete instrumented baseline, then add meaningful
  regression/contract tests for the remaining uncovered code until both languages
  pass90% on a stable full run.
- [x] Verify2-worker and4-worker execution, failure propagation, all report links,
  compiler evidence and no source/output races; update developer documentation.
- [ ] Complete new console feedback: native gauge-following HUD, laser beam,
  muzzle effects and shell origins for retargeted appearances. Preserve frozen
  releases0.27.0/0.27.1 and perform new module/overlay/NRO checks before transfer.

Ownership: natural_height_research owns runner/catalog and build test dispatch;
aim_extremes_native owns native coverage collector/census and its tests;
ammo_health_visibility owns Python contract/CLI tests and native gauge follower;
root owns the threshold policy/tests, dependencies, documentation and integration.
Additional emitter investigation remains read-only until native producer pose,
ownership and pointer lifetime are established.

Final acceptance: run-a8b6ssuc passed104/104 cases with four workers,175Python
tests and103native executables. C++10402/11419=91.09%; Python2344/2437=96.18%.
The current137-file C++ census is complete; no production lines were excluded.
Input content identities remained stable. Module/overlay/NRO builds and actual
linked ARM LR/fast-path review passed. Native guard verification passes4190raw
words/86typed relative pointers after15711loader relocations, with no guard
intersections among16654relocation entries. BRIDGE/GAME delivery and application
restart into0.27.2-p95 are verified against the frozen NRO hash. In-game mod
installation and gameplay acceptance remain pending user action.
