"""Run real packaging/setup code in isolated fake SD trees, without a console."""
from contextlib import redirect_stdout
from ftplib import error_perm
from importlib.util import module_from_spec, spec_from_file_location
from io import StringIO
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest.mock import patch
import hashlib
import json
import sys
import types
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def load(name):
    spec = spec_from_file_location('re4_test_' + name, ROOT / 'tools' / (name + '.py'))
    module = module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class PackageTests(unittest.TestCase):
    def setUp(self):
        self.temp = TemporaryDirectory(prefix='re4 package test ')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.out = self.root / 'build'
        self.cfg = {'sd_directory': 're4-controls', 'title_id': '0100E4C04E540000'}
        self.write('project.json', json.dumps(self.cfg).encode())
        self.private = b'PRIVATE_RE4_TEST_CREDENTIAL_0123456789'
        self.write('data/token.txt', self.private)
        self.write('private.keys', self.private)
        for name in ('re4-controls.nro', 'forwarder/own.nsp',
                     'experimental/atmosphere/contents/010099A00BC1E000/exefs/subsdk9',
                     'experimental/atmosphere/contents/010099A00BC1E000/exefs/main.npdm',
                     'experimental/atmosphere/contents/01002A000CD48000/exefs/subsdk9',
                     'experimental/atmosphere/contents/01002A000CD48000/exefs/main.npdm',
                     'experimental/switch/.overlays/re6-telemetry.ovl',
                     'experimental/switch/.overlays/re4-controls.ovl'):
            self.write('build/' + name, ('OWN_BINARY_' + name).encode())
        for name in ('docs/installation.md','docs/measurement-ru.md', 'LICENSE', 'third_party/exlaunch/LICENSE',
                     'third_party/libtesla/LICENSE', 'third_party/switch-debug-kit/LICENSE',
                     'third_party/nx-ovlloader/LICENSE.md','third_party/graphics-NOTICES.txt','third_party/NTON-GPL3.txt'):
            self.write(name, ('PUBLIC_' + name).encode())
        self.tool = load('package')
        self.tool.ROOT, self.tool.OUT = self.root, self.out

    def write(self, name, content):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)

    def package(self):
        with redirect_stdout(StringIO()):
            self.tool.package()

    def test_manifest_integrity_secrets_and_stale_loader_cleanup(self):
        stale = self.out / 'local-sd/switch/.overlays/ovlmenu.ovl'
        stale.parent.mkdir(parents=True)
        stale.write_bytes(b'stale optional loader must not be shipped')
        self.package()
        self.assertFalse(stale.exists())
        with zipfile.ZipFile(self.out / 're4-controls-experimental.zip') as archive:
            names = archive.namelist()
            self.assertEqual(len(names), len(set(names)))
            manifest = json.loads(archive.read('manifest.json'))
            self.assertEqual(set(names), set(manifest['files']) | {'manifest.json'})
            for name, digest in manifest['files'].items():
                data = archive.read(name)
                self.assertEqual(hashlib.sha256(data).hexdigest(), digest)
                self.assertNotIn(self.private, data)
                self.assertNotIn('..', Path(name).parts)
            self.assertFalse(any(name.endswith(('token.txt', '.keys', '.nca', '.nsz')) for name in names))
            self.assertFalse(any('ovlmenu.ovl' in name or 'exefs.nsp' in name for name in names))
        self.assertEqual((self.out / 'local-sd/switch/re4-controls/token.txt').read_bytes(), self.private)

    def test_ambiguous_forwarder_refused_before_output(self):
        self.write('build/forwarder/second.nsp', b'second')
        with self.assertRaises(RuntimeError):
            self.package()
        self.assertFalse((self.out / 'local-sd').exists())

    def test_unsafe_sd_path_refused_before_output(self):
        self.cfg['sd_directory'] = '../escape'
        self.write('project.json', json.dumps(self.cfg).encode())
        with self.assertRaises(ValueError):
            self.package()
        self.assertFalse((self.out / 'local-sd').exists())


