# Legacy manual-install ownership repair

> **For agentic workers:** Use superpowers:executing-plans, test-driven-development and independent review.

**Goal:** Resolve the user's confirmed RE4 Controls installation conflict after earlier manual app/mod updates, preserving unknown mods and saved settings.

**Root cause:** Console report from diagnostic0.18.3 identifies own0.16.1 subsdk9 (136140 bytes, FNV1a64 f4611f9aa197ebe2), common own main.npdm (1512 bytes,1cf444df4afaf2ca), and current0.18.2 overlay. Receipt RE4INST1 is valid but its first two ownership digests are zero. Overlay-only installation after a manual module copy records exactly that state; a later full install rejects the old module as unowned.

**Architecture:** Preserve the standard complete preflight/backup/publication/receipt flow. Only a valid own receipt with a zero ownership slot may migrate an exact game/index/size/digest identity in a trusted catalog compiled into the app. Default catalog contains only the verified own0.16.1 RE4 module. No file-name, magic-only, wildcard or user-entered force replacement is accepted. New full install fills the receipt normally.

**Constraints:** Existing root checkout and previous reload/Ashley/framing changes retained. No settings/calibration/other game/loader changes. Diagnostics read only three fixed file paths and report identities, never raw binaries or credentials. User has authorized console app deployment and is attempting mod installation; final app is uploaded after verification. Real gameplay acceptance remains separate.

## Tasks and review focus

- [x] Obtain actual console evidence without changing game files; compare with retained release artifacts.
- [x] Reproduce manual old-module + overlay-only receipt failure; implement strict trusted-artifact migration.
- [x] Verify unknown byte edits, absent/malformed receipts, wrong game/index, empty catalog and next-update receipt ownership.
- [x] Run full tests, ARM64 builds, independent review and public audit; prepare local0.18.4.
- [ ] Deploy local0.18.4 after console network access is restored.
- [ ] Verify deployed version/hash and recheck install receipt/files when user retries.

## Evidence

- Diagnostic0.18.3 was a read-only support-report extension, deployed as GAME
  SHA256 f2d53a572c3aa2644a30016bd5a7f74a408fdb494557b290c0943a9eab051e07,
  processA8. Report hashes match retained own release ZIP identities.
- Support-report regression first failed without installation identities, then
  passed with fixed-path bounded hashing and no source file/receipt mutation.
- Installer regression first failed on the manual module, then passed with
  trusted catalog migration through the real installer implementation.
- Independent review found no important safety/correctness issue and verified
  the default legacy catalog against the retained0.16.1 ZIP. Added explicit
  missing-receipt and nonzero mismatching-slot regressions after its coverage
  note; final targeted installer checks passed.
- Full `python build.py --test` passed (62 Python tests and all native suites).
  `python build.py --module --overlay --nro` passed, including31/40 hook budget.
- Prepared local0.18.4 NRO SHA256:
  `37379296da22b7d44c499ebc1f509b2b6d6235edfd4db93925fc9120806ba5a5`.
  Release validator and source/history/release audit passed with no findings.
- Initial upload retry could not connect even after the user opened the app;
  both known service ports timed out at the previous configured IP. Requested
  the current console IP before resuming network-dependent delivery.
