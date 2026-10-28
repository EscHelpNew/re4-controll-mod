# RE4 Switch binary findings

Inspected 2026-10-28. All addresses below are offsets from the **main NSO**,
not runtime absolute addresses. Hardware verification is still pending.

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

## Pending before game deployment

- Running turn and stationary turn sites; input scheme selection; aim/camera
  calculations and scoped weapons; reliable pause/cutscene/aim state signals.
- Loader compatibility and minimal permissions for settings access.
- Validation against the actual installed executable Build ID.
- No runtime hooks have yet been installed or claimed to work in RE4.

## References

- https://github.com/SciresM/hactool — upstream extraction tool and NCA logic.
- https://github.com/nicoboss/nsz — upstream NCZ decompression.
- https://switchbrew.org/wiki/NSO — NSO header, LZ4, segment hashes.
- https://switchbrew.org/wiki/NPDM — META flags and process architecture.
- https://github.com/nipkownix/re4_tweaks/blob/master/dllmain/ControllerTweaks.cpp
  — PC analogue turning/deadzone research. Its x86 patches are not portable.
