"""Read authenticated RE4 overlay diagnostics, including while its UI is hidden."""
from pathlib import Path
import argparse
from datetime import datetime, timezone
import json
import math
import re
import socket
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
PORT = 28773
LIMIT = 16384


class DiagnosticError(RuntimeError):
    pass


class TransportError(DiagnosticError):
    def __init__(self, fields):
        self.transport_error = fields
        super().__init__('Overlay transport failure: stage={stage} fd={fd} '
                         'result={result} errno={errno} bsd={bsd_result} '
                         'elapsed_ms={elapsed_ms}'.format(**fields))


def _transport_error(line):
    match = re.fullmatch(rb'ERR TRANSPORT ([1-8]) (-?[0-9]{1,10}) '
                         rb'(-?[0-9]{1,11}) ([0-9]{1,10}) '
                         rb'([0-9A-Fa-f]{8}) ([0-9]{1,10})', line)
    if match is None:
        return
    stage, fd, result, error = [int(match[i]) for i in range(1, 5)]
    elapsed = int(match[6])
    if not (-1 <= fd <= 0x7fffffff and -0x80000000 <= result <= 0x7fffffff
            and error <= 0x7fffffff and elapsed <= 0xffffffff):
        return
    raise TransportError({'stage': stage, 'fd': fd, 'result': result, 'errno': error,
                          'bsd_result': match[5].decode('ascii').upper(), 'elapsed_ms': elapsed})


def load_token(path):
    with Path(path).open('rb') as stream:
        data = stream.read(68)
    if len(data) > 66:
        raise DiagnosticError('Invalid local connection key')
    data = data.rstrip(b'\r\n')
    if re.fullmatch(rb'[0-9a-fA-F]{64}', data) is None:
        raise DiagnosticError('Invalid local connection key')
    return data.decode('ascii')


def _line(stream):
    line = stream.readline(100)
    if not line.endswith(b'\n') or len(line) > 98:
        raise DiagnosticError('Invalid diagnostic framing')
    return line[:-1]


def snapshot(host, token, port=PORT, timeout=5):
    if re.fullmatch('[0-9a-fA-F]{64}', token) is None:
        raise DiagnosticError('Invalid local connection key')
    with socket.create_connection((host, port), timeout) as conn, conn.makefile('rb') as stream:
        conn.settimeout(timeout)
        conn.sendall(('AUTH ' + token + '\n').encode('ascii'))
        greeting = _line(stream)
        _transport_error(greeting)
        if greeting != b'OK RE4-DIAG 1':
            raise DiagnosticError('Diagnostic authentication rejected')
        conn.sendall(b'SNAPSHOT\n')
        header = _line(stream)
        _transport_error(header)
        match = re.fullmatch(rb'OK JSON ([0-9]{1,5})', header)
        if not match or not 0 < int(match[1]) <= LIMIT:
            raise DiagnosticError('Invalid diagnostic payload length')
        data = stream.read(int(match[1]))
        if len(data) != int(match[1]):
            raise DiagnosticError('Incomplete diagnostic payload')
        try:
            def invalid_constant(_):
                raise ValueError()
            value = json.loads(data.decode('utf-8'), parse_constant=invalid_constant)
        except (ValueError, UnicodeError):
            raise DiagnosticError('Invalid diagnostic JSON') from None
        if not isinstance(value, dict) or value.get('schema') != 1 or value.get('game') != 'RE4':
            raise DiagnosticError('Unsupported diagnostic schema')
        return value


def save_snapshot(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('x', encoding='utf-8') as stream:
        json.dump(value, stream, ensure_ascii=False, indent=2, allow_nan=False)
        stream.write('\n')


def watch(host, token, output, duration=15, interval=.25, port=PORT):
    if not math.isfinite(duration) or not 0 < duration <= 3600 or not math.isfinite(interval) or not .1 <= interval <= 10:
        raise DiagnosticError('Duration must be 0..3600 seconds and interval 0.1..10 seconds')
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    count = successes = 0
    end = time.monotonic() + duration
    with output.open('x', encoding='utf-8') as stream:
        while time.monotonic() < end:
            start = time.monotonic()
            record = {'host_time_utc': datetime.now(timezone.utc).isoformat(), 'sample': count}
            try:
                record['data'] = snapshot(host, token, port, timeout=min(5, max(.1, end - start)))
                successes += 1
            except (OSError, DiagnosticError) as error:
                # Do not echo network payloads or credentials into a report.
                record['error'] = type(error).__name__
                if isinstance(error, TransportError):
                    record['transport_error'] = error.transport_error
            stream.write(json.dumps(record, ensure_ascii=False, allow_nan=False) + '\n')
            stream.flush()
            count += 1
            time.sleep(max(0, min(end - time.monotonic(), interval - (time.monotonic() - start))))
    return count, successes


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--host', required=True)
    parser.add_argument('--port', type=int, default=PORT)
    parser.add_argument('--token-file', type=Path, default=ROOT / 'data/token.txt')
    sub = parser.add_subparsers(dest='command', required=True)
    get = sub.add_parser('snapshot')
    get.add_argument('--output', type=Path)
    monitor = sub.add_parser('watch')
    monitor.add_argument('--output', required=True, type=Path)
    monitor.add_argument('--seconds', type=float, default=15)
    monitor.add_argument('--interval', type=float, default=.25)
    args = parser.parse_args()
    if not 0 < args.port <= 65535:
        raise DiagnosticError('Invalid diagnostic port')
    token = load_token(args.token_file)
    if args.command == 'snapshot':
        value = snapshot(args.host, token, args.port)
        if args.output:
            save_snapshot(args.output, value)
            print('Saved diagnostic snapshot:', args.output)
        else:
            print(json.dumps(value, ensure_ascii=True, indent=2, allow_nan=False))
    else:
        count, successes = watch(args.host, token, args.output, args.seconds, args.interval, args.port)
        print(f'Saved {count} samples ({successes} snapshots): {args.output}')
        if not successes:
            return 1
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, DiagnosticError) as error:
        print('Diagnostic error: ' + str(error), file=sys.stderr)
        raise SystemExit(1)
