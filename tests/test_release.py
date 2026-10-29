from pathlib import Path
from tempfile import TemporaryDirectory
import hashlib,json,struct,unittest,zipfile
from tools.release import prepare_release,PUBLIC_FILES

def nro():
    data=bytearray(8192+40+0x4000);data[16:20]=b'NRO0'
    for offset,value in [(24,8192),(36,4096),(40,4096),(44,4096),(48,8192)]:struct.pack_into('<I',data,offset,value)
    data[4096:4096+29]=b'GAME_REMOTE_RELOAD_BRIDGE_V2\0'
    data[8192:8196]=b'ASET';struct.pack_into('<QQ',data,8192+24,40,0x4000)
    start=8192+40;data[start:start+13]=b'RE4 Controls\0';data[start+0x200:start+0x20d]=b're4-controls\0'
    struct.pack_into('<Q',data,start+0x3038,0x0100E4C04E540000);data[start+0x3060:start+0x3066]=b'0.4.0\0'
    return bytes(data)

class ReleaseTests(unittest.TestCase):
    def test_allowlisted_release_manifest_hashes_and_private_file_refusal(self):
        with TemporaryDirectory() as temp:
            root=Path(temp);image=root/'app.nro';image.write_bytes(nro());package=root/'public.zip'
            files={name:(nro() if name.endswith('.nro') else b'public') for name in PUBLIC_FILES}
            def write(extra=False):
                manifest={'version':'0.4.0','files':{n:hashlib.sha256(b).hexdigest() for n,b in files.items()}}
                with zipfile.ZipFile(package,'w') as archive:
                    for name,data in files.items():archive.writestr(name,data)
                    archive.writestr('manifest.json',json.dumps(manifest))
                    if extra:archive.writestr('switch/re4-controls/token.txt',b'PRIVATE')
            write();out=root/'release';prepare_release(image,package,'0.4.0',out)
            self.assertEqual({p.name for p in out.iterdir()},{'re4-controls.nro','re4-controls-sd.zip','re4-controls-update.txt','SHA256SUMS'})
            manifest=(out/'re4-controls-update.txt').read_text()
            self.assertIn('version=0.4.0\n',manifest)
            self.assertIn(hashlib.sha256(nro()).hexdigest(),manifest)
            self.assertNotIn(b'\r', (out/'re4-controls-update.txt').read_bytes())
            write(True)
            with self.assertRaises(ValueError):prepare_release(image,package,'0.4.0',out)
            write()
            with self.assertRaises(ValueError):prepare_release(image,package,'0.4.1',out)
            image.write_bytes(b'PRIVATE')
            with self.assertRaises(ValueError):prepare_release(image,package,'0.4.0',out)

if __name__=='__main__':unittest.main()
