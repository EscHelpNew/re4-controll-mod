"""Validate attributed scene assets and embed deterministic RGB565 images."""
from pathlib import Path
import hashlib
import json
import re
from urllib.parse import urlsplit

IMAGE_WIDTH = 320
IMAGE_HEIGHT = 180
MAX_SOURCE_BYTES = 2 * 1024 * 1024
MAX_SOURCE_PIXELS = 8 * 1024 * 1024
MAX_SOURCE_DIMENSION = 8192
MAX_GYRO_SOURCE_BYTES = 4 * 1024 * 1024
PRODUCTS = {'re4': '70010000012858', 're6': '70010000015880'}


def load_sources(root: Path) -> dict:
    """Accept only the documented, public official-gallery source schema."""
    sources = json.loads((root / 'assets/graphics/sources.json').read_text(encoding='utf-8'))
    if not isinstance(sources, dict) or set(sources) != set(PRODUCTS):
        raise ValueError('Graphics sources must contain exactly re4 and re6')
    for game, product in PRODUCTS.items():
        entry = sources[game]
        if not isinstance(entry, dict):
            raise ValueError('Invalid graphics source record')
        digest = entry.get('sha256', '')
        if not isinstance(digest, str) or not re.fullmatch(r'[0-9a-f]{64}', digest):
            raise ValueError('Invalid graphics source hash')
        page = f'https://www.nintendo.com/us/store/products/resident-evil-{game[-1]}-switch/'
        if entry.get('source_page') != page:
            raise ValueError('Invalid official graphics source page')
        image = entry.get('image_url', '')
        expected_path = f'/image/upload/store/software/switch/{product}/{digest}.jpg'
        if not isinstance(image, str):
            raise ValueError('Invalid official graphics image URL')
        url = urlsplit(image)
        if (url.scheme, url.netloc, url.path, url.query, url.fragment) != (
                'https', 'assets.nintendo.com', expected_path, '', ''):
            raise ValueError('Invalid official graphics image URL')
        if (entry.get('width'), entry.get('height')) != (1280, 720):
            raise ValueError('Invalid official graphics source dimensions')
        if not all(isinstance(entry.get(key), str) and entry[key] for key in (
                'game', 'release_date', 'copyright', 'description')):
            raise ValueError('Incomplete graphics source attribution')
    return sources


def _check_size(source: Path) -> None:
    if not 0 < source.stat().st_size <= MAX_SOURCE_BYTES:
        raise ValueError('Graphics source size is empty or exceeds 2 MiB')


def convert_image(source: Path, expected_size: tuple[int, int] | None = None,
                  output_size: tuple[int, int] = (IMAGE_WIDTH, IMAGE_HEIGHT),
                  max_bytes: int = MAX_SOURCE_BYTES) -> list[int]:
    """Keep the complete scene, with RGB565 values in row-major pixel order."""
    from PIL import Image, ImageOps, UnidentifiedImageError

    if not 0 < source.stat().st_size <= max_bytes:
        raise ValueError('Graphics source size is empty or exceeds decoding limit')
    try:
        with Image.open(source) as original:
            width, height = original.size
            if (width < 1 or height < 1 or max(width, height) > MAX_SOURCE_DIMENSION
                    or width * height > MAX_SOURCE_PIXELS):
                raise ValueError('Graphics source dimensions exceed decoding limits')
            if expected_size is not None and original.size != expected_size:
                raise ValueError('Graphics source dimensions differ from attribution')
            image = ImageOps.pad(original.convert('RGB'), output_size,
                                 method=Image.Resampling.LANCZOS, color='black')
            raw = image.tobytes()
    except (UnidentifiedImageError, OSError, Image.DecompressionBombError) as error:
        raise ValueError('Invalid graphics source image') from error
    return [((raw[i] >> 3) << 11) | ((raw[i + 1] >> 2) << 5) | (raw[i + 2] >> 3)
            for i in range(0, len(raw), 3)]


def gyro_background(root: Path, sources: dict) -> tuple[dict, list[int]]:
    """Validate the attributed edited frame before decoding its bounded PNG."""
    entry = json.loads((root / 'assets/graphics/gyro-test-background.json').read_text(encoding='utf-8'))
    if not isinstance(entry, dict):
        raise ValueError('Invalid gyro background attribution')
    if (entry.get('source_file') != 're4-source.jpg'
            or entry.get('source_sha256') != sources['re4']['sha256']):
        raise ValueError('Invalid gyro background source identity')
    digest = entry.get('sha256', '')
    if not isinstance(digest, str) or not re.fullmatch(r'[0-9a-f]{64}', digest):
        raise ValueError('Invalid gyro background hash')
    for field in ('game', 'copyright', 'edit_tool', 'edit_date', 'description'):
        if not isinstance(entry.get(field), str) or not entry[field]:
            raise ValueError('Incomplete gyro background attribution')
    dimensions = entry.get('width'), entry.get('height')
    if any(type(value) is not int or value < 1 or value > MAX_SOURCE_DIMENSION for value in dimensions):
        raise ValueError('Invalid gyro background dimensions')
    source = root / 'assets/graphics/gyro-test-background.png'
    if not 0 < source.stat().st_size <= MAX_GYRO_SOURCE_BYTES:
        raise ValueError('Gyro background source exceeds 4 MiB')
    if hashlib.sha256(source.read_bytes()).hexdigest() != digest:
        raise ValueError('Gyro background source hash mismatch')
    return entry, convert_image(source, dimensions, (1280, 720), MAX_GYRO_SOURCE_BYTES)


