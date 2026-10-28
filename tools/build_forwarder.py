"""Build this project's own local forwarder. Never installs or uploads it."""
from pathlib import Path
import argparse
import contextlib
import json
import sys

ROOT = Path(__file__).resolve().parents[1]


def build(keys: Path) -> None:
    from PIL import Image, ImageDraw, ImageFont
    from nton.constants import Directories, Files
    output = (ROOT / 'build/forwarder').resolve()
    temporary = (ROOT / 'build/forwarder-tmp').resolve()
    output.relative_to(ROOT)
    temporary.relative_to(ROOT)
    output.mkdir(parents=True, exist_ok=True)
    Directories.output = output
    Directories.temp = temporary
    Files.keys = keys.resolve()
    if not Files.keys.is_file():
        raise ValueError('Local key file does not exist')
    config = json.loads((ROOT / 'project.json').read_text(encoding='utf-8-sig'))
    if config['title_id'] in ('0000000000000000', '010099A00BC1E000'):
        raise ValueError('Configure the diagnostic application\'s own nonzero title ID')
    image = Image.new('RGB', (256, 256), '#15251E')
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle((15, 15, 241, 241), radius=24, outline='#9EDABD', width=5)
    font = ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf', 62)
    draw.multiline_text((128, 128), 'RE4\nCTRL', fill='#D5F2E0', font=font,
                        anchor='mm', align='center', spacing=0)
    icon = output / 'icon.jpg'
    image.save(icon, quality=95)
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
    print('Built own forwarder:', packages[0])
    print('No console installation performed; installed-title collision check is still required.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--keys', required=True, type=Path)
    args = parser.parse_args()
    build(args.keys)
