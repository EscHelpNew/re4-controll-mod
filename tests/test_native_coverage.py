"""Coverage collection must retain unexecuted product code and reject bad data."""
from pathlib import Path
import json
import os
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import MagicMock, patch

from tools import native_coverage as nc


class NativeCoverageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.run_dir = self.root / 'build/run'
        self.run_dir.mkdir(parents=True)

    def write(self, name, text='inline int value(){return 1;}\n'):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding='utf-8')
        return path

    def public_support(self, run_dir=None):
        run_dir = run_dir or self.run_dir
        self.write('third_party/exlaunch/source/lib.hpp', '// pristine SDK\n')
        self.write('third_party/exlaunch/source/lib/util/math/sign_extend.hpp', '// nested SDK\n')
        generator = Path(__file__).resolve().parents[1] / 'third_party/switch-debug-kit/host/project.py'
        self.write('third_party/switch-debug-kit/host/project.py', generator.read_text(encoding='utf-8'))
        self.write('project.release.json', json.dumps(dict(name='RE4 Controls', author='re4-controls',
                   title_id='0100E4C04E540000', sd_directory='re4-controls', host='127.0.0.1')))
        self.write(str((run_dir / 'shared/build/graphics/graphics_preview_images.hpp').relative_to(self.root)), '#pragma once\n')
        self.write(str((run_dir / 'shared/build/graphics/gyro_test_background_image.hpp').relative_to(self.root)), '#pragma once\n')

    def test_fresh_checkout_stages_only_private_headers_and_public_config(self):
        self.public_support()
        self.write('project.json', 'PRIVATE_LOCAL_CONFIG_MUST_NOT_BE_READ')
        before = (self.root / 'third_party/exlaunch/source/lib.hpp').read_bytes()
        support = nc.stage_target_support(self.root, self.run_dir)
        self.assertEqual((support / 'exlaunch/source/lib.hpp').read_bytes(), before)
        header = (support / 'config/reload_project.h').read_text()
        self.assertIn('0x0100E4C04E540000ULL', header)
        self.assertNotIn('PRIVATE_', header)
        for absent in ('data', 'build/config', 'build/game-sdk', 'build/re6-game-sdk', 'build/graphics'):
            self.assertFalse((self.root / absent).exists())
        for source in ('source/game/module.cpp', 'source/re6/module.cpp', 'source/diagnostic/main.cpp'):
            command = nc.target_command(self.root, self.root / 'sdk', source, self.run_dir / 'target/job', support=support)
            self.assertIn('-I' + str(support / 'config'), command)
            self.assertIn('-I' + str(self.run_dir / 'shared/build/graphics'), command)
            self.assertNotIn('-I' + str(self.root / 'build/config'), command)
        self.assertEqual((self.root / 'third_party/exlaunch/source/lib.hpp').read_bytes(), before)

    def test_support_configuration_falls_back_to_public_example_and_safe_defaults(self):
        self.public_support()
        (self.root / 'project.release.json').rename(self.root / 'project.example.json')
        support = nc.stage_target_support(self.root, self.run_dir)
        self.assertIn('0100E4C04E540000', (support / 'config/reload_project.h').read_text())
        (self.root / 'project.example.json').unlink()
        support = nc.stage_target_support(self.root, self.run_dir)
        self.assertIn('0x0000000000000000ULL', (support / 'config/reload_project.h').read_text())
        self.assertFalse((self.root / 'data').exists())

    def test_missing_pristine_headers_or_private_graphics_fail_explicitly(self):
        with self.assertRaisesRegex(nc.NativeCoverageError, 'headers'):
            nc.stage_target_support(self.root, self.run_dir)
        self.public_support()
        (self.run_dir / 'shared/build/graphics/graphics_preview_images.hpp').unlink()
        with self.assertRaisesRegex(nc.NativeCoverageError, 'graphics'):
            nc.stage_target_support(self.root, self.run_dir)

    def test_instrumentation_retains_uncalled_inline_and_static_functions(self):
        flags = nc.compile_flags()
        for required in ('--coverage', '-O0', '-g', '-fprofile-abs-path',
                         '-fkeep-inline-functions', '-fkeep-static-functions', '-MMD'):
            self.assertIn(required, flags)
        self.assertNotIn('-DNDEBUG', flags)

    def test_windows_coverage_link_resolves_retained_system_inline_imports(self):
        with patch.object(nc.os, 'name', 'nt'):
            self.assertEqual(nc.link_flags(), ('-loleaut32',))
        with patch.object(nc.os, 'name', 'posix'):
            self.assertEqual(nc.link_flags(), ())
        self.assertNotIn('-loleaut32', nc.compile_flags())

    def test_inventory_includes_source_headers_inc_but_not_vendor_or_tests(self):
        for name in ('source/app.cpp', 'source/runtime.inc', 'source/local.hpp', 'include/re4/a.hpp',
                     'tests/test.cpp', 'third_party/a.hpp', 'build/generated/a.hpp', 'tools/tool.py'):
            self.write(name)
        self.assertEqual(nc.source_inventory(self.root),
                         ['include/re4/a.hpp', 'source/app.cpp', 'source/local.hpp', 'source/runtime.inc'])
        self.assertEqual(nc.canonical_source(self.root, str(self.root / 'source/app.cpp').replace('/', '\\')),
                         'source/app.cpp')
        self.assertEqual(nc.canonical_source(self.root, 'include/re4/a.hpp'), 'include/re4/a.hpp')
        self.assertIsNone(nc.canonical_source(self.root, '../outside.cpp'))
        self.assertIsNone(nc.canonical_source(self.root, 'tests/test.cpp'))

    def test_matching_gcov_is_compiler_sibling_and_version_checked(self):
        compiler = self.write('sdk/bin/aarch64-none-elf-g++.exe', '')
        gcov = self.write('sdk/bin/aarch64-none-elf-gcov.exe', '')
        def run(args, **_):
            return subprocess.CompletedProcess(args, 0, '16.1.0\n' if '-dumpfullversion' in args else 'gcov (devkitA64) 16.1.0\n')
        with patch.object(nc.subprocess, 'run', side_effect=run):
            result = nc.toolchain(str(compiler))
        self.assertEqual(result['gcov'], str(gcov))
        self.assertEqual(result['version'], '16.1.0')
        with patch.object(nc.subprocess, 'run', return_value=subprocess.CompletedProcess([], 0, '14.2.0')):
            self.assertEqual(nc.toolchain(str(compiler))['version'], '14.2.0')
        gcov.unlink()
        with self.assertRaisesRegex(nc.NativeCoverageError, 'gcov'):
            nc.toolchain(str(compiler))

    def test_mismatching_or_unreadable_toolchains_fail(self):
        compiler = self.write('sdk/g++.exe', '')
        self.write('sdk/gcov.exe', '')
        outputs = [subprocess.CompletedProcess([], 0, '14.2.0'), subprocess.CompletedProcess([], 0, 'gcov 16.1.0')]
        with patch.object(nc.subprocess, 'run', side_effect=outputs):
            with self.assertRaisesRegex(nc.NativeCoverageError, 'version'):
                nc.toolchain(str(compiler))
        with patch.object(nc.subprocess, 'run', return_value=subprocess.CompletedProcess([], 1, 'bad compiler')):
            with self.assertRaises(nc.NativeCoverageError):
                nc.toolchain(str(compiler))
        with patch.object(nc.shutil, 'which', return_value=None):
            with self.assertRaises(nc.NativeCoverageError):
                nc.toolchain('no-such-compiler')
        with patch.object(nc.subprocess, 'run', side_effect=OSError('not executable')):
            with self.assertRaisesRegex(nc.NativeCoverageError, 'Cannot inspect'):
                nc.toolchain(str(compiler))

    def test_host_profiles_must_exist_and_be_current(self):
        directory = self.run_dir / 'native/job'
        directory.mkdir(parents=True)
        with self.assertRaisesRegex(nc.NativeCoverageError, 'gcno'):
            nc.check_profiles(directory)
        note = directory / 'case.gcno'; note.write_bytes(b'notes')
        with self.assertRaisesRegex(nc.NativeCoverageError, 'gcda'):
            nc.check_profiles(directory)
        data = directory / 'case.gcda'; data.write_bytes(b'counts')
        os.utime(note, ns=(200, 200)); os.utime(data, ns=(100, 100))
        with self.assertRaisesRegex(nc.NativeCoverageError, 'stale'):
            nc.check_profiles(directory)
        os.utime(data, ns=(300, 300))
        self.assertEqual(nc.check_profiles(directory), [note])
        diagnostic = directory / 'run.stderr'
        diagnostic.write_text('libgcov profiling error: overwriting an existing profile data with a different checksum')
        with self.assertRaisesRegex(nc.NativeCoverageError, 'profiling'):
            nc.check_profiles(directory)
        diagnostic.write_text('ordinary test diagnostics\n')
        self.assertEqual(nc.check_profiles(directory), [note])

    def test_dependency_paths_with_spaces_and_continuations_are_normalized(self):
        source = self.write('source/app.cpp')
        header = self.write('include/space name.hpp')
        dep = self.write('build/run/native/job/app.d',
                         'app.o: ' + str(source).replace(' ', '\\ ') + ' \\\n ' + str(header).replace(' ', '\\ ') + '\n')
        dependencies = nc.read_dependencies(self.root, [dep])
        self.assertEqual(set(dependencies), {'source/app.cpp', 'include/space name.hpp'})
        self.assertEqual(dependencies['include/space name.hpp'], [str(dep)])
        dep.write_text('not a dependency rule')
        with self.assertRaisesRegex(nc.NativeCoverageError, 'Malformed'):
            nc.read_dependencies(self.root, [dep])

    def test_completeness_keeps_uncompiled_platform_source_and_declared_only_headers(self):
        self.write('source/runtime.cpp'); self.write('source/switch_only.cpp')
        self.write('include/types.hpp', 'struct Types;\n')
        report = {'files': [{'file': 'source/runtime.cpp', 'lines': [{'line_number': 1, 'count': 1}]}]}
        inventory = nc.completeness(self.root, report, {'include/types.hpp': ['proof.d']})
        self.assertFalse(inventory['complete'])
        self.assertEqual(inventory['unmeasured_files'], ['source/switch_only.cpp'])
        self.assertEqual(inventory['files']['include/types.hpp']['status'], 'compiled-no-runtime-counters')
        self.assertEqual(inventory['files']['include/types.hpp']['artifacts'], ['proof.d'])
        inventory = nc.completeness(self.root, report,
                                    {'include/types.hpp': ['proof.d'], 'source/switch_only.cpp': ['target.d']})
        self.assertTrue(inventory['complete'])

    def test_statistics_counts_lines_once_and_reports_branch_outcomes_separately(self):
        report = {'files': [{'file': 'source/a.cpp', 'lines': [
            {'line_number': 10, 'count': 3, 'branches': [{'count': 0}, {'count': 2}]},
            {'line_number': 11, 'count': 0}]}]}
        self.assertEqual(nc.statistics(report),
                         {'covered_lines': 1, 'total_lines': 2, 'line_percent': 50.0,
                          'covered_branches': 1, 'total_branches': 2, 'branch_percent': 50.0})
        self.assertEqual(nc.statistics({'files': []})['line_percent'], 0)
        with self.assertRaises(nc.NativeCoverageError):
            nc.statistics({'files': [{'file': 'source/a.cpp', 'lines': [{'line_number': 1, 'count': -1}]}]})
        with self.assertRaises(nc.NativeCoverageError):
            nc.statistics({'files': [{'file': 'source/a.cpp', 'lines': [{'line_number': 1, 'count': 1}, {'line_number': 1, 'count': 0}]}]})
        instantiated = {'files': [{'file': 'source/a.cpp', 'lines': [
            {'line_number': 1, 'function_name': 'template<int>', 'count': 3},
            {'line_number': 1, 'function_name': 'template<float>', 'count': 0}]}]}
        self.assertEqual(nc.statistics(instantiated)['total_lines'], 1)
        self.assertEqual(nc.statistics(instantiated)['covered_lines'], 1)

    def test_target_commands_compile_only_original_sources_with_target_sdk(self):
        for name in ('source/game/module.cpp', 'source/re6/module.cpp', 'source/diagnostic/main.cpp'):
            self.write(name)
        self.public_support()
        support = nc.stage_target_support(self.root, self.run_dir)
        sdk = self.root / 'sdk'
        for source in ('source/game/module.cpp', 'source/re6/module.cpp', 'source/diagnostic/main.cpp'):
            cmd = nc.target_command(self.root, sdk, source, self.run_dir / 'target/job')
            self.assertIn('-c', cmd); self.assertNotIn('-lnx', cmd)
            self.assertIn(str(self.root / source), cmd)
            self.assertIn('--coverage', cmd)
            self.assertTrue(cmd[cmd.index('-o') + 1].endswith('baseline.o'))
            self.assertIn('-D__SWITCH__', cmd)
        cmd = nc.target_command(self.root, sdk, 'source/re6/module.cpp', self.run_dir / 'target/re6')
        self.assertIn('-D__RTLD_6XX__', cmd)
        self.assertIn('-DEXL_LOAD_KIND_ENUM=2', cmd)
        self.assertIn('-I' + str(support / 'exlaunch/source/lib'), cmd)
        overlay = nc.target_command(self.root, sdk, 'source/overlay/main.cpp', self.run_dir / 'target/overlay')
        self.assertNotIn('-fno-rtti', overlay)
        (support / 'exlaunch/source/lib.hpp').unlink()
        with self.assertRaisesRegex(nc.NativeCoverageError, 'staging'):
            nc.target_command(self.root, sdk, 'source/game/module.cpp', self.run_dir / 'target/job')

    def test_command_failure_and_timeout_leave_reviewable_logs(self):
        log = self.run_dir / 'command.log'
        def completed(*args, **kwargs):
            Path(kwargs['stdout_path']).write_text('ok')
            return {'returncode': 0, 'timed_out': False}
        with patch.object(nc, 'run_logged', side_effect=completed) as run:
            nc.run_checked(['compiler', '-c'], self.root, log)
        self.assertIn('ok', log.read_text())
        self.assertEqual(run.call_args.args[0], ['compiler', '-c'])
        self.assertEqual(run.call_args.kwargs['cwd'], self.root)
        self.assertEqual(run.call_args.kwargs['stderr_path'], log.with_suffix('.log.stderr'))
        log.write_text('compile failed')
        with patch.object(nc, 'run_logged', return_value={'returncode': 2, 'timed_out': False}):
            with self.assertRaisesRegex(nc.NativeCoverageError, 'command.log'):
                nc.run_checked(['compiler'], self.root, log)
        self.assertIn('compile failed', log.read_text())
        with patch.object(nc, 'run_logged', return_value={'returncode': 1, 'timed_out': True}):
            with self.assertRaisesRegex(nc.NativeCoverageError, 'timed out'):
                nc.run_checked(['compiler'], self.root, log)

    def test_python_gcov_collector_uses_gcov_dll_path_instead_of_python_path(self):
        with patch.object(nc, 'tool_environment', return_value={'PATH': 'matching gcov runtime'}) as environment, \
             patch.object(nc, 'run_logged', return_value={'returncode': 0, 'timed_out': False}) as run:
            nc.run_checked(['python', '-m', 'gcovr', '--gcov-executable', 'sdk/gcov.exe'],
                           self.root, self.run_dir / 'gcov.log')
        self.assertEqual(environment.call_args.args[1], 'sdk/gcov.exe')
        self.assertEqual(run.call_args.kwargs['env'], {'PATH': 'matching gcov runtime'})

    def test_target_phase_compiles_every_own_translation_unit_in_isolated_directories(self):
        for name in ('source/a.cpp', 'source/nested/b.cpp', 'source/nested/c.cpp'):
            self.write(name)
        target = {'compiler': 'arm-g++', 'gcov': 'arm-gcov', 'version': '16.1.0'}
        self.public_support()
        with patch.dict(os.environ, {'DEVKITPRO': str(self.root / 'sdk')}), \
             patch.object(nc, 'toolchain', return_value=target), patch.object(nc, 'run_checked') as run:
            directory, result = nc.target_baseline(self.root, self.run_dir, 2)
        self.assertEqual(result, target)
        commands = [json.loads(p.read_text()) for p in directory.rglob('command.json')]
        self.assertEqual(len(commands), 3)
        self.assertEqual(run.call_count, 3)
        outputs = [cmd[cmd.index('-o') + 1] for cmd in commands]
        self.assertEqual(len(set(outputs)), 3)
        self.assertTrue(all(Path(p).is_relative_to(self.run_dir) for p in outputs))
        with patch.dict(os.environ, {'DEVKITPRO': '/opt/devkitpro'}), \
             patch.object(nc, 'toolchain', return_value=target), \
             patch.object(nc, 'run_checked', side_effect=nc.NativeCoverageError('compile failed')):
            with self.assertRaisesRegex(nc.NativeCoverageError, 'compile failed'):
                nc.target_baseline(self.root, self.run_dir, 2)
        with patch.object(nc, 'toolchain', return_value=target), patch.object(nc, 'source_inventory', return_value=[]):
            with self.assertRaisesRegex(nc.NativeCoverageError, 'No own target'):
                nc.target_baseline(self.root, self.run_dir, 2)
        scope = MagicMock()
        scope.run.side_effect = lambda call, *args: call(*args)
        with patch.object(nc, 'toolchain', return_value=target), \
             patch.object(nc, 'run_checked', side_effect=KeyboardInterrupt), patch.object(nc, 'ProcessScope', return_value=scope):
            with self.assertRaises(KeyboardInterrupt):
                nc.target_baseline(self.root, self.run_dir, 2)
        scope.cancel.assert_called_once()
        self.assertGreaterEqual(scope.run.call_count, 1)
        pool = MagicMock(); submitted = MagicMock()
        pool.__enter__.return_value = pool
        pool.submit.side_effect = [submitted, KeyboardInterrupt]
        with patch.object(nc, 'toolchain', return_value=target), \
             patch.object(nc, 'ThreadPoolExecutor', return_value=pool), patch.object(nc, 'ProcessScope', return_value=scope):
            with self.assertRaises(KeyboardInterrupt):
                nc.target_baseline(self.root, self.run_dir, 2)
        self.assertEqual(scope.cancel.call_count, 2)
        self.assertEqual(pool.submit.call_args_list[0].args[0], scope.run)
        submitted.cancel.assert_called_once()

    def test_collector_uses_matching_gcov_and_only_requested_counter_tree(self):
        report = self.run_dir / 'coverage/host.json'
        log = self.run_dir / 'gcov.log'
        with patch.object(nc, 'run_checked') as run:
            nc.collect(self.root, self.run_dir / 'native', 'matching-gcov', report, log, 3)
        command = run.call_args.args[0]
        self.assertIn('matching-gcov', command)
        self.assertIn(str(self.run_dir / 'native'), command)
        self.assertIn('--no-markers', command)
        self.assertIn('3', command)
        with patch.object(nc, 'run_checked') as run:
            nc.collect(self.root, self.run_dir / 'native', r'C:\SDK with space\bin\gcov.exe', report, log, 3)
        command = run.call_args.args[0]
        self.assertEqual(shlex.split(command[command.index('--gcov-executable') + 1]),
                         ['C:/SDK with space/bin/gcov.exe'])

    @unittest.skipUnless(os.name == 'nt', 'Windows gcov executable path regression')
    def test_actual_gcovr_launches_gcov_from_windows_path_with_spaces(self):
        gcov = shutil.which('gcov')
        if not gcov:
            self.skipTest('gcov is not installed')
        relocated = self.root / 'SDK with spaces/bin/gcov.exe'
        relocated.parent.mkdir(parents=True)
        shutil.copy2(gcov, relocated)
        encoded = shlex.quote(str(relocated).replace('\\', '/'))
        code = ('import sys;from gcovr.formats.gcov.read import GcovProgram;'
                'GcovProgram(sys.argv[-1]).identify_and_cache_capabilities();print("gcov launched")')
        # The relocated executable still needs its matching compiler DLLs.
        with patch.dict(os.environ, nc.tool_environment(os.environ.copy(), gcov)):
            log = self.run_dir / 'actual-gcov.log'
            nc.run_checked([sys.executable, '-c', code, '--gcov-executable', encoded], self.root, log)
        self.assertIn('gcov launched', log.read_text())

    def test_normalize_report_rejects_vendor_and_unknown_source_paths(self):
        self.write('source/a.cpp')
        report = {'gcovr/format_version': '0.14', 'files': [
            {'file': str(self.root / 'source/a.cpp'), 'lines': [{'line_number': 1, 'count': 0}]},
            {'file': 'third_party/vendor.hpp', 'lines': []}]}
        result = nc.normalize_report(self.root, report)
        self.assertEqual([f['file'] for f in result['files']], ['source/a.cpp'])
        report['files'][0]['lines'] = [
            {'line_number': 1, 'function_name': 'template<int>', 'count': 3},
            {'line_number': 1, 'function_name': 'template<float>', 'count': 0}]
        result = nc.normalize_report(self.root, report)
        self.assertEqual(result['files'][0]['lines'], [{'line_number': 1, 'count': 3}])
        report['files'][0]['file'] = 'source/missing.cpp'
        with self.assertRaisesRegex(nc.NativeCoverageError, 'missing.cpp'):
            nc.normalize_report(self.root, report)

    def test_complete_report_has_honest_line_totals_and_proof_inventory(self):
        self.write('source/a.cpp')
        native = self.run_dir / 'native'; native.mkdir()
        (native / 'a.gcno').write_bytes(b'n'); (native / 'a.gcda').write_bytes(b'd')
        self.write('build/run/native/a.d', 'a.o: ' + str(self.root / 'source/a.cpp') + '\n')
        data = {'gcovr/format_version': '0.14', 'files': [{'file': 'source/a.cpp',
                'lines': [{'line_number': 1, 'count': 5, 'branches': [{'count': 1}]}, {'line_number': 2, 'count': 0}], 'functions': []}]}
        def collect(root, counters, gcov, output, log, jobs):
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_text(json.dumps(data))
        def merge(command, cwd, log, **kwargs):
            if '--json' in command:
                self.assertIn('--merge-mode-functions=separate', command)
                Path(command[command.index('--json') + 1]).write_text(json.dumps(data))
        with patch.object(nc, 'toolchain', return_value={'compiler': 'g++', 'gcov': 'gcov', 'version': '14.2.0'}), \
             patch.object(nc, 'target_baseline', return_value=(self.run_dir / 'target', {'gcov': 'arm-gcov', 'version': '16.1.0'})), \
             patch.object(nc, 'collect', side_effect=collect), patch.object(nc, 'run_checked', side_effect=merge):
            result = nc.report_native(self.root, self.run_dir, 'g++', jobs=2)
        self.assertEqual((result['covered_lines'], result['total_lines']), (1, 2))
        self.assertTrue(result['completeness']['complete'])
        self.assertEqual(result['host_branches']['covered_branches'], 1)
        self.assertEqual(result['target_baseline_branches']['total_branches'], 1)
        self.assertTrue((self.run_dir / 'coverage/native-summary.json').is_file())

    def test_invalid_job_counts_or_output_outside_private_build_fail(self):
        for jobs in (0, 1, 5):
            with self.assertRaises(nc.NativeCoverageError):
                nc.report_native(self.root, self.run_dir, 'g++', jobs=jobs)
        with self.assertRaises(nc.NativeCoverageError):
            nc.report_native(self.root, self.root / 'source/output', 'g++')
        with self.assertRaises(nc.NativeCoverageError):
            nc.report_native(self.root, self.root / 'build/releases/0.27.1', 'g++')


if __name__ == '__main__':
    unittest.main()
