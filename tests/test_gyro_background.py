from importlib.util import module_from_spec, spec_from_file_location
from pathlib import Path
from tempfile import TemporaryDirectory
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


class GyroBackgroundTests(unittest.TestCase):
    def test_fullscreen_output_is_deterministic_and_keeps_thumbnails(self):
        with TemporaryDirectory() as directory:
            root = Path(directory)
            shutil.copytree(ROOT / 'assets/graphics', root / 'assets/graphics')
            thumbnail = tool.prepare(root)
            header = root / 'build/graphics/gyro_test_background_image.hpp'
            self.assertTrue(header.exists(), 'Missing fullscreen gyro background')
            first = header.read_bytes()
            self.assertEqual(tool.prepare(root), thumbnail)
            self.assertEqual(header.read_bytes(), first)
            text = first.decode('ascii')
            body = re.search(r'pixels\[1280 \* 720\] = \{(.*?)\};', text, re.S).group(1)
            self.assertEqual(len(re.findall(r'0x[0-9a-f]{4}', body)), 1280 * 720)
            self.assertIn('kWidth = 1280', text)
            self.assertIn('kHeight = 720', text)
            self.assertNotIn(str(root), text)
            old = thumbnail.read_text(encoding='ascii')
            for game in ('re4', 're6'):
                body = re.search(rf'{game}_pixels\[320 \* 180\] = \{{(.*?)\}};', old, re.S).group(1)
                self.assertEqual(len(re.findall(r'0x[0-9a-f]{4}', body)), 320 * 180)

    def test_derived_image_identity_and_tampering(self):
        record = json.loads((ROOT / 'assets/graphics/gyro-test-background.json').read_text())
        source = ROOT / 'assets/graphics/gyro-test-background.png'
        self.assertEqual(hashlib.sha256(source.read_bytes()).hexdigest(), record['sha256'])
        with Image.open(source) as image:
            self.assertEqual(image.size, (record['width'], record['height']))
        with TemporaryDirectory() as directory:
            root = Path(directory)
            shutil.copytree(ROOT / 'assets/graphics', root / 'assets/graphics')
            (root / 'assets/graphics/gyro-test-background.png').write_bytes(b'tampered')
            with self.assertRaisesRegex(ValueError, 'hash'):
                tool.prepare(root)
            self.assertFalse((root / 'build/graphics/gyro_test_background_image.hpp').exists())

    def test_derived_manifest_and_size_limits_reject_invalid_inputs(self):
        with TemporaryDirectory() as directory:
            root = Path(directory)
            shutil.copytree(ROOT / 'assets/graphics', root / 'assets/graphics')
            path = root / 'assets/graphics/gyro-test-background.json'
            original = json.loads(path.read_text())
            sources = tool.load_sources(root)
            for key, value in [('width', -1), ('height', 8193), ('width', True),
                               ('source_file', '../another.png'), ('sha256', 'invalid'),
                               ('source_sha256', '0' * 64), ('edit_tool', '')]:
                entry = dict(original)
                entry[key] = value
                path.write_text(json.dumps(entry))
                with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                    tool.gyro_background(root, sources)
            entry = dict(original)
            entry['width'] += 1
            path.write_text(json.dumps(entry))
            with self.assertRaisesRegex(ValueError, 'dimensions differ'):
                tool.gyro_background(root, sources)
            path.write_text(json.dumps(original))
            with (root / 'assets/graphics/gyro-test-background.png').open('wb') as source:
                source.truncate(4 * 1024 * 1024 + 1)
            with self.assertRaisesRegex(ValueError, '4 MiB'):
                tool.gyro_background(root, sources)


if __name__ == '__main__':
    unittest.main()
