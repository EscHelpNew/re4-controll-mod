"""Exercise build-time transformations in disposable trees, never the live build."""
from contextlib import redirect_stdout
from io import StringIO
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest.mock import patch
import json
import os
import re
import shutil
import subprocess
import unittest

from PIL import ImageFont

from tools import check_hook_abi, re6_module
from tools.arm_assembler import assembler_command

ROOT = Path(__file__).resolve().parents[1]
SDK = Path(os.environ.get('NXDK_MSYS_ROOT', 'C:/msys64')) / 'opt/devkitpro'
IMPORTS = {
    '_ZN2nn2ro6detail15g_pAutoLoadListE',
    '_ZN2nn2ro6detail36g_pLookupGlobalManualFunctionPointerE',
    '_ZN2nn2fs11GetFileSizeEPlNS0_10FileHandleE',
    '_ZN2nn2fs19MountSdCardForDebugEPKc',
    '_ZN2nn2fs8OpenFileEPNS0_10FileHandleEPKci',
    '_ZN2nn2fs8ReadFileENS0_10FileHandleElPvm',
    '_ZN2nn2fs9CloseFileENS0_10FileHandleE',
}


def execute_script(name, fake_root):
    """Keep original code filenames for coverage while redirecting __file__/cwd."""
    source = ROOT / 'tools' / name
    namespace = {'__name__': '__main__', '__file__': str(source)}
    resolve = Path.resolve
    def sandbox_path(path, *args, **kwargs):
        return fake_root / 'tools' / name if path == source else resolve(path, *args, **kwargs)
    before = Path.cwd()
    try:
        os.chdir(fake_root)
        with patch.object(Path, 'resolve', sandbox_path):
            exec(compile(source.read_text(encoding='utf-8-sig'), str(source), 'exec'), namespace)
    finally:
        os.chdir(before)
    return namespace


class BuildScriptTests(unittest.TestCase):
    def test_loader_build_stages_compatibility_and_preserves_vendor(self):
        with TemporaryDirectory() as temporary:
            root = Path(temporary)
            vendor = root / 'third_party/nx-ovlloader'
            (vendor / 'source').mkdir(parents=True)
            (vendor / 'source/main.c').write_text('NORETURN void worker(void) {}\n')
            original = {'title_id': '0x010000000000100d', 'title_id_range_min': '0x010000000000100d',
                        'title_id_range_max': '0x010000000000100d'}
            (vendor / 'ovll.json').write_text(json.dumps(original))
            (root / 'build').mkdir()
            calls = []
            def make(args, **kwargs):
                calls.append((args, kwargs))
                stage = Path(kwargs['cwd'])
                config = json.loads((stage / 'ovll.json').read_text())
                self.assertEqual(config['program_id'], '0x010000000000100d')
                self.assertEqual(config['process_category'], 0)
                self.assertTrue(config['allow_system_resource_pool'])
                self.assertNotIn('NORETURN', (stage / 'source/main.c').read_text())
                (stage / 'ovll.nsp').write_bytes(b'OWN_LOADER_FROM_FAKE_COMPILER')
            with patch('subprocess.run', side_effect=make), redirect_stdout(StringIO()):
                execute_script('build_overlay_loader.py', root)
            self.assertEqual((vendor / 'source/main.c').read_text(), 'NORETURN void worker(void) {}\n')
            self.assertEqual(json.loads((vendor / 'ovll.json').read_text()), original)
            self.assertEqual((root / 'build/diagnostic-romfs/nx-ovlloader.nsp').read_bytes(), b'OWN_LOADER_FROM_FAKE_COMPILER')
            self.assertEqual((root / 'build/diagnostic-romfs/boot2.flag').read_bytes(), b'')
            self.assertEqual(calls[0][0][1:], ['-j4', 'RELEASE=1'])
            self.assertTrue(calls[0][1]['check'])
            self.assertIn('TMPDIR', calls[0][1]['env'])

    def test_serif_generator_matches_the_shipped_geometry_and_is_deterministic(self):
        with TemporaryDirectory() as temporary:
            root = Path(temporary)
            font = root / 'assets/fonts/liberation-serif/LiberationSerif-BoldItalic.ttf'
            font.parent.mkdir(parents=True)
            shutil.copy2(ROOT / font.relative_to(root), font)
            output = root / 'include/re4/serif_glyphs.inc'
            output.parent.mkdir(parents=True)
            with redirect_stdout(StringIO()):
                execute_script('generate_serif_glyphs.py', root)
                first = output.read_bytes()
                # Hosted Pillow builds may omit RAQM; retain the shipped metrics.
                with patch.object(ImageFont.core, 'HAVE_RAQM', False):
                    execute_script('generate_serif_glyphs.py', root)
            self.assertEqual(first, output.read_bytes())
            self.assertEqual(first, (ROOT / 'include/re4/serif_glyphs.inc').read_bytes())
            glyph_section = first.decode().split('serif_glyphs[]={', 1)[1]
            codes = [int(code) for code in re.findall(r'^\{(\d+),', glyph_section, re.M)]
            self.assertEqual(codes, sorted([*range(32, 127), 0x401, *range(0x410, 0x450), 0x451]))


