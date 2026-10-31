"""Fetch exact completed telemetry CSVs while the own app is running.

Protocol: AUTH <64 hex> / OK AUTH, followed by one LIST or GET <basename>.
LIST: OK LIST <count>, FILE <name> <bytes> repeated count times, then END.
GET: OK FILE <bytes> <sha256>, exactly bytes raw data, then \nOK DONE\n.
The success trailer confirms the completed file remained stable during send.
All lines end in LF; names, lines, listing count, size and deadlines are bounded.
This is the project's own port, independent of the pinned monitor/reload kit.
"""
from contextlib import contextmanager
from pathlib import Path
import argparse
import hashlib
import os
import re
import socket
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
PORT = 28286
MAX_BYTES = 64 * 1024 * 1024
MAX_ENTRIES = 1024


class LogTransferError(Exception):
    pass


def _valid_name(name):
    return (isinstance(name, str) and 5 <= len(name) <= 100 and '..' not in name
            and re.fullmatch(r'[A-Za-z0-9_-][A-Za-z0-9_.-]*\.csv', name) is not None)


class _Connection:
    def __init__(self, sock, timeout):
        self.sock = sock
        self.timeout = timeout
        self.deadline = time.monotonic() + 120

    def _bound(self):
        remaining = self.deadline - time.monotonic()
        if remaining <= 0:
            raise LogTransferError('Telemetry transfer deadline exceeded')
        self.sock.settimeout(min(self.timeout, remaining))

    def send(self, data):
        self._bound()
        self.sock.sendall(data)

    def read(self, count):
        self._bound()
        data = self.sock.recv(count)
        if not data:
            raise LogTransferError('Incomplete telemetry reply')
        return data

    def line(self):
        data = bytearray()
        while len(data) < 256:
            byte = self.read(1)
            if byte == b'\n':
                return bytes(data)
            if byte[0] < 32 or byte[0] > 126:
                raise LogTransferError('Invalid telemetry reply')
            data += byte
        raise LogTransferError('Telemetry reply line exceeds limit')


@contextmanager
def _connect(host, token, port, timeout):
    if not isinstance(token, str) or re.fullmatch(r'[0-9a-fA-F]{64}', token) is None:
        raise ValueError('Invalid token file')
    if not 0 < timeout <= 120:
        raise ValueError('Timeout must be in (0, 120] seconds')
    try:
        with socket.create_connection((host, port), timeout=timeout) as sock:
            conn = _Connection(sock, timeout)
            conn.send(b'AUTH ' + token.encode('ascii') + b'\n')
            if conn.line() != b'OK AUTH':
                raise LogTransferError('Telemetry authentication rejected')
            yield conn
    except (OSError, TimeoutError):
        # Never reflect unauthenticated/malicious reply text or credentials.
        raise LogTransferError('Telemetry connection failed or timed out') from None


def list_logs(host, token, port=PORT, timeout=8):
    with _connect(host, token, port, timeout) as conn:
        conn.send(b'LIST\n')
        match = re.fullmatch(rb'OK LIST ([0-9]{1,4})', conn.line())
        if not match or int(match[1]) > MAX_ENTRIES:
            raise LogTransferError('Invalid telemetry listing or listing limit exceeded')
        result = []
        seen = set()
        for _ in range(int(match[1])):
            item = re.fullmatch(rb'FILE ([A-Za-z0-9_.-]+) ([0-9]{1,9})', conn.line())
            if not item:
                raise LogTransferError('Invalid telemetry listing entry')
            name, size = item[1].decode('ascii'), int(item[2])
            if not _valid_name(name) or name in seen or size > MAX_BYTES:
                raise LogTransferError('Unsafe telemetry listing entry')
            result.append({'name': name, 'bytes': size})
            seen.add(name)
        if conn.line() != b'END':
            raise LogTransferError('Incomplete telemetry listing')
        return result


def fetch_log(host, token, name, destination, port=PORT, timeout=8):
    """Stream into a fresh .part and publish only after length/hash/trailer pass.

    Existing completed and partial files are preserved. Failed new downloads
    remove only their own partial file; a concurrent destination is never replaced.
    """
    if not _valid_name(name):
        raise ValueError('Expected a completed telemetry CSV basename')
    destination = Path(destination)
    partial = destination.with_name(destination.name + '.part')
    if destination.exists() or destination.is_symlink() or partial.exists() or partial.is_symlink():
        raise FileExistsError('Destination or partial file already exists')
    destination.parent.mkdir(parents=True, exist_ok=True)
    created = False
    try:
        with partial.open('xb') as output:
            created = True
            with _connect(host, token, port, timeout) as conn:
                conn.send(b'GET ' + name.encode('ascii') + b'\n')
                match = re.fullmatch(rb'OK FILE ([0-9]{1,9}) ([0-9a-f]{64})', conn.line())
                if not match or int(match[1]) > MAX_BYTES:
                    raise LogTransferError('Telemetry file unavailable or size exceeds limit')
                remaining = int(match[1])
                expected = match[2].decode('ascii')
                digest = hashlib.sha256()
                while remaining:
                    data = conn.read(min(32768, remaining))
                    output.write(data)
                    digest.update(data)
                    remaining -= len(data)
                if conn.line() != b'' or conn.line() != b'OK DONE':
                    raise LogTransferError('Telemetry file changed or transfer was incomplete')
                if digest.hexdigest() != expected:
                    raise LogTransferError('Telemetry SHA256 mismatch')
            output.flush()
            os.fsync(output.fileno())
        # link() publishes atomically and fails if a destination appeared during
        # download, unlike replace()/POSIX rename(), which can overwrite it.
        os.link(partial, destination)
        return expected
    finally:
        if created:
            partial.unlink(missing_ok=True)


def _default_host():
    sys.path.insert(0, str(ROOT / 'third_party/switch-debug-kit/host'))
    from project import load
    return load(ROOT)['host']


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--host', help='Console IPv4 (default: local project.json)')
    parser.add_argument('--port', type=int, default=PORT)
    parser.add_argument('--timeout', type=float, default=8)
    parser.add_argument('--token-file', type=Path, default=ROOT / 'data/token.txt')
    commands = parser.add_subparsers(dest='command', required=True)
    commands.add_parser('list', help='List completed CSV basenames and sizes')
    fetch = commands.add_parser('fetch', help='Fetch named CSVs, or all completed CSVs')
    fetch.add_argument('names', nargs='*')
    fetch.add_argument('--output', type=Path, default=ROOT / 'build/telemetry')
    args = parser.parse_args(argv)
    try:
        with args.token_file.open('rb') as credential:
            raw = credential.read(68)
        if re.fullmatch(rb'[0-9a-fA-F]{64}(?:\r?\n)?', raw) is None:
            raise ValueError('Invalid token file')
        token = raw.rstrip(b'\r\n').decode('ascii')
        host = args.host or _default_host()
        kwargs = {'port': args.port, 'timeout': args.timeout}
        if args.command == 'list':
            for item in list_logs(host, token, **kwargs):
                print(f"{item['name']}\t{item['bytes']}")
        else:
            names = args.names or [item['name'] for item in list_logs(host, token, **kwargs)]
            for name in names:
                target = args.output / name
                digest = fetch_log(host, token, name, target, **kwargs)
                print(f'Verified {target} SHA256 {digest}')
        return 0
    except (LogTransferError, OSError, ValueError, UnicodeError) as error:
        print(f'Telemetry fetch failed: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
