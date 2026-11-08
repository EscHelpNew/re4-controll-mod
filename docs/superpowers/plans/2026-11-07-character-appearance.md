# Character appearance implementation

1. Validate unrelocated UDAS/DAT and donor BIN spans; reject ambiguous, truncated,
   cyclic, relocated or unsupported data without mutating input.
2. Build donor-to-Leon joint mapping and translation correction from bind tables.
   Blend normal native 8-byte weight combinations into native transposed output.
   Test multi-joint poses, secondary joints, malformed weights and finite output.
3. Finish native argument-substitution and resource completion audit. Identify
   exact body/piece/texture recipes for all requested donors. Keep asset evidence
   private and record instruction guards for every native entry point.
4. Add a bounded load-only console probe before donor drawing, with balanced
   terminal ownership and reset handling. Observe native completion/heap groups.
5. Integrate owned resource generations and visual-only function arguments.
   Preserve native hands, actor identity, inventory and gameplay routines.
6. Add desired/pending/applied appearance settings to app and overlay, Leon
   default, with a fresh paired wire/ABI version. Add actual callback tests.
7. Run full host checks, module/overlay/NRO builds and public artifact audit.
   Transfer only a verified concrete build; perform console gameplay validation.

The independently completed 0.22.0 D-pad/explosion/HP build can be transferred
while appearance work continues. It does not contain character appearance.

## Progress

- Pure raw archive, SHA-256 and retarget kernels passed strict host tests and
  independent review. Private installed-data validation covered nine archives
  and 25,776 palette matrices across seven donors and twelve articulated poses;
  no original assets were added to public fixtures.
- All seven exact donor recipes, pieces and texture packs are identified.
- Settings/UI use wire18/sharedABI22, Leon default, desired selection separate
  from applied/pending/failed telemetry. Twelve focused component checks passed.
- Native integration uses direct mounted game-file reads, fully owned pieces,
  render-argument substitution and a post-frame queued GPU acknowledgement.
  Texture pin policy has bounded exact/group/masked/selective cleanup tests;
  full renderer retirement remains a distinct native lifecycle.
- User reported a D-pad camera snap in delivered 0.22.0. The view owner reset was
  reproduced in the actual callback fixture and fixed. Separate 0.22.1 build
  excludes unfinished appearance UI, retains wire17/ABI21, and passed full host
  tests, module/overlay/NRO builds and public audit. Source snapshot is under
  build/hotfix0221; release NRO SHA256 is
  7dc02dce25aeddec414e60431dea1bd347649a7d87f1c5b35abc67ce9ab1c1fd.
  Awaiting console app readiness for that transfer; appearance integration
  continues independently.

## Verified local build

The runtime and independent final review are complete: no remaining P1/P2
finding. Full host suite returned0 (62Python checks plus all native targets,
build/appearance-tests-result.json). ARM64 module/overlay/NRO build returned0;
staged source comparison and header timestamps confirm the final frozen code.
All986 live native guard words match the authenticated original image; the new
SDK allocator-headroom import matches the verified SDK export. Public source,
history and v0.23.0 release artifact audit passed without findings.

Local v0.23.0 NRO SHA256:
0ec338854dfeec791c8fdaf6cbfcae55483cb50b99b648251160b4672d7a33bd.
No0.23.0 console transfer yet; current console receiver is unavailable and the
readiness question remains pending. The separate0.22.1 camera hotfix is also
ready locally. Gameplay/appearance validation requires the user's console.
