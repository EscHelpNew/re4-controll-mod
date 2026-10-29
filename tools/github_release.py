"""Publish this repository's audited allowlisted assets from main. Credentials stay in memory."""
from pathlib import Path
import argparse,hashlib,json,os,re,subprocess,sys,urllib.request,urllib.error,urllib.parse
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.release import ASSETS,validate_inputs
from tools.version import get_version
from tools.audit_public import known_material,scan_tree,scan_history,scan_archive,git
REPO='EscHelpNew/re4-controll-mod'

def publish_release(api,directory,version,commit,body):
    if re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+',version) is None or re.fullmatch('[0-9a-f]{40}',commit) is None:
        raise ValueError('Invalid release identity')
    if {p.name for p in directory.iterdir()}!=ASSETS or not all((directory/name).is_file() for name in ASSETS):
        raise ValueError('Release contains unexpected files')
    release=api.request('GET','/releases/tags/v'+version)
    if release is None:
        release=api.request('POST','/releases',payload={'tag_name':'v'+version,'target_commitish':commit,
            'name':'RE4 Controls v'+version,'body':body,'draft':True,'prerelease':False})
    existing={asset['name']:asset for asset in release.get('assets',[])}
    for name in sorted(ASSETS):
        data=(directory/name).read_bytes();digest='sha256:'+hashlib.sha256(data).hexdigest()
        asset=existing.get(name)
        if asset is None:
            if not release.get('draft'):raise ValueError('Published release is incomplete; refusing to change it')
            url=f'https://uploads.github.com/repos/{REPO}/releases/{release["id"]}/assets?name='+urllib.parse.quote(name)
            asset=api.request('POST',url,blob=data)
        if asset.get('size')!=len(data) or asset.get('digest')!=digest:
            raise ValueError('Uploaded asset verification failed: '+name)
        print('Verified release asset:',name,len(data),flush=True)
    if release.get('draft'):
        release=api.request('PATCH','/releases/'+str(release['id']),payload={'draft':False,'make_latest':'true'})
    return release['html_url']

class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self,req,fp,code,msg,headers,newurl):return None

class GitHub:
    def __init__(self):
        self._token=os.environ.get('GH_TOKEN') or os.environ.get('GITHUB_TOKEN')
        if not self._token:
            env=dict(os.environ,GIT_TERMINAL_PROMPT='0',GCM_INTERACTIVE='Never')
            result=subprocess.run(['git','credential','fill'],input='protocol=https\nhost=github.com\n\n',capture_output=True,text=True,env=env,timeout=20)
            credentials=dict(line.split('=',1) for line in result.stdout.splitlines() if '=' in line)
            self._token=credentials.get('password')
        if not self._token:raise RuntimeError('GitHub authentication is unavailable; no credential was saved')
        self._opener=urllib.request.build_opener(NoRedirect)
    def request(self,method,path,payload=None,blob=None):
        if path.startswith('/'):
            url='https://api.github.com/repos/'+REPO+path
        elif path.startswith('https://uploads.github.com/repos/'+REPO+'/releases/'):
            url=path
        else:raise ValueError('Refusing credential use outside the configured GitHub repository')
        headers={'User-Agent':'RE4-Controls-Release','Accept':'application/vnd.github+json',
            'X-GitHub-Api-Version':'2022-11-28','Authorization':'Bearer '+self._token}
        data=blob
        if payload is not None:data=json.dumps(payload).encode();headers['Content-Type']='application/json'
        elif blob is not None:headers['Content-Type']='application/octet-stream'
        request=urllib.request.Request(url,data=data,headers=headers,method=method)
        try:
            with self._opener.open(request,timeout=120) as response:return json.load(response)
        except urllib.error.HTTPError as error:
            if error.code==404 and method=='GET':return None
            raise RuntimeError(f'GitHub {method} failed: HTTP {error.code}') from None

if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--publish',action='store_true',required=True)
    parser.add_argument('--keys-file',type=Path);args=parser.parse_args()
    version=get_version();directory=ROOT/'build/releases'/('v'+version)
    branch=subprocess.check_output(git(ROOT,'branch','--show-current')).decode().strip()
    dirty=subprocess.check_output(git(ROOT,'status','--porcelain')).strip()
    commit=subprocess.check_output(git(ROOT,'rev-parse','HEAD')).decode().strip()
    remote=subprocess.check_output(git(ROOT,'remote','get-url','origin')).decode().strip()
    if branch!='main' or dirty or remote!='https://github.com/'+REPO+'.git':raise RuntimeError('Publish requires clean main and the configured origin')
    remote_main=subprocess.check_output(git(ROOT,'ls-remote','origin','refs/heads/main')).decode().split()[0]
    if remote_main!=commit:raise RuntimeError('Push the tested main commit before publishing')
    validate_inputs(directory/'re4-controls.nro',directory/'re4-controls-sd.zip',version)
    client=GitHub();known=known_material(args.keys_file)+[('GitHub credential',client._token.encode())]
    names=[p for p in subprocess.check_output(git(ROOT,'ls-files','-z')).decode().split('\0') if p]
    findings=scan_tree(ROOT,names,known);history,_=scan_history(ROOT,known);findings.extend(history)
    for file in directory.iterdir():findings.extend(scan_archive(file,known))
    if findings:raise RuntimeError('Publication audit failed; no release created: '+json.dumps(findings))
    body=(ROOT/'docs/releases'/('v'+version+'.md')).read_text(encoding='utf-8')
    print(publish_release(client,directory,version,commit,body))
