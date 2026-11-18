# RE6 Graphics

The separate **RE6 Graphics** overlay supports Resident Evil6 Switch update
v131072, Build ID `061D2A06CD700785E987C42CE2D72231` (zero-padded to32bytes).
Native gyro aiming and game controls are preserved.

The current RE Controls installer selects only **RE4 / RE5**. RE6 has no game tab
or installation action there. This legacy graphics module/overlay remains in
`re4-controls-sd.zip` and can be installed or updated manually:

1. Fully close RE6 through **HOME → X → Close**. Back up any existing own RE6
   module/overlay files. Do not overwrite another mod's ExeFS files.
2. Copy these three files from the same release SD ZIP to the matching SD paths:

   ```text
   atmosphere/contents/01002A000CD48000/exefs/subsdk9
   atmosphere/contents/01002A000CD48000/exefs/main.npdm
   switch/.overlays/re6-controls.ovl
   ```

3. Start RE6, open Uberhand/Tesla and select **RE6 Graphics**, then connect to RE6.
   After an overlay update return to the loader list and select it again to
   unload the older copy. Updating only the overlay does not update the module.

The overlay filename remains `re6-controls.ovl`; its menu is RE6 Graphics.
The old RE4 / RE6 Telemetry menu is retired. To remove a manual installation,
close RE6, remove only these own three files and restore your previous ExeFS
files. Keep existing configuration/captures and other mods.

The root contains Graphics, Connect to RE6 and Language. English is the initial
language; Russian is available there. Each graphics row has focused help.

Edit RE6 graphics in this separate overlay. The current standalone installer has
no RE6 graphics page. The overlay shows an official game-frame illustration with
the selected setting's area outlined; it explains where to look and is not a
screenshot of each selected mod mode.

| Option | Choices | Application |
|---|---|---|
| Texture filtering | Native, AF2x, AF4x, AF8x, AF16x | Restart RE6 |
| Anti-aliasing | FXAA3 (native), fixed | Original game rendering |
| Reset graphics | Restore native texture filtering | Restart RE6 |

Filtering defaults to Native. Configuration is independent of RE4 and stored in
`sdmc:/config/re4-controls/re6-settings.bin`, with backup recovery. A failed
save leaves the active settings unchanged. On cold launch the module reads
these settings before selecting its per-process sampler policy; the overlay
does not need to be opened to enable a saved choice.

The filtering adapter copies the native56-byte descriptor before the constructor
uses it, changing only eligible ordinary trilinear modes7/8. Point/mixed and
comparison samplers retain their original descriptors. Those flags do not
distinguish world textures from trilinear render-target samplers, so both can
be affected.

RE6 anti-aliasing stays native, using the game's stock FXAA3. The AA row is an
informational value, not a selector. Off, FXAA and FXAA3HQ were removed after
hardware reports of artifacts; the module no longer hooks or writes the AA
render path. Valid old settings files automatically migrate their AA value to
Native while retaining language and filtering. New saves/live updates send only
Native, including when connecting to an older module. Other native scene modes
remain owned by the engine; the mod does not force FXAA3 into originally-off scenes.

To update an older installation, close RE6 and replace the same three files from
the new SD ZIP as described above. Preserve `re6-settings.bin`; saved filtering
and language remain independent of RE4/RE5. Restart RE6 and reload its overlay.

Shadows and draw distance are not exposed: their safe live owner and complete
consumer paths are not yet verified. Host tests and exact instruction guards
cover the implemented paths; visual quality and performance require console
testing with each selected mode.

The module adds only SD permission bit21 for its settings to the native process
permissions; native services remain unchanged. The bit is defined in the
[Atmosphere access-control implementation](https://github.com/Atmosphere-NX/Atmosphere/blob/master/libraries/libstratosphere/include/stratosphere/fssrv/impl/fssrv_access_control_bits.hpp).

From0.28.6 the normal RE6 module retains graphics settings/filtering and its frame counter without a recording journal or telemetry-only controller/matrix hooks. The separate RE4 / RE6 Telemetry overlay is retired.
