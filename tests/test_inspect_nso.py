import hashlib
import pathlib
import struct
import tempfile
import unittest

import lz4.block

from tools.inspect_nso import inspect_nso, load_nso, inspect_npdm


def fixture(compressed=False):
    header = bytearray(0x100)
    header[:4] = b'NSO0'
    header[0x40:0x60] = bytes(range(32))
    struct.pack_into('<I', header, 0xC, 0x38 | (7 if compressed else 0))
    body = bytearray()
    for i, payload in enumerate((b'code' * 16, b'rodata' * 16, b'data' * 16)):
        packed = lz4.block.compress(payload, store_size=False) if compressed else payload
        struct.pack_into('<III', header, 0x10 + i * 16,
                         0x100 + len(body), i * 0x1000, len(payload))
        struct.pack_into('<I', header, 0x60 + i * 4, len(packed))
        header[0xA0 + i * 32:0xC0 + i * 32] = hashlib.sha256(payload).digest()
        body.extend(packed)
    struct.pack_into('<I', header, 0x3C, 0x80)
    return header + body


class NsoTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.path = pathlib.Path(self.directory.name) / 'main'

    def save(self, data):
        self.path.write_bytes(data)
        return self.path

    def test_identity_and_memory_layout(self):
        image = load_nso(self.save(fixture()))
        self.assertEqual(image.build_id, bytes(range(32)).hex().upper())
        self.assertEqual(image.memory[:64], b'code' * 16)
        self.assertEqual(image.memory[0x1000:0x1060], b'rodata' * 16)
        self.assertEqual(image.bss_size, 0x80)
        self.assertEqual(inspect_nso(self.path)['segments'][1]['memory_offset'], 0x1000)

    def test_lz4_and_segment_hashes(self):
        image = load_nso(self.save(fixture(True)))
        self.assertEqual(image.memory[0x2000:0x2040], b'data' * 16)

    def test_rejects_truncated_header_and_segment(self):
        for data in (b'NSO0', fixture()[:-1]):
            with self.subTest(size=len(data)), self.assertRaises(ValueError):
                load_nso(self.save(data))

    def test_rejects_hash_corruption(self):
        data = fixture()
        data[-1] ^= 1
        with self.assertRaisesRegex(ValueError, 'hash'):
            load_nso(self.save(data))

    def test_rejects_overlapping_or_huge_segments(self):
        for offset in (0, 0xF0000000):
            data = fixture()
            struct.pack_into('<I', data, 0x24, offset)
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                load_nso(self.save(data))

    def test_npdm_reports_instruction_architecture(self):
        data = bytearray(0x80)
        data[:4] = b'META'
        data[0xC] = 7
        self.assertEqual(inspect_npdm(self.save(data))['architecture'], 'aarch64')
        data[0xC] = 0
        self.assertEqual(inspect_npdm(self.save(data))['architecture'], 'arm32')


if __name__ == '__main__':
    unittest.main()
