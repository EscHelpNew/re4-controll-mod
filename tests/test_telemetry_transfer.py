"""Exercise the host client against actual TCP peers, including broken peers."""
from contextlib import contextmanager
from contextlib import redirect_stderr
from importlib.util import module_from_spec, spec_from_file_location
from pathlib import Path
from tempfile import TemporaryDirectory
import hashlib
import socket
import threading
import time
import unittest
from unittest.mock import patch
from io import StringIO

ROOT = Path(__file__).resolve().parents[1]
spec = spec_from_file_location('telemetry_logs', ROOT / 'tools/telemetry_logs.py')
logs = module_from_spec(spec)
spec.loader.exec_module(logs)
TOKEN = 'a' * 64  # Public deterministic test credential.


@contextmanager
def peer(handler):
    errors = []
    with socket.socket() as listener:
        listener.bind(('127.0.0.1', 0))
        listener.listen(1)
        listener.settimeout(2)
        def run():
            try:
                with listener.accept()[0] as conn:
                    conn.settimeout(2)
                    handler(conn)
            except (BrokenPipeError, ConnectionResetError):
                pass  # Client deliberately rejects malformed peers.
            except BaseException as error:
                errors.append(error)
        worker = threading.Thread(target=run)
        worker.start()
        try:
            yield listener.getsockname()[1]
        finally:
            worker.join(3)
            if worker.is_alive():
                raise AssertionError('Client/peer did not terminate')
            if errors:
                raise errors[0]


def line(conn):
    result = b''
    while not result.endswith(b'\n'):
        data = conn.recv(1)
        if not data:
            raise AssertionError('Client closed before sending request')
        result += data
    return result


def authenticated(conn, request):
    assert line(conn) == b'AUTH ' + TOKEN.encode() + b'\n'
    conn.sendall(b'OK AUTH\n')
    assert line(conn) == request


class TelemetryTransferTests(unittest.TestCase):
    def test_list_handles_fragmented_reply_and_exposes_only_valid_entries(self):
        def server(conn):
            authenticated(conn, b'LIST\n')
            for byte in b'OK LIST 2\nFILE a.csv 3\nFILE empty.csv 0\nEND\n':
                conn.sendall(bytes([byte]))
        with peer(server) as port:
            self.assertEqual(logs.list_logs('127.0.0.1', TOKEN, port=port),
                             [{'name': 'a.csv', 'bytes': 3}, {'name': 'empty.csv', 'bytes': 0}])

    def test_auth_rejection_is_not_echoed_and_does_not_create_output(self):
        def server(conn):
            line(conn)
            conn.sendall(('ERR ' + TOKEN + '\n').encode())
        with peer(server) as port:
            with self.assertRaises(logs.LogTransferError) as error:
                logs.list_logs('127.0.0.1', TOKEN, port=port)
        self.assertNotIn(TOKEN, str(error.exception))

    def test_list_rejects_traversal_part_duplicates_and_limits(self):
        for reply in [b'OK LIST 1\nFILE ../a.csv 3\nEND\n',
                      b'OK LIST 1\nFILE a.csv.part 3\nEND\n',
                      b'OK LIST 2\nFILE a.csv 3\nFILE a.csv 3\nEND\n',
                      b'OK LIST 1025\n', b'OK LIST 1\nFILE a.csv 67108865\nEND\n',
                      b'OK LIST 0\nWRONG\n', b'x' * 300 + b'\n']:
            def server(conn):
                authenticated(conn, b'LIST\n')
                conn.sendall(reply)
            with self.subTest(reply=reply[:30]), peer(server) as port:
                with self.assertRaises(logs.LogTransferError):
                    logs.list_logs('127.0.0.1', TOKEN, port=port)

    def test_fetch_streams_exact_bytes_and_atomically_publishes_verified_file(self):
        payload = b'frame,buttons\r\n1,42\r\n' * 10000
        digest = hashlib.sha256(payload).hexdigest()
        def server(conn):
            authenticated(conn, b'GET a.csv\n')
            conn.sendall(f'OK FILE {len(payload)} {digest}\n'.encode())
            for offset in range(0, len(payload), 13):
                conn.sendall(payload[offset:offset+13])
            conn.sendall(b'\nOK DONE\n')
        with TemporaryDirectory() as folder, peer(server) as port:
            target = Path(folder) / 'a.csv'
            self.assertEqual(logs.fetch_log('127.0.0.1', TOKEN, 'a.csv', target, port=port), digest)
            self.assertEqual(target.read_bytes(), payload)
            self.assertFalse(target.with_suffix('.csv.part').exists())

    def test_fetch_rejects_hash_mismatch_truncation_mutation_and_oversize(self):
        digest = hashlib.sha256(b'abc').hexdigest()
        replies = [f'OK FILE 3 {digest}\nxyz\nOK DONE\n'.encode(),
                   f'OK FILE 3 {digest}\nab'.encode(),
                   f'OK FILE 3 {digest}\nabc'.encode(),  # No stable-file trailer.
                   f'OK FILE 67108865 {digest}\n'.encode()]
        for reply in replies:
            def server(conn):
                authenticated(conn, b'GET a.csv\n')
                conn.sendall(reply)
            with self.subTest(reply=reply[:25]), TemporaryDirectory() as folder, peer(server) as port:
                target = Path(folder) / 'a.csv'
                with self.assertRaises(logs.LogTransferError):
                    logs.fetch_log('127.0.0.1', TOKEN, 'a.csv', target, port=port)
                self.assertFalse(target.exists())

    def test_fetch_never_overwrites_existing_completed_or_partial_file(self):
        for suffix in ('.csv', '.csv.part'):
            with TemporaryDirectory() as folder:
                target = Path(folder) / 'a.csv'
                existing = Path(folder) / ('a' + suffix)
                existing.write_bytes(b'preserve')
                with self.assertRaises((logs.LogTransferError, FileExistsError)):
                    logs.fetch_log('127.0.0.1', TOKEN, 'a.csv', target, port=1)
                self.assertEqual(existing.read_bytes(), b'preserve')

    def test_idle_peer_times_out(self):
        def server(conn):
            line(conn)
            time.sleep(.15)
        with peer(server) as port:
            with self.assertRaises((logs.LogTransferError, TimeoutError)):
                logs.list_logs('127.0.0.1', TOKEN, port=port, timeout=.05)

    def test_bad_token_and_remote_names_fail_before_connection(self):
        with self.assertRaises(ValueError):
            logs.list_logs('127.0.0.1', 'invalid', port=1)
        for name in ['../a.csv', 'a.csv.part', 'a\\b.csv', '/a.csv', 'a..csv']:
            with self.subTest(name=name), self.assertRaises(ValueError):
                logs.fetch_log('127.0.0.1', TOKEN, name, Path('unused.csv'), port=1)

    def test_cli_refuses_token_file_with_unread_trailing_bytes(self):
        with TemporaryDirectory() as folder:
            credential = Path(folder) / 'token.txt'
            credential.write_text(TOKEN + '\n' * 4 + 'junk', encoding='ascii')
            with patch.object(logs, 'list_logs', return_value=[]), redirect_stderr(StringIO()) as error:
                self.assertEqual(logs.main(['--host', '127.0.0.1', '--token-file', str(credential), 'list']), 1)
            self.assertNotIn(TOKEN, error.getvalue())


if __name__ == '__main__':
    unittest.main()
