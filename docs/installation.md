# Installation and updates

RE Controls 0.28.27 selects **RE4 / RE5** with **L/R**. RE4 supports the original
Switch port, Title ID `010099A00BC1E000`, main Build ID
`82C2A04A21D3E0B850F273D4149C900300000000000000000000000000000000`.
RE5 supports Title ID `010018100CD46000`, the exact v196608 and installed
v65536/v131072 mains listed in the [RE5 guide](re5-controls.md).
Atmosphere with `dmnt:cht` and a Tesla-compatible loader are required. The exact
executable profile is checked before hooks are enabled.

RE6 is no longer a selectable installer page. Its separate legacy graphics
module/overlay remains in the SD ZIP for supported v131072, preserving native
controls. Use the manual files in [RE6 Graphics](re6-graphics.md), also supplied
as `README-re6-graphics.md` in the ZIP. No RE6 installation action is exposed in
the current two-game selector.

## Install the standalone NRO

RE4 offers independent **Default / Custom / Quality**
graphics presets. Default restores native graphics; Quality selects AF16x,
handheld 900p-to-720p SSAA, object distance 1.75x and native shadows. Docked
rendering remains native. Custom preserves your last manual graphics choices.
Object distance changes loaded geometry culling only. To apply graphics fully,
close RE4 through **HOME → X → Close** and relaunch it; returning to the game
menu or reopening the overlay is insufficient. Quality image quality and
performance still require hardware checks. Update module and overlay together
with **Install / update mod**.

1. Copy `re4-controls.nro` from the release to
   `sdmc:/switch/re4-controls/re4-controls.nro`.
2. Close the target game and launch hbmenu in **application mode**: hold R while starting an
   installed title, then select RE Controls. Keep the target game fully closed.
3. The app prepares its own `bridge.nro`/`game.nro` files and HOME shortcut.
   It updates only its own application, `0100E4C04E540000`.
4. Select **RE4** or **RE5** with **L/R**, then **Install / update mod**. The game must be closed.

No console keys or GitHub token are required. Existing Uberhand/Tesla files,
game saves and RomFS translations are preserved. A foreign `subsdk9` or
`main.npdm` produces a conflict rather than being silently replaced.

## Install the SD ZIP manually

Copy the selected game's three files and the shared app images from `re4-controls-sd.zip` to the SD root:

```text
atmosphere/contents/010099A00BC1E000/exefs/subsdk9
atmosphere/contents/010099A00BC1E000/exefs/main.npdm
switch/.overlays/re4-controls.ovl
atmosphere/contents/010018100CD46000/exefs/subsdk9
atmosphere/contents/010018100CD46000/exefs/main.npdm
switch/.overlays/re5-controls.ovl
switch/re4-controls/bridge.nro
switch/re4-controls/game.nro
```

`install/re4-controls-forwarder.nsp` is the own optional HOME launcher for manual
installation. Do not copy an entire ZIP over another mod's ExeFS files without
checking compatibility. The standalone installer performs ownership checks for you.

## In-game controls

Start the selected game again. Open your overlay loader using its configured
shortcut and select **RE4 Controls → Connect to game** or
**RE5 Controls → Connect to RE5**. Connecting sends saved settings to the mod.
RE5 has a separate local-player selector; see the [RE5 guide](re5-controls.md).
The detailed sensitivity and preview instructions below describe RE4.
On the development console, the existing loader shortcut is **L + R + D-pad Up**;
other loader installations may use a different combination.

Settings are saved on SD and apply without restarting the game:

- **Camera sensitivity** adjusts normal right-stick look.
- **Left-stick aim X/Y** and **Right-stick aim X/Y** adjust firearm aiming separately.
- Stick deadzones default to0.05. Four response curves are available.
- Stick gains are capped at1.00, the native maximum.
- **Gyro aiming** defaults to off. When enabled, motion adds to stick input only
  during firearm aiming. **Gyro aiming speed** multiplies the X/Y sensitivities.
- **Gyro deadzone** is measured in degrees/second; default0.50. Two readings
  separated by10ms of sensor time are averaged. Both gyro inversions default to on.

The standalone **Controller diagnostics** page shows all four stick axes and raw
motion. Numeric left/right deadzones and muted red hatching mark each axis's zero
interval (axial deadzones). Press **ZL** once to open **Gyro test**; it is also under
**Control settings → Gyro → Debug mode** in the app. No hold is required.
The transparent list edits gyro enabled, speed, deadzone (degrees/second), X/Y
gains and X/Y inversions. The eight rows are these seven settings and **Save and
apply**. In Gyro test, **X** calibrates a stationary controller. **Y** returns the
live dot to its default position (810, 350), zeroes preview angles and clears
pending motion, keeping all settings unchanged.
Changes, including calibration, preview immediately without writing
the settings file. Choose **Save and apply** to validate and save, then return to
the entry page and selection; **B** cancels. Save failure keeps the mode open and
active settings unchanged. The fixed Leon background and dot projection are an
illustration; the filter and angular integration use the game's gyro path.
Outside Gyro test, diagnostics retains **Y** for calibration and **X** for preview reset.

## Update over Wi-Fi