class HookAbiTests(unittest.TestCase):
    def test_actual_assembled_dispatcher_and_register_corruptions(self):
        compiler = assembler_command(SDK)
        original = (ROOT / 'third_party/exlaunch/source/lib/hook/nx64/inline_asm.s').read_text()
        variants = [('', original),
                    ('context base', original.replace('CTX_STACK_FLOAT_SIZE, 0x200', 'CTX_STACK_FLOAT_SIZE, 0x210')),
                    ('Saved q0', original.replace('stp q0, q1, [sp, #0x00]', 'stp q0, q1, [sp, #0x10]')),
                    ('Saved x0', original.replace('stp x0, x1, [sp, #0x00]', 'stp x0, x1, [sp, #0x10]')),
                    ('dispatcher is missing', '.text\n nop\n')]
        with TemporaryDirectory() as temporary:
            source, obj = Path(temporary) / 'dispatcher.s', Path(temporary) / 'dispatcher.o'
            for error, assembly in variants:
                with self.subTest(error=error):
                    source.write_text(assembly)
                    result = subprocess.run([*compiler, '-c', str(source), '-o', str(obj)],
                                            capture_output=True, text=True, timeout=30)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    if error:
                        with self.assertRaisesRegex(ValueError, error): check_hook_abi.check(obj)
                    else:
                        with redirect_stdout(StringIO()) as printed: check_hook_abi.check(obj)
                        self.assertIn('pre-saved LR intact PASS', printed.getvalue())

    def test_re5_nzcv_order_and_effective_stack_slot(self):
        compiler = assembler_command(SDK)
        original = (ROOT / 'third_party/exlaunch/source/lib/hook/nx64/inline_asm.s').read_text()
        staged = original.replace('    stp x28, x29, [sp, #0xE0]',
            '    stp x28, x29, [sp, #0xE0]\n    mrs x16, nzcv\n    str x16, [sp, #0xF8]')
        staged = staged.replace('.macro armRecoverRegisters',
            '.macro armRecoverRegisters\n    ldr x16, [sp, #0xF8]\n    msr nzcv, x16')
        variants = [('', staged), ('missing NZCV', original),
            ('stack slot', staged.replace('ldr x16, [sp, #0xF8]', 'ldr x16, [sp, #0xF0]')),
            ('order', staged.replace('    msr nzcv, x16', '').replace('    blr x20', '    msr nzcv, x16\n    blr x20'))]
        with TemporaryDirectory() as temporary:
            source, obj = Path(temporary) / 'dispatcher.s', Path(temporary) / 'dispatcher.o'
            for error, assembly in variants:
                source.write_text(assembly)
                result = subprocess.run([*compiler, '-c', str(source), '-o', str(obj)], capture_output=True, text=True, timeout=30)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                if error:
                    with self.assertRaisesRegex(ValueError, error): check_hook_abi.check(obj, nzcv=True)
                else:
                    with redirect_stdout(StringIO()): check_hook_abi.check(obj, nzcv=True)