class FtpReadTests(unittest.TestCase):
    def test_missing_file_is_distinct_from_permission_error(self):
        tool = load('dbi_setup')
        class Ftp:
            def retrbinary(self, command, sink):
                raise error_perm(self.error)
        ftp = Ftp()
        ftp.error = '550 Not found'
        self.assertIsNone(tool.get(ftp, '/switch/re4-controls/token.txt'))
        ftp.error = '530 Login required'
        with self.assertRaises(error_perm):
            tool.get(ftp, '/switch/re4-controls/token.txt')

    def test_setup_refuses_different_directory_before_network(self):
        tool = load('dbi_setup')
        with TemporaryDirectory(prefix='re4 ftp test ') as temp:
            tool.ROOT = Path(temp)
            (tool.ROOT / 'project.json').write_text('{"sd_directory":"BLACK"}')
            with patch.object(tool, 'FTP') as ftp:
                with self.assertRaises(ValueError):
                    tool.setup('192.0.2.1', 1)
                ftp.assert_not_called()


class ReloadWrapperTests(unittest.TestCase):
    def test_root_configuration_and_explicit_token_override(self):
        for explicit in (False, True):
            project = types.ModuleType('project')
            project.load = lambda root: root
            args = ['reload.py', 'status'] + (['--token-file', 'explicit-test-token.txt'] if explicit else [])
            with patch.dict(sys.modules, {'project': project}), patch.object(sys, 'argv', args), \
                    patch.object(sys, 'path', list(sys.path)), patch('runpy.run_path') as run:
                tool = load('reload')
                self.assertEqual(project.load(Path('unrelated-cwd')), ROOT)
                run.assert_called_once_with(str(tool.KIT / 'host/reload.py'), run_name='__main__')
                self.assertEqual(sys.argv.count('--token-file'), 1)
                token_index = sys.argv.index('--token-file') + 1
                expected = 'explicit-test-token.txt' if explicit else str(ROOT / 'data/token.txt')
                self.assertEqual(sys.argv[token_index], expected)


class ForwarderTests(unittest.TestCase):
    def test_game_zero_and_malformed_title_ids_are_refused(self):
        tool=load('build_forwarder')
        for title in ('010099A00BC1E000','010099a00bc1e000','0000000000000000','invalid'):
            with TemporaryDirectory(prefix='re4 title guard ') as temp:
                tool.ROOT=root=Path(temp)
                (root/'project.json').write_text(json.dumps({'title_id':title}))
                (root/'test.keys').write_bytes(b'FAKE_TEST_DATA')
                constants=types.ModuleType('nton.constants')
                constants.Directories=types.SimpleNamespace();constants.Files=types.SimpleNamespace()
                with patch.dict(sys.modules,{'nton.constants':constants}),patch.object(sys,'path',list(sys.path)), \
                     patch('tools.prepare_icon.prepare',side_effect=AssertionError('Invalid Title ID reached icon build')):
                    with self.assertRaises(ValueError):tool.build(root/'test.keys')

    def test_icon_build_preserves_previous_on_failure_and_publishes_single_package(self):
        for fail in (True,False):
            tool=load('build_forwarder')
            with TemporaryDirectory(prefix='re4 forwarder test ') as temp:
                root=tool.ROOT=Path(temp)
                (root/'assets').mkdir();(root/'build/forwarder').mkdir(parents=True)
                # Match the same canonical root through another path spelling;
                # Windows CI similarly expands RUNNER~1 into runneradmin.
                tool.ROOT=root/'assets'/'..'
                (root/'assets/icon-original.png').write_bytes((ROOT/'assets/icon-original.png').read_bytes())
                (root/'build/forwarder/old.nsp').write_bytes(b'previous-launcher')
                (root/'build/re4-controls.nro').write_bytes(b'own-nro')
                (root/'test.keys').write_bytes(b'FAKE_TEST_DATA')
                (root/'project.json').write_text(json.dumps({'title_id':'0100E4C04E540000','name':'RE4 Controls','author':'re4-controls'}))
                constants=types.ModuleType('nton.constants')
                constants.Directories=types.SimpleNamespace()
                constants.Files=types.SimpleNamespace()
                main=types.ModuleType('nton.main')
                def build(args,standalone_mode):
                    self.assertFalse(standalone_mode)
                    self.assertIn('/switch/re4-controls/bridge.nro',args)
                    if fail: raise RuntimeError('simulated builder failure')
                    (constants.Directories.output/'new-own.nsp').write_bytes(b'new-launcher')
                main.main=build
                with patch.dict(sys.modules,{'nton.constants':constants,'nton.main':main}), \
                        patch.object(sys,'path',list(sys.path)),redirect_stdout(StringIO()):
                    if fail:
                        with self.assertRaises(RuntimeError):tool.build(root/'test.keys')
                        self.assertEqual((root/'build/forwarder/old.nsp').read_bytes(),b'previous-launcher')
                    else:
                        tool.build(root/'test.keys')
                        self.assertEqual([p.name for p in (root/'build/forwarder').glob('*.nsp')],['new-own.nsp'])
                        backups=list((root/'build/forwarder-previous').glob('*.nsp'))
                        self.assertEqual(len(backups),1)
                        self.assertEqual(backups[0].read_bytes(),b'previous-launcher')


