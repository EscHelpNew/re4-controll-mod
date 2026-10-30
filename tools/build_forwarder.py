"""Build this project's own local forwarder. Never installs or uploads it."""
from pathlib import Path
import argparse
import contextlib
import json
import sys
import shutil
import hashlib
import uuid

ROOT = Path(__file__).resolve().parents[1]


def build(keys: Path) -> None:
    sys.path.insert(0, str(ROOT))
    from tools.prepare_icon import prepare
    from nton.constants import Directories, Files
    output = (ROOT / 'build/forwarder-stage' / uuid.uuid4().hex).resolve()
    published = (ROOT / 'build/forwarder').resolve()
    temporary = (ROOT / 'build/forwarder-tmp').resolve()
    output.relative_to(ROOT.resolve())
    published.relative_to(ROOT.resolve())
    temporary.relative_to(ROOT.resolve())
    output.mkdir(parents=True, exist_ok=True)
    Directories.output = output
    Directories.temp = temporary
    Files.keys = keys.resolve()
    if not Files.keys.is_file():
        raise ValueError('Local key file does not exist')
    config = json.loads((ROOT / 'project.json').read_text(encoding='utf-8-sig'))
    title = config.get('title_id', '')
    if not isinstance(title, str) or len(title) != 16 or any(c not in '0123456789abcdefABCDEF' for c in title):
        raise ValueError('Diagnostic title ID must contain sixteen hexadecimal digits')
    config['title_id'] = title.upper()
    if config['title_id'] in ('0000000000000000', '010099A00BC1E000'):
        raise ValueError('Configure the diagnostic application\'s own nonzero title ID')
    icon = output / 'icon.jpg'
    prepare(ROOT / 'assets/icon-original.png', icon)
    from nton.main import main
    log = ROOT / 'build/forwarder-build.log'
    with log.open('w', encoding='utf-8') as stream:
        with contextlib.redirect_stdout(stream), contextlib.redirect_stderr(stream):
            main(['build', str(ROOT / 'build/re4-controls.nro'), '--sdmc',
                  '/switch/re4-controls/bridge.nro', '--id', config['title_id'],
                  '--name', config['name'], '--publisher', config['author'],
                  '--icon', str(icon)], standalone_mode=False)
    packages = list(output.glob('*.nsp'))
    if len(packages) != 1:
        raise RuntimeError('Forwarder build did not produce exactly one NSP; inspect private build log')
    published.mkdir(parents=True, exist_ok=True)
    previous = list(published.glob('*.nsp'))
    for package in previous:
        backup = ROOT / 'build/forwarder-previous' / (hashlib.sha256(package.read_bytes()).hexdigest()[:16] + '-' + package.name)
        backup.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(package, backup)
    target = published / packages[0].name
    shutil.copy2(packages[0], target)
    for package in previous:
        if package != target:
            package.unlink()
    shutil.copy2(icon, published / 'icon.jpg')
    print('Built own forwarder:', target)
    print('No console installation performed; installed-title collision check is still required.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--keys', required=True, type=Path)
    args = parser.parse_args()
    build(args.keys)
