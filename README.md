# RE4 Controls for Nintendo Switch

<img src="assets/icon-original.png" width="128" alt="RE4 Controls">

Smooth stick controls and motion aiming for the original Nintendo Switch release
of **Resident Evil 4**. Includes an English/Russian overlay, a standalone installer
and updates from GitHub Releases. English is the default language.

[Download](https://github.com/EscHelpNew/re4-controll-mod/releases/latest) ·
[Русская инструкция](docs/README.ru.md) · [Installation](docs/installation.md)

## Features

- Proportional turning, walking, camera and firearm aiming.
- Separate left/right aiming sensitivities and adjustable stick deadzones.
- Linear, exponential, logarithmic and blended response curves.
- Remap 20 game-visible buttons, including menus and QTE, with swaps, duplicate
  targets or unbound buttons. The overlay keeps its physical controls.
- Adjustable camera return speed and a continuous 360-degree free camera.
- Additive gyro aiming **only while aiming a firearm**; independent X/Y gains,
  motion speed, calibration and a deadzone in degrees/second.
- Gyro filtering averages two readings separated by 10 ms of sensor time.
- Live settings, help for every control, English/Russian language selection.
- A standalone installer that creates its own HOME shortcut with the supplied icon.
- In-app HTTPS updates with size, SHA-256, application identity and version checks.

Stick speed is capped at the game's native maximum. Default stick deadzones
are 0.05. Both gyro inversions default to on; gyro aiming defaults to off.
Sensitivity/speed defaults are 1.00, except gyro aiming speed at 0.80.
Existing saved sensitivities are preserved when updating.

In **Control settings → Button mapping**, select a physical button and its game
target. HOME/Capture belong to the system and are outside game remapping.
**Camera return speed** ranges from 1.00 (native return) to 0.00 (hold the bounded
look angle). **Free camera 360** adds continuous right-stick rotation with no
return, using Camera sensitivity and the chosen curve. Smooth sticks must be on.
Pitch remains within the native vertical range; normal weapon aiming remains separate.

## Supported game

| Item | Supported value |
|---|---|
| Game | Resident Evil 4, original Nintendo Switch port |
| Title ID | `010099A00BC1E000` |
| Main Build ID | `82C2A04A21D3E0B850F273D4149C900300000000000000000000000000000000` |
| Runtime | Atmosphere with `dmnt:cht`; Tesla-compatible overlay loader |
| Tested console firmware | Horizon 22.1.0 with CFW |

The module checks the title, full Build ID and original instructions before
installing hooks. Other builds remain unsupported until their executable is checked.
This is an **experimental homebrew mod**. Special weapons and every scripted/QTE
context have not been exhaustively tested. Game files are not included.

## Install

1. Download `re4-controls.nro` from Releases and copy it to
   `sdmc:/switch/re4-controls/re4-controls.nro`.
2. Close RE4. Open hbmenu in **application mode** by holding R while launching
   an installed title, then start RE4 Controls. It prepares its own HOME shortcut.
3. Choose **Install / update mod**. Keep your existing Uberhand/Tesla installation;
   RE4 Controls installs only its own overlay.
4. Start RE4, open your loader's overlay menu, select **RE4 Controls**, then
   **Connect to game**. Adjust settings and enable gyro if wanted.

The SD ZIP is an alternative manual installation package. Installation and removal
details are in [docs/installation.md](docs/installation.md).

## Update

Open **RE4 Controls → Updates → Check for updates**. If a newer release exists,
choose **Download / install**. Reopen the app after installation, then choose
**Install / update mod** while RE4 is closed to update its bundled game module.

Updates use the public releases of this repository. No GitHub token or console key
is required. A private repository or missing release produces an unavailable message.
Previous app images are retained; failed validation leaves the current app intact.

## Build and test

Windows build requirements: Python 3.13, devkitA64/libnx, switch-tools, switch-sdl2,
switch-curl and switch-zlib. Host C++ tests use MinGW64 and SDL2.
See [build instructions](docs/building.md) and [test coverage](docs/testing.md).

```powershell
python -m venv .venv
.venv\Scripts\python.exe -m pip install -r requirements-dev.txt
Copy-Item project.release.json project.json
.venv\Scripts\python.exe build.py --test
.venv\Scripts\python.exe build.py --module --overlay --nro
```

The own HOME payload is included in `assets/home-launcher.bin`. Normal NRO builds
need no console keys. Regenerating the forwarder is a separate local packaging step.
Developer LAN reload support uses the pinned
[Switch Debug Kit](https://github.com/EscHelpNew/switch-monitor); its credential is
local and excluded from source and public release assets.

## Contributing and licenses

Include the game Build ID, controller style, settings and reproduction steps in bug
reports. Never attach keys, tokens, game containers or private debug logs.
See [CONTRIBUTING.md](CONTRIBUTING.md) and [CHANGELOG.md](CHANGELOG.md).

Own code: [MIT](LICENSE). The game module uses exlaunch (GPL-2.0), and the overlay
uses libtesla (GPL-2.0-or-later). Complete pinned sources and dependency notices
are in `third_party/`. Release artifacts include their notices.
