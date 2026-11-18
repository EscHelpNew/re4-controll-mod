from pathlib import Path
import copy
import json
import tempfile
import unittest
from tools.re5_module import TITLE, SOURCE_NPDM_SHA256, graphics_permissions, sdk6_header, stage_layout
from tools.payload import FILES, RE5_FILES, RE6_FILES, stage_payload
ROOT=Path(__file__).resolve().parents[1]
class Re5BuilderTests(unittest.TestCase):
    def test_permissions_preserve_native_services_and_minimum_privileges(self):
        profile=json.loads((ROOT/('profiles/'+TITLE+'/npdm.json')).read_text())
        original=copy.deepcopy(profile)
        original['filesystem_access']['permissions']='0x4000000000000000'
        result=graphics_permissions(original)
        self.assertEqual(result['service_access'],original['service_access'])
        self.assertEqual(int(result['filesystem_access']['permissions'],16),0x4000000000200000)
        self.assertEqual(result['kernel_capabilities'],profile['kernel_capabilities'])
        self.assertEqual(original['filesystem_access']['permissions'],'0x4000000000000000')
        original['title_id']='0x01002a000cd48000'
        with self.assertRaises(ValueError):graphics_permissions(original)
        self.assertEqual(len(SOURCE_NPDM_SHA256),64)
    def test_sdk6_lookup_is_guarded(self):
        text='extern rtld::lookup_global_t g_LookupGlobalManualFunctionPointer;'
        self.assertIn('g_pLookupGlobalManualFunctionPointerE',sdk6_header(text))
        with self.assertRaises(ValueError):sdk6_header('unexpected')
    def test_pool_and_discovery_are_isolated(self):
        import shutil
        with tempfile.TemporaryDirectory() as temp:
            stage=Path(temp)
            for name in ('source','misc'):shutil.copytree(ROOT/'third_party/exlaunch'/name,stage/name)
            stage_layout(stage)
            self.assertIn('JitSize = 0x3000',(stage/'source/program/setting.hpp').read_text())
            assembly=(stage/'source/lib/hook/nx64/inline_asm.s').read_text()
            self.assertIn('mrs x16, nzcv',assembly)
            self.assertIn('str x16, [sp, #0xF8]',assembly)
            self.assertIn('msr nzcv, x16',assembly)
            self.assertIn('HookMax >= 60',(stage/'source/lib/hook/nx64/hook_impl.cpp').read_text())
            self.assertIn('KEEP(*(.data.re5controls))',(stage/'misc/link.ld').read_text())
            self.assertIn('KEEP(*(.data.re5identity))',(stage/'misc/link.ld').read_text())
            self.assertIn('__re5_identity_start__ - __re5_control_end__ == 0',(stage/'misc/link.ld').read_text())
            self.assertIn('__re5_identity_end__ - __re5_identity_start__ == 112',(stage/'misc/link.ld').read_text())
            linker=(stage/'misc/link.ld').read_text()
            self.assertIn('KEEP(*(.data.re5camera))',linker)
            self.assertIn('__re5_camera_start__ - __re5_control_start__ == 688',linker)
            self.assertIn('__re5_camera_end__ - __re5_camera_start__ == 5808',linker)
            with self.assertRaises(ValueError):stage_layout(stage)
    def test_re5_payload_must_be_complete_and_stale_files_removed(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);sources=root/'sources';sources.mkdir()
            files={name:sources/name for name in FILES|RE5_FILES|RE6_FILES}
            for path in files.values():path.write_bytes(b'public')
            base={name:files[name] for name in FILES|RE6_FILES}
            for name in RE5_FILES:
                with self.assertRaises(ValueError):stage_payload(root,{**base,name:files[name]})
            stage=stage_payload(root,files)
            self.assertEqual({p.name for p in stage.iterdir()},set(files))
            stage_payload(root,base)
            self.assertEqual({p.name for p in stage.iterdir()},set(base))
