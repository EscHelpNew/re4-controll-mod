# 0.27.4 weapon effects and overlay resource investigation

The user reports misplaced Red9 muzzle flash and casing while playing Ramon,
requests the same audit for all weapons, and reports rapid battery discharge
with the overlay.

- Preserve published0.27.3 and its exact files. Use0.27.4 for new builds.
- The live0.27.3 snapshot confirms Ramon and stock Red9 (B05488, seven bones,
 1643 vertices). Its mesh is WEP03 BIN3; its flash reuses WEP02 effect group39.
- The complete motion-slot census found no new ordinary firearm flash group or
 generic casing producer. Red9 fire2DAF40 and reload2DB2AC already have policies.
- An authenticated-shape, synthetic-vertex seven-bone weighted Red9 adapter
 already passes ownership, drawing and both effect corrections. This does not
 reproduce the console defect and is not sufficient to claim it fixed.
- Add bounded pointer-free emitter observations to an optional shared sidecar.
 Keep core ABI23/488 unchanged, no new server or game-changing RPC. Retained
 caller, failure, ownership and pose evidence avoids continuous host polling.
- Register producer, SDK reader, JSON and real linker regression fixtures;
 inspect the paired ARM image and original-call ABI before diagnostic delivery.
- A real listener-stage9 fault fixture reproduces2179 retries in250ms.
 A cancellable100ms recovery delay reduces this to2, with immediate shutdown
 and successful fixed-port restart. This is a conditional CPU/socket/log churn
 defect, not proof of a growing memory leak or battery attribution.
- Audit normal/hidden overlay lifetimes, sockets, files and static character
 storage. Do not change the user's graphics quality without a request.
- Complete actual-game emitter diagnosis, add the corresponding failing test,
 then run the single measured test command with4 workers and90% line gates.
 Publish or deliver only exact matched artifacts; keep hardware claims separate.

## Captured console evidence and follow-up

- The installed0.27.4 reports Ramon Active. Retained laser counters are246/246,
  Red9 BODY10 casing transforms10/10, held particles0/20993 and mode1 copies0/5.
  Both failing channels reach the owned held mesh but fail source qualification.
  This does not prove that the final casing render position is correct.
- A suspected cBone-as-matrix argument is disproved:1EA138 resets x22 to
  effect+40 before dispatch;279710 copies exactly48bytes from its source.
- Qualifying every foreign particle inspected held geometry before checking its
  source. An actual callback regression fails with11 memory queries; the early
  source filter reduces this to4 while retaining the full original ownership,
  pose and generation checks on accepted inputs. All7 focused cases pass.
- The first counters conflate every effect-record rejection as Matrix. Retain
  separate pool/state/owner/generation/part/group/script reasons in the existing
  fixed-size optional sidecar. No core ABI or correction policy is widened.
- Follow-up console counters retain the generic Matrix failure even with the
  new record-stage observations: the resolver is null before that code runs.
  Actual install assigns effect_script immediately before reset clears all
  CharacterNative bindings. A new actual-install regression fails on that null
  pointer; moving the assignment after reset passes. Existing fixtures had
  hidden the initialization defect by manually providing their own resolver.
- Authenticated physical WEP03 and WEP18 effects also use part5, outside the
  original parts2..4. A seven-cBone Red9 native callback fixture fails before
  admitting that additional source and passes afterward; part6 and missing
  part5 retain native behavior. This is an input qualification change only,
  with the existing exact held ownership, resources and pose checks retained.
- The first measured suite was intentionally stopped before changing frozen
  inputs for these fixes. Rerun the complete4-worker measured suite against
  the new stable sources. Neither hardware rendering nor battery improvement
  is claimed verified before another installed-game check.
- Independent review found diagnostic evidence could be overwritten by a
  nonmatching auxiliary holder. Per-handle nonpublishing observations now
  qualify the source before owner work and retain matched evidence. Actual
  Skin/Draw fixtures with two holders at BODY10/BODY16 reproduce both the
  successful attribution bug and loss of primary descriptor failure evidence;
  both regressions pass after correction. BODY10 diagnostic displacement and
  parent explicitly follow the same parent0 used by the correction. Reviewer
  re-review cleared the finding before the final source freeze.
- Final measured run `build/tests/run-w6wnyb01`:111/111 cases passed with4
  workers (including175 Python tests). C++ covers10851/11858 executable lines
  (91.51%,140 own files); Python2344/2437 (96.18%,28 own files). The335-input
  source snapshot stayed stable. Paired final candidate SHA256 is
  `af627b3c9811eda1bc483c099612fe6413092d5638fc3a5324009a51cde5ca2c`.
  Console delivery and installed-game visual confirmation remain pending;
  the standalone application did not answer the final connection check.

## Subsequent delivery

The version-only0.27.5 build was uploaded with a verified image hash and its
stored GAME image was launched successfully. Its compiled module text matches
the tested0.27.4 final candidate. The user subsequently accepted it and requested
a release. The release is superseded by0.27.6, which also adds the separately
requested, confirmed one-time money action and SharedABI24. Battery power was
not measured; the listener recovery and particle checks remain the bounded
resource fixes described above.
