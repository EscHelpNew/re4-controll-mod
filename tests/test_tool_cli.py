"""Exercise host command entry points with local peers and temporary artifacts."""
from contextlib import contextmanager, redirect_stdout, redirect_stderr
from io import BytesIO, StringIO
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest.mock import patch
import hashlib
import json
import os
import shutil
import socket
import struct
import subprocess
import sys
import threading
import types
import unittest
import urllib.error
import urllib.parse
import zipfile

from tools import audit_public, github_release, inspect_nso, live_diagnostics, telemetry_analyze, telemetry_logs, release, version
from test_inspect_nso import fixture as nso_fixture
from test_telemetry_analyze import row, stream
from test_release import nro

ROOT = Path(__file__).resolve().parents[1]
TOKEN = 'a' * 64


@contextmanager
def peer(handlers):
    errors = []
    with socket.socket() as listener:
        listener.bind(('127.0.0.1', 0)); listener.listen(4); listener.settimeout(3)
        def serve():
            try:
                for handler in handlers:
                    with listener.accept()[0] as conn:
                        conn.settimeout(3); handler(conn)
            except BaseException as error: errors.append(error)
        worker = threading.Thread(target=serve, daemon=True); worker.start()
        try: yield listener.getsockname()[1]
        finally:
            worker.join(4)
            if worker.is_alive(): raise AssertionError('Local fixture peer did not finish')
            if errors: raise errors[0]


def read_line(conn):
    value = b''
    while not value.endswith(b'\n'):
        byte = conn.recv(1)
        if not byte: raise AssertionError('Client closed before its fixed request')
        value += byte
    return value


def telemetry_reply(request, response):
    def send(conn):
        assert read_line(conn) == b'AUTH ' + TOKEN.encode() + b'\n'
        conn.sendall(b'OK AUTH\n')
        assert read_line(conn) == request
        conn.sendall(response)
    return send


def diagnostic_reply(conn):
    assert read_line(conn) == b'AUTH ' + TOKEN.encode() + b'\n'
    conn.sendall(b'OK RE4-DIAG 1\n')
    assert read_line(conn) == b'SNAPSHOT\n'
    data = b'{"schema":1,"game":"RE4","visible":false}'
    conn.sendall(b'OK JSON ' + str(len(data)).encode() + b'\n' + data)


def script(name, root, argv):
    """Execute the actual script with a sandbox __file__, retaining code identity."""
    path = ROOT / 'tools' / name
    namespace = {'__name__': '__main__', '__file__': str(path)}
    resolve = Path.resolve
    def sandbox_path(candidate, *args, **kwargs):
        return root / 'tools' / name if candidate == path else resolve(candidate, *args, **kwargs)
    with patch.object(sys, 'argv', [name, *map(str, argv)]), patch.object(sys, 'path', list(sys.path)), \
         patch.object(Path, 'resolve', sandbox_path):
        try: exec(compile(path.read_text(encoding='utf-8-sig'), str(path), 'exec'), namespace)
        except SystemExit as exit: return exit.code
    return 0


