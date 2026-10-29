from pathlib import Path
from tempfile import TemporaryDirectory
import hashlib,unittest,io,urllib.error
from tools.github_release import publish_release,GitHub,NoRedirect
from tools.release import ASSETS
class Fake:
    def __init__(self,bad=False):self.calls=[];self.bad=bad
    def request(self,method,path,payload=None,blob=None):
        self.calls.append((method,path,payload))
        if method=='GET':return None
        if method=='POST' and path=='/releases':return {'id':7,'draft':True,'assets':[]}
        if method=='POST':
            name=path.split('name=')[1]
            return {'name':name,'size':len(blob),'digest':'sha256:'+('0'*64 if self.bad else hashlib.sha256(blob).hexdigest())}
        return {'html_url':'https://github.com/EscHelpNew/re4-controll-mod/releases/tag/v0.4.1'}
class GitHubReleaseTests(unittest.TestCase):
    def test_credential_host_boundary_and_errors_do_not_expose_auth(self):
        client=object.__new__(GitHub);client._token='LOCAL_ONLY_FAKE_CREDENTIAL'
        class Opener:
            calls=[]
            def open(self,request,timeout):
                self.calls.append(request)
                return io.BytesIO(b'{"ok":true}')
        opener=Opener();client._opener=opener
        self.assertEqual(client.request('GET','/releases'),{'ok':True})
        self.assertEqual(opener.calls[0].get_header('Authorization'),'Bearer '+client._token)
        with self.assertRaises(ValueError):client.request('POST','https://example.invalid/upload',blob=b'public')
        self.assertEqual(len(opener.calls),1)
        self.assertIsNone(NoRedirect().redirect_request(None,None,302,'',{},'https://example.invalid/'))
        class Failure:
            def open(self,request,timeout):raise urllib.error.HTTPError(request.full_url,403,'forbidden',{},None)
        client._opener=Failure()
        with self.assertRaises(RuntimeError) as error:client.request('POST','/releases',payload={})
        self.assertNotIn(client._token,str(error.exception))
    def test_draft_before_assets_and_publish_after_digest_checks(self):
        with TemporaryDirectory() as temp:
            root=Path(temp)
            for name in ASSETS:(root/name).write_bytes(b'public')
            api=Fake();url=publish_release(api,root,'0.4.1','a'*40,'release notes')
            self.assertTrue(url.endswith('/v0.4.1'))
            self.assertTrue(api.calls[1][2]['draft'])
            self.assertEqual(len([c for c in api.calls if c[0]=='POST' and 'uploads.github.com' in c[1]]),4)
            self.assertEqual(api.calls[-1][0],'PATCH')
            bad=Fake(True)
            with self.assertRaises(ValueError):publish_release(bad,root,'0.4.1','a'*40,'notes')
            self.assertFalse(any(c[0]=='PATCH' for c in bad.calls))
            (root/'token.txt').write_bytes(b'PRIVATE_TEST')
            rejected=Fake()
            with self.assertRaises(ValueError):publish_release(rejected,root,'0.4.1','a'*40,'notes')
            self.assertEqual(rejected.calls,[])
if __name__=='__main__':unittest.main()
