"""Audit public source/history/artifacts. Reports paths and categories, never values."""
from pathlib import Path,PurePosixPath
import argparse,json,os,re,subprocess,zipfile
ROOT=Path(__file__).resolve().parents[1]
BLOCKED={'.git','.venv','.tools','.worktrees','.superpowers','.aws','.codex','build','data'}
PATTERNS=[('GitHub credential',re.compile(rb'(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{60,})')),
          ('private key',re.compile(rb'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----'))]

def scan_material(path,data,known):
    categories={name for name,value in known if value and value in data}
    categories.update(name for name,pattern in PATTERNS if pattern.search(data))
    return [{'path':str(path),'category':name} for name in sorted(categories)]

def forbidden(path):
    normalized=PurePosixPath(path.replace('\\','/'))
    return normalized.is_absolute() or '..' in normalized.parts or any(p in BLOCKED for p in normalized.parts)

def scan_tree(root,paths,known):
    report=[]
    for name in paths:
        path=Path(name)
        if forbidden(name) or name=='project.json' or path.suffix.lower() in ('.keys','.nca','.ncz','.nsz','.nsp') or path.name.lower()=='token.txt':
            report.append({'path':name,'category':'private/generated path'});continue
        file=root/name
        if file.is_file():report.extend(scan_material(name,file.read_bytes(),known))
    return report

def git(root,*args):
    return ['git','-c','safe.directory='+root.resolve().as_posix(),'-C',str(root),*args]

def scan_history(root,known):
    rows=subprocess.check_output(git(root,'rev-list','--objects','--all')).decode().splitlines()
    process=subprocess.Popen(git(root,'cat-file','--batch'),stdin=subprocess.PIPE,stdout=subprocess.PIPE)
    report=[];count=0
    try:
        for row in rows:
            oid,_,path=row.partition(' ')
            process.stdin.write((oid+'\n').encode());process.stdin.flush()
            header=process.stdout.readline().decode().split()
            if len(header)!=3:raise RuntimeError('Invalid Git object response')
            size=int(header[2]);data=process.stdout.read(size)
            if len(data)!=size or process.stdout.read(1)!=b'\n':raise RuntimeError('Truncated Git object')
            if header[1]=='blob':
                count+=1;report.extend(scan_material('history:'+oid+':'+path,data,known))
    finally:
        process.stdin.close();process.stdout.close();process.wait(timeout=10)
    return report,count

def scan_archive(path,known):
    report=scan_material(path.name,path.read_bytes(),known)
    if zipfile.is_zipfile(path):
        with zipfile.ZipFile(path) as archive:
            for name in archive.namelist():
                if forbidden(name) or name.lower().endswith(('.keys','/token.txt')):
                    report.append({'path':path.name+':'+name,'category':'private archive path'})
                report.extend(scan_material(path.name+':'+name,archive.read(name),known))
    return report

def known_material(keys=None,credential_manager=False):
    known=[]
    def add(label,value):
        if value and len(value)>=16 and len(set(value.lower()))>2:known.append((label,value))
    token=ROOT/'data/token.txt'
    if token.is_file():add('local receiver credential',token.read_bytes().strip())
    if keys:
        for line in keys.read_text(encoding='utf-8-sig').splitlines():
            if '=' not in line:continue
            value=line.split('=',1)[1].strip()
            if re.fullmatch('[0-9a-fA-F]{32,}',value):
                add('local key material',value.encode());add('local key material',value.upper().encode())
                add('local binary key material',bytes.fromhex(value))
    for name in ('GH_TOKEN','GITHUB_TOKEN'):add('GitHub credential',os.environ.get(name,'').encode())
    if credential_manager:
        env=dict(os.environ,GIT_TERMINAL_PROMPT='0',GCM_INTERACTIVE='Never')
        result=subprocess.run(['git','credential','fill'],input='protocol=https\nhost=github.com\n\n',capture_output=True,text=True,env=env,timeout=20)
        for line in result.stdout.splitlines():
            if line.startswith('password='):add('GitHub credential',line.split('=',1)[1].encode())
    return known

if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__)
    parser.add_argument('--history',action='store_true');parser.add_argument('--keys-file',type=Path)
    parser.add_argument('--credential-manager',action='store_true');parser.add_argument('--release',type=Path)
    args=parser.parse_args();known=known_material(args.keys_file,args.credential_manager)
    names=subprocess.check_output(git(ROOT,'ls-files','--cached','--others','--exclude-standard','-z')).decode().split('\0')
    report=scan_tree(ROOT,[n for n in names if n],known);objects=0
    if args.history:
        history,objects=scan_history(ROOT,known);report.extend(history)
    if args.release:
        for file in args.release.iterdir():
            if file.is_file():report.extend(scan_archive(file,known))
    print(json.dumps({'working_files':len([n for n in names if n]),'history_blobs':objects,'findings':report},ensure_ascii=True))
    raise SystemExit(1 if report else 0)