class FtpSetupTests(unittest.TestCase):
    def test_verified_owned_files_only_and_failed_publish_rollback(self):
        for fail, mismatch in ((False, False), (True, False), (False, True)):
            tool = load('dbi_setup')
            with TemporaryDirectory(prefix='re4 setup test ') as temp:
                tool.ROOT = root = Path(temp)
                (root / 'project.json').write_text('{"sd_directory":"re4-controls"}')
                (root / 'build/forwarder').mkdir(parents=True)
                (root / 'build/forwarder/own.nsp').write_bytes(b'own-nsp')
                (root / 'build/re4-controls.nro').write_bytes(b'own-nro')
                (root / 'data').mkdir()
                (root / 'data/token.txt').write_bytes(b'A' * 64)
                base = '/switch/re4-controls/'
                class Ftp:
                    files = {base + 'bridge.nro': b'wrong' if mismatch else b'own-nro',
                             base + 'game.nro': b'own-nro', base + 'token.txt': b'B' * 64}
                    writes = []
                    def __enter__(self): return self
                    def __exit__(self, *args): pass
                    def connect(self, *args, **kwargs): pass
                    def login(self): pass
                    def cwd(self, path): pass
                    def mkd(self, path): pass
                    def retrbinary(self, command, sink):
                        path = command.removeprefix('RETR ')
                        if path not in self.files: raise error_perm('550 missing')
                        sink(self.files[path])
                    def storbinary(self, command, stream):
                        path = command.removeprefix('STOR ')
                        self.writes.append(path); self.files[path] = stream.read()
                    def rename(self, old, new):
                        if fail and old == base + 'token.txt.incoming': raise OSError('interrupted publish')
                        self.files[new] = self.files.pop(old)
                ftp = Ftp()
                with patch.object(tool, 'FTP', return_value=ftp), redirect_stdout(StringIO()):
                    if fail or mismatch:
                        with self.assertRaises((ValueError, OSError)):
                            tool.setup('192.0.2.1', 1)
                        self.assertEqual(ftp.files[base + 'token.txt'], b'B' * 64)
                    else:
                        tool.setup('192.0.2.1', 1)
                        self.assertEqual(ftp.files[base + 'token.txt'], b'A' * 64)
                        self.assertEqual(ftp.files[base + 'token.txt.re4-controls.previous'], b'B' * 64)
                        self.assertEqual(ftp.files[base + 're4-controls-forwarder.nsp'], b'own-nsp')
                self.assertFalse(any(path.endswith('.nro') for path in ftp.writes))
                if mismatch: self.assertEqual(ftp.writes, [])


if __name__ == '__main__':
    unittest.main()
