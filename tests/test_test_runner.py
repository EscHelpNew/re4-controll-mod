"""Behavioral checks for bounded, isolated host-test orchestration."""
import contextlib
import gzip
import io
import importlib.util
import json
import os
import shutil
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
if importlib.util.find_spec('tools.test_runner'):
    from tools import test_runner as runner
    from tools.test_catalog import NATIVE_CASES, NativeCase
else:
    runner = None
    NATIVE_CASES = ()

try:
    from tools import test_process
except ImportError:
    test_process = None


class TestRunnerTests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(runner, 'parallel host-test runner is required')
        self.temp = tempfile.TemporaryDirectory(prefix='re4 runner test ')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.run = self.root / 'result'
        self.run.mkdir()

    def case(self, name='example', **kwargs):
        return NativeCase(name=name, sources=('tests/example.cpp',), **kwargs)

    def native(self, case, coverage=False):
        return runner._native_case(case, self.root, self.run, 'compiler.exe', coverage)

    @staticmethod
    def logged(argv, *, returncode=0, out='', err='', timed_out=False, **kwargs):
        for path, content in ((kwargs['stdout_path'], out), (kwargs['stderr_path'], err)):
            Path(path).parent.mkdir(parents=True, exist_ok=True)
            Path(path).write_text(content)
        return dict(command=argv, cwd=str(kwargs['cwd']), timeout=kwargs['timeout'],
                    stdout=str(kwargs['stdout_path']), stderr=str(kwargs['stderr_path']),
                    returncode=returncode, timed_out=timed_out, seconds=0.)

    def test_catalog_retains_every_committed_native_spec(self):
        expected = {p.stem for p in (ROOT / 'tests').glob('*_tests.cpp')} | {'installer_preview'}
        self.assertEqual({c.name for c in NATIVE_CASES}, expected)
        self.assertEqual(len(NATIVE_CASES), 109)
        self.assertEqual(sum(c.standard == 'c++17' for c in NATIVE_CASES), 38)
        self.assertEqual(sum(c.standard == 'c++20' for c in NATIVE_CASES), 71)
        for case in NATIVE_CASES:
            self.assertEqual(case.sources[0], 'tests/' + case.name + '.cpp')

    def test_catalog_preserves_special_libraries_and_generated_inputs(self):
        cases = {c.name: c for c in NATIVE_CASES}
        self.assertEqual(cases['telemetry_transfer_tests'].libraries, ('advapi32', 'ws2_32'))
        self.assertEqual(cases['live_debug_transport_tests'].libraries, ('ws2_32',))
        self.assertEqual(cases['installer_preview'].libraries, ('SDL2',))
        self.assertEqual({c.name for c in NATIVE_CASES if c.graphics},
                         {'installer_preview', 're6_overlay_ui_tests', 'overlay_ui_tests', 'profile_ui_tests'})
        self.assertEqual(cases['actual_home_tests'].runtime_inputs, ('assets/home-launcher.bin',))
        self.assertEqual({c.name for c in NATIVE_CASES if c.c_sources}, {'update_tests', 'update_worker_tests'})

    def test_real_pool_is_parallel_and_bounded_for_each_supported_size(self):
        for jobs in (2, 3, 4):
            barrier = threading.Barrier(jobs)
            lock = threading.Lock()
            active = maximum = 0
            def work(index):
                nonlocal active, maximum
                with lock:
                    active += 1
                    maximum = max(maximum, active)
                if index < jobs:
                    barrier.wait(timeout=3)
                time.sleep(.01)
                with lock:
                    active -= 1
                return index
            calls = [lambda i=i: work(i) for i in range(jobs * 2)]
            self.assertEqual(runner._collect(calls, jobs), list(range(jobs * 2)))
            self.assertEqual(maximum, jobs)

    def test_fast_completed_case_is_reported_before_slow_first_case(self):
        release = threading.Event()
        completed = []
        def slow():
            self.assertTrue(release.wait(timeout=2), 'completion callback was blocked behind first case')
            return 'slow'
        def record(index, result):
            completed.append((index, result))
            if index == 1:
                release.set()
        self.assertEqual(runner._collect([slow, lambda: 'fast'], 2, completed=record), ['slow', 'fast'])
        self.assertEqual(completed[0], (1, 'fast'))

    def test_invalid_pool_sizes_rejected_before_output_creation(self):
        for jobs in (0, 1, 5, -2):
            with self.assertRaises(ValueError):
                runner.run_tests(self.root, jobs=jobs)
        self.assertFalse((self.root / 'build').exists())

    def test_coverage_cli_requires_test_mode(self):
        result = subprocess.run([sys.executable, str(ROOT / 'build.py'), '--coverage'],
                                capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 2)
        self.assertIn('--coverage requires --test', result.stderr)

    def test_pool_submission_interrupt_cancels_owned_processes_and_pending_futures(self):
        from unittest.mock import MagicMock
        future = MagicMock()
        pool = MagicMock()
        pool.__enter__.return_value = pool
        pool.submit.side_effect = [future, KeyboardInterrupt()]
        scope = MagicMock()
        with patch.object(runner, 'ThreadPoolExecutor', return_value=pool), \
                patch.object(runner, 'ProcessScope', return_value=scope, create=True):
            with self.assertRaises(KeyboardInterrupt):
                runner._collect([lambda: 1, lambda: 2], 2)
        scope.cancel.assert_called_once()
        self.assertEqual(pool.submit.call_args_list[0].args[0], scope.run)
        future.cancel.assert_called_once()

    def test_compile_and_run_have_owned_cwd_tmp_and_logs(self):
        before = dict(os.environ)
        calls = []
        def execute(argv, **kwargs):
            calls.append((argv, kwargs))
            return self.logged(argv, out='output', err='warning', **kwargs)
        with patch.object(runner, 'run_logged', side_effect=execute):
            result = self.native(self.case())
        self.assertEqual(result['status'], 'passed')
        compile_call, run_call = calls
        self.assertEqual(Path(compile_call[1]['cwd']).name, 'objects')
        self.assertEqual(Path(run_call[1]['cwd']).name, 'work')
        self.assertTrue((Path(run_call[1]['cwd']) / 'build').is_dir())
        self.assertEqual(run_call[1]['timeout'], 20)
        self.assertGreaterEqual(compile_call[1]['timeout'], 120)
        for key in ('TEMP', 'TMP', 'TMPDIR'):
            self.assertEqual(Path(run_call[1]['env'][key]).parent.name, 'example')
        self.assertEqual(dict(os.environ), before)
        self.assertEqual(Path(result['steps'][0]['stderr']).read_text(), 'warning')

    def test_compile_failure_does_not_launch_stale_executable(self):
        with patch.object(runner, 'run_logged', side_effect=lambda argv, **kw: self.logged(argv, returncode=7, err='bad compile', **kw)) as call:
            result = self.native(self.case())
        self.assertEqual(result['status'], 'compile_failed')
        self.assertEqual(call.call_count, 1)
        self.assertEqual(result['steps'][0]['returncode'], 7)

    def test_run_failure_is_retained(self):
        codes = iter((0, 9))
        with patch.object(runner, 'run_logged', side_effect=lambda argv, **kw: self.logged(argv, returncode=next(codes), out='failure', **kw)):
            result = self.native(self.case())
        self.assertEqual(result['status'], 'run_failed')
        self.assertEqual(result['steps'][-1]['returncode'], 9)

    def test_timeout_and_missing_compiler_have_logs(self):
        for failure, timed_out in (('timeout', True), ('compiler missing', False)):
            with self.subTest(failure=failure), patch.object(runner, 'run_logged', side_effect=lambda argv, **kw:
                    self.logged(argv, returncode=None, err=failure, timed_out=timed_out, **kw)):
                result = self.native(self.case(name=failure.replace(' ', '_')))
            self.assertIn(result['status'], ('timed_out', 'compile_failed'))
            self.assertTrue(Path(result['steps'][0]['stderr']).read_text())

    def test_update_c_objects_and_coverage_are_owned_per_executable(self):
        cases = [c for c in NATIVE_CASES if c.c_sources]
        commands = []
        with patch.object(runner, 'run_logged', side_effect=lambda argv, **kw:
                          commands.append(argv) or self.logged(argv, **kw)):
            for case in cases:
                self.assertEqual(self.native(case, True)['status'], 'passed')
        objects = [Path(c[c.index('-o') + 1]) for c in commands if '-std=c11' in c]
        self.assertEqual(len(objects), 2)
        self.assertNotEqual(objects[0], objects[1])
        for command in commands:
            if any(x.startswith('-std=') for x in command):
                self.assertIn('--coverage', command)
                self.assertIn('-O0', command)

    def test_coverage_compiles_same_basename_sources_to_distinct_objects(self):
        case = NativeCase('twins', ('source/settings.cpp', 'source/re6/settings.cpp'))
        commands = []
        with patch.object(runner, 'run_logged', side_effect=lambda argv, **kw:
                          commands.append(argv) or self.logged(argv, **kw)):
            self.assertEqual(self.native(case, True)['status'], 'passed')
        compiles = [c for c in commands if '-c' in c]
        self.assertEqual(len(compiles), 2)
        objects = [c[c.index('-o') + 1] for c in compiles]
        self.assertEqual(len(set(objects)), 2)
        link = commands[-2]
        self.assertTrue(all(p in link for p in objects))
        self.assertFalse(any(p.endswith('.cpp') for p in link))
        for command in compiles:
            self.assertIn('-MMD', command)
            self.assertNotIn('-loleaut32', command)

    def test_coverage_only_real_definition_dependencies_preserve_normal_recipes(self):
        for name in ('modern_move_tests', 'live_debug_runtime_tests'):
            case = next(c for c in NATIVE_CASES if c.name == name)
            commands = []
            with patch.object(runner, 'run_logged', side_effect=lambda argv, **kw:
                              commands.append(argv) or self.logged(argv, **kw)):
                self.assertEqual(self.native(case)['status'], 'passed')
                normal = commands[:]
                commands.clear()
                self.assertEqual(self.native(case, True)['status'], 'passed')
            definition = str(self.root / 'source/settings.cpp')
            self.assertFalse(any(definition in c for c in normal))
            self.assertTrue(any(definition in c and '-c' in c for c in commands))

    def test_gcov_profile_errors_fail_case_even_when_native_exit_is_zero(self):
        for stream in ('out', 'err'):
            with self.subTest(stream=stream):
                def execute(argv, **kwargs):
                    text = 'libgcov profiling error:counter.gcda:overwriting an existing profile data with a different checksum'
                    return self.logged(argv, **({stream: text} if len(argv) == 1 else {}), **kwargs)
                with patch.object(runner, 'run_logged', side_effect=execute):
                    result = self.native(self.case('gcov_' + stream), True)
                self.assertEqual(result['status'], 'coverage_failed')
                self.assertIn('libgcov', result['error'])

    def test_actual_same_basename_translation_units_keep_both_counter_sets(self):
        cxx = os.environ.get('CXX') or shutil.which('g++')
        if not cxx:
            self.skipTest('host GCC is unavailable')
        env = test_process.tool_environment(os.environ.copy(), cxx)
        gcov = Path(cxx).with_name(Path(cxx).name.replace('g++', 'gcov'))
        if not gcov.is_file():
            self.skipTest('matching host gcov is unavailable')
        for name, body in (('a/value.cpp', 'int left_value(){return 7;}'),
                           ('b/value.cpp', 'int right_value(){return 9;}'),
                           ('main.cpp', 'int left_value();int right_value();int main(){return left_value()+left_value()+right_value()==23?0:1;}')):
            target = self.root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(body)
        case = NativeCase('real_twins', ('a/value.cpp', 'b/value.cpp', 'main.cpp'))
        result = runner._native_case(case, self.root, self.run, cxx, True)
        self.assertEqual(result['status'], 'passed', result)
        folder = self.run / 'native/real_twins'
        for index, source in enumerate(case.sources):
            target = runner._object_path(folder, source)
            self.assertTrue(target.with_suffix('.gcda').is_file())
            step = test_process.run_logged([str(gcov), '--json-format', str(target)], cwd=folder,
                env=env, stdout_path=folder / f'gcov-{index}.stdout', stderr_path=folder / f'gcov-{index}.stderr', timeout=20)
            self.assertEqual(step['returncode'], 0, step)
        files = {}
        for report in folder.glob('*.gcov.json.gz'):
            with gzip.open(report, 'rt') as stream:
                for entry in json.load(stream)['files']:
                    files[Path(entry['file']).resolve()] = entry
        for relative, expected in (('a/value.cpp', 2), ('b/value.cpp', 1)):
            entry = files[(self.root / relative).resolve()]
            self.assertEqual(len(entry['functions']), 1)
            self.assertEqual(entry['functions'][0]['execution_count'], expected)
            self.assertTrue(any(line['count'] == expected for line in entry['lines']))

    def test_owned_runtime_asset_is_copied(self):
        source = self.root / 'assets/home-launcher.bin'
        source.parent.mkdir()
        source.write_bytes(b'owned fixture')
        with patch.object(runner, 'run_logged', side_effect=self.logged):
            result = self.native(self.case(runtime_inputs=('assets/home-launcher.bin',)))
        self.assertEqual(result['status'], 'passed')
        self.assertEqual((self.run / 'native/example/work/assets/home-launcher.bin').read_bytes(), b'owned fixture')
        self.assertEqual(source.read_bytes(), b'owned fixture')

    def test_missing_runtime_asset_is_explicit_setup_failure(self):
        with patch.object(runner, 'run_logged') as call:
            result = self.native(self.case(runtime_inputs=('assets/missing',)))
        self.assertEqual(result['status'], 'setup_failed')
        call.assert_not_called()

    def test_all_results_survive_one_worker_exception(self):
        cases = (self.case(name='bad'), self.case(name='good'))
        def native(case, *args):
            if case.name == 'bad':
                raise RuntimeError('unexpected worker failure')
            return dict(name=case.name, kind='native', status='passed', steps=[])
        python = dict(name='python', kind='python', status='passed', steps=[])
        with contextlib.redirect_stdout(io.StringIO()), \
                patch.object(runner, 'NATIVE_CASES', cases), patch.object(runner, '_prepare', return_value=[]), \
                patch.object(runner, '_python_case', return_value=python), patch.object(runner, '_native_case', side_effect=native):
            self.assertEqual(runner._suite(self.root, self.run, 2, False), 1)
        result = json.loads((self.run / 'results.json').read_text())
        self.assertEqual([c['name'] for c in result['cases']], ['python', 'bad', 'good'])
        self.assertEqual(result['passed'], 2)
        self.assertEqual(result['failed'], 1)
        self.assertEqual(result['cases'][1]['status'], 'worker_failed')
        self.assertEqual(json.loads((self.run / 'native/good/result.json').read_text())['status'], 'passed')
        self.assertEqual(json.loads((self.run / 'progress.json').read_text())['completed'], 3)

    def test_preparation_failure_skips_only_dependent_cases(self):
        cases = (self.case(name='graphics', graphics=True), self.case(name='plain'))
        preparations = [dict(name='graphics', status='setup_failed', error='bad image')]
        python = dict(name='python', kind='python', status='passed', steps=[])
        def native(case, *args):
            return dict(name=case.name, kind='native', status='passed', steps=[])
        with contextlib.redirect_stdout(io.StringIO()), \
                patch.object(runner, 'NATIVE_CASES', cases), patch.object(runner, '_prepare', return_value=preparations), \
                patch.object(runner, '_python_case', return_value=python), patch.object(runner, '_native_case', side_effect=native):
            self.assertEqual(runner._suite(self.root, self.run, 2, False), 1)
        result = json.loads((self.run / 'results.json').read_text())
        self.assertEqual([c['status'] for c in result['cases']], ['passed', 'skipped_dependency', 'passed'])

    def test_case_runtime_paths_do_not_collide(self):
        with patch.object(runner, 'run_logged', side_effect=self.logged):
            a, b = runner._collect([lambda: self.native(self.case('first')),
                                    lambda: self.native(self.case('second'))], 2)
        self.assertNotEqual(a['steps'][-1]['cwd'], b['steps'][-1]['cwd'])

    def test_coverage_child_requires_owned_directory_and_marker(self):
        outside = self.root / 'outside'
        outside.mkdir()
        for folder in (outside, self.root / 'build/tests/not-owned'):
            folder.mkdir(parents=True, exist_ok=True)
            with patch.dict(os.environ, {runner._CHILD_RUN: str(folder)}):
                with self.assertRaises((ValueError, FileNotFoundError)):
                    runner._child_run(self.root)
        owned = runner._new_run(self.root)
        with patch.dict(os.environ, {runner._CHILD_RUN: str(owned)}):
            self.assertEqual(runner._child_run(self.root), owned)

    def test_preview_rejects_absent_blank_and_wrong_size_frames(self):
        from PIL import Image
        directory = self.root / 'images'
        directory.mkdir()
        with self.assertRaises(RuntimeError):
            runner._validate_preview(directory)
        path = directory / 'installer-blank.bmp'
        for size in ((1280, 720), (10, 10)):
            Image.new('RGB', size).save(path)
            with self.assertRaises(RuntimeError):
                runner._validate_preview(directory)

    def test_run_entry_uses_unique_runs_and_records_exit_code(self):
        with patch.object(runner, '_suite', return_value=1) as suite:
            self.assertEqual(runner.run_tests(self.root), 1)
            first = suite.call_args.args[1]
            self.assertEqual(runner.run_tests(self.root), 1)
            second = suite.call_args.args[1]
        self.assertNotEqual(first, second)
        latest = json.loads((self.root / 'build/tests/latest.json').read_text())
        self.assertEqual(latest['exit_code'], 1)
        self.assertEqual(latest['run_dir'], str(second))

    def test_prepare_copies_graphics_and_generates_both_bitmaps_once(self):
        source = self.root / 'assets/graphics/source.bin'
        source.parent.mkdir(parents=True)
        source.write_bytes(b'original')
        with patch.object(runner, 'prepare_graphics') as graphics, patch.object(runner, 'prepare_bitmap') as bitmaps:
            result = runner._prepare(self.root, self.run)
        self.assertEqual([r['status'] for r in result], ['passed', 'passed'])
        self.assertEqual(graphics.call_count, 1)
        self.assertEqual(bitmaps.call_count, 2)
        self.assertEqual((self.run / 'shared/assets/graphics/source.bin').read_bytes(), b'original')
        self.assertFalse((self.root / 'build/graphics').exists())

    def test_preparation_failures_are_reported_independently(self):
        with patch.object(runner, 'prepare_bitmap', side_effect=ValueError('bad bitmap')):
            result = runner._prepare(self.root, self.run)
        self.assertEqual([r['status'] for r in result], ['setup_failed', 'setup_failed'])
        self.assertIn('bad bitmap', result[1]['error'])

    def test_python_discovery_keeps_all_specs_and_owned_environment(self):
        with patch.object(runner, 'run_logged', side_effect=lambda argv, **kw: self.logged(argv, out='71 specs', **kw)) as call:
            result = runner._python_case(self.root, self.run, 'selected-g++.exe')
        self.assertEqual(result['status'], 'passed')
        args, kw = call.call_args
        self.assertIn(str(self.root / 'tests'), args[0])
        self.assertEqual(kw['env']['CXX'], 'selected-g++.exe')
        self.assertTrue(kw['env']['PYTHONPATH'].startswith(str(self.root)))
        self.assertEqual(Path(kw['cwd']), self.run / 'python/work')

    def test_preview_copies_generated_logos_and_sets_child_sdl_environment(self):
        folder = self.run / 'shared/build'
        folder.mkdir(parents=True)
        for name in ('app-logo.bmp', 'app-logo-re6.bmp'):
            (folder / name).write_bytes(name.encode())
        calls = []
        with patch.object(runner, 'run_logged', side_effect=lambda argv, **kw:
                          calls.append(kw) or self.logged(argv, **kw)), \
                patch.object(runner, '_validate_preview') as validate:
            result = self.native(self.case(name='installer_preview'))
        self.assertEqual(result['status'], 'passed')
        self.assertEqual(calls[-1]['env']['SDL_VIDEODRIVER'], 'dummy')
        self.assertEqual((self.run / 'native/installer_preview/work/build/app-logo-re6.bmp').read_bytes(), b'app-logo-re6.bmp')
        validate.assert_called_once()

    @unittest.skipUnless(importlib.util.find_spec('coverage'), 'optional coverage.py is not installed')
    def test_actual_coverage_wrapper_measures_build_entry_and_python_child(self):
        tools = self.root / 'tools'
        tools.mkdir()
        (tools / '__init__.py').write_text('')
        (tools / 'example.py').write_text('def first():\n    return 1\ndef child():\n    return 2\n')
        (tools / 'unimported.py').write_text('def not_imported():\n    return 42\n')
        alias_child = ('import importlib.util;'
                       f'spec=importlib.util.spec_from_file_location("different_build_alias",{str(self.root / "build.py")!r});'
                       'alias=importlib.util.module_from_spec(spec);spec.loader.exec_module(alias);alias.aliased_only()')
        (self.root / 'build.py').write_text(
            'import json,os,subprocess,sys,importlib.util\nfrom pathlib import Path\n'
            'def aliased_only():\n    return "measured alias body"\n'
            'if __name__ == "__main__":\n'
            '    spec=importlib.util.spec_from_file_location("different_build_alias",__file__)\n'
            '    alias=importlib.util.module_from_spec(spec);spec.loader.exec_module(alias)\n'
            f'    subprocess.run([sys.executable,"-c",{alias_child!r}],check=True)\n'
            '    from tools.example import first\n    first()\n'
            '    subprocess.run([sys.executable,"-c","from tools.example import child; child()"],check=True)\n'
            '    folder=Path(os.environ["RE4_TEST_COVERAGE_RUN"])\n'
            '    (folder/"results.json").write_text(json.dumps({"cases":[],"failed":0}))\n')
        with contextlib.redirect_stdout(io.StringIO()), \
                patch('tools.native_coverage.report_native', return_value={'complete': True}), \
                patch('tools.coverage_gate.check_coverage', return_value=0) as gate:
            self.assertEqual(runner._coverage_run(self.root, self.run, 2), 0)
        report = json.loads((self.run / 'coverage/python.json').read_text())
        report['files'] = {name.replace('\\', '/'): value for name, value in report['files'].items()}
        self.assertGreaterEqual(len(report['files']['build.py']['executed_lines']), 6)
        self.assertIn(4, report['files']['build.py']['executed_lines'])
        self.assertIn('tools/example.py', report['files'], list(report['files']))
        self.assertEqual(report['files']['tools/example.py']['missing_lines'], [])
        self.assertEqual(report['files']['tools/unimported.py']['executed_lines'], [])
        self.assertEqual(report['files']['tools/unimported.py']['missing_lines'], [1, 2])
        self.assertTrue((self.run / 'coverage/python/html/index.html').is_file())
        self.assertEqual(gate.call_args.args[1], self.run / 'coverage/native-summary.json')

    def test_coverage_report_error_preserves_suite_results(self):
        (self.root / 'build.py').write_text('# measured entry\n')
        (self.run / 'results.json').write_text(json.dumps({'cases': [{'name': 'kept'}], 'failed': 0}))
        with contextlib.redirect_stdout(io.StringIO()), \
                patch.object(runner, 'run_logged', side_effect=self.logged), \
                patch('tools.native_coverage.report_native', side_effect=RuntimeError('missing gcovr')):
            self.assertEqual(runner._coverage_run(self.root, self.run, 4), 1)
        result = json.loads((self.run / 'results.json').read_text())
        self.assertEqual(result['cases'], [{'name': 'kept'}])
        self.assertFalse(result['coverage_reports']['passed'])
        self.assertIn('missing gcovr', result['coverage_reports']['native']['error'])

    def test_coverage_snapshot_precedes_suite_and_changed_inputs_block_reports(self):
        entry = self.root / 'build.py'
        entry.write_text('# original input\n')
        manifest = self.run / 'coverage/source-manifest.json'
        calls = []
        def execute(argv, **kwargs):
            self.assertTrue(manifest.is_file(), 'input identity was captured after the measured suite')
            calls.append(argv)
            entry.write_text('# changed while suite ran\n')
            return self.logged(argv, **kwargs)
        with contextlib.redirect_stdout(io.StringIO()), patch.object(runner, 'run_logged', side_effect=execute), \
                patch('tools.native_coverage.report_native') as native, patch('tools.coverage_gate.check_coverage') as gate:
            self.assertEqual(runner._coverage_run(self.root, self.run, 2), 1)
        self.assertEqual(len(calls), 1)
        native.assert_not_called()
        gate.assert_not_called()
        self.assertIn('Coverage inputs changed', json.loads((self.run / 'results.json').read_text())['coverage_reports']['native']['error'])

    def test_logged_process_preserves_output_and_persists_command_before_launch(self):
        self.assertIsNotNone(test_process, 'owned process-tree helper is required')
        out, err = self.run / 'positive.stdout', self.run / 'positive.stderr'
        code = (f'import sys;from pathlib import Path;assert Path({str(out.with_suffix(".command.json"))!r}).is_file();'
                'print("actual stdout",flush=True);print("actual stderr",file=sys.stderr,flush=True)')
        step = test_process.run_logged([sys.executable, '-c', code], cwd=self.root, env=os.environ.copy(),
                                       stdout_path=out, stderr_path=err, timeout=5)
        self.assertEqual(step['returncode'], 0)
        self.assertFalse(step['timed_out'])
        self.assertEqual(out.read_text().strip(), 'actual stdout')
        self.assertEqual(err.read_text().strip(), 'actual stderr')
        command = json.loads(Path(step['command_file']).read_text())
        self.assertEqual(command['command'], [sys.executable, '-c', code])
        self.assertEqual(command['cwd'], str(self.root))

    def test_missing_executable_is_recorded_without_losing_logs(self):
        step = test_process.run_logged([str(self.root / 'missing.exe')], cwd=self.root, env=os.environ.copy(),
                                       stdout_path=self.run / 'missing.stdout', stderr_path=self.run / 'missing.stderr', timeout=1)
        self.assertIsNone(step['returncode'])
        self.assertIn('error', step)
        self.assertTrue(Path(step['stderr']).read_text())
        self.assertTrue(Path(step['result_file']).is_file())

    def test_tool_environment_prepends_matching_compiler_bin_and_preserves_suffix(self):
        compiler = self.root / 'matching toolchain/bin/g++.exe'
        compiler.parent.mkdir(parents=True)
        compiler.write_bytes(b'compiler fixture')
        environment = dict(PATH='unrelated-ucrt/bin' + os.pathsep + 'user/bin', KEEP='unchanged')
        actual = test_process.tool_environment(environment, str(compiler))
        self.assertEqual(actual['PATH'], str(compiler.parent) + os.pathsep + environment['PATH'])
        self.assertEqual(actual['KEEP'], 'unchanged')
        self.assertEqual(environment['PATH'], 'unrelated-ucrt/bin' + os.pathsep + 'user/bin')

    def test_native_and_python_stages_use_selected_compiler_environment(self):
        compiler = self.root / 'matching/bin/g++.exe'
        compiler.parent.mkdir(parents=True)
        compiler.write_bytes(b'compiler fixture')
        environments = []
        def logged(argv, **kwargs):
            environments.append(kwargs['env'])
            return self.logged(argv, **kwargs)
        with patch.object(runner, 'run_logged', side_effect=logged):
            self.assertEqual(runner._native_case(self.case(), self.root, self.run, str(compiler), False)['status'], 'passed')
            self.assertEqual(runner._python_case(self.root, self.run, str(compiler))['status'], 'passed')
        self.assertEqual(len(environments), 3)
        for environment in environments:
            self.assertEqual(environment['PATH'].split(os.pathsep)[0], str(compiler.parent))

    @unittest.skipUnless(os.name == 'nt', 'Windows retained inline wrapper link dependency')
    def test_coverage_only_retained_windows_inline_link_dependency(self):
        commands = []
        def logged(argv, **kwargs):
            commands.append(argv)
            return self.logged(argv, **kwargs)
        with patch.object(runner, 'run_logged', side_effect=logged):
            self.assertEqual(self.native(self.case('normal'), False)['status'], 'passed')
            self.assertEqual(self.native(self.case('coverage'), True)['status'], 'passed')
        normal = next(c for c in commands if '-std=c++17' in c and '--coverage' not in c)
        coverage = next(c for c in commands if '-std=c++17' in c and '--coverage' in c and '-c' not in c)
        self.assertNotIn('-loleaut32', normal)
        self.assertIn('-loleaut32', coverage)
        object_path = str(runner._object_path(self.run / 'native/coverage', 'tests/example.cpp'))
        self.assertGreater(coverage.index('-loleaut32'), coverage.index(object_path))

    @unittest.skipUnless(os.name == 'nt', 'Windows descendant enumeration fallback')
    def test_descendant_enumeration_failure_still_terminates_our_parent(self):
        spawned = []
        original = subprocess.Popen
        def spawn(*args, **kwargs):
            process = original(*args, **kwargs)
            spawned.append(process)
            return process
        try:
            with patch.object(test_process, '_windows_children', side_effect=OSError('snapshot unavailable')), \
                    patch.object(test_process.subprocess, 'Popen', side_effect=spawn):
                step = test_process.run_logged([sys.executable, '-c', 'import time;time.sleep(30)'],
                    cwd=self.root, env=os.environ.copy(), stdout_path=self.run / 'enum.stdout',
                    stderr_path=self.run / 'enum.stderr', timeout=.2, kill_timeout=2)
            self.assertIsNotNone(spawned[0].poll())
            self.assertFalse(step['tree_terminated'])
            self.assertIn('snapshot unavailable', step['termination_error'])
        finally:
            for process in spawned:
                if process.poll() is None:
                    process.kill()
                    process.wait(timeout=3)

    def test_process_cancellation_is_logged_and_owned_process_is_reaped(self):
        original_wait = subprocess.Popen.wait
        injected = False
        def interrupted(process, *args, **kwargs):
            nonlocal injected
            if not injected:
                injected = True
                raise KeyboardInterrupt()
            return original_wait(process, *args, **kwargs)
        with patch.object(test_process.subprocess.Popen, 'wait', interrupted):
            with self.assertRaises(KeyboardInterrupt):
                test_process.run_logged([sys.executable, '-c', 'import time;time.sleep(30)'],
                    cwd=self.root, env=os.environ.copy(), stdout_path=self.run / 'cancel.stdout',
                    stderr_path=self.run / 'cancel.stderr', timeout=2, kill_timeout=2)
        step = json.loads((self.run / 'cancel.result.json').read_text())
        self.assertTrue(step['cancelled'])
        self.assertFalse(self.process_alive(step['pid']))

    def test_real_timeout_kills_owned_grandchild_without_touching_unrelated_process(self):
        self.assertIsNotNone(test_process, 'owned process-tree helper is required')
        pid_file = self.run / 'grandchild.pid'
        child = ('import subprocess,sys,time;from pathlib import Path;'
                 'p=subprocess.Popen([sys.executable,"-c","import time;print(123,flush=True);time.sleep(30)"]);'
                 f'Path({str(pid_file)!r}).write_text(str(p.pid));'
                 'print("parent ready",flush=True);time.sleep(30)')
        unrelated = subprocess.Popen([sys.executable, '-c', 'import time;time.sleep(30)'],
                                     stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            start = time.monotonic()
            step = test_process.run_logged([sys.executable, '-c', child], cwd=self.root, env=os.environ.copy(),
                                           stdout_path=self.run / 'timeout.stdout', stderr_path=self.run / 'timeout.stderr',
                                           timeout=1, kill_timeout=2)
            self.assertLess(time.monotonic() - start, 5)
            self.assertTrue(step['timed_out'])
            self.assertIn('parent ready', (self.run / 'timeout.stdout').read_text())
            self.assertIsNone(unrelated.poll())
            self.assertTrue(pid_file.is_file())
            for pid in (step['pid'], int(pid_file.read_text())):
                self.assertFalse(self.process_alive(pid), f'owned process {pid} survived timeout')
        finally:
            unrelated.kill()
            unrelated.wait(timeout=3)

    @staticmethod
    def process_alive(pid):
        if os.name == 'nt':
            import ctypes
            api = ctypes.WinDLL('kernel32', use_last_error=True)
            api.OpenProcess.argtypes = (ctypes.c_uint32, ctypes.c_int, ctypes.c_uint32)
            api.OpenProcess.restype = ctypes.c_void_p
            api.WaitForSingleObject.argtypes = (ctypes.c_void_p, ctypes.c_uint32)
            api.CloseHandle.argtypes = (ctypes.c_void_p,)
            handle = api.OpenProcess(0x100000, 0, pid)
            if not handle:
                return False
            try:
                return api.WaitForSingleObject(handle, 0) == 258
            finally:
                api.CloseHandle(handle)
        try:
            os.kill(pid, 0)
            return True
        except ProcessLookupError:
            return False


if __name__ == '__main__':
    unittest.main()
