"""Collect native line coverage without dropping unexecuted product sources."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import hashlib
import json
import os
import re
import shlex
import shutil
import subprocess
import sys
from tools.test_process import ProcessScope, run_logged, tool_environment


class NativeCoverageError(RuntimeError):
    pass


def compile_flags():
    return ('--coverage', '-O0', '-g', '-fprofile-abs-path',
            '-fkeep-inline-functions', '-fkeep-static-functions', '-MMD')


def link_flags():
    # Retaining MinGW's unused oleauto.h inline wrappers exposes their VarCmp
    # import. Resolve the Windows system dependency rather than dropping code.
    return ('-loleaut32',) if os.name == 'nt' else ()


def canonical_source(root, name):
    root = Path(root).resolve()
    path = Path(str(name).replace('\\', '/'))
    try:
        relative = (path if path.is_absolute() else root / path).resolve().relative_to(root)
    except ValueError:
        return None
    if not relative.parts or relative.parts[0] not in ('source', 'include') or relative.suffix not in ('.cpp', '.inc', '.hpp', '.h'):
        return None
    return relative.as_posix()


def source_inventory(root):
    root = Path(root)
    return sorted(p.relative_to(root).as_posix() for folder in ('source', 'include')
                  for p in (root / folder).rglob('*') if p.is_file() and p.suffix in ('.cpp', '.inc', '.hpp', '.h'))


def toolchain(cxx):
    compiler = Path(cxx) if Path(cxx).is_file() else Path(shutil.which(str(cxx)) or '')
    if not compiler.is_file():
        raise NativeCoverageError('Compiler not found: ' + str(cxx))
    gcov = compiler.with_name(compiler.name.replace('g++', 'gcov').replace('gcc', 'gcov'))
    if gcov == compiler or not gcov.is_file():
        raise NativeCoverageError('Matching sibling gcov not found for ' + str(compiler))
    versions = []
    for command in ([str(compiler), '-dumpfullversion'], [str(gcov), '--version']):
        try:
            result = subprocess.run(command, capture_output=True, text=True, check=False, timeout=30,
                                    env=tool_environment(os.environ.copy(), command[0]))
        except (OSError, subprocess.TimeoutExpired) as error:
            raise NativeCoverageError('Cannot inspect coverage toolchain: ' + str(error)) from error
        match = re.search(r'\b\d+\.\d+(?:\.\d+)?\b', result.stdout)
        if result.returncode or not match:
            raise NativeCoverageError('Cannot read compiler/gcov version: ' + str(command))
        versions.append(match.group())
    if versions[0] != versions[1]:
        raise NativeCoverageError('Compiler/gcov version mismatch: ' + ' / '.join(versions))
    return {'compiler': str(compiler), 'gcov': str(gcov), 'version': versions[0]}


def check_profiles(directory):
    notes = sorted(Path(directory).rglob('*.gcno'))
    if not notes:
        raise NativeCoverageError('No instrumented host gcno files: ' + str(directory))
    for note in notes:
        data = note.with_suffix('.gcda')
        if not data.is_file():
            raise NativeCoverageError('Successful host execution did not produce gcda: ' + str(note))
        if data.stat().st_mtime_ns < note.stat().st_mtime_ns:
            raise NativeCoverageError('Refusing stale coverage profile: ' + str(data))
    for log in Path(directory).rglob('*.stderr'):
        if 'libgcov' in log.read_text(encoding='utf-8', errors='replace').lower():
            raise NativeCoverageError('Refusing runtime profiling diagnostics: ' + str(log))
    return notes


def read_dependencies(root, paths):
    found = {}
    for path in paths:
        text = Path(path).read_text(encoding='utf-8', errors='replace').replace('\\\n', ' ')
        parts = re.split(r':\s+', text, maxsplit=1)
        if len(parts) != 2:
            raise NativeCoverageError('Malformed compiler dependency artifact: ' + str(path))
        # GCC normally emits forward slashes. Also accept native Windows
        # separators while retaining Make's escaped spaces for shlex.
        paths = re.sub(r'\\(?![ \t\\])', '/', parts[1])
        for name in shlex.split(paths):
            source = canonical_source(root, name)
            if source:
                found.setdefault(source, []).append(str(path))
    return found


def normalize_report(root, report):
    files = []
    for record in report['files']:
        name = canonical_source(root, record['file'])
        if not name:
            continue
        if not (Path(root) / name).is_file():
            raise NativeCoverageError('Coverage refers to missing product source: ' + name)
        physical = physical_lines(record)
        # Preserve every physical executable line, while retaining the raw
        # per-specialization/function and branch data in the separate traces.
        record = {'file': name, 'lines': [{'line_number': number, 'count': count}
                                         for number, count in sorted(physical.items())], 'functions': []}
        files.append(record)
    return dict(report, files=sorted(files, key=lambda item: item['file']))


def physical_lines(source):
    seen, lines = set(), {}
    for line in source['lines']:
        number, count = line['line_number'], line['count']
        key = number, line.get('function_name')
        if key in seen or number < 1 or count < 0 or line.get('gcovr/excluded', False):
            raise NativeCoverageError('Invalid/duplicate/excluded executable line in ' + source['file'])
        seen.add(key)
        lines[number] = lines.get(number, 0) + count
    return lines


def completeness(root, report, dependencies):
    measured = {f['file'] for f in report['files'] if f['lines']}
    files, missing = {}, []
    for name in source_inventory(root):
        if name in measured:
            files[name] = {'status': 'reported', 'artifact': 'native.json'}
        elif name in dependencies:
            # The retained-inline/static compiler configuration included this
            # file but emitted no runtime counters: declarations, compile-time
            # data or templates not instantiated in supported configurations.
            files[name] = {'status': 'compiled-no-runtime-counters',
                           'artifacts': dependencies[name],
                           'reason': 'Compiler dependency present; retained-inline/static build emitted no counters.'}
        else:
            files[name] = {'status': 'unmeasured'}
            missing.append(name)
    return {'complete': not missing, 'unmeasured_files': missing, 'files': files,
            'expected_files': list(files), 'non_executable_files': {
                name: proof for name, proof in files.items() if proof['status'] == 'compiled-no-runtime-counters'}}


def statistics(report):
    covered = total = branches = taken = 0
    for source in report['files']:
        lines = physical_lines(source)
        total += len(lines); covered += sum(count > 0 for count in lines.values())
        for line in source['lines']:
            for branch in line.get('branches', []):
                branches += 1; taken += branch['count'] > 0
    return {'covered_lines': covered, 'total_lines': total,
            'line_percent': covered * 100 / total if total else 0,
            'covered_branches': taken, 'total_branches': branches,
            'branch_percent': taken * 100 / branches if branches else 0}


def run_checked(command, cwd, log, timeout=300):
    log = Path(log); log.parent.mkdir(parents=True, exist_ok=True)
    command = [str(v) for v in command]
    tool = shlex.split(command[command.index('--gcov-executable') + 1])[0] if '--gcov-executable' in command else command[0]
    result = run_logged(command, cwd=cwd, env=tool_environment(os.environ.copy(), tool),
                        stdout_path=log, stderr_path=log.with_suffix(log.suffix + '.stderr'), timeout=timeout)
    if result['timed_out']:
        raise NativeCoverageError('Coverage command timed out; see ' + str(log))
    if result['returncode'] != 0:
        raise NativeCoverageError('Coverage command failed; see ' + str(log))


def stage_target_support(root, run_dir):
    """Prepare compile-only SDK inputs without a product build or local secrets."""
    root, run_dir = Path(root), Path(run_dir)
    vendor = root / 'third_party/exlaunch/source'
    generator = root / 'third_party/switch-debug-kit/host/project.py'
    if not (vendor / 'lib.hpp').is_file() or not generator.is_file():
        raise NativeCoverageError('Pristine vendor headers/public header generator missing')
    graphics = run_dir / 'shared/build/graphics'
    if not all((graphics / name).is_file() for name in ('graphics_preview_images.hpp', 'gyro_test_background_image.hpp')):
        raise NativeCoverageError('Private prepared graphics missing: ' + str(graphics))
    support = run_dir / 'target-support'
    shutil.copytree(vendor, support / 'exlaunch/source', dirs_exist_ok=True)
    public = next((root / name for name in ('project.release.json', 'project.example.json')
                   if (root / name).is_file()), None)
    config = json.loads(public.read_text(encoding='utf-8-sig')) if public else dict(
        name='RE4 Controls', author='re4-controls', title_id='0000000000000000',
        sd_directory='re4-controls', host='127.0.0.1')
    # Execute the existing pure header generator under its original filename;
    # never call load()/prepare(), which read local config or generate a token.
    # exec also avoids placing a Python bytecode cache in the vendor checkout.
    namespace = {'__name__': '_coverage_public_project', '__file__': str(generator)}
    exec(compile(generator.read_text(encoding='utf-8'), str(generator), 'exec'), namespace)
    header = namespace['header'](config)
    (support / 'config').mkdir(parents=True, exist_ok=True)
    (support / 'config/reload_project.h').write_text(header, encoding='utf-8')
    evidence = {'public_config': public.name if public else 'safe-defaults',
                'vendor_sha256': {p.relative_to(vendor).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                                  for p in sorted(vendor.rglob('*')) if p.is_file()}}
    (support / 'support-manifest.json').write_text(json.dumps(evidence, indent=2), encoding='utf-8')
    return support


def target_command(root, sdk, source, directory, support=None):
    root, sdk, directory = Path(root), Path(sdk), Path(directory)
    support = Path(support) if support else directory.parent.parent / 'target-support'
    compiler = sdk / 'devkitA64/bin/aarch64-none-elf-g++.exe'
    includes = [root / 'include', root / 'source/overlay', root / 'source/re6_overlay',
                support / 'config', support.parent / 'shared/build/graphics', root / 'third_party/switch-debug-kit/receiver',
                root / 'third_party/libtesla/include', sdk / 'libnx/include',
                sdk / 'portlibs/switch/include/SDL2', sdk / 'portlibs/switch/include']
    defines = ['-D__SWITCH__']
    rtti = []
    if source in ('source/game/module.cpp', 'source/re6/module.cpp'):
        stage = support / 'exlaunch/source'
        if not (stage / 'lib.hpp').is_file():
            raise NativeCoverageError('Real module SDK staging headers missing: ' + str(stage))
        includes.insert(0, stage)
        includes.extend(sorted(p for p in stage.iterdir() if p.is_dir()))
        defines.append('-U__RTLD_6XX__' if source.startswith('source/game/') else '-D__RTLD_6XX__')
        defines.extend(['-DEXL_LOAD_KIND=Module', '-DEXL_LOAD_KIND_ENUM=2', '-DEXL_PROGRAM_ID=' +
                        ('0x010099A00BC1E000' if source.startswith('source/game/') else '0x01002A000CD48000')])
        rtti = ['-fno-rtti', '-fno-asynchronous-unwind-tables', '-fno-unwind-tables']
    return [str(compiler), '-std=gnu++23', '-fno-exceptions', *rtti,
            '-march=armv8-a+crc+crypto', '-mtune=cortex-a57', '-ffixed-x18', '-mtp=soft', '-fPIE',
            *defines, *compile_flags(), *['-I' + str(p) for p in includes],
            '-MF', str(directory / 'dependencies.d'), '-c', str(root / source), '-o', str(directory / 'baseline.o')]


def target_baseline(root, run_dir, jobs):
    support = stage_target_support(root, run_dir)
    sdk_value = os.environ.get('DEVKITPRO')
    sdk = Path(sdk_value) if sdk_value and sdk_value != '/opt/devkitpro' else Path('C:/msys64/opt/devkitpro')
    target = toolchain(str(sdk / 'devkitA64/bin/aarch64-none-elf-g++.exe'))
    output = Path(run_dir) / 'target-baseline'; output.mkdir(parents=True, exist_ok=True)
    sources = [p for p in source_inventory(root) if p.endswith('.cpp')]
    if not sources:
        raise NativeCoverageError('No own target translation units found')
    def compile_one(source):
        directory = output / source.replace('/', '_').replace('.', '_')
        directory.mkdir(parents=True, exist_ok=True)
        command = target_command(root, sdk, source, directory, support=support)
        (directory / 'command.json').write_text(json.dumps(command, indent=2), encoding='utf-8')
        run_checked(command, root, directory / 'compile.log')
    errors = []
    scope = ProcessScope()
    with ThreadPoolExecutor(max_workers=jobs) as pool:
        futures = []
        try:
            for source in sources:
                futures.append(pool.submit(scope.run, compile_one, source))
            for future in futures:
                try:
                    future.result()
                except NativeCoverageError as error:
                    errors.append(str(error))
        except BaseException:
            scope.cancel()
            for future in futures:
                future.cancel()
            raise
    if errors:
        raise NativeCoverageError('\n'.join(errors))
    return output, target


def collect(root, counters, gcov, output, log, jobs):
    output = Path(output); output.parent.mkdir(parents=True, exist_ok=True)
    # gcovr parses this option with POSIX shlex even on Windows.
    gcov_command = shlex.quote(str(gcov).replace('\\', '/'))
    run_checked([sys.executable, '-m', 'gcovr', '--root', str(root), '--gcov-executable', gcov_command,
                 '--filter', '^source/', '--filter', '^include/', '--no-markers', '-j', str(jobs),
                 '--json', str(output), str(counters)], root, log)


def report_native(root, run_dir, cxx, jobs=2):
    root, run_dir = Path(root).resolve(), Path(run_dir).resolve()
    if jobs not in (2, 3, 4) or not run_dir.is_relative_to(root / 'build') or 'releases' in run_dir.relative_to(root / 'build').parts:
        raise NativeCoverageError('Coverage requires2..4workers and a private build output directory')
    host = toolchain(cxx)
    check_profiles(run_dir / 'native')
    output = run_dir / 'coverage'; output.mkdir(parents=True, exist_ok=True)
    collect(root, run_dir / 'native', host['gcov'], output / 'host.json', output / 'host.log', jobs)
    baseline, target = target_baseline(root, run_dir, jobs)
    collect(root, baseline, target['gcov'], output / 'target-baseline.json', output / 'target-baseline.log', jobs)
    merged = output / 'native-raw.json'
    run_checked([sys.executable, '-m', 'gcovr', '--root', str(root),
                 '--json-add-tracefile', str(output / 'host.json'), '--json-add-tracefile', str(output / 'target-baseline.json'),
                 '--merge-lines', '--merge-mode-functions=separate', '--json', str(merged),
                 '--html-details', str(output / 'native.html')], root, output / 'merge.log')
    report = normalize_report(root, json.loads(merged.read_text(encoding='utf-8')))
    normalized = output / 'native.json'
    normalized.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    dependencies = read_dependencies(root, [*(run_dir / 'native').rglob('*.d'), *baseline.rglob('*.d')])
    summary = {**statistics(report), 'report': str(normalized), 'raw_report': str(merged),
               'line_metric': 'physical executable source lines; template instances merged',
               'host_toolchain': host, 'target_toolchain': target,
               'host_branches': statistics(json.loads((output / 'host.json').read_text(encoding='utf-8'))),
               'target_baseline_branches': statistics(json.loads((output / 'target-baseline.json').read_text(encoding='utf-8'))),
               'completeness': completeness(root, report, dependencies)}
    # Mixed compiler branch IDs are not a semantic aggregate. Keep these two
    # populations explicit; only executable-line union drives the90percent gate.
    for key in ('covered_branches', 'total_branches', 'branch_percent'):
        summary.pop(key)
    (output / 'native-summary.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
    return summary
