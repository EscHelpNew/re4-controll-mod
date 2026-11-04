from importlib.util import spec_from_file_location, module_from_spec
from pathlib import Path
from tempfile import TemporaryDirectory
from PIL import Image
import hashlib
import unittest

ROOT=Path(__file__).resolve().parents[1]
spec=spec_from_file_location('prepare_icon',ROOT/'tools/prepare_icon.py')
tool=module_from_spec(spec);spec.loader.exec_module(tool)


class IconTests(unittest.TestCase):
    def test_attached_artwork_preserved_and_switch_format(self):
        for name in ('icon-original.png','icon-re.png'):
            with self.subTest(artwork=name),TemporaryDirectory(prefix='re4 icon ') as temp:
                source=ROOT/'assets'/name
                before=hashlib.sha256(source.read_bytes()).digest()
                output=tool.prepare(source,Path(temp)/'icon.jpg')
                with Image.open(output) as icon:
                    self.assertEqual(icon.size,(256,256))
                    self.assertEqual((icon.format,icon.mode),('JPEG','RGB'))
                    self.assertLess(output.stat().st_size,0x20000)
                    # The red-on-black design remains identifiable.
                    r,g,b=icon.getpixel((140,128))
                    self.assertGreater(r,180);self.assertLess(g,45);self.assertLess(b,45)
                bitmap=tool.prepare_bitmap(source,Path(temp)/'app-logo.bmp')
                with Image.open(bitmap) as logo:
                    self.assertEqual(logo.size,(256,256))
                    self.assertEqual((logo.format,logo.mode),('BMP','RGB'))
                self.assertEqual(hashlib.sha256(source.read_bytes()).digest(),before)

    def test_original_cannot_be_replaced(self):
        source=ROOT/'assets/icon-original.png'
        with self.assertRaises(ValueError):
            tool.prepare(source,source)