class DiagnosticCliTests(unittest.TestCase):
    def test_snapshot_stdout_and_exclusive_file_are_actual_authenticated_responses(self):
        with TemporaryDirectory() as temporary:
            root = Path(temporary); token = root / 'token'; token.write_text(TOKEN)
            for save in (False, True):
                with self.subTest(save=save), peer([diagnostic_reply]) as port, redirect_stdout(StringIO()) as printed:
                    argv = ['--host', '127.0.0.1', '--port', str(port), '--token-file', str(token), 'snapshot']
                    if save: argv += ['--output', str(root / 'snapshot.json')]
                    with patch.object(sys, 'argv', ['diagnostics', *argv]): self.assertEqual(live_diagnostics.main(), 0)
                data = json.loads((root / 'snapshot.json').read_text() if save else printed.getvalue())
                self.assertEqual(data, {'schema': 1, 'game': 'RE4', 'visible': False})
            with self.assertRaises(FileExistsError): live_diagnostics.save_snapshot(root / 'snapshot.json', {})

    def test_watch_success_failure_and_script_error_exit_do_not_leak_credentials(self):
        with TemporaryDirectory() as temporary:
            root = Path(temporary); token = root / 'token'; token.write_text(TOKEN)
            def reject(conn):
                read_line(conn); conn.sendall(b'ERR AUTH ' + TOKEN.encode() + b'\n')
            for ok in (True, False):
                output = root / ('success.jsonl' if ok else 'failure.jsonl')
                with peer([diagnostic_reply if ok else reject]) as port, redirect_stdout(StringIO()):
                    args = ['--host', '127.0.0.1', '--port', port, '--token-file', token,
                            'watch', '--output', output, '--seconds', '.1', '--interval', '.1']
                    self.assertEqual(script('live_diagnostics.py', root, args), 0 if ok else 1)
                rows = [json.loads(line) for line in output.read_text().splitlines()]
                self.assertEqual(len(rows), 1)
                self.assertEqual('data' in rows[0], ok)
                self.assertNotIn(TOKEN, output.read_text())
            with redirect_stderr(StringIO()) as error:
                self.assertEqual(script('live_diagnostics.py', root,
                    ['--host', '127.0.0.1', '--port', 0, '--token-file', token, 'snapshot']), 1)
            self.assertIn('Invalid diagnostic port', error.getvalue()); self.assertNotIn(TOKEN, error.getvalue())
            for duration, interval in ((0, .1), (1, 0), (float('nan'), .1), (1, float('inf'))):
                with self.assertRaises(live_diagnostics.DiagnosticError):
                    live_diagnostics.watch('127.0.0.1', TOKEN, root / 'invalid', duration, interval)
            self.assertFalse((root / 'invalid').exists())


class InspectionAndAnalysisCliTests(unittest.TestCase):
    def test_inspector_cli_writes_exact_decompressed_image_and_npdm_report(self):
        with TemporaryDirectory() as temporary:
            root = Path(temporary); source = root / 'own.nso'; source.write_bytes(nso_fixture(True))
            npdm = root / 'own.npdm'; meta = bytearray(128); meta[:4] = b'META'; meta[12] = 7; npdm.write_bytes(meta)
            output = root / 'private/memory.bin'
            with patch.object(sys, 'argv', ['inspect', str(source), '--npdm', str(npdm), '--memory-output', str(output)]), redirect_stdout(StringIO()) as printed:
                inspect_nso.main()
            report = json.loads(printed.getvalue())
            self.assertEqual(report['process'], {'architecture': 'aarch64', 'address_space_type': 3})
            self.assertEqual(report['segments'][2]['memory_offset'], 8192)
            self.assertEqual(output.read_bytes()[8192:8256], b'data' * 16)
            with self.assertRaisesRegex(ValueError, 'NPDM'): inspect_nso.inspect_npdm(source)
            data = nso_fixture(); struct.pack_into('<I', data, 12, 0x80); source.write_bytes(data)
            with self.assertRaisesRegex(ValueError, 'ZBIC'): inspect_nso.load_nso(source)
            data = nso_fixture(True); data[256:260] = b'\xff' * 4; source.write_bytes(data)
            with self.assertRaisesRegex(ValueError, 'compressed'): inspect_nso.load_nso(source)

    def test_analyzer_cli_uses_observed_tick_units_and_rejects_malformed_capture(self):
        with TemporaryDirectory() as temporary:
            root = Path(temporary); journal = root / 'journal.csv'; report = root / 'reports/motion.json'
            journal.write_text(stream([row(1, x=0), row(2, x=2)]).getvalue(), newline='')
            with patch.object(sys, 'argv', ['analyze', str(journal), '--output', str(report)]), redirect_stdout(StringIO()) as printed:
                telemetry_analyze.main()
            value = json.loads(report.read_text())
            self.assertAlmostEqual(value['intervals'][0]['player_speed_native_s'], 60)
            self.assertEqual(value['rejected_intervals'], 0); self.assertIn('valid_intervals=1', printed.getvalue())
            valid = journal.read_text()
            changes = [('session=4', 'session=bad'), ('session=4', 'session=0'),
                       ('session=4', 'malformed'), ('capacity=256', 'capacity=bad'),
                       ('#capture,start_after_sequence=0,capacity=256', '#capture,start_after_sequence=0'),
                       ('sequence,dropped_total', 'sequence,sequence'),
                       ('sample_id,', 'missing_id,'), ('start_after_sequence=0', 'start_after_sequence=-1')]
            for before, after in changes:
                with self.subTest(after=after), self.assertRaises(ValueError):
                    telemetry_analyze.read_journal(StringIO(valid.replace(before, after)))


