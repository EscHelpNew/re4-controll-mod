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

| Option | Choices | Application |
|---|---|---|
| Texture filtering | Native, AF2x, AF4x, AF8x, AF16x | Restart RE6 |
| Anti-aliasing | Native, Off, FXAA, FXAA3, FXAA3HQ | Live in the primary renderer |
| Reset graphics | Restore both options to Native | Filtering still needs restart |

Both defaults are Native. Configuration is independent of RE4 and stored in
`sdmc:/config/re4-controls/re6-settings.bin`, with backup recovery. A failed
save leaves the active settings unchanged. On cold launch the module reads
these settings before selecting its per-process sampler policy; the overlay
does not need to be opened to enable a saved choice.

The filtering adapter copies the native56-byte descriptor before the constructor
uses it, changing only eligible ordinary trilinear modes7/8. Point/mixed and
comparison samplers retain their original descriptors. Those flags do not
distinguish world textures from trilinear render-target samplers, so both can
be affected.

AA overrides only the verified current primary scene resource while its native
render call consumes the mode. Its previous value is restored afterward, unless
the engine changed it. The global renderer default is not overwritten. Native
SMAA resources, unsupported objects and concurrent/reentrant override attempts
retain the original behavior. Forced FXAA is limited to resources already using
a native FXAA mode; native-off resources are kept off. No new AA buffers are allocated.

Shadows and draw distance are not exposed: their safe live owner and complete
consumer paths are not yet verified. Host tests and exact instruction guards
cover the implemented paths; visual quality and performance require console
testing with each selected mode.

The module adds only SD permission bit21 for its settings to the native process
permissions; native services remain unchanged. The bit is defined in the
[Atmosphere access-control implementation](https://github.com/Atmosphere-NX/Atmosphere/blob/master/libraries/libstratosphere/include/stratosphere/fssrv/impl/fssrv_access_control_bits.hpp).
