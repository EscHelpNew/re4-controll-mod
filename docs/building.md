# Building RE4 Controls

## Tested toolchain

- Windows, Python 3.13 and the pinned `requirements-dev.txt` packages.
- devkitA64 GCC 16.1, libnx 4.12 and switch-tools in devkitPro/MSYS2.
- switch-sdl2 (2.28.5), switch-curl (7.69.1) and switch-zlib.
- MinGW64 GCC and SDL2 for host tests; Windows Segoe UI is used only for test renders.

The default SDK directory is `C:/msys64/opt/devkitpro`; the host compiler and SDL2
default to `C:/msys64/mingw64`. `CXX` can select another host compiler.
No game files or console keys are needed for normal builds.

```powershell
python -m venv .venv
.venv\Scripts\python.exe -m pip install -r requirements-dev.txt
Copy-Item project.release.json project.json
.venv\Scripts\python.exe build.py --test --jobs 4 --coverage
.venv\Scripts\python.exe build.py --module --overlay --nro
.venv\Scripts\python.exe tools\package.py
.venv\Scripts\python.exe tools\release.py
```

`project.json` is local and ignored. The public release template contains only
application identity and a loopback development address. It contains no credential.

The test command accepts `--jobs 2`, `3` or `4` (default: `4`). Each case uses
private temporary files, executables and coverage profiles. Omit `--coverage`
for a faster test-only run. Logs and reports are retained in a unique
`build/tests/run-*` directory; `build/tests/latest.json` identifies the last run.
The coverage report is `coverage/index.html` inside that directory. The command
fails if tests fail, a production source is unaccounted for, or executable-line
coverage is below 90% for either C++ or Python. Branch coverage is reported
separately. Target-only C++ code remains in the denominator through an ARM
compiler baseline; instrumented objects are never packaged into the mod.

The own HOME payload is already built in `assets/home-launcher.bin`. Packaging can
reconstruct the own forwarder NSP from it without keys. Regeneration is optional:
use `tools/build_forwarder.py --keys <local-path>` and then `tools/launcher_bundle.py`
locally, followed by a new NRO build. Keys are inputs to these tools and must never
be copied into source, RomFS, examples or release directories.

## Checks and release

Every fix build must increase the PATCH component of MAJOR.MINOR.PATCH, even
before publication. Never reuse a version for changed fix binaries: fixing
0.17.0 produces 0.17.1. Set the version only in
[`include/re4/version.hpp`](../include/re4/version.hpp); `tools/version.py` and
the build/package tools derive it from that header. Add the matching CHANGELOG
entry and `docs/releases/vX.Y.Z.md`, update current-version documentation, then
rebuild and validate the artifacts with the new embedded version. Earlier test
results remain tied to their original binary and version.

Host tests cover input/settings, actual game callbacks, actual overlay IPC/UI,
installer transactions, real SDL rendering, HOME installation, OTA workers and
public packaging. ARM builds separately check hook context ABI and linking.
CI runs the Windows host suite and a public-history audit.

Before publishing, run the source/history/artifact audit. Optional local key-file
and credential-manager inputs let the maintainer also check for known personal
material without printing its value. Never pass credentials as command-line text.

```powershell
.venv\Scripts\python.exe tools\audit_public.py --history --release build\releases\v0.4.1
```

`tools/github_release.py --publish` requires clean `main`, the configured origin,
and the same commit already pushed to `origin/main`. It uses an existing credential
manager or environment credential in memory, creates a draft, uploads only four
allowlisted assets, checks server digests, then publishes. Users of the NRO do not
need that maintainer credential. This tool does not change repository visibility.
