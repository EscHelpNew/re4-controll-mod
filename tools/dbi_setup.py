"""Stage the own forwarder and SD credential through DBI FTP (not NRO uploads).

NROs are transferred exclusively with tools/reload.py / Switch Debug Kit.
Existing differing files are backed up locally before replacement. No game,
firmware, save or BLACK paths are touched by this setup command.
"""
from datetime import datetime, timezone
from ftplib import FTP, error_perm
from io import BytesIO
from pathlib import Path
import argparse
import hashlib
import json

ROOT = Path(__file__).resolve().parents[1]


def get(ftp, path):
    out = BytesIO()
    try:
        ftp.retrbinary('RETR ' + path, out.write)
    except error_perm as exc:
        if str(exc).startswith('550'):
            return None
        raise
    return out.getvalue()


def mkdirs(ftp, path):
    current = ''
    for part in path.strip('/').split('/'):
        current += '/' + part
        try:
            ftp.mkd(current)
        except error_perm:
            # Distinguish an existing directory from permission/storage errors.
            ftp.cwd(current)
    ftp.cwd('/')


def setup(host, port):
    config = json.loads((ROOT / 'project.json').read_text(encoding='utf-8-sig'))
    directory = config['sd_directory']
    if directory != 're4-controls':
        raise ValueError('Review setup allowlist for a different SD directory')
    packages = list((ROOT / 'build/forwarder').glob('*.nsp'))
    if len(packages) != 1:
        raise ValueError('Build exactly one own forwarder first')
    token = (ROOT / 'data/token.txt').read_bytes()
    if len(token.strip()) != 64 or any(c not in b'0123456789abcdefABCDEF' for c in token.strip()):
        raise ValueError('Invalid local receiver credential')
    expected = (ROOT / 'build/re4-controls.nro').read_bytes()
    base = '/switch/re4-controls'
    with FTP() as ftp:
        ftp.connect(host, port, timeout=15)
        ftp.login()
        for name in ('bridge.nro', 'game.nro'):
            actual = get(ftp, base + '/' + name)
            if actual != expected:
                raise ValueError(name + ' differs: first upload it using tools/reload.py put')
        files = {
            base + '/token.txt': token,
            base + '/re4-controls-forwarder.nsp': packages[0].read_bytes(),
        }
        stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
        backup = ROOT / 'data/console-backups' / stamp
        for remote, content in files.items():
            old = get(ftp, remote)
            if old == content:
                print('Already matches:', remote)
                continue
            if old is not None:
                local = backup / remote.lstrip('/')
                local.parent.mkdir(parents=True, exist_ok=True)
                local.write_bytes(old)
            mkdirs(ftp, remote.rsplit('/', 1)[0])
            incoming = remote + '.incoming'
            ftp.storbinary('STOR ' + incoming, BytesIO(content))
            if get(ftp, incoming) != content:
                raise ValueError('Read-back verification failed: ' + remote)
            previous = remote + '.re4-controls.previous'
            if old is not None:
                if get(ftp, previous) is not None:
                    raise ValueError('Prior SD backup already exists: ' + previous)
                ftp.rename(remote, previous)
            try:
                ftp.rename(incoming, remote)
            except BaseException:
                if old is not None:
                    ftp.rename(previous, remote)
                raise
            if get(ftp, remote) != content:
                raise ValueError('Published file verification failed: ' + remote)
            print('Staged and read-back verified:', remote)
        print('Own forwarder SHA256:', hashlib.sha256(packages[0].read_bytes()).hexdigest())
        print('Install the staged NSP in DBI, then open RE4 Controls from HOME.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--host', default='192.168.1.131')
    parser.add_argument('--port', type=int, default=5000)
    args = parser.parse_args()
    setup(args.host, args.port)
