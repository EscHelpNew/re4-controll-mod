"""Run the complete host suite with bounded workers and owned output trees."""
from concurrent.futures import ThreadPoolExecutor, as_completed
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile
import time

from tools.prepare_graphics import prepare as prepare_graphics
from tools.prepare_icon import prepare_bitmap
from tools.test_catalog import NATIVE_CASES
from tools.test_process import ProcessScope, run_logged, tool_environment

COMPILE_TIMEOUT = 300
RUN_TIMEOUT = 20
_CHILD_RUN = 'RE4_TEST_COVERAGE_RUN'


def _collect(calls, jobs, completed=None):
    if jobs not in (2, 3, 4):
        raise ValueError('Test jobs must be 2, 3 or 4')
    scope = ProcessScope()
    with ThreadPoolExecutor(max_workers=jobs) as pool:
        futures = []
        try:
            for call in calls:
                futures.append(pool.submit(scope.run, call))
            order = {future: index for index, future in enumerate(futures)}
            results = [None] * len(futures)
            for future in as_completed(futures):
                index = order[future]
                results[index] = future.result()
                if completed:
                    completed(index, results[index])
            return results
        except BaseException:
            for future in futures:
                future.cancel()
            scope.cancel()
            raise


def _command(argv, cwd, env, logs, name, timeout):
    return run_logged(argv, cwd=cwd, env=env, stdout_path=logs / (name + '.stdout'),
                      stderr_path=logs / (name + '.stderr'), timeout=timeout)


def _env(folder, root):
    env = os.environ.copy()
    for key in ('TEMP', 'TMP', 'TMPDIR'):
        env[key] = str(folder / 'tmp')
    env['PYTHONPATH'] = str(root) + os.pathsep + env.get('PYTHONPATH', '')
    env['PYTHONPYCACHEPREFIX'] = str(folder / 'pycache')
    return env


def _folders(folder):
    for name in ('objects', 'work/build', 'tmp'):
        (folder / name).mkdir(parents=True, exist_ok=True)


def _coverage_flags(enabled):
    if not enabled:
        return ()
    from tools.native_coverage import compile_flags
    return compile_flags()


def _coverage_link_flags(enabled):
    if not enabled:
        return ()
    from tools.native_coverage import link_flags
    return link_flags()


def _absolute(root, path):
    return str(root / path)


def _object_path(folder, source):
    # GCC derives its .gcno/.gcda names from the object output. Distinguish
    # translation units such as source/settings.cpp and source/re6/settings.cpp.
    key = hashlib.sha256(source.replace('\\', '/').encode()).hexdigest()[:16]
    return folder / 'objects' / (Path(source).stem + '-' + key + '.o')


