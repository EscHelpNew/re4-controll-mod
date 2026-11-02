# RE6 native AA, version 0.16.1

The user reported artifacts in manual Off/FXAA/FXAA3HQ and explicitly chose to
remove those modes and keep the game's original FXAA3. Native analysis confirmed
that these choices affect more than the last shader, but did not prove a safe
repair for every alternate rendering path. This update removes the override;
it does not claim that the alternate shaders or native AA-off path are repaired.

## Verified behavior

- The RE6 module now installs five hooks. There is no AA render trampoline,
  scene AA write, renderer AA write, alternate shader or extra AA allocation.
- The overlay and installer display fixed FXAA3; AA clicks cannot save/apply.
- Valid old AA bytes0..4 decode to Native while retaining language/filtering.
  New saves and IPC encode Native, including for an older installed module.
  Malformed settings still fail validation and retain backup/default recovery.
- All35 RE4 native game/helper files are byte-identical to the prepared0.16.0
  source, except the shared version header is updated separately. Prior knife,
  moving-fire and scoped controls remain included.

## Validation evidence

- Full `python build.py --test`: PASS, 59 Python tests and all host C++ programs.
- Targeted settings migration and five-hook regressions failed before their
  fixes, then passed; overlay, installer and shared/IPC tests pass.
- The final diagnostic-text adjustment was checked by the actual IPC fixture.
- `python build.py --module --overlay --nro`: PASS, both modules, all overlays
  and NRO. Both compiled hook-context ABI checks passed.
- English/Russian RE6 graphics installer renders were inspected: the fixed
  value and help fit without clipping. Independent review found no blocker;
  its minor pre-acknowledgement AA diagnostic was removed.

NRO SHA-256:
`b9ea43fc933d0bc29ee074e212bad3d852581b32cfe42c604b7bccb832738c55`.

## Console confirmation, November 2

Version0.16.1 was transferred into both application slots, restarted and
reconnected with the exact NRO SHA-256 above. The user then confirmed that
everything works. They also confirmed visible RE4 AF16x after a full game
restart. This is basic hardware acceptance; exhaustive per-weapon, scene and
controller coverage is not claimed.

For subsequent updates, close RE6 and install its module and menus together
before relaunching. Additional regressions should include formerly saved manual
AA settings and retained AF/language. Host fixtures do not render the full game.
