from contextlib import redirect_stdout
from copy import deepcopy
from io import StringIO
import json
from pathlib import Path
import tempfile
import unittest

from tools.coverage_gate import CoverageError, assess_coverage, capture_coverage_inputs, check_coverage


class CoverageGateTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for directory in ('tools', 'source', 'include', 'reports'):
            (self.root / directory).mkdir()
        (self.root / 'build.py').write_text('\n'.join(f'value{i}={i}' for i in range(10)))
        (self.root / 'tools/helper.py').write_text('# Declarations only.\n')
        (self.root / 'source/main.cpp').write_text('\n'.join(f'int value{i}(){{return {i};}}' for i in range(10)))
        (self.root / 'include/types.hpp').write_text('struct Value {int value;};\n')
        (self.root / 'types.d').write_text('types.o: include/types.hpp\n')
        self.native = {'covered_lines': 9, 'total_lines': 10,
                       'host_branches': {'covered': 1, 'total': 7},
                       'target_baseline_branches': {'covered': 0, 'total': 11},
                       'completeness': {'complete': True, 'unmeasured_files': [],
                                        'expected_files': ['source/main.cpp', 'include/types.hpp'],
                                        'non_executable_files': {'include/types.hpp': {
                                            'reason': 'Compiler emitted declarations only.', 'artifacts': ['types.d']}}}}
        self.cpp = {'gcovr/format_version': '0.14', 'files': [{'file': 'source/main.cpp', 'lines': [
            {'line_number': i, 'count': int(i <= 9), 'branches': []} for i in range(1, 11)]}]}
        counts = {'covered_lines': 9, 'num_statements': 10, 'covered_branches': 1, 'num_branches': 4}
        zero = {'covered_lines': 0, 'num_statements': 0, 'covered_branches': 0, 'num_branches': 0}
        self.python = {'files': {'build.py': {'summary': counts}, 'tools/helper.py': {'summary': zero}},
                       'totals': deepcopy(counts)}
        self.manifest = capture_coverage_inputs(self.root)

    def assess(self, minimum=90):
        return assess_coverage(self.root, self.native, self.cpp, self.python, minimum,
                               source_manifest=self.manifest)

    def test_changed_source_and_test_inputs_reject_stale_reports(self):
        for filename in ('tools/helper.py', 'source/main.cpp', 'tests/check.cpp'):
            with self.subTest(filename=filename):
                path = self.root / filename
                path.parent.mkdir(parents=True, exist_ok=True)
                old = path.read_bytes() if path.exists() else None
                path.write_text('changed executable content\n')
                with self.assertRaisesRegex(CoverageError, 'inputs changed'):
                    self.assess()
                if old is None:
                    path.unlink()
                else:
                    path.write_bytes(old)
        self.assertTrue(self.assess()['passed'])

    def test_missing_or_malformed_manifest_fails_closed(self):
        for manifest in (None, {}, {'schema_version': 1, 'files': []},
                         {'schema_version': True, 'files': {}},
                         {'schema_version': 2, 'files': {}},
                         {'schema_version': 1, 'files': {'../outside.py': 'a' * 64}},
                         *({'schema_version': 1, 'files': {'build.py': digest}}
                           for digest in (None, 'bad', 'Z' * 64))):
            with self.subTest(manifest=manifest), self.assertRaises(CoverageError):
                assess_coverage(self.root, self.native, self.cpp, self.python,
                                source_manifest=manifest)

    def test_asset_dependency_changes_and_removed_source_reject_stale_reports(self):
        (self.root / 'assets').mkdir()
        asset = self.root / 'assets/test.bin'
        asset.write_bytes(b'original')
        (self.root / 'requirements-dev.txt').write_text('dependency==1.0\n')
        self.manifest = capture_coverage_inputs(self.root)
        self.assertTrue(self.assess()['passed'])
        for path in (asset, self.root / 'requirements-dev.txt', self.root / 'build.py'):
            with self.subTest(path=path.name):
                data = path.read_bytes()
                path.unlink()
                with self.assertRaises((CoverageError, OSError)):
                    self.assess()
                path.write_bytes(data + b'changed')
                with self.assertRaisesRegex(CoverageError, 'inputs changed'):
                    self.assess()
                path.write_bytes(data)

    def test_exact_line_threshold_is_independent_of_branch_percent(self):
        result = self.assess()
        self.assertTrue(result['passed'])
        self.assertEqual(result['combined']['line_percent'], 90)
        self.assertEqual(result['python']['branches']['percent'], 25)
        self.assertEqual(result['cpp']['host_branches'], {'covered': 1, 'total': 7})

    def test_each_language_must_pass_without_weighted_masking_or_rounding(self):
        self.cpp['files'][0]['lines'][9]['count'] = 1
        self.native['covered_lines'] = 10
        self.python['files']['build.py']['summary']['covered_lines'] = 8
        self.python['totals']['covered_lines'] = 8
        self.assertFalse(self.assess()['passed'])  # Combined90% cannot hide Python80%.
        self.assertFalse(self.assess(90.000001)['passed'])

    def test_normalizes_absolute_windows_and_relative_paths(self):
        self.cpp['files'][0]['file'] = str(self.root / 'source/main.cpp').replace('/', '\\')
        self.python['files'][str(self.root / 'build.py')] = self.python['files'].pop('build.py')
        self.assertTrue(self.assess()['passed'])

    def test_unmeasured_and_contradictory_native_census_fail(self):
        original = deepcopy(self.native)
        for change in ({'complete': False}, {'unmeasured_files': ['source/missing.cpp']},
                       {'expected_files': ['source/main.cpp']},
                       {'expected_files': ['source/main.cpp', 'source/main.cpp', 'include/types.hpp']},
                       {'non_executable_files': {}},
                       {'non_executable_files': {'include/types.hpp': {'reason': '', 'artifacts': []}}}):
            with self.subTest(change=change):
                self.native = deepcopy(original)
                self.native['completeness'].update(change)
                with self.assertRaises(CoverageError):
                    self.assess()

    def test_unknown_outside_and_duplicate_source_files_fail(self):
        for filename in ('tests/fake.cpp', '../outside.cpp', str(self.root.parent / 'outside.cpp')):
            with self.subTest(filename=filename):
                self.cpp['files'][0]['file'] = filename
                with self.assertRaises(CoverageError):
                    self.assess()
        self.cpp['files'][0]['file'] = 'source/main.cpp'
        self.cpp['files'].append(deepcopy(self.cpp['files'][0]))
        with self.assertRaises(CoverageError):
            self.assess()

    def test_native_raw_counts_must_match_summary_and_reject_manual_exclusion(self):
        self.native['covered_lines'] = 10
        with self.assertRaises(CoverageError):
            self.assess()
        self.native['covered_lines'] = 9
        self.cpp['files'][0]['lines'][0]['gcovr/excluded'] = True
        with self.assertRaises(CoverageError):
            self.assess()

    def test_noncode_or_zero_line_cannot_hide_exclusion(self):
        for extra in ({'gcovr/noncode': True}, {'line_number': 0}):
            with self.subTest(extra=extra):
                self.cpp['files'][0]['lines'][-1] = {'line_number': 10, 'count': 0,
                    'gcovr/excluded': True, **extra}
                self.native['total_lines'] = 9
                with self.assertRaisesRegex(CoverageError, 'Excluded'):
                    self.assess()
    def test_noncode_and_duplicate_instantiation_lines_do_not_inflate_counts(self):
        lines = self.cpp['files'][0]['lines']
        lines.append({'line_number': 0, 'count': 99})
        lines.append({'line_number': 11, 'count': 99, 'gcovr/noncode': True})
        lines.append({'line_number': 1, 'count': 5, 'function_name': 'another_instantiation'})
        self.assertEqual(self.assess()['cpp']['total_lines'], 10)

    def test_python_report_must_include_untested_sources(self):
        del self.python['files']['tools/helper.py']
        with self.assertRaisesRegex(CoverageError, 'omits project'):
            self.assess()

    def test_python_excluded_lines_cannot_remove_production_code(self):
        original = deepcopy(self.python)
        for location in ('line_list', 'file_count', 'total_count'):
            with self.subTest(location=location):
                self.python = deepcopy(original)
                if location == 'line_list':
                    self.python['files']['build.py']['excluded_lines'] = [10]
                elif location == 'file_count':
                    self.python['files']['build.py']['summary']['num_excluded'] = 1
                else:
                    self.python['totals']['num_excluded'] = 1
                with self.assertRaisesRegex(CoverageError, 'exclu'):
                    self.assess()

    def test_python_totals_and_real_line_counters_are_authoritative(self):
        self.python['totals']['percent_covered'] = 100
        self.python['files']['build.py']['summary']['covered_lines'] = 8
        with self.assertRaisesRegex(CoverageError, 'totals'):
            self.assess()
        self.python['totals']['covered_lines'] = 8
        self.assertFalse(self.assess()['passed'])

    def test_bad_threshold_and_counter_types_fail(self):
        for minimum in (True, '90', -1, 101, float('nan'), float('inf')):
            with self.subTest(minimum=minimum), self.assertRaises(CoverageError):
                self.assess(minimum)
        original = deepcopy(self.python)
        for key, bad in [('covered_lines', -1), ('num_statements', True),
                         ('covered_lines', 11), ('covered_branches', 5)]:
            self.python = deepcopy(original)
            self.python['files']['build.py']['summary'][key] = bad
            with self.subTest(key=key, bad=bad), self.assertRaises(CoverageError):
                self.assess()

    def test_zero_denominator_and_no_branches(self):
        self.python['files']['build.py']['summary'].update(covered_branches=0, num_branches=0)
        self.python['totals'].update(covered_branches=0, num_branches=0)
        self.assertIsNone(self.assess()['python']['branches']['percent'])
        self.native['covered_lines'] = 0
        self.cpp['files'][0]['lines'] = []
        with self.assertRaises(CoverageError):
            self.assess()

    def write_reports(self):
        directory = self.root / 'reports'
        for name, value in [('native-summary.json', self.native), ('native.json', self.cpp),
                            ('python.json', self.python), ('source-manifest.json', self.manifest)]:
            (directory / name).write_text(json.dumps(value), encoding='utf-8')
        return directory

    def test_check_writes_pass_and_failed_reports_and_escapes_html(self):
        directory = self.write_reports()
        with redirect_stdout(StringIO()) as output:
            result = check_coverage(self.root, directory / 'native-summary.json', directory / 'python.json', directory)
        self.assertEqual(result, 0)
        self.assertIn('cpp: 9/10', output.getvalue())
        self.assertTrue(json.loads((directory / 'summary.json').read_text())['passed'])
        self.assertIn('Python detail', (directory / 'index.html').read_text())
        (directory / 'native-summary.json').write_text('{broken')
        with redirect_stdout(StringIO()):
            self.assertEqual(check_coverage(self.root, directory / 'native-summary.json', directory / 'python.json', directory), 1)
        self.assertIn('error', json.loads((directory / 'summary.json').read_text()))

    def test_malformed_nested_report_is_a_recorded_failure(self):
        self.cpp['files'] = [None]
        directory = self.write_reports()
        with redirect_stdout(StringIO()):
            self.assertEqual(check_coverage(self.root, directory / 'native-summary.json', directory / 'python.json', directory), 1)
        self.assertFalse(json.loads((directory / 'summary.json').read_text())['passed'])

    def test_compiler_evidence_must_exist_and_empty_file_can_be_nonexecutable(self):
        self.cpp['files'].append({'file': 'include/types.hpp', 'lines': []})
        self.assertTrue(self.assess()['passed'])
        (self.root / 'types.d').unlink()
        with self.assertRaisesRegex(CoverageError, 'dependency artifact'):
            self.assess()

    def test_bad_nested_containers_and_empty_filenames_fail_closed(self):
        original = deepcopy(self.cpp)
        for invalid in ({'files': {}}, {'files': [{'file': '', 'lines': []}]},
                        {'files': [{'file': 'source/main.cpp', 'lines': {}}]},
                        {'files': [{'file': 'source/main.cpp', 'lines': [None]}]}):
            self.cpp = invalid
            directory = self.write_reports()
            with self.subTest(report=invalid), redirect_stdout(StringIO()):
                self.assertEqual(check_coverage(self.root, directory / 'native-summary.json', directory / 'python.json', directory), 1)
        self.cpp = original


if __name__ == '__main__':
    unittest.main()