def _native_case(case, root, run_dir, cxx, coverage):
    folder = run_dir / 'native' / case.name
    result = dict(name=case.name, kind='native', status='setup_failed', steps=[])
    start = time.monotonic()
    try:
        _folders(folder)
        env = tool_environment(_env(folder, root), cxx)
        for relative in case.runtime_inputs:
            destination = folder / 'work' / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(root / relative, destination)
        if case.name == 'installer_preview':
            for name in ('app-logo.bmp', 'app-logo-re6.bmp'):
                shutil.copyfile(run_dir / 'shared/build' / name, folder / 'work/build' / name)
            env['SDL_VIDEODRIVER'] = 'dummy'
            env['PATH'] = 'C:/msys64/mingw64/bin' + os.pathsep + env.get('PATH', '')
            env = tool_environment(env, cxx)
        flags = _coverage_flags(coverage)
        objects = []
        for index, source in enumerate(case.c_sources):
            target = _object_path(folder, source) if coverage else folder / 'objects' / (Path(source).stem + '.o')
            cc = str(Path(cxx).with_name(Path(cxx).name.replace('g++', 'gcc').replace('clang++', 'clang')))
            command = [cc, '-std=c11', '-I' + str(root / 'tests/update_stubs'),
                       '-I' + str(root / 'third_party/switch-debug-kit/receiver'), *flags,
                       '-c', _absolute(root, source), '-o', str(target)]
            step = _command(command, folder / 'objects', env, folder, 'c_compile_' + str(index), COMPILE_TIMEOUT)
            result['steps'].append(step)
            if step['returncode'] != 0:
                result['status'] = 'timed_out' if step['timed_out'] else 'compile_failed'
                return result
            objects.append(str(target))
        includes = ['-I' + str(run_dir / 'shared/build/graphics' if path == 'build/graphics' else root / path)
                    for path in case.include_dirs]
        system_includes = [part for path in case.system_include_dirs for part in ('-isystem', _absolute(root, path))]
        sources = list(case.sources)
        if coverage:
            sources.extend(case.coverage_sources)
            for index, source in enumerate(sources):
                target = _object_path(folder, source)
                command = [cxx, '-std=' + case.standard, *case.flags, *flags, *includes, *system_includes,
                           '-c', _absolute(root, source), '-o', str(target)]
                step = _command(command, folder / 'objects', env, folder, 'cpp_compile_' + str(index), COMPILE_TIMEOUT)
                result['steps'].append(step)
                if step['returncode'] != 0:
                    result['status'] = 'timed_out' if step['timed_out'] else 'compile_failed'
                    return result
                objects.append(str(target))
            sources = []
        executable = folder / 'objects' / (case.name + '.exe')
        command = [cxx, '-std=' + case.standard, *case.flags, *flags, *includes, *system_includes,
                   *[_absolute(root, path) for path in sources], *objects,
                   *['-L' + _absolute(root, path) for path in case.library_dirs],
                   *['-l' + name for name in case.libraries], *_coverage_link_flags(coverage), '-o', str(executable)]
        step = _command(command, folder / 'objects', env, folder, 'compile', COMPILE_TIMEOUT)
        result['steps'].append(step)
        if step['returncode'] != 0:
            result['status'] = 'timed_out' if step['timed_out'] else 'compile_failed'
            return result
        step = _command([str(executable)], folder / 'work', env, folder, 'run', RUN_TIMEOUT)
        result['steps'].append(step)
        if step['returncode'] != 0:
            result['status'] = 'timed_out' if step['timed_out'] else 'run_failed'
            return result
        if coverage:
            for stream in ('stdout', 'stderr'):
                if 'libgcov profiling error:' in Path(step[stream]).read_text(encoding='utf-8', errors='replace'):
                    result['status'] = 'coverage_failed'
                    result['error'] = 'libgcov profiling error; see ' + step[stream]
                    return result
        if case.name == 'installer_preview':
            _validate_preview(folder / 'work/build')
        result['status'] = 'passed'
    except Exception as error:
        result['error'] = str(error)
    finally:
        result['seconds'] = time.monotonic() - start
    return result


def _validate_preview(directory):
    from PIL import Image
    bitmaps = sorted(directory.glob('installer-*.bmp'))
    if not bitmaps:
        raise RuntimeError('Installer produced no preview frames')
    for bitmap in bitmaps:
        with Image.open(bitmap) as frame:
            colors = frame.getcolors(1280 * 720)
            if frame.size != (1280, 720) or colors is None or len(colors) < 30:
                raise RuntimeError('Installer rendered a blank or malformed frame: ' + bitmap.name)
            frame.save(bitmap.with_suffix('.png'))


def _python_case(root, run_dir, cxx):
    folder = run_dir / 'python'
    _folders(folder)
    env = tool_environment(_env(folder, root), cxx)
    env['CXX'] = cxx
    step = _command([sys.executable, '-m', 'unittest', 'discover', '-s', str(root / 'tests'), '-v'],
                    folder / 'work', env, folder, 'unittest', 300)
    return dict(name='python', kind='python', steps=[step], seconds=step['seconds'],
                status='passed' if step['returncode'] == 0 else 'timed_out' if step['timed_out'] else 'run_failed')


