from importlib.util import module_from_spec, spec_from_file_location
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest.mock import patch
import hashlib
import json
import re
import shutil
import unittest

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
spec = spec_from_file_location('prepare_graphics', ROOT / 'tools/prepare_graphics.py')
tool = module_from_spec(spec)
spec.loader.exec_module(tool)


class GraphicsAssetTests(unittest.TestCase):
    def test_rgb565_primary_colors_and_row_order(self):
        with TemporaryDirectory() as temp:
            source = Path(temp) / 'colors.png'
            image = Image.new('RGB', (320, 180), 'red')
            for y in range(90, 180):
                for x in range(320):
                    image.putpixel((x, y), (0, 255, 0))
            image.putpixel((319, 179), (0, 0, 255))
            image.save(source)
            pixels = tool.convert_image(source)
            self.assertEqual(len(pixels), 320 * 180)
            self.assertEqual(pixels[:320 * 90], [0xF800] * (320 * 90))
            self.assertEqual(pixels[320 * 90:-1], [0x07E0] * (320 * 90 - 1))
            self.assertEqual(pixels[-1], 0x001F)

    def test_resize_preserves_complete_scene_with_black_padding(self):
        with TemporaryDirectory() as temp:
            source = Path(temp) / 'square.png'
            Image.new('RGB', (100, 100), 'white').save(source)
            pixels = tool.convert_image(source)
            self.assertEqual(len(pixels), 320 * 180)
            self.assertEqual(pixels[90 * 320:90 * 320 + 70], [0] * 70)
            self.assertEqual(pixels[90 * 320 + 70:90 * 320 + 250], [0xFFFF] * 180)
            self.assertEqual(pixels[90 * 320 + 250:91 * 320], [0] * 70)

    def test_rejects_large_file_before_pillow_opens_it(self):
        with TemporaryDirectory() as temp:
            source = Path(temp) / 'oversized.jpg'
            with source.open('wb') as stream:
                stream.truncate(tool.MAX_SOURCE_BYTES + 1)
            with patch('PIL.Image.open') as opener:
                with self.assertRaisesRegex(ValueError, 'size'):
                    tool.convert_image(source)
                opener.assert_not_called()

    def test_rejects_large_dimensions_before_loading_pixels(self):
        with TemporaryDirectory() as temp:
            source = Path(temp) / 'large.png'
            Image.new('1', (4000, 3000)).save(source)
            with patch('PIL.PngImagePlugin.PngImageFile.load', side_effect=AssertionError('decoded')):
                with self.assertRaisesRegex(ValueError, 'dimensions'):
                    tool.convert_image(source)

    def test_missing_and_invalid_images_fail(self):
        with TemporaryDirectory() as temp:
            source = Path(temp) / 'missing.jpg'
            with self.assertRaises(FileNotFoundError):
                tool.convert_image(source)
            source.write_bytes(b'not an image')
            with self.assertRaises(ValueError):
                tool.convert_image(source)

    def test_official_asset_identity_and_documented_hashes(self):
        manifest = tool.load_sources(ROOT)
        self.assertEqual(set(manifest), {'re4', 're6'})
        for game, entry in manifest.items():
            source = ROOT / 'assets/graphics' / (game + '-source.jpg')
            digest = hashlib.sha256(source.read_bytes()).hexdigest()
            self.assertEqual(digest, entry['sha256'])
            self.assertIn(digest, (ROOT / 'assets/graphics/SOURCES.md').read_text(encoding='utf-8'))
            with Image.open(source) as image:
                self.assertEqual((image.format, image.mode, image.size), ('JPEG', 'RGB', (1280, 720)))
        self.assertIn('/70010000012858/', manifest['re4']['image_url'])
        self.assertIn('/70010000015880/', manifest['re6']['image_url'])
        self.assertEqual(manifest['re4']['release_date'], '2019-05-21')

    def test_generated_header_is_deterministic_bounded_and_public(self):
        with TemporaryDirectory() as temp:
            root = Path(temp)
            shutil.copytree(ROOT / 'assets/graphics', root / 'assets/graphics')
            originals = {p.name: p.read_bytes() for p in (root / 'assets/graphics').iterdir()}
            output = tool.prepare(root)
            self.assertEqual(output, root / 'build/graphics/graphics_preview_images.hpp')
            first = output.read_bytes()
            self.assertEqual(tool.prepare(root).read_bytes(), first)
            header = first.decode('ascii')
            self.assertIn('namespace graphics_preview', header)
            self.assertIn('kImageWidth = 320', header)
            self.assertIn('kImageHeight = 180', header)
            self.assertIn('#include <cstdint>', header)
            for game in ('re4', 're6'):
                body = re.search(rf'{game}_pixels\[320 \* 180\] = \{{(.*?)\}};', header, re.S).group(1)
                self.assertEqual(len(re.findall(r'0x[0-9a-f]{4}', body)), 320 * 180)
            self.assertNotIn(str(root), header)
            self.assertNotIn(str(ROOT), header)
            self.assertEqual(originals, {p.name: p.read_bytes() for p in (root / 'assets/graphics').iterdir()})

    def test_rejects_tampered_sources(self):
        with TemporaryDirectory() as temp:
            root = Path(temp)
            shutil.copytree(ROOT / 'assets/graphics', root / 'assets/graphics')
            (root / 'assets/graphics/re4-source.jpg').write_bytes(b'tampered')
            with self.assertRaisesRegex(ValueError, 'hash'):
                tool.prepare(root)
            self.assertFalse((root / 'build/graphics/graphics_preview_images.hpp').exists())

    def test_manifest_rejects_wrong_game_urls_dimensions_and_extra_assets(self):
        with TemporaryDirectory() as temp:
            root = Path(temp)
            directory = root / 'assets/graphics'
            directory.mkdir(parents=True)
            manifest = json.loads((ROOT / 'assets/graphics/sources.json').read_text(encoding='utf-8'))
            variants = []
            for key, value in [('image_url', 'https://example.com/private.jpg'),
                               ('source_page', 'file:///private/image'),
                               ('width', 100000), ('sha256', 'invalid')]:
                variant = json.loads(json.dumps(manifest))
                variant['re4'][key] = value
                variants.append(variant)
            variant = json.loads(json.dumps(manifest))
            variant['unexpected'] = variant['re4']
            variants.append(variant)
            for variant in variants:
                (directory / 'sources.json').write_text(json.dumps(variant), encoding='utf-8')
                with self.subTest(variant=variant), self.assertRaises(ValueError):
                    tool.load_sources(root)
