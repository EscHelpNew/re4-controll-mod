# RE4 Switch binary findings

Inspected 2026-10-28. All addresses below are offsets from the **main NSO**,
not runtime absolute addresses. Turning was confirmed on hardware on 2026-10-29;
movement/camera/aim paths and additive firearm gyro were subsequently confirmed
by the user through0.2.2. Special weapon/state coverage remains limited.

## Identity and extraction

Title ID: `010099A00BC1E000`; update container version: `131072`.
Full main Build ID:
`82C2A04A21D3E0B850F273D4149C900300000000000000000000000000000000`.
NPDM META flags `0x07`: AArch64, 64-bit address space. NCA SDK version: 5.4.0.0.

The small update contains complete ExeFS. No extraction of the 12 GB base is
needed for code analysis. NSZ 5.0.0 decompressed the update to NSP; official
hactool 1.4.0 extracted the PFS0 and program NCA's ExeFS using locally supplied
keys and the ticket from that same package. Secrets were passed internally,
never printed. hactool's default-key-path warning is harmless with explicit `-k`.

Extracted modules: main (5,260,117 bytes compressed NSO), rtld (7,353), sdk
(5,103,989), subsdk0 (3,344,893), main.npdm (1,508).
`tools/inspect_nso.py` verifies decompressed segment hashes and reports identity.
The files remain in ignored `data/extracted/` and `data/analysis/`.

## Verified input chain

- `0x654550/60/70`: PLT stubs for SDK `GetNpadState` FullKey, Handheld, JoyDual.
- `0x220B40`: common controller reader; dispatches to those three variants.
- `0x221040`, `0x2214E0`, `0x221980`: converters for those controller styles.
- `0x2201A0`: signed linear deadzone function; converters pass `0.08f` from
  `0x6E89D0`. It subtracts the deadzone and rescales the remaining range.
- `0x2201E0`: normalized float to signed 16-bit conversion.
- `0x220140` and tail wrapper `0x2202F0`: reader API.
- `0x25F680`: further gamepad normalization; default result object at
  `main+0x11C79F0`, first raw signed axes at offsets 4/6/8/A and normalized
  floats at C/10/14/18. Defaults for further left/right deadzones at
  `main+0xAFC708` and `main+0xAFC70A` are 7849 and 8689.
- `0x25FB10`: game-level PadRead; calls that normalization at `0x25FB78`, then
  converts axes to signed 8-bit/32-bit game values and applies extra thresholds.
- GOT entry `main+0xB25DA8` resolves to game input structure `main+0x124EC38`.
  Left axes at 0/4; right axes signed bytes at 8/9; input flags at +0x18.
- GOT entry `main+0xB25D78` resolves to the global pointer slot at
  `main+0xB1AE08`. The pointed-to global uses deltaTime at +0xA4.

Thus raw input is already analogue. Adjusting only the HID vector cannot fix
constant-speed turn code downstream.

## Concrete turn candidates

The following sites check right/left bits 2/3 in game input flags, then change
the player yaw at object+0xB4 by a constant loaded through GOT `0xB27D80`.
That entry resolves to `main+0xA0A578`, value `0.04188790172338486` radians.

- Forward movement: right/left block `0x30D84C..0x30D8FC`, uses global deltaTime.
  Right speed load `0x30D858`, multiplication `0x30D86C`, yaw write `0x30D878`.
  Left speed load `0x30D8D8`, multiplication `0x30D8EC`, yaw write `0x30D8F8`.
- Backward movement: right/left block `0x30D9DC..0x30DA1C`; constant speed
  is used without that deltaTime multiplication. Keep this distinction explicit.
- `0x31F204..0x31F23C` and `0x3106C0..0x310718`: similar patterns in other
  player states; exact state identification is pending and must precede patching.

The forward/backward interpretation is supported by checks for input bits 0/1
and comparison with the independently reverse-engineered PC logic. Names are
research labels; execution in the target game must confirm state coverage.

## Runtime findings, 2026-10-29

Installed update is 65536. Its main/sdk/rtld/subsdk0 are byte-identical to update
131072; their Build IDs are the same. The original NPDM signature regions differ.
The module checks the Program ID, full main Build ID and original instructions
before installing any hooks. Loader SDK 5 compatibility is implemented in the
disposable exlaunch build directory. Existing Uberhand is retained unchanged.