def _prepare(root, run_dir):
    shared = run_dir / 'shared'
    results = []
    preparations = (
        ('graphics', lambda: (shutil.copytree(root / 'assets/graphics', shared / 'assets/graphics'), prepare_graphics(shared))),
        ('bitmaps', lambda: (prepare_bitmap(root / 'assets/icon-original.png', shared / 'build/app-logo.bmp'),
                             prepare_bitmap(root / 'assets/icon-re6.png', shared / 'build/app-logo-re6.bmp'))),
    )
    for name, prepare in preparations:
        start = time.monotonic()
        item = dict(name=name, status='passed')
        try:
            prepare()
        except Exception as error:
            item.update(status='setup_failed', error=str(error))
        item['seconds'] = time.monotonic() - start
        results.append(item)
    return results


def _write_json(path, value):
    temporary = path.with_suffix(path.suffix + '.tmp')
    temporary.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')
    temporary.replace(path)


def _guard(name, kind, call):
    try:
        return call()
    except Exception as error:
        return dict(name=name, kind=kind, status='worker_failed', steps=[], error=str(error))


def _suite(root, run_dir, jobs, coverage):
    cxx = os.environ.get('CXX', 'C:/msys64/mingw64/bin/g++.exe')
    prepared = _prepare(root, run_dir)
    failed_dependencies = {p['name'] for p in prepared if p['status'] != 'passed'}
    calls = [lambda: _guard('python', 'python', lambda: _python_case(root, run_dir, cxx))]
    for case in NATIVE_CASES:
        failed = sorted(failed_dependencies & ({'graphics'} if case.graphics else set())
                        | (failed_dependencies & {'bitmaps'} if case.name == 'installer_preview' else set()))
        if failed:
            calls.append(lambda case=case, failed=failed: dict(name=case.name, kind='native', status='skipped_dependency',
                                                              dependencies=failed, steps=[]))
        else:
            calls.append(lambda case=case: _guard(case.name, 'native', lambda: _native_case(case, root, run_dir, cxx, coverage)))
    progress = {}
    def completed(index, case):
        progress[index] = case
        folder = run_dir / ('python' if case['kind'] == 'python' else 'native/' + case['name'])
        folder.mkdir(parents=True, exist_ok=True)
        _write_json(folder / 'result.json', case)
        _write_json(run_dir / 'progress.json', dict(completed=len(progress), total=len(calls),
                    cases=[progress[i] for i in sorted(progress)]))
        print(f"[{len(progress)}/{len(calls)}] {case['status']}: {case['name']}", flush=True)
    print(f'Host tests: {len(calls)} cases, {jobs} workers; output: {run_dir}', flush=True)
    cases = _collect(calls, jobs, completed=completed)
    result = dict(schema_version=1, root=str(root), run_dir=str(run_dir), jobs=jobs, coverage=coverage,
                  compiler=cxx, preparations=prepared, cases=cases,
                  passed=sum(c['status'] == 'passed' for c in cases), failed=sum(c['status'] != 'passed' for c in cases))
    _write_json(run_dir / 'results.json', result)
    print(f"Host tests: {result['passed']}/{len(cases)} passed; results: {run_dir / 'results.json'}", flush=True)
    return int(bool(result['failed'] or failed_dependencies))


def _new_run(root):
    parent = root / 'build/tests'
    parent.mkdir(parents=True, exist_ok=True)
    folder = Path(tempfile.mkdtemp(prefix='run-', dir=parent))
    _write_json(folder / 'owner.json', dict(root=str(root)))
    return folder


def _child_run(root):
    value = os.environ.get(_CHILD_RUN)
    if not value:
        return None
    folder = Path(value).resolve()
    folder.relative_to((root / 'build/tests').resolve())
    if json.loads((folder / 'owner.json').read_text(encoding='utf-8')) != dict(root=str(root)):
        raise ValueError('Coverage child does not own this test run')
    return folder


