"""Inspect locally extracted NSOs; never writes game material into source files."""
from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import struct

import lz4.block

MAX_IMAGE_SIZE = 512 * 1024 * 1024


@dataclass(frozen=True)
class Segment:
    name: str
    file_offset: int
    memory_offset: int
    size: int
    stored_size: int
    compressed: bool


@dataclass(frozen=True)
class NsoImage:
    build_id: str
    segments: tuple[Segment, ...]
    memory: bytes
    bss_size: int
    header: bytes


def load_nso(path: Path) -> NsoImage:
    raw = path.read_bytes()
    if len(raw) < 0x100 or raw[:4] != b'NSO0':
        raise ValueError('Invalid or truncated NSO header')
    flags = struct.unpack_from('<I', raw, 0xC)[0]
    if flags & 0x80:
        raise ValueError('ZBIC compression is not supported by this inspector')
    segments = []
    ranges = []
    chunks = []
    for index, name in enumerate(('text', 'rodata', 'data')):
        file_offset, memory_offset, size = struct.unpack_from('<III', raw, 0x10 + index * 16)
        compressed = bool(flags & (1 << index))
        stored_size = struct.unpack_from('<I', raw, 0x60 + index * 4)[0] if compressed else size
        end = memory_offset + size
        if end > MAX_IMAGE_SIZE or file_offset < 0x100 or file_offset + stored_size > len(raw):
            raise ValueError(f'{name}: invalid segment range')
        if any(memory_offset < hi and lo < end for lo, hi in ranges):
            raise ValueError(f'{name}: overlapping memory segments')
        ranges.append((memory_offset, end))
        payload = raw[file_offset:file_offset + stored_size]
        try:
            payload = lz4.block.decompress(payload, uncompressed_size=size) if compressed else payload
        except lz4.block.LZ4BlockError as error:
            raise ValueError(f'{name}: invalid compressed data') from error
        if len(payload) != size:
            raise ValueError(f'{name}: incorrect decompressed size')
        expected_hash = raw[0xA0 + index * 32:0xC0 + index * 32]
        if flags & (1 << (index + 3)) and hashlib.sha256(payload).digest() != expected_hash:
            raise ValueError(f'{name}: segment hash mismatch')
        segments.append(Segment(name, file_offset, memory_offset, size, stored_size, compressed))
        chunks.append(payload)
    memory = bytearray(max(hi for _, hi in ranges))
    for segment, payload in zip(segments, chunks):
        memory[segment.memory_offset:segment.memory_offset + segment.size] = payload
    return NsoImage(raw[0x40:0x60].hex().upper(), tuple(segments), bytes(memory),
                    struct.unpack_from('<I', raw, 0x3C)[0], raw[:0x100])


def inspect_nso(path: Path) -> dict:
    image = load_nso(path)
    return {'name': path.name, 'build_id': image.build_id, 'bss_size': image.bss_size,
            'segments': [vars(segment) for segment in image.segments]}


def inspect_npdm(path: Path) -> dict:
    data = path.read_bytes()
    if len(data) < 0x80 or data[:4] != b'META':
        raise ValueError('Invalid or truncated NPDM header')
    return {'architecture': 'aarch64' if data[0xC] & 1 else 'arm32',
            'address_space_type': (data[0xC] >> 1) & 7}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('nso', type=Path)
    parser.add_argument('--npdm', type=Path)
    parser.add_argument('--memory-output', type=Path,
                        help='Write decompressed image to a private, ignored analysis directory')
    args = parser.parse_args()
    report = inspect_nso(args.nso)
    if args.npdm:
        report['process'] = inspect_npdm(args.npdm)
    if args.memory_output:
        args.memory_output.parent.mkdir(parents=True, exist_ok=True)
        args.memory_output.write_bytes(load_nso(args.nso).memory)
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
