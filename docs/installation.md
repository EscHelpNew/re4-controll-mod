# Installation and updates

The separate read-only RE6 observer/recording overlay supports update v131072.
See the [measurement procedure](measurement-ru.md); it preserves native RE6
controls. Hardware capture is still an experimental validation step.

RE4 Controls supports the original Switch port of Resident Evil4,
Title ID `010099A00BC1E000`, main Build ID
`82C2A04A21D3E0B850F273D4149C900300000000000000000000000000000000`.
It requires Atmosphere with `dmnt:cht` and a Tesla-compatible overlay loader.
The executable profile is checked before hooks are enabled.

## Install the standalone NRO

Release **0.17.2** includes independent RE4 **Default / Custom / Quality**
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
2. Close RE4 and launch hbmenu in **application mode**: hold R while starting an
   installed title, then select RE4 Controls. Do not use Album while RE4 is suspended.
3. The app prepares its own `bridge.nro`/`game.nro` files and HOME shortcut.
   It updates only its own application, `0100E4C04E540000`.
4. Select **Install / update mod**. The game must be closed.

No console keys or GitHub token are required. Existing Uberhand/Tesla files,
game saves and RomFS translations are preserved. A foreign `subsdk9` or
`main.npdm` produces a conflict rather than being silently replaced.

## Install the SD ZIP manually

Extract the following directories from `re4-controls-sd.zip` to the SD root:

```text
atmosphere/contents/010099A00BC1E000/exefs/subsdk9
atmosphere/contents/010099A00BC1E000/exefs/main.npdm
switch/.overlays/re4-controls.ovl
switch/re4-controls/bridge.nro
switch/re4-controls/game.nro
```

`install/re4-controls-forwarder.nsp` is the own optional HOME launcher for manual
installation. Do not copy an entire ZIP over another mod's ExeFS files without
checking compatibility. The standalone installer performs ownership checks for you.

## In-game controls

Start RE4 again. Open your overlay loader using its configured shortcut and select
**RE4 Controls → Connect to game**. Connecting sends your saved settings to the mod.
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

Reopen RE4 Controls after installation. With RE4 closed, choose **Install / update
mod** to install the new bundled game module and overlay. For RE6, close RE6 and
choose **Install RE6 module** on its page. Updating the app alone does not update
installed modules or overlays. A private repository or missing release returns
an unavailable message; users should never enter a token.

## Manual update, including the same version

1. Close RE4 Controls and both RE4 and RE6 completely before replacing files.
   Close suspended games through **HOME → X → Close**.
2. Download `re4-controls.nro` and `re4-controls-sd.zip` from the **same release**.
   Copy the downloaded NRO and the two NROs inside the ZIP to these paths,
   replacing all three even when their displayed version is unchanged:

   ```text
   re4-controls.nro → sdmc:/switch/re4-controls/re4-controls.nro
   ZIP: switch/re4-controls/bridge.nro → sdmc:/switch/re4-controls/bridge.nro
   ZIP: switch/re4-controls/game.nro → sdmc:/switch/re4-controls/game.nro
   ```

3. Reopen RE4 Controls in application mode; it prepares its own HOME shortcut.
   With the games still closed, choose **RE4 → Install / update mod** and, if
   installed, **RE6 → Install RE6 module** to replace the modules and overlays.
4. Launch the game again and connect its overlay to the game.

Replacing only `re4-controls.nro` can leave the HOME shortcut loading an older
`bridge.nro` or `game.nro`, particularly for same-version replacements. Keep all
three app images together. Saved settings and calibration are preserved; do not
delete the settings files or reset settings to update.

## Remove the mod

Close RE4, open the standalone app and select **Disable mod**. It disables only
files matching its own installation receipt. If installing manually, remove only
the three game/overlay files listed above and restore your previous ExeFS files.
The own HOME application can be removed separately from System Settings.

## Русский кратко

Скопируйте NRO на SD, закройте RE4 и запустите приложение из hbmenu в режиме
приложения. Выберите «Установить / обновить мод». Затем в игре откройте свой
загрузчик оверлеев, выберите RE4 Controls и «Подключиться к игре».

Обычная камера, прицел левым/правым стиком и гироскоп регулируются отдельно.
Гироскоп действует только при наведении из оружия. Мёртвая зона гироскопа —0.50°/с,
мёртвые зоны стиков —0.05; максимальная скорость стиков —1.00.

Обновление: «Обновления → Проверить обновления → Скачать / установить», затем
откройте приложение заново и обновите мод при закрытой игре. Для RE6 выберите
«Установить модуль RE6» на её странице. Само обновление приложения не заменяет
установленные игровые модули и оверлеи.

При ручной замене, даже на ту же версию, полностью закройте приложение и обе
игры. Из одного релиза скопируйте `re4-controls.nro` и файлы `bridge.nro` /
`game.nro` из SD ZIP в `sdmc:/switch/re4-controls/`, заменив все три. Снова
откройте приложение и обновите модули RE4/RE6 при закрытых играх. Настройки и
калибровка сохраняются. Для полного применения графики RE4 требуется
**HOME → X → Закрыть** и новый запуск игры.

Ключи и токены пользователю не нужны. Отключение мода: «Отключить мод» при
закрытой RE4.

The package is experimental. Special weapons and every scripted/QTE state have
not been exhaustively checked. Developer build/reload instructions and dependency
notices are available in the source repository.
