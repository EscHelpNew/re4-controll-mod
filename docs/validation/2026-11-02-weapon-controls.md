# RE4 Controls 0.16.0 validation

Validated against source baseline c1dfc5e, using a temporary source copy and the
installed host/devkitA64 toolchains. Source changes are transferred to the root
RE4 checkout only after source-identity checks, retaining backups.

- `python build.py --test`: PASS, 59 Python tests and all host C++ programs.
- `python build.py --module --overlay --nro`: PASS for both modules, all overlays
  and the standalone installer. Compiled FP/GP hook-layout checks pass.
- New hook usage:30/40 slots. All new instruction guards agree with the local
  supported executable; its NSO segment hashes and analysis image were checked.
- `python tools/package.py` and `python tools/release.py`: PASS; release
  validation checks NRO identity/version, paired payloads and archive hashes.
- Independent review: native zoom old-value ordering issue reproduced with a
  failing regression and fixed by committing at15568; final review found no
  further important issue.
- Source/history/release audit: no findings. No game files or console credentials
  are included in the four release assets.

The NRO version is0.16.0, SHA-256:
`aeead2384c7126a932e3d2ab839da42921058d123e5f2864a53b8454b923a241`.

## Hardware acceptance

After the combined0.16.1 update was deployed and restarted with its exact
digest verified, the user confirmed that everything works. RE4 AF16x was
separately confirmed after a complete game restart. The sequence below remains
the checklist for broader coverage; the user confirmation is not a claim that
every weapon/state/controller combination has been exhaustively tested.

Verify the full downward knife and firearm range, moving first/repeated shots
and recovery with each supported gun, wall/stair collisions, and rifle bolt
cycling. Inside sniper scopes, the player must stay still, leftY must zoom,
rightXY/gyro must aim smoothly, and native magnification feedback must update.
Test firing/reloading, scope exit/re-entry, weapon changes, pause/HOME and sensor
reconnection. Host engine fixtures and a successful ARM build do not establish
the complete game's animation or collision behavior on a real console.