def prepare(root: Path) -> Path:
    """Generate a public C++ header; source images are never modified."""
    root = Path(root)
    sources = load_sources(root)
    converted = {}
    for game, entry in sources.items():
        source = root / 'assets/graphics' / f'{game}-source.jpg'
        _check_size(source)
        if hashlib.sha256(source.read_bytes()).hexdigest() != entry['sha256']:
            raise ValueError(f'Graphics source hash mismatch: {game}')
        converted[game] = convert_image(source, (entry['width'], entry['height']))

    gyro_source, gyro_pixels = gyro_background(root, sources)

    lines = ['// Generated by tools/prepare_graphics.py; do not edit.',
             '// Official CAPCOM gameplay screenshots; see assets/graphics/SOURCES.md.',
             '#pragma once', '#include <cstdint>', '', 'namespace graphics_preview {',
             'inline constexpr int kImageWidth = 320;',
             'inline constexpr int kImageHeight = 180;', '']
    for game in PRODUCTS:
        lines.append(f'// Source SHA-256: {sources[game]["sha256"]}')
        lines.append(f'inline constexpr std::uint16_t {game}_pixels[320 * 180] = {{')
        pixels = converted[game]
        for start in range(0, len(pixels), 16):
            lines.append('    ' + ', '.join(f'0x{pixel:04x}' for pixel in pixels[start:start + 16]) + ',')
        lines.extend(['};', ''])
    # Approved close-up from the unchanged, attributed RE4 source frame.
    # Keep the HUD dynamic; the embedded image contains no health bar or number.
    from PIL import Image
    with Image.open(root / 'assets/graphics/re4-source.jpg') as scene:
        closeup = scene.convert('RGB').crop((415, 105, 640, 300)).resize((256, 222), Image.Resampling.LANCZOS)
    health_pixels = [((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
                     for r, g, b in zip(*[iter(closeup.tobytes())] * 3)]
    lines.append('inline constexpr std::uint16_t health_pixels[256 * 222] = {')
    for start in range(0, len(health_pixels), 16):
        lines.append('    ' + ', '.join(f'0x{pixel:04x}' for pixel in health_pixels[start:start + 16]) + ',')
    lines.extend(['};', ''])
    with Image.open(root / 'assets/graphics/re4-source.jpg') as scene:
        floor = scene.convert('RGB').crop((735, 445, 1005, 625)).resize((256, 171), Image.Resampling.LANCZOS)
    floor_pixels = [((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
                    for r, g, b in zip(*[iter(floor.tobytes())] * 3)]
    lines.append('inline constexpr std::uint16_t grenade_pixels[256 * 171] = {')
    for start in range(0, len(floor_pixels), 16):
        lines.append('    ' + ', '.join(f'0x{pixel:04x}' for pixel in floor_pixels[start:start + 16]) + ',')
    lines.extend(['};', ''])
    lines.extend(['}  // namespace graphics_preview', ''])
    destination = root / 'build/graphics/graphics_preview_images.hpp'
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text('\n'.join(lines), encoding='ascii', newline='\n')
    background = ['// Generated by tools/prepare_graphics.py; do not edit.',
                  '// ImageGen-edited CAPCOM frame; see assets/graphics/SOURCES.md.',
                  f'// Source SHA-256: {gyro_source["sha256"]}',
                  '#pragma once', '#include <cstdint>', '', 'namespace re4::gyro_background {',
                  'inline constexpr int kWidth = 1280;',
                  'inline constexpr int kHeight = 720;',
                  'inline constexpr std::uint16_t pixels[1280 * 720] = {']
    for start in range(0, len(gyro_pixels), 16):
        background.append('    ' + ', '.join(f'0x{pixel:04x}' for pixel in gyro_pixels[start:start + 16]) + ',')
    background.extend(['};', '}  // namespace re4::gyro_background', ''])
    (destination.parent / 'gyro_test_background_image.hpp').write_text(
        '\n'.join(background), encoding='ascii', newline='\n')
    return destination


if __name__ == '__main__':
    prepare(Path(__file__).resolve().parents[1])
