import importlib.util
import json
from pathlib import Path
import socket
import tempfile
import threading
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('live_diagnostics', ROOT / 'tools/live_diagnostics.py')
client = importlib.util.module_from_spec(spec)
spec.loader.exec_module(client)
TOKEN = '1' * 64


class Server:
    def __init__(self, reply, greeting=b'OK RE4-DIAG 1\n'):
        self.listener = socket.socket()
        self.listener.bind(('127.0.0.1', 0))
        self.listener.listen(1)
        self.port = self.listener.getsockname()[1]
        self.requests = []

        def run():
            with self.listener:
                conn, _ = self.listener.accept()
                with conn, conn.makefile('rb') as stream:
                    self.requests.append(stream.readline(100))
                    conn.sendall(greeting)
                    if greeting != b'OK RE4-DIAG 1\n':
                        return
                    self.requests.append(stream.readline(100))
                    for byte in reply:
                        conn.sendall(bytes([byte]))
        self.thread = threading.Thread(target=run, daemon=True)
        self.thread.start()

    def finish(self):
        self.thread.join(3)
        assert not self.thread.is_alive()


class LiveDiagnosticsTests(unittest.TestCase):
    def test_transport_failure_fields_before_auth_and_snapshot(self):
        line = b'ERR TRANSPORT 2 4 -1 22 0000D401 3\n'
        expected = {'stage': 2, 'fd': 4, 'result': -1, 'errno': 22,
                    'bsd_result': '0000D401', 'elapsed_ms': 3}
        for greeting, reply in [(line, b''), (b'OK RE4-DIAG 1\n', line)]:
            with self.subTest(greeting=greeting):
                server = Server(reply, greeting)
                with self.assertRaises(client.DiagnosticError) as caught:
                    client.snapshot('127.0.0.1', TOKEN, server.port)
                server.finish()
                self.assertEqual(getattr(caught.exception, 'transport_error', None), expected)
                self.assertNotIn(TOKEN, str(caught.exception))

    def test_transport_metadata_is_bounded_and_does_not_echo_arbitrary_reply(self):
        for line in [b'ERR TRANSPORT 9 4 -1 22 00000000 3\n',
                     b'ERR TRANSPORT 2 2147483648 -1 22 00000000 3\n',
                     b'ERR TRANSPORT 2 4 -2147483649 22 00000000 3\n',
                     b'ERR TRANSPORT 2 4 -1 22 00000000 4294967296\n',
                     b'ERR TRANSPORT 2 4 -1 22 00000000 3 ' + TOKEN.encode() + b'\n']:
            server = Server(b'', line)
            with self.assertRaises(client.DiagnosticError) as caught:
                client.snapshot('127.0.0.1', TOKEN, server.port)
            server.finish()
            self.assertIsNone(getattr(caught.exception, 'transport_error', None))
            self.assertNotIn(TOKEN, str(caught.exception))

    def test_watch_saves_safe_transport_failure_without_game_snapshot(self):
        server = Server(b'', b'ERR TRANSPORT 3 63 -1 22 00000000 0\n')
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp) / 'watch.jsonl'
            count, successes = client.watch('127.0.0.1', TOKEN, output,
                                            duration=.1, interval=.1, port=server.port)
            rows = [json.loads(line) for line in output.read_text(encoding='utf-8').splitlines()]
        server.finish()
        self.assertGreaterEqual(count, 1)
        self.assertEqual(successes, 0)
        self.assertEqual(rows[0].get('transport_error'),
                         {'stage': 3, 'fd': 63, 'result': -1, 'errno': 22,
                          'bsd_result': '00000000', 'elapsed_ms': 0})
        self.assertNotIn(TOKEN, json.dumps(rows))

    def test_partial_framing_and_authenticated_fixed_request(self):
        value = {'schema': 1, 'game': 'RE4', 'overlay_version': '0.26.0'}
        body = json.dumps(value).encode()
        server = Server(b'OK JSON ' + str(len(body)).encode() + b'\n' + body)
        self.assertEqual(client.snapshot('127.0.0.1', TOKEN, server.port), value)
        server.finish()
        self.assertEqual(server.requests, [('AUTH ' + TOKEN + '\n').encode(), b'SNAPSHOT\n'])

    def test_bad_auth_never_sends_snapshot_or_echoes_secret(self):
        server = Server(b'', b'ERR AUTH ' + TOKEN.encode() + b'\n')
        with self.assertRaises(client.DiagnosticError) as caught:
            client.snapshot('127.0.0.1', TOKEN, server.port)
        server.finish()
        self.assertNotIn(TOKEN, str(caught.exception))
        self.assertEqual(len(server.requests), 1)

    def test_bad_lengths_payload_and_schema_rejected(self):
        for reply in [b'OK JSON 16385\n', b'OK JSON 0\n', b'OK JSON 10\n{}',
                      b'OK JSON 2\n[]', b'OK JSON 3\nNaN', b'OK JSON 2\n{}']:
            with self.subTest(reply=reply):
                server = Server(reply)
                with self.assertRaises(client.DiagnosticError):
                    client.snapshot('127.0.0.1', TOKEN, server.port)
                server.finish()

    def test_strict_token_and_exclusive_output(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'key'
            path.write_bytes(TOKEN.encode() + b'\r\n')
            self.assertEqual(client.load_token(path), TOKEN)
            for data in [TOKEN.encode() + b'X', TOKEN.encode() + b'\n\n\n', b'bad']:
                path.write_bytes(data)
                with self.assertRaises(client.DiagnosticError):
                    client.load_token(path)
            output = Path(temp) / 'report.json'
            client.save_snapshot(output, {'schema': 1})
            with self.assertRaises(FileExistsError):
                client.save_snapshot(output, {'schema': 2})
            self.assertEqual(json.loads(output.read_text()), {'schema': 1})


if __name__ == '__main__':
    unittest.main()