def _coverage_config(root, run_dir):
    coverage = run_dir / 'coverage'
    (coverage / 'python').mkdir(parents=True)
    config = coverage / 'python.coveragerc'
    config.write_text('[run]\nbranch = True\nparallel = True\npatch = subprocess\n'
                      f'include =\n    {root / "build.py"}\n    {root / "tools/*.py"}\n    {root / "tools/**/*.py"}\n'
                      f'data_file = {coverage / "python/.coverage"}\n', encoding='utf-8')
    return config


def _coverage_run(root, run_dir, jobs):
    from tools.coverage_gate import capture_coverage_inputs, verify_coverage_inputs
    manifest = capture_coverage_inputs(root)
    (run_dir / 'coverage').mkdir(parents=True, exist_ok=True)
    _write_json(run_dir / 'coverage/source-manifest.json', manifest)
    config = _coverage_config(root, run_dir)
    env = os.environ.copy()
    env[_CHILD_RUN] = str(run_dir)
    env['COVERAGE_RCFILE'] = str(config)
    env['PYTHONPYCACHEPREFIX'] = str(run_dir / 'coverage/pycache')
    print('Coverage suite progress:', run_dir / 'coverage-suite.stdout', flush=True)
    suite = _command([sys.executable, '-m', 'coverage', 'run', '--rcfile', str(config), '-m', 'build',
                      '--test', '--jobs', str(jobs), '--coverage'], root, env, run_dir, 'coverage-suite', 3600)
    reports = [suite]
    failed = suite['returncode'] != 0
    try:
        verify_coverage_inputs(root, manifest)
        # Explicit files keep never-imported tools at zero and count build.py even
        # when importlib loads it under a fixture-specific module name.
        morfs = [str(root / 'build.py'), *[str(p) for p in sorted((root / 'tools').rglob('*.py'))]]
        for arguments in (['combine', '--keep'], ['json', '-o', str(run_dir / 'coverage/python.json')],
                          ['html', '-d', str(run_dir / 'coverage/python/html')]):
            command = [sys.executable, '-m', 'coverage', arguments[0], '--rcfile', str(config), *arguments[1:],
                       *(morfs if arguments[0] in ('json', 'html') else [])]
            step = _command(command, root, env, run_dir / 'coverage', 'python-' + arguments[0], 120)
            reports.append(step)
            failed |= step['returncode'] != 0
        from tools.native_coverage import report_native
        from tools.coverage_gate import check_coverage
        native = report_native(root, run_dir, os.environ.get('CXX', 'C:/msys64/mingw64/bin/g++.exe'), jobs=jobs)
        failed |= bool(check_coverage(root, run_dir / 'coverage/native-summary.json',
                                      run_dir / 'coverage/python.json', run_dir / 'coverage'))
    except Exception as error:
        native = dict(status='failed', error=str(error))
        failed = True
    path = run_dir / 'results.json'
    result = json.loads(path.read_text(encoding='utf-8')) if path.exists() else dict(cases=[], failed=1)
    result['coverage_reports'] = dict(steps=reports, native=native, passed=not failed)
    _write_json(path, result)
    print('Coverage results:', run_dir / 'coverage/summary.json', flush=True)
    return int(failed)


def run_tests(root: Path, jobs: int = 4, coverage: bool = False) -> int:
    if jobs not in (2, 3, 4):
        raise ValueError('Test jobs must be 2, 3 or 4')
    root = root.resolve()
    child = _child_run(root) if coverage else None
    run_dir = child or _new_run(root)
    code = _suite(root, run_dir, jobs, coverage) if child or not coverage else _coverage_run(root, run_dir, jobs)
    if not child:
        _write_json(root / 'build/tests/latest.json', dict(run_dir=str(run_dir), results=str(run_dir / 'results.json'), exit_code=code))
    return code
