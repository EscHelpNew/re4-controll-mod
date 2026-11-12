"""Select a real AArch64 ELF assembler for unlinked host ABI probes."""
from pathlib import Path
import shutil


def assembler_command(sdk: Path) -> list[str]:
    compiler = sdk / 'devkitA64/bin/aarch64-none-elf-gcc.exe'
    if compiler.is_file():
        return [str(compiler)]
    clang = shutil.which('clang')
    if clang:
        return [clang, '--target=aarch64-none-elf']
    raise RuntimeError('ARM64 ABI tests require devkitA64 GCC or Clang on PATH; '
                       'install either assembler to run the tests.')
