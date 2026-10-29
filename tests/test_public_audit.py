from pathlib import Path
from tempfile import TemporaryDirectory
import subprocess,unittest,zipfile
from tools.audit_public import scan_material,scan_tree,scan_history,scan_archive

class PublicAuditTests(unittest.TestCase):
    def test_no_values_in_reports_and_archive_contents_checked(self):
        secret=b'FAKE_PERSONAL_SECRET_ONLY_FOR_TEST'
        report=scan_material('own.txt',secret, [('known credential',secret)])
        self.assertEqual(report,[{'path':'own.txt','category':'known credential'}])
        self.assertNotIn(secret.decode(),str(report))
        self.assertTrue(scan_material('text',('ghp_'+'x'*36).encode(),[]))
        with TemporaryDirectory() as temp:
            root=Path(temp);(root/'own.txt').write_bytes(b'public')
            self.assertEqual(scan_tree(root,['own.txt'],[]),[])
            self.assertTrue(scan_tree(root,['data/token.txt'],[]))
            archive=root/'package.zip'
            with zipfile.ZipFile(archive,'w') as output:output.writestr('hidden.txt',secret)
            self.assertTrue(scan_archive(archive,[('known credential',secret)]))

    def test_removed_secret_is_found_in_git_history(self):
        secret=b'FAKE_REMOVED_SECRET_ONLY_FOR_TEST'
        with TemporaryDirectory() as temp:
            root=Path(temp)
            def git(*args):return subprocess.run(['git','-c','user.name=Test','-c','user.email=test@example.invalid',*args],cwd=root,capture_output=True,check=True)
            git('init');(root/'old.txt').write_bytes(secret);git('add','old.txt');git('commit','-m','test fixture')
            git('rm','old.txt');git('commit','-m','remove fixture')
            report,count=scan_history(root,[('known credential',secret)])
            self.assertGreater(count,0);self.assertTrue(any(item['category']=='known credential' for item in report))
            self.assertNotIn(secret.decode(),str(report))

if __name__=='__main__':unittest.main()