class TelemetryCliTests(unittest.TestCase):
    def test_explicit_list_default_host_and_fetch_all_verify_exact_bytes(self):
        with TemporaryDirectory() as temporary:
            root = Path(temporary); token = root / 'token'; token.write_text(TOKEN)
            (root / 'project.json').write_text(json.dumps({'name': 'RE4 Controls', 'author': 'test',
                'title_id': '0000000000000000', 'sd_directory': 're4-controls', 'host': '127.0.0.1'}))
            project = root / 'third_party/switch-debug-kit/host/project.py'; project.parent.mkdir(parents=True)
            shutil.copy2(ROOT / 'third_party/switch-debug-kit/host/project.py', project)
            listing = b'OK LIST 1\nFILE sample.csv 4\nEND\n'
            with peer([telemetry_reply(b'LIST\n', listing)]) as port, redirect_stdout(StringIO()) as printed:
                self.assertEqual(telemetry_logs.main(['--host', '127.0.0.1', '--port', str(port), '--token-file', str(token), 'list']), 0)
            self.assertEqual(printed.getvalue(), 'sample.csv\t4\n')
            data = b'a,b\n'; digest = hashlib.sha256(data).hexdigest()
            reply = f'OK FILE 4 {digest}\n'.encode() + data + b'\nOK DONE\n'
            with peer([telemetry_reply(b'LIST\n', listing), telemetry_reply(b'GET sample.csv\n', reply)]) as port, \
                 patch.object(telemetry_logs, 'ROOT', root), patch.object(sys, 'path', list(sys.path)), redirect_stdout(StringIO()) as printed:
                self.assertEqual(telemetry_logs.main(['--port', str(port), '--token-file', str(token), 'fetch', '--output', str(root / 'downloads')]), 0)
            self.assertEqual((root / 'downloads/sample.csv').read_bytes(), data)
            self.assertIn(digest, printed.getvalue()); self.assertFalse((root / 'downloads/sample.csv.part').exists())
            with self.assertRaisesRegex(ValueError, 'Timeout'):
                telemetry_logs.list_logs('127.0.0.1', TOKEN, timeout=0)
            with self.assertRaisesRegex(ValueError, 'token'):
                telemetry_logs.list_logs('127.0.0.1', 'bad')
            with self.assertRaisesRegex(ValueError, 'basename'):
                telemetry_logs.fetch_log('127.0.0.1', TOKEN, '../escape', root / 'bad')