Choose **Updates → Check for updates → Download / install**. The app checks the
public GitHub release manifest, size, SHA-256, NRO structure, application identity
and embedded version before replacing both app images. Previous images are kept,
and failed publication attempts restore the old files when possible.

Reopen RE Controls after installation. With the target game closed, select RE4
or RE5 and choose **Install / update mod** to install its bundled module/overlay.
For legacy RE6 Graphics, update its three manual SD ZIP files with RE6 closed;
there is no RE6 installer tab. Updating the app alone does not update installed
modules/overlays. Reselect a replaced overlay from the loader list to unload its
old copy. A private repository or missing release returns an unavailable message;
users should never enter a token.

## Manual update, including the same version

1. Close RE Controls and all affected games completely before replacing files.
   Close suspended games through **HOME → X → Close**.
2. Download `re4-controls.nro` and `re4-controls-sd.zip` from the **same release**.
   Copy the downloaded NRO and the two NROs inside the ZIP to these paths,
   replacing all three even when their displayed version is unchanged:

   ```text
   re4-controls.nro → sdmc:/switch/re4-controls/re4-controls.nro
   ZIP: switch/re4-controls/bridge.nro → sdmc:/switch/re4-controls/bridge.nro
   ZIP: switch/re4-controls/game.nro → sdmc:/switch/re4-controls/game.nro
   ```

3. Reopen RE Controls in application mode; it prepares its own HOME shortcut.
   With the games closed, select each used **RE4 / RE5** page and choose
   **Install / update mod**. For legacy RE6 Graphics replace the three manual
   files listed in `README-re6-graphics.md` from the same SD ZIP.
4. Launch the game again and connect its overlay to the game.

Replacing only `re4-controls.nro` can leave the HOME shortcut loading an older
`bridge.nro` or `game.nro`, particularly for same-version replacements. Keep all
three app images together. Saved settings and calibration are preserved; do not
delete the settings files or reset settings to update.

## Remove the mod

Close the target RE4/RE5 game, select its page in the app and choose **Disable mod**.
It disables only files matching its own installation receipt. For manual installs,
remove only that game's own three module/overlay files and restore your previous
ExeFS files. Disable preserves saved settings. Legacy RE6 uses the separate
manual-file procedure; there is no current RE6 page.
The own HOME application can be removed separately from System Settings.

## Русский кратко

Скопируйте NRO на SD, закройте нужную игру и запустите приложение из hbmenu в
режиме приложения. Кнопками L/R выберите RE4 или RE5 и «Установить / обновить мод».
Затем в игре откройте свой загрузчик оверлеев, выберите RE4 Controls или
RE5 Controls и подключитесь к игре. Перед настройкой RE5 выберите локального игрока.
Страницы RE6 больше нет; её отдельная графика устанавливается вручную из трёх
файлов SD ZIP, перечисленных в `README-re6-graphics.md`.

Обычная камера, прицел левым/правым стиком и гироскоп регулируются отдельно.
Гироскоп действует только при наведении из оружия. Мёртвая зона гироскопа —0.50°/с,
мёртвые зоны стиков —0.05; максимальная скорость стиков —1.00.

Обновление: «Обновления → Проверить обновления → Скачать / установить», затем
откройте приложение заново и обновите мод на странице RE4/RE5 при закрытой игре.
Для прежнего модуля графики RE6 замените три его файла из SD ZIP вручную.
Само обновление приложения не заменяет
установленные игровые модули и оверлеи.

При ручной замене, даже на ту же версию, полностью закройте приложение и нужные
игры. Из одного релиза скопируйте `re4-controls.nro` и файлы `bridge.nro` /
`game.nro` из SD ZIP в `sdmc:/switch/re4-controls/`, заменив все три. Снова
откройте приложение и обновите модули RE4/RE5 при закрытых играх. Настройки и
калибровка сохраняются. Для полного применения графики RE4 требуется
**HOME → X → Закрыть** и новый запуск игры.

Ключи и токены пользователю не нужны. Отключение мода: «Отключить мод» при
закрытой выбранной RE4/RE5.

The package is experimental. Special weapons and every scripted/QTE state have
not been exhaustively checked. Developer build/reload instructions and dependency
notices are available in the source repository.


Since 0.28.6 the retired RE4 / RE6 Telemetry menu is absent from normal packages. Startup retires only an owned `re6-telemetry.ovl` with a matching old receipt; changed or foreign files remain untouched. Existing capture files are kept. To remove old native frame journals, update the respective RE4/RE6 modules with each game closed and restart the games.

For a controlled recovery launch, use hbmenu/netloader with `--recovery`.
This skips automatic HOME-icon publication and GitHub initialization. Local
installation and settings remain available. For local updates or reports,
choose **Updates → Local update → Start server** explicitly (available from both
RE4/RE5 pages). The server starts Off on every launch and reports its IP/port.
Stop or Back waits for the receiver before leaving; close/reopen manually after
a successful update. This workflow does not automatically restart the app.

Application reports rotate through five fixed files under
`switch/re4-controls/app-report-0.log` to `app-report-4.log`, each at most64KiB
(total 320 KiB, within 1 MiB). They record startup/action/error/exit stages. An authenticated
report download is available only while the local server is enabled. Console
startup and sleep/wake hangs were previously reported; a universal fix remains unproven.
