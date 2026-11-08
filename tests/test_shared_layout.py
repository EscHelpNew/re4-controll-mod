from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from tools.shared_layout import stage_shared_layout

ROOT = Path(__file__).resolve().parents[1]


class SharedLayoutTests(unittest.TestCase):
    def test_real_link_keeps_shared_marker_before_large_initialized_state(self):
        compiler = Path(os.environ.get('CXX', 'C:/msys64/mingw64/bin/g++.exe'))
        with tempfile.TemporaryDirectory() as directory:
            stage = Path(directory)
            (stage / 'misc').mkdir()
            vendor = ROOT / 'third_party/exlaunch/misc/link.ld'
            original = vendor.read_bytes()
            shutil.copy2(vendor, stage / 'misc/link.ld')
            stage_shared_layout(stage)
            text = (stage / 'misc/link.ld').read_text()
            data = text.split('  .data : {', 1)[1].split('  } :data', 1)[0]
            (stage / 'layout.ld').write_text('SECTIONS { .data : {' + data + '} }')
            (stage / 'layout.cpp').write_text('''
extern "C" {
unsigned char large_state[0x34B48]={1};
__attribute__((used,section(".data.re4control")))
unsigned char control[488]="RE4CTL_SHARED23";
}
''')
            for command in ([str(compiler), '-c', '-fno-common', '-fno-ident', str(stage / 'layout.cpp'), '-o', str(stage / 'layout.o')],
                            [str(compiler.with_name('ld.exe')), '-r', '-T', str(stage / 'layout.ld'), str(stage / 'layout.o'), '-o', str(stage / 'linked.o')],
                            [str(compiler.with_name('objcopy.exe')), '-O', 'binary', '--only-section=.data', str(stage / 'linked.o'), str(stage / 'data.bin')]):
                result = subprocess.run(command, capture_output=True, text=True, timeout=20)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            output = (stage / 'data.bin').read_bytes()
            self.assertEqual(output.find(b'RE4CTL_SHARED23'), 0)
            self.assertIn(1, output[488:520])
            self.assertEqual(vendor.read_bytes(), original)
            source = (stage / 'layout.cpp').read_text()
            for invalid in (source.replace('.data.re4control', '.data.missing'),
                            source.replace('control[488]', 'control[8192]')):
                with self.subTest(source=invalid):
                    (stage / 'layout.cpp').write_text(invalid)
                    compiled = subprocess.run([str(compiler), '-c', '-fno-ident', str(stage / 'layout.cpp'),
                                               '-o', str(stage / 'layout.o')], capture_output=True, text=True, timeout=20)
                    self.assertEqual(compiled.returncode, 0, compiled.stdout + compiled.stderr)
                    linked = subprocess.run([str(compiler.with_name('ld.exe')), '-r', '-T', str(stage / 'layout.ld'),
                                             str(stage / 'layout.o'), '-o', str(stage / 'linked.o')],
                                            capture_output=True, text=True, timeout=20)
                    self.assertNotEqual(linked.returncode, 0)
                    self.assertIn('RE4 IPC block must occupy the first writable page', linked.stderr)

    def test_unknown_linker_revision_is_rejected_before_writing(self):
        with tempfile.TemporaryDirectory() as directory:
            stage = Path(directory)
            (stage / 'misc').mkdir()
            linker = stage / 'misc/link.ld'
            linker.write_text('SECTIONS { .data : { *(.data*) } }')
            before = linker.read_bytes()
            with self.assertRaises(ValueError):
                stage_shared_layout(stage)
            self.assertEqual(linker.read_bytes(), before)


if __name__ == '__main__':
    unittest.main()
