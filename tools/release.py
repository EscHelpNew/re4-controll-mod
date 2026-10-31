"""Prepare exactly four public release assets; never reads personal credentials."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import shutil
import struct
import sys
import zipfile
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.version import get_version

PUBLIC_FILES={
    'atmosphere/contents/010099A00BC1E000/exefs/subsdk9',
    'atmosphere/contents/010099A00BC1E000/exefs/main.npdm',
    'switch/.overlays/re4-controls.ovl','switch/re4-controls/bridge.nro','switch/re4-controls/game.nro',
    'atmosphere/contents/01002A000CD48000/exefs/subsdk9',
    'atmosphere/contents/01002A000CD48000/exefs/main.npdm',
    'switch/.overlays/re6-telemetry.ovl','switch/.overlays/re6-controls.ovl','README-measurement-ru.md','README-re6-graphics.md',
    'install/re4-controls-forwarder.nsp','README-install.md',
    'licenses/MIT.txt','licenses/exlaunch-GPL2.txt','licenses/libtesla-GPL2.txt',
    'licenses/Switch-Debug-Kit-MIT.txt','licenses/nx-ovlloader-GPL2.txt','licenses/graphics-NOTICES.txt','licenses/NTON-GPL3.txt'
}
ASSETS={'re4-controls.nro','re4-controls-sd.zip','re4-controls-update.txt','SHA256SUMS'}

def inspect_nro(data):
    if len(data)<128 or len(data)>64*1024*1024 or data[16:20]!=b'NRO0':raise ValueError('Not a release NRO')
    image=struct.unpack_from('<I',data,24)[0];end=0
    if image<4096 or image%4096 or image>len(data):raise ValueError('Invalid NRO image')
    for i in range(3):
        offset,size=struct.unpack_from('<II',data,32+i*8)
        if offset!=end or offset%4096 or size%4096 or size>image-offset:raise ValueError('Invalid NRO sections')
        if i==0 and size<4096:raise ValueError('Missing NRO text')
        if i==1 and b'GAME_REMOTE_RELOAD_BRIDGE_V2\0' not in data[offset:offset+size]:raise ValueError('Missing reload bridge')
        end=offset+size
    if end!=image or data[image:image+4]!=b'ASET' or len(data)-image<40:raise ValueError('Missing NRO metadata')
    offset,size=struct.unpack_from('<QQ',data,image+24)
    if size!=0x4000 or offset>len(data)-image or size>len(data)-image-offset:raise ValueError('Invalid NACP bounds')
    nacp=data[image+offset:image+offset+size]
    if struct.unpack_from('<Q',nacp,0x3038)[0]!=0x0100E4C04E540000:raise ValueError('Foreign application ID')
    if nacp[:13]!=b'RE4 Controls\0' or nacp[0x200:0x20d]!=b're4-controls\0':raise ValueError('Foreign application identity')
    version=nacp[0x3060:0x3070].split(b'\0',1)[0].decode('ascii')
    if re.fullmatch(r'(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)',version) is None:raise ValueError('Invalid release version')
    return version

def validate_inputs(nro,sd_zip,version):
    data=nro.read_bytes()
    if inspect_nro(data)!=version:raise ValueError('NRO version differs from release')
    with zipfile.ZipFile(sd_zip) as archive:
        if set(archive.namelist())!=PUBLIC_FILES|{'manifest.json'} or len(archive.namelist())!=len(PUBLIC_FILES)+1:
            raise ValueError('Unexpected files in public SD package')
        manifest=json.loads(archive.read('manifest.json'))
        if manifest.get('version')!=version or set(manifest.get('files',{}))!=PUBLIC_FILES:raise ValueError('Package manifest mismatch')
        for name,digest in manifest['files'].items():
            if hashlib.sha256(archive.read(name)).hexdigest()!=digest:raise ValueError('Package digest mismatch')
        for name in ('switch/re4-controls/bridge.nro','switch/re4-controls/game.nro'):
            if archive.read(name)!=data:raise ValueError('SD package and standalone NRO differ')
    return data

def prepare_release(nro,sd_zip,version,destination):
    data=validate_inputs(nro,sd_zip,version)
    destination.mkdir(parents=True,exist_ok=True)
    if any(p.name not in ASSETS or not p.is_file() for p in destination.iterdir()):raise ValueError('Unexpected file in release directory')
    shutil.copy2(nro,destination/'re4-controls.nro')
    shutil.copy2(sd_zip,destination/'re4-controls-sd.zip')
    sha=hashlib.sha256(data).hexdigest()
    text=f'RE4-CONTROLS-UPDATE 1\nversion={version}\nbytes={len(data)}\nsha256={sha}\n'
    (destination/'re4-controls-update.txt').write_bytes(text.encode('ascii'))
    sums=''.join(hashlib.sha256((destination/name).read_bytes()).hexdigest()+'  '+name+'\n' for name in sorted(ASSETS-{'SHA256SUMS'}))
    (destination/'SHA256SUMS').write_bytes(sums.encode('ascii'))
    print('Public release prepared:',destination,'version',version,'NRO SHA256',sha)

if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__)
    parser.add_argument('--version',default=get_version())
    args=parser.parse_args()
    prepare_release(ROOT/'build/re4-controls.nro',ROOT/'build/re4-controls-experimental.zip',args.version,ROOT/'build/releases'/('v'+args.version))
