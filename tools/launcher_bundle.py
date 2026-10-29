"""Prepare an own-title-only forwarder payload; keys stay on the build computer."""
from pathlib import Path
import argparse
import hashlib
import struct
import subprocess
import tempfile

OWN=0x0100E4C04E540000

def encode_bundle(title,version,database,contents):
    if title!=OWN or not 0<=version<=0xffffffff or len(contents)!=3:
        raise ValueError('Only the RE4 Controls launcher is supported')
    if [kind for kind,_ in contents]!=[0,1,3] or len(database)!=96:
        raise ValueError('Expected own Meta, Program, Control and database')
    if database[:8]!=struct.pack('<HHHBB',16,3,0,0,0):
        raise ValueError('Unexpected database header')
    records=[];payload=[]
    for index,(kind,data) in enumerate(contents):
        if not 0<len(data)<=4*1024*1024:raise ValueError('Invalid own content size')
        digest=hashlib.sha256(data).digest()
        info=digest[:16]+struct.pack('<I4B',len(data),0,0,kind,0)
        if database[24+index*24:48+index*24]!=info:
            raise ValueError('Metadata/content mismatch')
        records.append(digest[:16]+digest+struct.pack('<IB11x',len(data),kind))
        payload.append(data)
    return struct.pack('<8sQIIII',b'R4HOME01',title,version,len(database),3,0)+database+b''.join(records)+b''.join(payload)

def export_nsp(bundle,destination):
    """Repackage only the already-built own encrypted content; no keys required."""
    if len(bundle)<320:raise ValueError('Truncated own HOME bundle')
    magic,title,version,db_size,count,reserved=struct.unpack_from('<8sQIIII',bundle)
    if (magic,title,db_size,count,reserved)!=(b'R4HOME01',OWN,96,3,0):raise ValueError('Unexpected HOME bundle')
    contents=[];names=[];offset=320
    for i in range(3):
        record=bundle[128+i*64:192+i*64];size=struct.unpack_from('<I',record,48)[0];kind=record[52]
        if size>len(bundle)-offset:raise ValueError('Truncated own content')
        data=bundle[offset:offset+size];contents.append((kind,data));offset+=size
        names.append(hashlib.sha256(data).hexdigest()[:32]+('.cnmt.nca' if kind==0 else '.nca'))
    if offset!=len(bundle) or encode_bundle(title,version,bundle[32:128],contents)!=bundle:
        raise ValueError('Own bundle failed integrity checks')
    strings=b'';entries=b'';offset=0
    for name,(_,data) in zip(names,contents):
        entries+=struct.pack('<QQII',offset,len(data),len(strings),0);strings+=name.encode('ascii')+b'\0';offset+=len(data)
    strings=strings.ljust((len(strings)+15)&~15,b'\0')
    payload=struct.pack('<4sIII',b'PFS0',3,len(strings),0)+entries+strings+b''.join(data for _,data in contents)
    destination.parent.mkdir(parents=True,exist_ok=True);destination.write_bytes(payload)

def generate(nsp,keys,hactool,destination):
    raw=nsp.read_bytes()
    if len(raw)<16:raise ValueError('Truncated NSP')
    magic,count,string_size,_=struct.unpack_from('<4sIII',raw)
    if magic!=b'PFS0' or count!=3 or string_size>4096:raise ValueError('Expected own three-NCA NSP')
    strings=16+count*24;start=strings+string_size
    files={}
    with tempfile.TemporaryDirectory(prefix='re4-home-',dir=destination.parent) as temporary:
        stage=Path(temporary)
        for i in range(count):
            off,size,nameoff,_=struct.unpack_from('<QQII',raw,16+i*24)
            if nameoff>=string_size or start+off+size>len(raw):raise ValueError('Bad NSP bounds')
            end=raw.index(0,strings+nameoff,strings+string_size)
            name=raw[strings+nameoff:end].decode('ascii')
            if Path(name).name!=name or not name.endswith('.nca'):raise ValueError('Unexpected content name')
            data=raw[start+off:start+off+size]
            if hashlib.sha256(data).hexdigest()[:32]!=name[:32]:raise ValueError('Bad NCA content ID')
            file=stage/name;file.write_bytes(data)
            target=stage/(name+'.unpack')
            result=subprocess.run([str(hactool),'-k',str(keys),'--suppresskeys','--disablekeywarns',
                '--romfsdir='+str(target),'--section0dir='+str(target),str(file)],capture_output=True,text=True)
            if result.returncode:raise RuntimeError('Own NCA extraction failed (details suppressed)')
            lines=result.stdout.splitlines()
            def field(label):
                values=[line.split(':',1)[1].strip() for line in lines if line.strip().startswith(label+':')]
                return values[0] if values else ''
            if int(field('Title ID'),16)!=OWN:raise ValueError('Foreign title in forwarder')
            kind={'Meta':0,'Program':1,'Control':3}.get(field('Content Type'))
            if kind is None or kind in files:raise ValueError('Unexpected content type')
            files[kind]=data
            if kind==0:
                cnmts=list(target.glob('*.cnmt'))
                if len(cnmts)!=1:raise ValueError('Missing own CNMT')
                cnmt=cnmts[0].read_bytes()
        if set(files)!={0,1,3}:raise ValueError('Incomplete forwarder')
        tid,version,meta_type,_,ext,count,metas,attr,storage=struct.unpack_from('<QIBBHHHBB',cnmt)
        if tid!=OWN or meta_type!=0x80 or (ext,count,metas,attr,storage)!=(16,2,0,0,0):
            raise ValueError('Unexpected own application CNMT')
        if len(cnmt)!=0x20+ext+count*56+32:raise ValueError('Unexpected CNMT length')
        infos={}
        for i in range(count):
            record=cnmt[0x20+ext+i*56:0x20+ext+(i+1)*56]
            kind=record[54]
            if kind not in (1,3) or kind in infos:raise ValueError('Unexpected packaged content')
            if hashlib.sha256(files[kind]).digest()!=record[:32]:raise ValueError('CNMT digest mismatch')
            infos[kind]=record[32:]
        meta_info=hashlib.sha256(files[0]).digest()[:16]+struct.pack('<I4B',len(files[0]),0,0,0,0)
        database=struct.pack('<HHHBB',16,3,0,0,0)+cnmt[0x20:0x30]+meta_info+infos[1]+infos[3]
        blob=encode_bundle(OWN,version,database,[(k,files[k]) for k in (0,1,3)])
        destination.write_bytes(blob)
    print('Validated own HOME bundle:',destination,'bytes',len(blob),'SHA256',hashlib.sha256(blob).hexdigest())

if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__)
    parser.add_argument('nsp',type=Path);parser.add_argument('--keys',type=Path,required=True)
    parser.add_argument('--hactool',type=Path,default=Path('.tools/hactool/hactool.exe'))
    parser.add_argument('--out',type=Path,default=Path('assets/home-launcher.bin'))
    args=parser.parse_args();generate(args.nsp,args.keys,args.hactool.resolve(),args.out)
