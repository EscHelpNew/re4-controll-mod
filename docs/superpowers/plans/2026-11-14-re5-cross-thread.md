# RE5 cross-thread input publication implementation plan

> **For agentic workers:** Use superpowers:subagent-driven-development for implementation and a fresh scoped review before delivery.

**Goal:** Make accepted local input available coherently to RE5 camera and movement callbacks running on other game workers.

**Architecture:** Retain exact identity/ownership/pad/age guards. Publish complete per-player ControlFrame copies through nonblocking mailboxes after native input completes; keep writer-owned gyro/response state under the input lock and same-thread inline scope unchanged. Protect newly reachable shared orbit and gait state with nonblocking guards.

**Tech Stack:** C++20/native ARM64 hook adapters and actual callback host fixtures.

**Spec:** Hardware0.28.9 records119 accepted inputs but683 cross-thread fresh-frame rejections in2seconds; inputthread2A5 differs from camera2C3. Current permanent first-thread pin is invalid. User requests freecamera/firstperson/movingaim/knife/reload forbothplayers, defaultOffdiagnostics, versionincrementandpreservedtranslation.

## Global constraints
- No spinning, waiting, unsafe plain-frame cross-thread reads, or ownership/build-guard bypass.
- Keep actor, command, local player, physical stick/motion route and100ms freshness checks.
- Native pre-desired pose cleanup remains independent of input/settings gates.
- Version0.28.10 before delivery;303 protectedinstructions/profile retained.
- Root checkout; directconsolePUT onlywithuserreadiness; NEVER SDKRUN/selfrestart.

## Review focus
- Worker migration and camera onanotherworker mustworkwithouttornframe reads.
- Input scope remains unavailable to unrelated threads.
- Missing/stale/replaced actor/command/pad mustleave native behavior untouched.
- Busy mailbox/gait/orbit skips immediately without waiting.
- Both local players keep isolated state under concurrent calls.

### Implementation
- [x] Reproduce workerA input/workerB camera failure in an actual callback fixture.
- [x] Add coherent nonblocking mailboxes and remove permanentthread authorization; retain last-thread diagnostic.
- [x] Protect orbit/pose and gaitshared state; adapt committed-frame fixtures without weakening negative guards.
- [x] Targeted concurrency, worker migration, latest/legacy callback tests; fullsuite and ARM builds.
- [ ] Fresh scoped review, source/archive audit, direct0.28.10 delivery and fullnativeinstallation.
- [ ] Hardwareverify remainingfeatures and nativecounter paths; appstartupfreezes remain separate.
