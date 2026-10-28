"""Package only this project's allowlisted binaries; never game files or keys."""
from pathlib import Path
import hashlib
import json
import shutil
import zipfile

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build'


def package() -> None:
    config = json.loads((ROOT / 'project.json').read_text(encoding='utf-8-sig'))
    nro = OUT / 're4-controls.nro'
    own = list((OUT / 'forwarder').glob('*.nsp'))
    if len(own) != 1:
        raise RuntimeError('Build this project\'s own forwarder first')
    prefix = 'atmosphere/contents/010099A00BC1E000/exefs/'
    files = {
        prefix + 'subsdk9': OUT / 'experimental' / prefix / 'subsdk9',
        prefix + 'main.npdm': OUT / 'experimental' / prefix / 'main.npdm',
        'switch/.overlays/re4-controls.ovl': OUT / 'experimental/switch/.overlays/re4-controls.ovl',
        f'switch/{config["sd_directory"]}/bridge.nro': nro,
        f'switch/{config["sd_directory"]}/game.nro': nro,
        'install/re4-controls-forwarder.nsp': own[0],
        'README-install.md': ROOT / 'docs/installation.md',
        'licenses/MIT.txt': ROOT / 'LICENSE',
        'licenses/exlaunch-GPL2.txt': ROOT / 'third_party/exlaunch/LICENSE',
        'licenses/libtesla-GPL2.txt': ROOT / 'third_party/libtesla/LICENSE',
        'licenses/Switch-Debug-Kit-MIT.txt': ROOT / 'third_party/switch-debug-kit/LICENSE',
        'licenses/nx-ovlloader-GPL2.txt': ROOT / 'third_party/nx-ovlloader/LICENSE.md',
    }
    manifest = {'status': 'experimental; hardware acceptance pending',
                'features': ['proportional movement turning', 'turning overlay', 'sensor diagnostic NRO'],
                'verified': ['diagnostic sticks/gyro on console', 'NRO changed-image reload and new PID', 'cold-launch credential'],
                'pending': ['movement hook activation', 'stationary turning', 'in-game gyro', 'aim/camera gains', 'overlay loader hardware'],
                'files': {name: hashlib.sha256(path.read_bytes()).hexdigest() for name, path in files.items()}}
    destination = OUT / 're4-controls-experimental.zip'
    local = OUT / 'local-sd'
    # Rebuild this generated tree so removed optional artifacts cannot linger
    # and overwrite an existing console's overlay menu on a later copy.
    if local.exists():
        if local.resolve().parent != OUT.resolve() or local.name != 'local-sd':
            raise RuntimeError('Unexpected generated SD tree path')
        shutil.rmtree(local)
    with zipfile.ZipFile(destination, 'w', zipfile.ZIP_DEFLATED) as archive:
        for name, path in files.items():
            archive.write(path, name)
            target = local / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)
        archive.writestr('manifest.json', json.dumps(manifest, indent=2) + '\n')
    token = local / f'switch/{config["sd_directory"]}/token.txt'
    shutil.copy2(ROOT / 'data/token.txt', token)
    (OUT / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print('Public package:', destination)
    print('Private local SD tree (includes receiver credential):', local)


if __name__ == '__main__':
    package()
