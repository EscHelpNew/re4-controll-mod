from pathlib import Path
from tempfile import TemporaryDirectory
import unittest
from tools.payload import stage_payload,FILES
class PayloadTests(unittest.TestCase):
    def test_only_fixed_payload_files_and_stale_material_removed(self):
        with TemporaryDirectory() as temp:
            root=Path(temp);source=root/'sources';source.mkdir()
            files={}
            for name in FILES:
                files[name]=source/name;files[name].write_bytes(b'public')
            stage=root/'build/diagnostic-romfs';stage.mkdir(parents=True)
            (stage/'token.txt').write_bytes(b'PRIVATE_STALE_TEST_VALUE')
            stage_payload(root,files)
            self.assertEqual({p.name for p in stage.iterdir()},FILES)
            with self.assertRaises(ValueError):stage_payload(root,{**files,'token.txt':source/'token.txt'})
            self.assertEqual({p.name for p in stage.iterdir()},FILES)
if __name__=='__main__':unittest.main()
