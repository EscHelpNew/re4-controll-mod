# RE6 native AA correction

The user reported artifacts for Off, FXAA and FXAA3HQ and explicitly chose to
remove those choices and retain stock FXAA3. Native analysis verified the enum
and synchronous shader consumer, but did not establish a safe visual repair for
the alternate paths. No claim is made that a global AA setter repairs them.

- [x] Preserve all current RE4 0.16.0 changes in the prepared source.
- [x] Add failing regressions for legacy AA migration and removal of the sixth,
  AA-render trampoline; remove that hook rather than applying another override.
- [x] Canonicalize valid legacy settings to Native while retaining language/AF;
  encode Native on save/IPC so new menus are also safe with older modules.
- [x] Make AA informational in both menus: FXAA3 (native), no save/apply on clicks.
- [x] Run the full host suite, module/overlay/NRO build, independent review and
  source/history/package audit. Inspect updated installer renders.
- [x] Transfer only verified changed files and allowlisted artifacts into the
  root checkout, with original hash checks and backups; provide version0.16.1.

Constraints: supported-build checks remain; native RE6 gyro and gameplay stay
owned by the game; no AA allocation, shader replacement or scene AA writes;
no console upload or publishing. Preserve the settings format and independent
RE4 settings. Hardware acceptance is a cold RE6 launch with the updated module,
including a previously saved manual AA choice, native scene rendering and AF.
