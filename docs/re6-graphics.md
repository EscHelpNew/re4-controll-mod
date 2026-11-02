# RE6 Graphics

The separate **RE6 Graphics** overlay supports Resident Evil6 Switch update
v131072, Build ID `061D2A06CD700785E987C42CE2D72231` (zero-padded to32bytes).
Native gyro aiming and game controls are preserved.

Close RE6, open the standalone installer, press R for the RE6 page and select
**Install RE6 module**. This installs the module and both overlays. **Install
RE6 overlay** updates only the two menus. Start RE6, open Uberhand and select
**RE6 Graphics**. The **RE4 / RE6 Telemetry** menu remains available separately.

The root contains Graphics, Connect to RE6 and Language. English is the initial
language; Russian is available there. Each graphics row has focused help.

The standalone installer also has **RE6 → Graphics**, editing the same separate
settings for the next launch of RE6. Both menus show an official game-frame
illustration with the selected setting's area outlined. The pictures explain
where to look; they are not screenshots of each selected mod mode.

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

To update from an older build, close RE6 and use **RE6 → Install RE6 module** in
the new installer. That updates the game module and both menus together.

Shadows and draw distance are not exposed: their safe live owner and complete
consumer paths are not yet verified. Host tests and exact instruction guards
cover the implemented paths; visual quality and performance require console
testing with each selected mode.

The module adds only SD permission bit21 for its settings to the native process
permissions; native services remain unchanged. The bit is defined in the
[Atmosphere access-control implementation](https://github.com/Atmosphere-NX/Atmosphere/blob/master/libraries/libstratosphere/include/stratosphere/fssrv/impl/fssrv_access_control_bits.hpp).
