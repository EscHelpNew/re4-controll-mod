"""Verify the actual ARM64 dispatcher stack saves, not its incorrect C++ header."""
from pathlib import Path
import re

from capstone import Cs, CS_ARCH_ARM64, CS_MODE_ARM
from elftools.elf.elffile import ELFFile


def check(path: Path, nzcv: bool = False) -> None:
    with path.open('rb') as stream:
        elf = ELFFile(stream)
        section = elf.get_section_by_name('.text.exl_inline_float_hook_impl')
        if section is None:
            raise ValueError('Float hook dispatcher is missing')
        code = section.data()
    saved = {}
    stack = 0
    context = None
    for _, _, mnemonic, operands in Cs(CS_ARCH_ARM64, CS_MODE_ARM).disasm_lite(code, 0):
        if mnemonic == 'sub' and (match := re.fullmatch(r'sp, sp, #(0x[0-9a-f]+|\d+)', operands)):
            stack -= int(match[1], 0)
        elif mnemonic == 'stp' and (match := re.fullmatch(r'([xq]\d+), ([xq]\d+), \[sp(?:, #(0x[0-9a-f]+|\d+))?\]', operands)):
            first, second, immediate = match.groups()
            address = stack + int(immediate or '0', 0)
            saved[first] = address
            saved[second] = address + (16 if first.startswith('q') else 8)
        elif mnemonic == 'mov' and operands == 'x0, sp':
            context = stack
        elif mnemonic == 'blr':
            break
    if context != -0x300:
        raise ValueError(f'Unexpected dispatcher context base: {context}')
    for i in range(32):
        if saved.get(f'q{i}') != context + i * 16:
            raise ValueError(f'Saved q{i} is incompatible with project adapter')
    for i in range(30):
        if saved.get(f'x{i}') != context + 0x200 + i * 8:
            raise ValueError(f'Saved x{i} offset changed')
    # Entry saves LR at original SP-0x10, outside the FP area. Reordering pushes
    # would overwrite it with Q31, so verify this remains the GP LR slot.
    if context + 0x200 + 30 * 8 != -0x10:
        raise ValueError('Pre-saved link register would be corrupted')
    if nzcv:
        instructions = list(Cs(CS_ARCH_ARM64, CS_MODE_ARM).disasm_lite(code, 0))
        stack = 0
        events = {}
        for index, (_, _, mnemonic, operands) in enumerate(instructions):
            operands = operands.lower()
            if mnemonic in ('sub', 'add') and (match := re.fullmatch(r'sp, sp, #(0x[0-9a-f]+|\d+)', operands)):
                stack += (-1 if mnemonic == 'sub' else 1) * int(match[1], 0)
            elif mnemonic == 'mrs' and operands == 'x16, nzcv':
                events['mrs'] = (index, stack)
            elif mnemonic in ('str', 'ldr') and (match := re.fullmatch(r'x16, \[sp, #(0x[0-9a-f]+|\d+)\]', operands)):
                events[mnemonic] = (index, stack + int(match[1], 0))
            elif mnemonic == 'blr':
                events['call'] = (index, stack)
            elif mnemonic == 'msr' and operands == 'nzcv, x16':
                events['msr'] = (index, stack)
        keys = ('mrs', 'str', 'call', 'ldr', 'msr')
        if any(key not in events for key in keys):
            raise ValueError('RE5 dispatcher missing NZCV preservation')
        if [events[key][0] for key in keys] != sorted(events[key][0] for key in keys):
            raise ValueError('RE5 dispatcher NZCV save/restore order is invalid')
        if events['str'][1] != -8 or events['ldr'][1] != -8:
            raise ValueError('RE5 dispatcher NZCV save/restore stack slot changed')
    print('Compiled hook ABI: FP +0x000, GP +0x200, pre-saved LR intact PASS')


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('object', type=Path)
    parser.add_argument('--nzcv', action='store_true')
    args = parser.parse_args()
    check(args.object, args.nzcv)
