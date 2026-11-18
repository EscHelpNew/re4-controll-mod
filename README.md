# RE Controls for Nintendo Switch

<img src="assets/icon-re.png" width="128" alt="RE Controls">

Smooth stick controls, motion aiming and camera options for the original Nintendo
Switch ports of **Resident Evil 4** and **Resident Evil 5**. Includes an
English/Russian overlay, a standalone installer and GitHub updates. English is
the default language. Release filenames remain `re4-controls.*`.

[Download](https://github.com/EscHelpNew/re4-controll-mod/releases/latest) ·
[Русская инструкция](docs/README.ru.md) · [Installation](docs/installation.md)

The current release is **[0.28.27](docs/releases/v0.28.27.md)** (2026-11-18),
including changes since public 0.27.7.

## Added

- **RE5 support:** a gray page, controls/gyro for two local players, controller
  tests, graphics presets and an owned module/overlay installer. Both supported
  RE5 executable profiles are included.
- Explicit **Updates → Local update → Start server**, manual restart after
  transfer, and bounded application reports for troubleshooting.

## Fixed

- RE5 **Modern** camera direction and anchoring: camera and character turn
  together, vertical right-stick look works, and movement while aiming/shooting,
  using the knife and reloading works in the accepted console test.
- RE4 fixed native Firepower1.0 health-number references for upgraded weapons
  and linked Saddler/Salazar HP owners; Ada's story camera and case rotation;
  red indication of the purchased extended Firepower row. Actual HP/damage stay native.

**Known issue:** intermittent application startup and sleep/wake hangs were
reported. A universal fix has not been established.
[Release details and validation limits](docs/releases/v0.28.27.md).

## Controls and features

Use **L/R** in the standalone app to select **RE4** or **RE5**. Settings are
independent. In RE5 choose **Local player 1/2** before editing controls, testing
the controller or calibrating gyro. Controls, calibration and Custom profiles
are separate; graphics are shared by both local views. AI/network actors retain
native controls.

Both games offer response curves, deadzones/sensitivities, button mapping,
additive firearm gyro, camera options, a weapon-ray reticle and optional movement
while aiming, using the knife or reloading. Native eligibility, collision and
weapon timing remain in charge. Scopes keep the character stationary: right
stick/gyro aim and left stick changes zoom.

For RE5 select **Controls → Profile → Modern**. Left stick moves; right stick
turns the character and camera together and changes vertical look. Overview
pitch has a ±35° comfort band. Default restores native controls with custom gyro
and mapping off. Custom preserves manual controls. See the [RE5 guide](docs/re5-controls.md).

RE4 retains its **RE6** controls preset, based on measured response: fast overview,
slower aiming, unified sticks and moving aim/knife/reload. This is an RE4 control
scheme; the installer game tabs are RE4 and RE5. RE4 also includes:

- Cosmetic Ashley, Ada, Luis, Krauser, Merchant, Ramon and Villager appearances
  over Leon's gameplay, adjusted camera height, held weapons/effects and case
  rotation. Actual playable Ada/Ashley retain their supported paths.
- Optional D-pad weapon/grenade switching, native-font weapon labels, current
  upgraded Firepower in the case, additional weapon upgrades and spare-ammo HUD.
- Optional grenade explosion marker, instant pickup, startup-logo button skip,
  sway removal, first-person view and dot/crosshair reticle.
- **Game → Receive 200 000 pesetas**, with warning and confirmation. Its SD
  receipt prevents repeat grants across restarts and save changes; save after
  receiving it. [Details](docs/money-bonus.md).

RE4 enemy health bars/numbers start **Off** in **Game**, after **Graphics**.
A fresh native aimed target is required. Thin translucent green/yellow/red bars
fade in over 200 ms and out over 300 ms; their ratio uses actual current/max HP.
Numbers use a fixed normal-close, unfrozen, unbonused Firepower1.0 body reference
for the current weapon class. Upgrades do not inflate displayed starting health;
extra native damage removes more displayed HP. Automated checks cover 32
authenticated classes and 256 common-handler subtypes. Scripted, zero-damage or
unknown references retain the previous numeric scale. Linked boss parts use
their actual HP owner. [Validation](docs/releases/v0.28.27.md).

Stick deadzones default to 0.05; gyro to Off, both gyro inversions to On, and gyro
aiming speed to 0.80. Saved settings are preserved on update. Gyro averages
readings 10 ms apart and interpolates presentation with 10 ms delay. Motion
during ordinary weapon raising is discarded before aiming starts.

In **Controller diagnostics**, press **ZL** once for **Gyro test**, or use the
Gyro group's Debug entry. **X** calibrates a stationary controller, **Y** centers
the preview, **B** cancels, and **Save and apply** commits the draft. The static
scene/red dot is a preview, not live gameplay. RE5 saves the selected player's
draft. HOME/Capture remain system buttons; overlay controls remain physical.

## Graphics

Controls/graphics have independent profiles. Manual edits preserve Custom;
Default or Quality keeps the last Custom available for restoration.

| Game / preset | Filtering | Anti-aliasing | Object distance | Shadows |
|---|---|---|---|---|
| RE4 Default | Native | Native | 1.00x | Native |
| RE4 Quality | AF16x | Handheld 900p → 720p SSAA | 1.75x | Native |
| RE5 Default | Native | Native | 1.00x | Native |
| RE5 Quality | AF16x | SMAA1X | 1.75x | Native |

RE4 docked rendering stays native. Distance changes loaded geometry's culling,
not texture resolution or streamed LODs. RE5 shadow Off affects supported
stage-shadow passes. Illustrated help is not a captured comparison of settings.
Higher settings cost GPU time; every mode has not been physically validated.

**After graphics changes fully close the game: HOME → X → Close, then relaunch.**
Returning to a game menu or reopening the overlay is insufficient.

The SD package retains the separate legacy **RE6 Graphics** module/overlay for
update v131072, Build ID `061D2A06CD700785E987C42CE2D72231` (zero-padded to 32
bytes). Native RE6 controls/gyro remain. RE6 is no longer an installer game tab.
Standalone RE4 / RE6 Telemetry, automatic frame journals and the CSV download
server are retired; existing captures and other mods are kept.

## Supported games

| Game | Title ID | Main Build ID / update |
|---|---|---|
| RE4, original Switch port | `010099A00BC1E000` | `82C2A04A21D3E0B850F273D4149C900300000000000000000000000000000000` |
| RE5 | `010018100CD46000` | `4E46401BF2FE53A44B69CC07FD951FE6`, v196608 |
| RE5, installed older main | `010018100CD46000` | `C517ECBB79DE97338E98147A6B5B5F2B`, v65536/v131072 |

RE5 IDs are zero-padded to 32 bytes. Atmosphere with `dmnt:cht` and a
Tesla-compatible loader are required. The development console uses Horizon
22.1.0 with CFW. Exact title, executable and instruction checks precede hooks;
unknown builds stay inactive. Game files are not included.

The v0.28.27 user test accepted RE5 Modern camera, vertical look and all requested
combat movement on the installed older main with attached Joy-Con, local player
1 (internal slot 0). Both profiles and both local players have host regressions.
Split-screen, the newer main, first-person and every weapon/scene have not
received equivalent physical acceptance.

## Install and update

1. Copy `re4-controls.nro` to `sdmc:/switch/re4-controls/re4-controls.nro`.
2. Close the target game. Launch hbmenu in **application mode** by holding R while
   starting an installed title, then open RE Controls. It prepares its HOME shortcut.
3. Select RE4 or RE5 with **L/R**, then **Install / update mod**. Existing
   Uberhand/Tesla, saves, translations and other mods are preserved; foreign-file
   conflicts are rejected.
4. Start the game, open your loader, select **RE4 Controls** or **RE5 Controls**,
   connect and choose your settings.

`re4-controls-sd.zip` is the alternative manual SD package.
[Installation](docs/installation.md) · [RE5 installation](docs/re5-controls.md#install-and-update)

For GitHub updates choose **Updates → Check for updates → Download / install**.
Reopen the app, close the game and run **Install / update mod** again. Updating
only the app/overlay does not replace the game module. After replacing an overlay,
return to Uberhand/Tesla's list and reselect it to unload the older copy.

For manual updates close the app/games and replace all three app images from one
release in `sdmc:/switch/re4-controls/`: `re4-controls.nro`, `bridge.nro` and
`game.nro` (the latter two are in the SD ZIP). Reopen the app and install each
used RE4/RE5 module again. Settings stay saved. Updates validate size, SHA-256,
application identity and version; no GitHub token or console keys are needed.

Local updates start only through **Updates → Local update → Start server**.
The server starts **Off**, accepts authenticated transfers/report downloads and
rejects automatic RUN/restart. Stop or Back cancels an incomplete transfer and
joins the worker. Reopen the app manually after upload. GitHub networking starts
only when **Check for updates** is selected.

The app retains `app-report-0.log` through `app-report-4.log` in its SD folder:
five reports, at most 64 KiB each / 320 KiB total, within a 1 MiB budget. They
record startup/action/error/exit stages, not frames or credentials. Completed
writes are flushed; a hang may leave only its last completed stage. Optional RE5
**Monitor** starts Off and resets to Off on overlay reload. RE4 Normal launch
mode leaves its optional diagnostic server off. `--recovery` skips automatic
HOME-icon publication and GitHub initialization.

## Build, verification and contributing

Version 0.28.27 passed **142/142 host cases**, Switch module/overlay/NRO builds,
package validation and public audit. Coverage was not recalculated; older
percentages describe their historical builds.
[Validation](docs/releases/v0.28.27.md) · [Tests](docs/testing.md)

Windows requirements: Python 3.13, devkitA64/libnx, switch-tools, switch-sdl2,
switch-curl and switch-zlib. Host C++ tests use MinGW64 and SDL2.
[Build instructions](docs/building.md).

```powershell
python -m venv .venv
.venv\Scripts\python.exe -m pip install -r requirements-dev.txt
Copy-Item project.release.json project.json
.venv\Scripts\python.exe build.py --test --jobs 4
.venv\Scripts\python.exe build.py --module --overlay --nro
```

For bugs include game Build ID, mod version, controller, settings and reproduction
steps. Keep keys, tokens, game containers and private reports out of public
attachments. [CONTRIBUTING.md](CONTRIBUTING.md) · [CHANGELOG.md](CHANGELOG.md).
This remains an experimental homebrew mod.

Own code: [MIT](LICENSE). exlaunch: GPL-2.0; libtesla: GPL-2.0-or-later. Pinned
sources/notices are in `third_party/` and release assets. Normal builds need no
console keys. Developer LAN reload uses the pinned
[Switch Debug Kit](https://github.com/EscHelpNew/switch-monitor); its local
credential is excluded from source and public assets.