class AuditAndCredentialBoundaryTests(unittest.TestCase):
    def test_audit_cli_checks_temporary_git_history_and_release_archive(self):
        with TemporaryDirectory() as temporary:
            root = Path(temporary)
            subprocess.run(['git', 'init', '--quiet'], cwd=root, check=True, capture_output=True)
            (root / '.gitignore').write_text('data/\nbuild/\n')
            (root / 'own.txt').write_text('public test source')
            subprocess.run(['git', 'add', '.gitignore', 'own.txt'], cwd=root, check=True, capture_output=True)
            # An empty history is still queried through the actual git protocol;
            # the archive path is ignored by source discovery but audited explicitly.
            directory = root / 'build/releases'; directory.mkdir(parents=True)
            archive = directory / 'own.zip'
            with zipfile.ZipFile(archive, 'w') as output: output.writestr('README.txt', b'public')
            with patch.dict(os.environ, {'GH_TOKEN': '', 'GITHUB_TOKEN': ''}), patch.object(audit_public, 'ROOT', root), redirect_stdout(StringIO()) as printed:
                self.assertEqual(script('audit_public.py', root, ['--history', '--release', directory]), 0)
            report = json.loads(printed.getvalue())
            self.assertEqual(report, {'working_files': 2, 'history_blobs': 0, 'findings': []})
            with zipfile.ZipFile(archive, 'w') as output: output.writestr('data/token.txt', b'public fixture')
            with patch.dict(os.environ, {'GH_TOKEN': '', 'GITHUB_TOKEN': ''}), patch.object(audit_public, 'ROOT', root), redirect_stdout(StringIO()) as printed:
                self.assertEqual(script('audit_public.py', root, ['--release', directory]), 1)
            findings = json.loads(printed.getvalue())['findings']
            self.assertEqual(findings, [{'path': 'own.zip:data/token.txt', 'category': 'private archive path'}])

    def test_publication_cli_checks_identity_and_audit_before_any_api_write(self):
        with TemporaryDirectory() as temporary:
            root = Path(temporary)
            subprocess.run(['git', 'init', '--quiet'], cwd=root, check=True, capture_output=True)
            header = root / 'include/re4/version.hpp'; header.parent.mkdir(parents=True)
            header.write_text('namespace re4 { inline constexpr const char* kVersion="0.4.0"; }\n')
            notes = root / 'docs/releases/v0.4.0.md'; notes.parent.mkdir(parents=True); notes.write_text('Own release notes')
            build = root / 'build'; build.mkdir(); image = build / 'own.nro'; image.write_bytes(nro())
            package = build / 'own.zip'
            files = {name: nro() if name.endswith('.nro') else b'public' for name in release.PUBLIC_FILES}
            with zipfile.ZipFile(package, 'w') as archive:
                for name, data in files.items(): archive.writestr(name, data)
                archive.writestr('manifest.json', json.dumps({'version': '0.4.0', 'files': {name: hashlib.sha256(data).hexdigest() for name, data in files.items()}}))
            directory = build / 'releases/v0.4.0'
            with redirect_stdout(StringIO()): release.prepare_release(image, package, '0.4.0', directory)
            credential = 'FAKE_GITHUB_CREDENTIAL_FOR_PUBLICATION_CLI'
            source = root / 'own.txt'; source.write_text('public')
            network = []; branch = 'main'; remote_commit = 'a' * 40
            def git(args, **kwargs):
                if args[-2:] == ['branch', '--show-current']: return branch.encode()
                if args[-2:] == ['status', '--porcelain']: return b''
                if args[-2:] == ['rev-parse', 'HEAD']: return b'a' * 40
                if args[-3:] == ['remote', 'get-url', 'origin']: return b'https://github.com/EscHelpNew/re4-controll-mod.git'
                if args[-3:] == ['ls-remote', 'origin', 'refs/heads/main']: return (remote_commit + '\trefs/heads/main\n').encode()
                if args[-2:] == ['ls-files', '-z']: return b'own.txt\0'
                if args[-3:] == ['rev-list', '--objects', '--all']: return b''
                raise AssertionError('Unexpected git request: ' + repr(args))
            class Api:
                def open(self, request, timeout):
                    method, url = request.get_method(), request.full_url; network.append((method, url))
                    if method == 'GET': raise urllib.error.HTTPError(url, 404, '', {}, None)
                    if method == 'POST' and url.endswith('/releases'):
                        self_payload = json.loads(request.data)
                        assert self_payload['draft'] and self_payload['target_commitish'] == 'a' * 40
                        value = {'id': 7, 'draft': True, 'assets': []}
                    elif method == 'POST':
                        name = urllib.parse.parse_qs(urllib.parse.urlsplit(url).query)['name'][0]
                        value = {'name': name, 'size': len(request.data), 'digest': 'sha256:' + hashlib.sha256(request.data).hexdigest()}
                    else:
                        assert method == 'PATCH' and json.loads(request.data)['draft'] is False
                        value = {'html_url': 'https://github.com/EscHelpNew/re4-controll-mod/releases/tag/v0.4.0'}
                    return BytesIO(json.dumps(value).encode())
            with patch.object(version, 'ROOT', root), patch.object(audit_public, 'ROOT', root), \
                 patch.dict(os.environ, {'GH_TOKEN': credential, 'GITHUB_TOKEN': ''}), \
                 patch('subprocess.check_output', side_effect=git), patch('urllib.request.build_opener', return_value=Api()):
                for failure in ('branch', 'remote', 'audit'):
                    branch = 'topic' if failure == 'branch' else 'main'
                    remote_commit = 'b' * 40 if failure == 'remote' else 'a' * 40
                    source.write_text(credential if failure == 'audit' else 'public')
                    with self.subTest(failure=failure), redirect_stdout(StringIO()), self.assertRaises(RuntimeError):
                        script('github_release.py', root, ['--publish'])
                    self.assertEqual(network, [])
                source.write_text('public')
                with redirect_stdout(StringIO()) as printed: self.assertEqual(script('github_release.py', root, ['--publish']), 0)
            self.assertEqual([method for method, _ in network], ['GET', 'POST', 'POST', 'POST', 'POST', 'POST', 'PATCH'])
            self.assertIn('/tag/v0.4.0', printed.getvalue()); self.assertNotIn(credential, printed.getvalue())

    def test_known_material_reads_only_requested_sources_and_reports_no_values(self):
        with TemporaryDirectory() as temporary:
            root = Path(temporary); (root / 'data').mkdir()
            token = bytes(range(32)).hex().encode(); (root / 'data/token.txt').write_bytes(token + b'\n')
            key = bytes(range(16)).hex(); keyfile = root / 'fake.keys'
            keyfile.write_text('comment\ninvalid=nothex\nfixture=' + key + '\n')
            credential = 'FAKE_GITHUB_CREDENTIAL_FOR_HOST_TEST'
            with patch.object(audit_public, 'ROOT', root), patch.dict(os.environ, {'GH_TOKEN': credential}, clear=True), \
                 patch.object(audit_public.subprocess, 'run', return_value=subprocess.CompletedProcess([], 0, 'password=ANOTHER_FAKE_CREDENTIAL_FOR_TEST\n', '')):
                known = audit_public.known_material(keyfile, credential_manager=True)
            values = {value for _, value in known}
            self.assertTrue({token, key.encode(), key.upper().encode(), bytes.fromhex(key), credential.encode()}.issubset(values))
            findings = audit_public.scan_material('sample', token + credential.encode(), known)
            self.assertEqual({r['category'] for r in findings}, {'local receiver credential', 'local key material', 'GitHub credential'})
            self.assertNotIn(token.decode(), json.dumps(findings)); self.assertNotIn(credential, json.dumps(findings))
            (root / 'data/token.txt').write_text('a' * 64)
            with patch.object(audit_public, 'ROOT', root), patch.dict(os.environ, {}, clear=True):
                self.assertEqual(audit_public.known_material(), [])

    def test_github_auth_sources_and_404_never_escape_repository_boundary(self):
        credential = 'FAKE_GITHUB_CREDENTIAL_FOR_HOST_TEST'
        with patch.dict(os.environ, {'GH_TOKEN': credential, 'GITHUB_TOKEN': ''}): client = github_release.GitHub()
        self.assertEqual(client._token, credential)
        completed = subprocess.CompletedProcess([], 0, 'username=fixture\npassword=' + credential + '\n', '')
        with patch.dict(os.environ, {'GH_TOKEN': '', 'GITHUB_TOKEN': ''}), patch.object(github_release.subprocess, 'run', return_value=completed):
            client = github_release.GitHub()
        class Missing:
            def open(self, request, timeout): raise urllib.error.HTTPError(request.full_url, 404, '', {}, None)
        client._opener = Missing()
        self.assertIsNone(client.request('GET', '/releases/tags/v0.0.1'))
        with self.assertRaisesRegex(RuntimeError, 'HTTP 404'): client.request('POST', '/releases', blob=b'own')
        with self.assertRaises(ValueError): client.request('GET', 'https://api.github.com/repos/foreign/repo')
        with patch.dict(os.environ, {'GH_TOKEN': '', 'GITHUB_TOKEN': ''}), patch.object(github_release.subprocess, 'run', return_value=subprocess.CompletedProcess([], 0, '', '')):
            with self.assertRaisesRegex(RuntimeError, 'authentication'): github_release.GitHub()