class Re6BuildStagingTests(unittest.TestCase):
    def setUp(self):
        self.temporary = TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        vendor = self.root / 'third_party/exlaunch'
        shutil.copytree(ROOT / 'third_party/exlaunch/source', vendor / 'source')
        shutil.copytree(ROOT / 'third_party/exlaunch/misc', vendor / 'misc')
        for name in ('Makefile', 'config.mk'): shutil.copy2(ROOT / 'third_party/exlaunch' / name, vendor / name)
        for name in ('source/re6/module.cpp', 'source/re6/settings.cpp', 'profiles/01002A000CD48000/npdm.json'):
            target = self.root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / name, target)
        self.vendor = vendor
        self.original = {p.relative_to(vendor): p.read_bytes() for p in vendor.rglob('*') if p.is_file()}
        self.calls = []
        self.imports = IMPORTS
        self.npdm_error = False

    def subprocess(self, args, **kwargs):
        args = list(map(str, args)); self.calls.append((args, kwargs))
        if args[0].endswith('make.exe'):
            stage = Path(kwargs['cwd'])
            (stage / 'deploy').mkdir(exist_ok=True)
            (stage / 'deploy/subsdk9').write_bytes(b'OWN_RE6_MODULE')
            return subprocess.CompletedProcess(args, 0)
        if args[0].endswith('nm.exe'):
            return subprocess.CompletedProcess(args, 0, ''.join(' U ' + name + '\n' for name in sorted(self.imports)), '')
        if args[0].endswith('npdmtool.exe'):
            Path(args[-1]).write_bytes(b'OWN_RE6_NPDM')
            return subprocess.CompletedProcess(args, 0, 'Failed fixture' if self.npdm_error else '', '')
        raise AssertionError('Unexpected external command: ' + repr(args))

    def build(self):
        with patch.object(re6_module.subprocess, 'run', side_effect=self.subprocess), redirect_stdout(StringIO()):
            return re6_module.build(self.root, SDK, lambda args: self.calls.append((list(map(str, args)), {})))

    def test_real_staging_minimal_permissions_loader_flags_and_object_cache(self):
        package = self.build()
        self.assertEqual(package, self.root / 'build/experimental/atmosphere/contents/01002A000CD48000/exefs')
        self.assertEqual((package / 'subsdk9').read_bytes(), b'OWN_RE6_MODULE')
        stage = self.root / 'build/re6-game-sdk'
        config = json.loads((stage / 'config.json').read_text())
        self.assertEqual(int(config['filesystem_access']['permissions'], 16), 0x4000000000200000)
        original = json.loads((self.root / 'profiles/01002A000CD48000/npdm.json').read_text())
        self.assertEqual(config['service_access'], original['service_access'])
        self.assertIn('g_pLookupGlobalManualFunctionPointerE', (stage / 'source/rtld.hpp').read_text())
        self.assertIn('if(i >= HookMax)', (stage / 'source/lib/hook/nx64/hook_impl.cpp').read_text())
        self.assertIn('JitSize = 0x1000', (stage / 'source/program/setting.hpp').read_text())
        objects = stage / 'build'; objects.mkdir()
        stale = objects / 'stale.o'; stale.write_bytes(b'cached')
        keep = objects / 'keep.txt'; keep.write_bytes(b'not an object')
        self.build(); self.assertTrue(stale.exists())
        (stage / 'build-flags.txt').write_text('old flags')
        self.build(); self.assertFalse(stale.exists()); self.assertEqual(keep.read_bytes(), b'not an object')
        for name, data in self.original.items(): self.assertEqual((self.vendor / name).read_bytes(), data)
        make = next(args for args, _ in self.calls if args[0].endswith('make.exe'))
        self.assertIn('PROGRAM_ID=01002A000CD48000', make)
        self.assertTrue(any('-D__RTLD_6XX__' in arg for arg in make))
        self.assertTrue(any('check_hook_abi.py' in ' '.join(args) for args, _ in self.calls))

    def test_invalid_profile_loader_allocator_imports_and_npdm_fail_closed(self):
        cases = ('profile', 'layout', 'flags', 'allocator', 'imports', 'npdm')
        for case in cases:
            with self.subTest(case=case):
                # Restore source guards between independent invalid inputs.
                for name, data in self.original.items(): (self.vendor / name).write_bytes(data)
                profile = self.root / 'profiles/01002A000CD48000/npdm.json'
                shutil.copy2(ROOT / 'profiles/01002A000CD48000/npdm.json', profile)
                self.imports = IMPORTS; self.npdm_error = False
                if case == 'profile':
                    cfg = json.loads(profile.read_text()); cfg['program_id'] = '0x1'; profile.write_text(json.dumps(cfg))
                else:
                    changed = {'layout': ('source/rtld/ModuleObject.hpp', 'sizeof(ModuleObject) == 0xD0'),
                               'flags': ('misc/mk/common.mk', '-D__RTLD_6XX__'),
                               'allocator': ('source/lib/hook/nx64/hook_impl.cpp', 'if(i > HookMax)')}
                    if case in changed:
                        name, token = changed[case]; path = self.vendor / name
                        path.write_text(path.read_text().replace(token, 'unexpected revision'))
                    elif case == 'imports': self.imports = {'foreign_import'}
                    else: self.npdm_error = True
                with self.assertRaisesRegex((ValueError, RuntimeError),
                     {'profile': 'Invalid RE6', 'layout': 'layout/flags', 'flags': 'layout/flags',
                      'allocator': 'allocator revision', 'imports': 'import mismatch', 'npdm': 'NPDM generation'}[case]):
                    self.build()
                if case not in ('imports', 'npdm'):
                    self.assertFalse(any(args[0].endswith('make.exe') for args, _ in self.calls))
                self.calls.clear()
