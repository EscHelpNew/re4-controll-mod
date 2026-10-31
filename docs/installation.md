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

The local0.6.0 build also has **Graphics** settings. Defaults match the game.
Filtering and handheld900p SSAA require restarting RE4; SSAA leaves docked
rendering native. Object distance scales loaded geometry culling only, and the
shadow switch controls rendered shadows. Graphics still needs visual/FPS checks
on hardware. Update module and overlay together with **Install / update mod**.

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

The standalone **Controller diagnostics** page shows raw sticks and motion.
Hold ZL for the aiming preview. Y calibrates a stationary controller and saves
the bias; X resets the preview. Each setting has a focused description.

## Update over Wi-Fi

Choose **Updates → Check for updates → Download / install**. The app checks the
public GitHub release manifest, size, SHA-256, NRO structure, application identity
and embedded version before replacing both app images. Previous images are kept,
and failed publication attempts restore the old files when possible.

Reopen RE4 Controls after installation. With RE4 closed, choose **Install / update
mod** to install the new bundled game module and overlay. A private repository or
missing release returns an unavailable message; users should never enter a token.

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
откройте приложение заново и обновите мод при закрытой игре. Ключи и токены
пользователю не нужны. Отключение мода: «Отключить мод» при закрытой RE4.

The package is experimental. Special weapons and every scripted/QTE state have
not been exhaustively checked. Developer build/reload instructions and dependency
notices are available in the source repository.