Normal movement dispatcher `0x30D430` uses table `0xB067C0`: idle, walk, back,
run, stationary turn, quick turn (indices 0..5). Active player pointer slot is
`0x113B398`, actor mode/routine bytes are +0x114/+0x115. Correction of bounded
net yaw +0xB4 around this dispatcher covers animation-driven turning, which
individual float speed sites did not fully cover. User confirmed L-stick turns
now respond to sensitivity. Quick turn and other routines are excluded.

Movement animation advance `0x2B8F10` takes a motion pointer and returns unsigned
motion flags. It advances frame +0xD4 by speed +0xDC times global deltaTime and
updates packed interpolation frame +0xC8. Primary motion is actor+0x228, secondary
motion pointer actor+0x320. Root update `0x2B7C60` evaluates the previous frame's
root displacement, advances animation, then continues normal ground/physics.
The new hook temporarily scales only the primary/secondary motion speed during
normal manual walk/back/run dispatch; restores it immediately after the call.
No final world position is scaled and collision code is unchanged. Root motion
uses the previously advanced animation frame, so speed changes have one-frame
latency. User confirmed movement speed changes in0.2.0.

Camera update `0x19B20` reads game right-stick signed bytes at input+8/+9, or
+0xC0/+0xC1 when mode `0x11C7D68` equals 1. It computes pitch +0x270 from byte/59
and yaw +0x274 from byte/59 times -pi/3; zero input decays existing offsets.
Scoped replacement from raw SDK right stick preserves full byte range +/-63,
sets configurable deadzone/sensitivity, and restores both bytes after the call.
It affects free-look amplitude/response, not a free-rotating camera rate.

Common firearm aim calculation `0x33DF80` takes actor and integer mode, used by
the shared weapon handler `0x31CC60` (e.g. `0x31CE5C`). Input source is a distinct
structure at `0x124E9C8`, not the movement input at `0x124EC38`. It changes current
and pending normalized pitch at `0x1265A08/+4`, relative yaw actor+0x524, and body
yaw +0xB4 after relative limits. Post-call bounded corrections scale only manual
stick/dpad deltas, preserving large resets and state transitions. The caller
then performs aim/model updates (`0x3183F0`). Scoped/special weapon coverage
remains unverified. Older `0x305xxx/0x308xxx` fields +0x660/+0x680 belong to other
state paths and are not used by this adapter.

Shared ABI 4 exposes separate movement/camera/aim counters. Settings wire format
2 reuses the old reserved float slot for independent movement gain, keeps the
64-byte wire size/checksum and reads version 1 with movement gain default 1.

Hardware feedback: user confirms all three new paths (walking speed, camera,
aim sensitivity) work in 0.2.0. Gyro requested strictly as an additive aiming
input while aiming a firearm, with stick input retained simultaneously.

## Six-axis adapter evidence

SDK exports provide Result-returning detail functions (public wrappers abort on
failure): GetSixAxisSensorHandle `0x2C6440` (handles*, count*, capacity, pad ID,
style bitset), Start `0x2C0F60`/Stop `0x2C0FE0` (handle const&), GetStates
`0x2C6610` (count*, states*, capacity, handle const&). Handles are 4 bytes.
FullKey/Handheld/JoyDual styles 1/2/4, IDs 0/32; JoyDual supports two handles.
GetStates validates handle kind then dispatches to style-specific readers.
State stride 0x60, sampling number +8, angular velocity +28, attributes +88.
The layout matches the independently working diagnostic libnx sensor reader.

The adapter starts only the selected controller's sensor while enabled,
prefer the connected right Joy-Con, consume oldest-to-newest fresh samples,
and discard motion on initial selection/resume gaps over 100 ms. Sensor service
errors stop polling until selection changes or gyro is toggled off/on.
No public SDK assert-on-error wrappers are used. Every pointer/capacity passed
to the detail functions is fixed and valid; only verified handle outputs are
passed to Start/GetStates. Native backend tests cover lifecycle and failures.

