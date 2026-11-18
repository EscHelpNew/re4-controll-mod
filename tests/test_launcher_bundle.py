import hashlib
import struct
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory
from types import SimpleNamespace
from unittest.mock import patch
from tools.launcher_bundle import encode_bundle,generate,export_nsp

OWN=0x0100E4C04E540000

def own_control():
    data=bytearray(0x4000)
    data[:13]=b'RE4 Controls\0';data[0x200:0x20d]=b're4-controls\0'
    data[0x3060:0x3070]=b'0.28.11'.ljust(16,b'\0')
    for offset in (0x3038,0x3078,0x30b0):struct.pack_into('<Q',data,offset,OWN)
    struct.pack_into('<Q',data,0x3070,OWN+0x1000)
    return bytes(data)
class LauncherBundleTests(unittest.TestCase):
    def test_own_three_contents_and_database(self):
        contents=[(kind,bytes([kind+1])*100) for kind in (0,1,3)]
        infos=[hashlib.sha256(data).digest()[:16]+struct.pack('<I4B',len(data),0,0,kind,0) for kind,data in contents]
        database=struct.pack('<HHHBB',16,3,0,0,0)+bytes(16)+b''.join(infos)
        data=encode_bundle(OWN,0,database,contents)
        self.assertEqual(data[:8],b'R4HOME01')
        self.assertEqual(struct.unpack_from('<Q',data,8)[0],OWN)
        self.assertEqual(len(data),32+len(database)+3*64+300)
        with TemporaryDirectory() as temp:
            target=Path(temp)/'own.nsp';export_nsp(data,target)
            packed=target.read_bytes();self.assertEqual(packed[:4],b'PFS0')
            self.assertEqual(struct.unpack_from('<I',packed,4)[0],3)
            for _,payload in contents:self.assertIn(payload,packed)
            with self.assertRaises(ValueError):export_nsp(data[:-1],target)
        for title in (0,0x010099A00BC1E000,0x0100000000001000):
            with self.assertRaises(ValueError):encode_bundle(title,0,database,contents)
        with self.assertRaises(ValueError):encode_bundle(OWN,0,database,contents[:2])
        with self.assertRaises(ValueError):encode_bundle(OWN,0,bytes(len(database)),contents)

    def test_generator_validates_nsp_extraction_title_and_cnmt_before_publish(self):
        files={kind:bytes([kind+1])*100 for kind in (0,1,3)}
        infos={kind:hashlib.sha256(data).digest()[:16]+struct.pack('<I4B',len(data),0,0,kind,0) for kind,data in files.items()}
        header=struct.pack('<QIBBHHHBB',OWN,0,0x80,0,16,2,0,0,0).ljust(32,b'\0')
        cnmt=header+bytes(16)+b''.join(hashlib.sha256(files[k]).digest()+infos[k] for k in (1,3))+bytes(32)
        names={hashlib.sha256(data).hexdigest()[:32]+('.cnmt.nca' if kind==0 else '.nca'):kind for kind,data in files.items()}
        strings=b'';entries=b'';offset=0
        for name,kind in names.items():
            entries+=struct.pack('<QQII',offset,len(files[kind]),len(strings),0)
            strings+=name.encode()+b'\0';offset+=len(files[kind])
        nsp=struct.pack('<4sIII',b'PFS0',3,len(strings),0)+entries+strings+b''.join(files.values())
        for case in ('good','extract failure','foreign nca','foreign cnmt','missing cnmt','bad digest','bad bounds','shifted control','missing control','foreign control','save allocation'):
            with self.subTest(case=case),TemporaryDirectory() as temp:
                root=Path(temp);source=root/'own.nsp';output=root/'home.bin';output.write_bytes(b'previous')
                raw=bytearray(nsp)
                if case=='bad bounds':struct.pack_into('<Q',raw,16,len(raw)+1)
                source.write_bytes(raw)
                def extract(args,**kwargs):
                    name=Path(args[-1]).name;kind=names[name]
                    target=Path(next(a.split('=',1)[1] for a in args if a.startswith('--section0dir=')))
                    target.mkdir()
                    if kind==0 and case!='missing cnmt':
                        data=bytearray(cnmt)
                        if case=='foreign cnmt':struct.pack_into('<Q',data,0,0x010099A00BC1E000)
                        if case=='bad digest':data[48]^=1
                        (target/'own.cnmt').write_bytes(data)
                    if kind==3 and case!='missing control':
                        control=bytearray(own_control())
                        if case=='shifted control':control[0x3060:0x306f]=b'0.17.2'.ljust(16,b'\0')
                        if case=='foreign control':struct.pack_into('<Q',control,0x3078,OWN+1)
                        if case=='save allocation':struct.pack_into('<Q',control,0x3080,1)
                        (target/'control.nacp').write_bytes(control)
                    title=0x010099A00BC1E000 if case=='foreign nca' else OWN
                    return SimpleNamespace(returncode=int(case=='extract failure'),stdout=f'Title ID: {title:016x}\nContent Type: '+{0:'Meta',1:'Program',3:'Control'}[kind])
                with patch('tools.launcher_bundle.subprocess.run',side_effect=extract):
                    if case=='good':
                        generate(source,root/'keys',root/'tool',output)
                        self.assertEqual(output.read_bytes()[:8],b'R4HOME01')
                    else:
                        with self.assertRaises((ValueError,RuntimeError)):generate(source,root/'keys',root/'tool',output)
                        self.assertEqual(output.read_bytes(),b'previous')

if __name__=='__main__':unittest.main()
