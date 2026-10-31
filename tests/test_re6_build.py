import copy
import unittest
from tools.re6_module import observer_permissions, sdk6_header


class Re6BuildTests(unittest.TestCase):
    def test_sdk6_manual_lookup_uses_the_actual_export_spelling(self):
        original = 'namespace nn::ro::detail {\n    extern rtld::lookup_global_t g_LookupGlobalManualFunctionPointer;\n}\n'
        fixed = sdk6_header(original)
        self.assertIn('g_LookupGlobalManualFunctionPointer asm("_ZN2nn2ro6detail36g_pLookupGlobalManualFunctionPointerE")', fixed)
        self.assertEqual(original.count('asm('), 0)
        with self.assertRaises(ValueError):
            sdk6_header('unexpected upstream revision')

    def test_observer_keeps_native_services_filesystem_and_scheduler(self):
        original = {
            'title_id': '0x01002a000cd48000', 'title_id_range_min': '0x0100000000010000',
            'title_id_range_max': '0x01ffffffffffffff', 'main_thread_priority': 44,
            'filesystem_access': {'permissions': '0x4000000000000000'},
            'service_access': ['hid', 'nvdrv', 'fsp-srv'], 'service_host': [],
            'kernel_capabilities': [
                {'type': 'kernel_flags', 'value': {'highest_thread_priority': 59, 'lowest_thread_priority': 28}},
                {'type': 'syscalls', 'value': {'svcQueryMemory': '0x06'}},
                {'type': 'debug_flags', 'value': {'allow_debug': True, 'force_debug': False}},
            ],
        }
        before = copy.deepcopy(original)
        result = observer_permissions(original)
        self.assertEqual(original, before)
        for field in ('filesystem_access', 'service_access', 'service_host', 'main_thread_priority'):
            self.assertEqual(result[field], original[field])
        calls = next(c['value'] for c in result['kernel_capabilities'] if c['type'] == 'syscalls')
        self.assertEqual(calls, {'svcQueryMemory': '0x06', 'svcMapProcessMemory': '0x74', 'svcUnmapProcessMemory': '0x75'})
        self.assertEqual(result['kernel_capabilities'][0], original['kernel_capabilities'][0])
        self.assertEqual(result['title_id_range_min'], result['title_id'])
        self.assertEqual(result['title_id_range_max'], result['title_id'])

    def test_rejects_unrelated_process_or_incomplete_permissions(self):
        for original in ({'title_id': '0x010099a00bc1e000'},
                         {'title_id': '0x01002a000cd48000', 'kernel_capabilities': []}):
            with self.assertRaises(ValueError):
                observer_permissions(original)


if __name__ == '__main__':
    unittest.main()