Game `0x317630` is the camera's own is-aiming predicate. Its state logic excludes
reload/transition substates within routine6. Gyro also requires active player,
mode0, firearm routines6/11/15 and held abstract aim bit0x10. Motion is added
before the common firearm aim calculation `0x33DF80`, so normal stick calculation
still runs and model updates see the gyro change. Horizontal baseline uses
body yaw radians; vertical baseline adds to the game's normalized pitch [-1,1],
with independent user gain (not a claim of physically exact pitch tracking).

## References

Version0.3.0 uses shared ABI7 with the same200-byte block and settings wirev4.
Flags4..5 store the response curve and bit6 stores language. Old settings load
English/linear; legacy walking-gain slots validate then reset to1.00 because
the manual walking-speed control is removed. Old default0.15 deadzones migrate
to0.05; other custom values are preserved. Both inversions stay on by default.

Stick shaping follows deadzone normalization with x, x², log(1+9x)/log(10), or
an exponential/log blend. The hybrid blend is smoothstep over x=0.25..0.75,
keeping monotonicity, endpoints and sign. All movement/turn/camera/aim stick
paths consume the selected curve; six-axis integration does not. Native
callback tests verify curve propagation to animation speed, yaw, camera and aim.

The standalone NRO now links installed SDL2 and uses HOS pl:u Standard shared
font with unchanged stb_truetype. Font memory stays mapped until after texture/
renderer cleanup. The actual renderer is exercised on Windows with EN/RU frames,
then cross-built for Switch. Remote-reload polling remains on every main-loop
iteration and the existing receiver finish path remains after cleanup.

Version0.2.2 corrects native sway interaction: function0x33EC50 receives actor,
changed-input flags, pitch*, relative-yaw*. Bit0 records a fresh manual baseline
in actor+0x938 weapon context fields+0x28/+0x2C. With no changed-input flag,
the function bounds sway around the old baseline. Injecting gyro coordinates
without that flag reproduced a narrow vertical window. The scoped AimSway
trampoline sets bit0 only while a real gyro delta is applied to the same actor
inside the common aiming calculation. It leaves native behavior otherwise.
Pitch is converted to radians by pi/2 on entry and back on exit. The actual
callback regression reproduces the clamp and verifies accumulation beyond it;
the user confirmed the corrected vertical range on the console.

Current shared ABI6 keeps the200-byte layout of ABI5 but requires the matching
overlay so older modules cannot silently reject settings wirev3. Wirev3 caps
movement/turn/camera/stick-aim gains at1.00. Decoders for old wirev1/v2 validate
the old0..4 range and reduce those gains to1 while preserving other settings.
Both gyro inversions default on after the user's axis-direction test. Their
existing saved choices are retained.

- https://github.com/SciresM/hactool — upstream extraction tool and NCA logic.
- https://github.com/nicoboss/nsz — upstream NCZ decompression.
- https://switchbrew.org/wiki/NSO — NSO header, LZ4, segment hashes.
- https://switchbrew.org/wiki/NPDM — META flags and process architecture.
- https://github.com/nipkownix/re4_tweaks/blob/master/dllmain/ControllerTweaks.cpp
  — PC analogue turning/deadzone research. Its x86 patches are not portable.
# 0.4: source-aware aiming before native consumers

`33DF80` consumes signed int32 effective axes at `124E9C8+0/+4`.
Post-return scaling is too late for all consumers: native code has already updated
weapon sway baselines, aim animation and cached relative yaw. The adapter now
temporarily injects shaped axes before the original call and restores them after it.

The source follows `25FB10` routing: input device `AFF39C==2`, control type
`*(B1AE10)+B==2`, state pointer `*(B1AE08)`, flags `5096/5099`. Default left scale
is63; standard remapped right scale is127; the legacy right branch uses63 times
the state's frame factor at+A4. SDK axis signs are retained, with native pitch
inversion left inside the original function. Physical D-pad flags are preserved.
Separate right aiming gains are quantized into settings-v6 flag bits8..14/16..22;
the64-byte settings size remains unchanged. Shared ABI9 is240bytes.

The native `11C7D68==1` path uses a different pitch triplet at`12661C4`, another
reason to process input before the original function rather than rewriting only
the usual pitch globals after it. No additional game hook is installed.
