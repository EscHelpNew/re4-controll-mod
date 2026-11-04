# RE icon and RE4 graphics presets ? local validation 0.17.0

Date: 2026-11-04. Base revision: `bfe9042cc2166beac3a330b01e1c3b7120e2a104` plus the task working-tree changes.
No commit, push, GitHub publication or console deployment was performed for this task.

## Implemented

- Application, hbmenu, installer and own HOME artwork use `assets/icon-re.png`; the original 4 artwork remains unchanged.
- Default restores native filtering, AA, shadows and object distance 1.00x.
- Quality uses AF16x, existing Handheld SSAA, object distance 1.75x and native shadows. SSAA is handheld 900p to 720p; docked rendering remains native.
- Custom retains manual graphics values, including exact Default/Quality recipes. Built-in selections preserve that snapshot. Profile labels continue to follow the active canonical values.
- Both English/Russian interfaces explain full restart via HOME > X > Close, then relaunch; returning to the game menu or overlay is insufficient.

## Executed checks

- `build.py --test`: exit 0; 59 Python tests and the full native C++/actual callback/UI/SDL suite passed. Log: `build/tests-0.17.0.log`.
- New profile regressions were observed failing before implementation and passing afterwards by the implementation agent. Root full suite reran them after integration.
- `build.py --module --overlay --nro`: exit 0; both game modules, overlays and standalone NRO linked for ARM64. Hook-context ABI checks passed. Log: `build/build-0.17.0.log`.
- Independent read-only review: no substantive findings; `git diff --check` passed.
- SDL EN/RU Quality screens viewed at `build/installer-graphics-quality-en.png` and `build/installer-graphics-quality-ru.png`: RE icon, exact values and four help lines fit.
- Own forwarder regenerated through the existing local-key workflow. Its Control NCA was extracted with private output suppressed: the actual 256x256 JPEG shows RE, display version is 0.17.0 and base content version remains 0. JPEG is recompressed by NTON; it is not byte-identical to the input JPEG.
- NRO metadata version and embedded icon match the prepared 0.17.0 assets; staged HOME payload equals `assets/home-launcher.bin`.
- `tools/package.py` and `tools/release.py`: exit 0. SD ZIP contains the new unique forwarder and both NRO launch files match the final NRO byte-for-byte.
- `tools/audit_public.py --keys-file <local-path> --release build/releases/v0.17.0`: exit 0; 559 working files, no findings. No key values are included in this report.

NRO SHA-256: `c03144b9d66eb1dfa67cadaed18f9c6d7aaad894a56021f3f71545a51504de6a`.

## Remaining hardware checks

The user acceptance of 0.16.1 is historical and does not cover this build. On Switch, check existing HOME icon refresh, both live preset pickers and persistence/reopen behavior. Verify full-restart application, handheld/docked Quality image, scene-specific performance and actual overlay font layout. No new FPS, image-quality or console acceptance claim is made here.

A/Right traverses Default > Quality > Custom. If Custom is absent, it reports MissingCustom and remains on Quality; Left returns to Default. This retains the existing profile model.

This report identifies the earlier preset-only c03144b9 local artifact, retained under `build/previous/v0.17.0-presets-c03144b9/`. The later gyro-debug artifact has its own [validation report](2026-11-04-gyro-debug.md).
