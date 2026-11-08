"""Keep the IPC block inside old overlays' bounded module-discovery window."""
from pathlib import Path


def stage_shared_layout(stage: Path) -> None:
    linker = Path(stage) / 'misc/link.ld'
    text = linker.read_text(encoding='utf-8')
    original = '  .data : {\n    *(.data .data.*)\n  } :data'
    if text.count(original) != 1:
        raise ValueError('Unexpected exlaunch writable-data linker revision')
    replacement = '''  .data : {
    __re4_control_start__ = .;
    KEEP(*(.data.re4control))
    __re4_control_end__ = .;
    ASSERT(__re4_control_end__ > __re4_control_start__ &&
           __re4_control_end__ - __re4_control_start__ <= 0x1000,
           "RE4 IPC block must occupy the first writable page");
    *(.data .data.*)
  } :data'''
    linker.write_text(text.replace(original, replacement, 1), encoding='utf-8')
