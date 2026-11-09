"""Verify release build inputs, staging and rejection boundaries without SDK builds."""
from contextlib import redirect_stdout, redirect_stderr
from importlib.util import module_from_spec, spec_from_file_location
from io import StringIO
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest.mock import patch
import hashlib
import json
import shutil
import struct
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]


def load_build():
    spec = spec_from_file_location('re4_build_contract_fixture', ROOT / 'build.py')
    module = module_from_spec(spec)
    with patch.object(sys, 'path', list(sys.path)): spec.loader.exec_module(module)
    return module


def module_image(valid=True, data_size=512):
    header = bytearray(256); header[:4] = b'NSO0'; body = bytearray()
    for i, content in enumerate((b'CODE' * 16, b'RODATA' * 8,
                                 (b'RE4CTL_SHARED24\0' if valid else b'WRONG_CONTROL_ID') + bytes(max(0, data_size - 16)))):
        struct.pack_into('<III', header, 16 + i * 16, 256 + len(body), i * 4096, len(content))
        body.extend(content)
    return header + body


class BuildContracts(unittest.TestCase):
    def setUp(self):
        self.temp = TemporaryDirectory(); self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name); self.out = self.root / 'build'; self.out.mkdir()
        self.build = load_build(); self.build.ROOT = self.root; self.build.OUT = self.out
        self.build.KIT = self.root / 'third_party/switch-debug-kit'
        self.build.SDK = self.root / 'fake-sdk'
        self.calls = []; self.npdm_error = False; self.image_valid = True; self.data_size = 512

    def copy(self, name):
        destination = self.root / name; destination.parent.mkdir(parents=True, exist_ok=True)
        if (ROOT / name).is_dir(): shutil.copytree(ROOT / name, destination, dirs_exist_ok=True)
        else: shutil.copy2(ROOT / name, destination)

    def command(self, args, **kwargs):
        args = list(map(str, args)); self.calls.append((args, kwargs)); tool = Path(args[0]).name
        if tool == 'make.exe':
            stage = Path(kwargs['cwd']); (stage / 'deploy').mkdir(exist_ok=True)
            (stage / 'deploy/subsdk9').write_bytes(module_image(self.image_valid, self.data_size))
        elif tool == 'npdmtool.exe':
            Path(args[-1]).write_bytes(b'OWN_NPDM')
        elif tool == 'nacptool.exe':
            Path(args[5]).write_bytes(b'OWN_NACP')
        elif tool == 'elf2nro.exe':
            Path(args[2]).write_bytes(b'OWN_EXECUTABLE_FROM_COMPILER')
        elif '-o' in args:
            output = Path(args[args.index('-o') + 1]); output.parent.mkdir(parents=True, exist_ok=True)
            output.write_bytes(b'OWN_OBJECT')
        elif 'check_hook_abi.py' not in ' '.join(args):
            raise AssertionError('Unexpected SDK boundary: ' + repr(args))
        return subprocess.CompletedProcess(args, 0, 'Failed test' if tool == 'npdmtool.exe' and self.npdm_error else '', '')

    def invoke(self, function):
        with patch.object(self.build.subprocess, 'run', side_effect=self.command), redirect_stdout(StringIO()) as printed:
            function()
        return printed.getvalue()

    def assets(self):
        for name in ('assets/graphics', 'assets/icon-re.png', 'assets/icon-original.png', 'assets/icon-re6.png',
                     'assets/home-launcher.bin', 'assets/fonts/liberation-serif/LICENSE'):
            self.copy(name)

    def vendor(self):
        for name in ('third_party/exlaunch/source', 'third_party/exlaunch/misc',
                     'third_party/exlaunch/Makefile', 'third_party/exlaunch/config.mk',
                     'profiles/010099A00BC1E000/npdm.json'):
            self.copy(name)
        (self.root / 'source/game').mkdir(parents=True)
        for path in (ROOT / 'source/game').glob('*'):
            if path.is_file(): shutil.copy2(path, self.root / 'source/game' / path.name)
        for name in ('source/input.cpp', 'source/settings.cpp'): self.copy(name)

    def test_run_executes_in_project_root_preserves_env_and_propagates_failure(self):
        output = self.root / 'observed.json'
        code = 'import json,os;from pathlib import Path;Path("observed.json").write_text(json.dumps([os.getcwd(),os.environ["FIXTURE_VALUE"]]))'
        self.build.run([Path(sys.executable), '-c', code], env={'FIXTURE_VALUE': 'bounded fixture'}, timeout=10)
        self.assertEqual(json.loads(output.read_text()), [str(self.root), 'bounded fixture'])
        with self.assertRaises(subprocess.CalledProcessError):
            self.build.run([sys.executable, '-c', 'raise SystemExit(17)'], timeout=10)

    def test_cli_help_is_inert_and_worker_limit_rejects_unsupported_parallelism(self):
        self.copy('include/re4/version.hpp')
        source = ROOT / 'build.py'; resolve = Path.resolve
        def sandbox_path(path, *args, **kwargs):
            return self.root / 'build.py' if path == source else resolve(path, *args, **kwargs)
        before = {p.relative_to(self.root): p.read_bytes() for p in self.root.rglob('*') if p.is_file()}
        for argv, status in (([], None), (['--jobs', '1'], 2), (['--jobs', '5'], 2)):
            with self.subTest(argv=argv), patch.object(sys, 'argv', ['build.py', *argv]), \
                 patch.object(sys, 'path', list(sys.path)), patch.object(Path, 'resolve', sandbox_path), \
                 redirect_stdout(StringIO()) as output, redirect_stderr(StringIO()) as errors:
                namespace = {'__name__': '__main__', '__file__': str(source)}
                if status is None:
                    exec(compile(source.read_text(), str(source), 'exec'), namespace)
                    self.assertIn('--coverage', output.getvalue())
                else:
                    with self.assertRaises(SystemExit) as caught:
                        exec(compile(source.read_text(), str(source), 'exec'), namespace)
                    self.assertEqual(caught.exception.code, status)
                    self.assertIn('invalid choice', errors.getvalue())
        self.assertEqual(before, {p.relative_to(self.root): p.read_bytes() for p in self.root.rglob('*') if p.is_file()})

    def test_module_stages_sdk5_loader_hook_budget_and_first_writable_ipc(self):
        self.vendor(); vendor = self.root / 'third_party/exlaunch'
        original = {p.relative_to(vendor): p.read_bytes() for p in vendor.rglob('*') if p.is_file()}
        self.invoke(self.build.module); stage = self.out / 'game-sdk'
        self.assertEqual((stage/'source/program/money_bonus.inc').read_bytes(),(ROOT/'source/game/money_bonus.inc').read_bytes())
        package = self.out / 'experimental/atmosphere/contents/010099A00BC1E000/exefs'
        self.assertEqual((package / 'subsdk9').read_bytes(), module_image())
        self.assertEqual((package / 'main.npdm').read_bytes(), b'OWN_NPDM')
        loader = (stage / 'source/rtld/ModuleObject.cpp').read_text()
        self.assertIn('#ifdef __RTLD_6XX__\n        if (address == 0', loader)
        self.assertIn('JitSize = 0x4000', (stage / 'source/program/setting.hpp').read_text())
        self.assertIn('KEEP(*(.data.re4control))', (stage / 'misc/link.ld').read_text())
        for name in ('health_name.inc', 'ammo_counter.inc', 'character_appearance.inc'):
            self.assertEqual((stage / 'source/program' / name).read_bytes(), (ROOT / 'source/game' / name).read_bytes())
        objects = stage / 'build'; objects.mkdir(); stale = objects / 'old.o'; stale.write_bytes(b'stale')
        keep = objects / 'keep.bin'; keep.write_bytes(b'keep')
        self.invoke(self.build.module); self.assertTrue(stale.exists())
        (stage / 'build-flags.txt').write_text('obsolete flags')
        self.invoke(self.build.module); self.assertFalse(stale.exists()); self.assertEqual(keep.read_bytes(), b'keep')
        for name, data in original.items(): self.assertEqual((vendor / name).read_bytes(), data)
        make, options = next((args, kw) for args, kw in self.calls if args[0].endswith('make.exe'))
        self.assertIn('PROGRAM_ID=010099A00BC1E000', make)
        self.assertTrue(any('-U__RTLD_6XX__' in arg for arg in make)); self.assertTrue(options['check'])
        self.assertEqual(Path(options['cwd']), stage)

    def test_module_refuses_changed_loader_wrong_ipc_and_rejected_npdm(self):
        self.vendor(); loader = self.root / 'third_party/exlaunch/source/rtld/ModuleObject.cpp'
        text = loader.read_text(); loader.write_text(text.replace('address == 0 && ro::g_LookupGlobalManualFunctionPointer', 'address == 1'))
        with self.assertRaisesRegex(RuntimeError, 'loader revision'): self.invoke(self.build.module)
        self.assertEqual(self.calls, [])
        loader.write_text(text)
        for valid, size, error in ((False, 512, False), (True, 400, False), (True, 512, True)):
            with self.subTest(valid=valid, size=size, npdm_error=error):
                self.image_valid = valid; self.data_size = size; self.npdm_error = error
                with self.assertRaisesRegex(RuntimeError, 'NPDM generation' if error else 'first writable data page'):
                    self.invoke(self.build.module)

    def test_overlay_outputs_are_separate_native_source_sets_with_release_metadata(self):
        self.assets()
        for function in (self.build.overlay, self.build.telemetry_overlay, self.build.re6_graphics_overlay):
            self.invoke(function)
        outputs = self.out / 'experimental/switch/.overlays'
        self.assertEqual({p.name for p in outputs.iterdir()}, {'re4-controls.ovl', 're6-telemetry.ovl', 're6-controls.ovl'})
        compile_commands = [args for args, _ in self.calls if '-specs=' + str(self.build.SDK / 'libnx/switch.specs') in args]
        self.assertEqual(len(compile_commands), 3)
        re4, telemetry, re6 = compile_commands
        self.assertIn('source/launch_mode.cpp', re4)
        self.assertIn('source/overlay/live_debug.cpp', re4)
        self.assertIn('source/re6_overlay/worker.cpp', telemetry)
        self.assertNotIn('source/overlay/live_debug.cpp', telemetry)
        self.assertIn('source/re6/settings_file.cpp', re6)
        self.assertNotIn('source/overlay/game_link.cpp', re6)
        for command in compile_commands:
            self.assertIn('-fPIE', command); self.assertIn('-ffixed-x18', command)
        metadata = [args for args, _ in self.calls if Path(args[0]).name == 'nacptool.exe']
        self.assertEqual(len(metadata), 3)
        self.assertTrue(all(args[4] == self.build.VERSION for args in metadata))
        self.assertTrue(all(p.read_bytes() == b'OWN_EXECUTABLE_FROM_COMPILER' for p in outputs.iterdir()))

    def test_diagnostic_build_has_complete_payload_own_identity_and_exact_manifest(self):
        self.assets()
        (self.root / 'project.example.json').write_text(json.dumps({'name': 'RE4 Controls', 'author': 're4-controls',
            'title_id': '0000000000000000', 'sd_directory': 're4-controls', 'host': '127.0.0.1'}))
        artifacts = ('experimental/atmosphere/contents/010099A00BC1E000/exefs/subsdk9',
                     'experimental/atmosphere/contents/010099A00BC1E000/exefs/main.npdm',
                     'experimental/atmosphere/contents/01002A000CD48000/exefs/subsdk9',
                     'experimental/atmosphere/contents/01002A000CD48000/exefs/main.npdm',
                     'experimental/switch/.overlays/re4-controls.ovl', 'experimental/switch/.overlays/re6-telemetry.ovl',
                     'experimental/switch/.overlays/re6-controls.ovl')
        for name in artifacts:
            path = self.out / name; path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(b'OWN_MODULE')
        printed = self.invoke(self.build.diagnostic)
        self.assertIn('automatic RUN remains disabled', printed)
        manifest = json.loads((self.out / 'diagnostic.json').read_text())
        self.assertEqual(manifest, {'file': 're4-controls.nro', 'bytes': 28,
            'sha256': hashlib.sha256(b'OWN_EXECUTABLE_FROM_COMPILER').hexdigest(), 'title_id': '0000000000000000'})
        payload = self.out / 'diagnostic-romfs'
        self.assertEqual({p.name for p in payload.iterdir()}, {'app-logo.bmp', 'app-logo-re6.bmp', 'home-launcher.bin',
            'subsdk9', 'main.npdm', 're4-controls.ovl', 'liberation-serif-OFL.txt',
            're6-subsdk9', 're6-main.npdm', 're6-telemetry.ovl', 're6-controls.ovl'})
        token = (self.root / 'data/token.txt').read_bytes()
        self.assertFalse(any(token in p.read_bytes() for p in payload.iterdir()))
        self.assertIn('NXDK_TITLE_ID 0x0000000000000000ULL', (self.out / 'config/reload_project.h').read_text())
        sources = {arg for args, _ in self.calls for arg in args if arg.endswith('.cpp')}
        self.assertTrue({'source/launch_mode.cpp', 'source/update.cpp', 'source/diagnostic/home_launcher.cpp',
                         'source/re6/log_server.cpp', 'source/diagnostic/main.cpp'}.issubset(sources))
        c_sources = {Path(arg).name for args, _ in self.calls for arg in args if arg.endswith('.c')}
        self.assertEqual(c_sources, {'reload_core.c', 'reload_bridge.c', 'remote_reload.c', 'reload_launch.c', 'reload_reports.c', 'reload_main.c'})
