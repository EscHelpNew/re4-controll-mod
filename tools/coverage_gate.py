"""Fail closed on incomplete coverage or fewer than90% executable lines."""
from pathlib import Path
import hashlib
import html
import json
import math


class CoverageError(ValueError):
    pass


def capture_coverage_inputs(root):
    """Content identity for own sources, fixtures and public test assets."""
    root = Path(root).resolve()
    paths = {root / 'build.py'}
    for name in ('requirements-dev.txt',):
        if (root / name).is_file():
            paths.add(root / name)
    for name in ('source', 'include', 'tools', 'tests', 'assets'):
        paths.update(p for p in (root / name).rglob('*')
                     if p.is_file() and '__pycache__' not in p.parts and p.suffix != '.pyc')
    return {'schema_version': 1, 'files': {
        _relative(root, str(p)): hashlib.sha256(p.read_bytes()).hexdigest()
        for p in sorted(paths)}}


def verify_coverage_inputs(root, manifest):
    manifest = _mapping(manifest, 'coverage input manifest')
    if type(manifest.get('schema_version')) is not int or manifest['schema_version'] != 1:
        raise CoverageError('Invalid coverage input manifest schema')
    files = _mapping(manifest.get('files'), 'coverage input files')
    for name, digest in files.items():
        if (_relative(root, name) != name or not isinstance(digest, str)
                or len(digest) != 64 or any(c not in '0123456789abcdef' for c in digest)):
            raise CoverageError('Invalid coverage input identity')
    if files != capture_coverage_inputs(root)['files']:
        raise CoverageError('Coverage inputs changed after the run started; rerun tests')


def _mapping(value, label):
    if not isinstance(value, dict):
        raise CoverageError(f'Invalid {label} object')
    return value


def _sequence(value, label):
    if not isinstance(value, list):
        raise CoverageError(f'Invalid {label} list')
    return value


def _count(value, label):
    if isinstance(value, bool) or not isinstance(value, int) or value < 0:
        raise CoverageError(f'Invalid {label} counter')
    return value


def _relative(root, filename):
    if not isinstance(filename, str) or not filename:
        raise CoverageError('Missing source filename')
    path = Path(filename.replace('\\', '/'))
    if not path.is_absolute():
        path = root / path
    try:
        return path.resolve().relative_to(root.resolve()).as_posix()
    except ValueError as error:
        raise CoverageError(f'Coverage source outside project: {filename}') from error


def _metrics(covered, total, label):
    covered, total = _count(covered, label), _count(total, label)
    if not total or covered > total:
        raise CoverageError(f'Invalid {label} coverage range')
    return {'covered_lines': covered, 'total_lines': total,
            'line_percent': 100.0 * covered / total}


def _native(root, summary, report):
    summary, report = _mapping(summary, 'native summary'), _mapping(report, 'native report')
    complete = _mapping(summary.get('completeness', {}), 'native census')
    if complete.get('complete') is not True or complete.get('unmeasured_files') != []:
        raise CoverageError('Native coverage census is incomplete')
    expected = {p.relative_to(root).as_posix() for directory in ('source', 'include')
                for p in (root / directory).rglob('*')
                if p.is_file() and p.suffix in ('.cpp', '.inc', '.hpp', '.h')}
    declared = [_relative(root, p) for p in _sequence(complete.get('expected_files', []), 'native census')]
    if not expected or set(declared) != expected or len(set(declared)) != len(declared):
        raise CoverageError('Native coverage census differs from project sources')
    nonexec = complete.get('non_executable_files', {})
    if not isinstance(nonexec, dict):
        raise CoverageError('Invalid non-executable native census')
    proven = set()
    for filename, proof in nonexec.items():
        name = _relative(root, filename)
        if not isinstance(proof, dict) or not proof.get('reason') or not proof.get('artifacts'):
            raise CoverageError(f'Missing compiler evidence for {name}')
        for artifact in _sequence(proof['artifacts'], 'compiler evidence'):
            path = Path(artifact)
            if not path.is_absolute():
                path = root / path
            if not path.is_file():
                raise CoverageError(f'Missing compiler dependency artifact for {name}')
        proven.add(name)
    measured, reported = set(), set()
    lines = {}
    for file in _sequence(report.get('files', []), 'native files'):
        file = _mapping(file, 'native file')
        name = _relative(root, file.get('file'))
        if name not in expected or name in reported:
            raise CoverageError(f'Unexpected or duplicate native file: {name}')
        reported.add(name)
        source_lines = _sequence(file.get('lines', []), 'native lines')
        if source_lines:
            measured.add(name)
        for line in source_lines:
            line = _mapping(line, 'native line')
            number = _count(line.get('line_number'), 'native line')
            if line.get('gcovr/excluded'):
                raise CoverageError(f'Excluded executable source: {name}:{number}')
            if line.get('gcovr/noncode') or number == 0:
                continue
            count = _count(line.get('count'), 'native execution')
            key = name, number
            lines[key] = lines.get(key, False) or count > 0
    if measured & proven or measured | proven != expected:
        raise CoverageError('Native source has neither counters nor compiler evidence')
    metrics = _metrics(sum(lines.values()), len(lines), 'C++')
    if metrics['covered_lines'] != summary.get('covered_lines') or metrics['total_lines'] != summary.get('total_lines'):
        raise CoverageError('Native summary does not match raw coverage')
    metrics['host_branches'] = summary.get('host_branches', {})
    metrics['target_baseline_branches'] = summary.get('target_baseline_branches', {})
    metrics['source_files'] = len(expected)
    return metrics


def _python(root, report):
    report = _mapping(report, 'Python report')
    expected = {'build.py'} | {p.relative_to(root).as_posix()
                              for p in (root / 'tools').rglob('*.py')
                              if '__pycache__' not in p.parts}
    measured, covered, total, branch_covered, branches = set(), 0, 0, 0, 0
    for filename, file in _mapping(report.get('files', {}), 'Python files').items():
        name = _relative(root, filename)
        if name not in expected or name in measured:
            raise CoverageError(f'Unexpected or duplicate Python file: {name}')
        measured.add(name)
        file = _mapping(file, 'Python file')
        value = _mapping(file['summary'], 'Python counters')
        if (_sequence(file.get('excluded_lines', []), 'Python excluded lines')
                or _count(value.get('num_excluded', 0), 'Python excluded lines')):
            raise CoverageError(f'Python report excludes production lines: {name}')
        c, t = _count(value['covered_lines'], 'Python covered'), _count(value['num_statements'], 'Python total')
        if c > t:
            raise CoverageError(f'Invalid Python file counters: {name}')
        covered += c
        total += t
        bc, bt = _count(value.get('covered_branches', 0), 'Python branches'), _count(value.get('num_branches', 0), 'Python branches')
        if bc > bt:
            raise CoverageError('Invalid Python branch range')
        branch_covered += bc
        branches += bt
    if measured != expected:
        raise CoverageError('Python coverage omits project sources: ' + ', '.join(sorted(expected - measured)))
    totals = _mapping(report.get('totals', {}), 'Python totals')
    if _count(totals.get('num_excluded', 0), 'Python excluded lines'):
        raise CoverageError('Python totals exclude production lines')
    if any(totals.get(key, 0) != value for key, value in
           [('covered_lines', covered), ('num_statements', total),
            ('covered_branches', branch_covered), ('num_branches', branches)]):
        raise CoverageError('Python totals do not match individual files')
    result = _metrics(covered, total, 'Python')
    result['branches'] = {'covered': branch_covered, 'total': branches,
                          'percent': 100.0 * branch_covered / branches if branches else None}
    result['source_files'] = len(expected)
    return result


def assess_coverage(root, native_summary, native_report, python_report, minimum=90.0,
                    *, source_manifest=None):
    root = Path(root).resolve()
    if isinstance(minimum, bool) or not isinstance(minimum, (int, float)) or not math.isfinite(minimum) or not 0 <= minimum <= 100:
        raise CoverageError('Invalid minimum line coverage')
    native = _native(root, native_summary, native_report)
    python = _python(root, python_report)
    verify_coverage_inputs(root, source_manifest)
    passed = all(value['covered_lines'] * 100 >= minimum * value['total_lines']
                 for value in (native, python))
    combined = _metrics(native['covered_lines'] + python['covered_lines'],
                        native['total_lines'] + python['total_lines'], 'combined')
    return {'passed': passed, 'minimum_line_percent': minimum, 'cpp': native,
            'python': python, 'combined': combined,
            'policy': 'Both languages must meet executable-line threshold; branches are reported separately.'}


def check_coverage(root, native_summary, python_json, output_dir, minimum=90.0):
    """Read standard reports, write one index/summary and return a CI exit code."""
    output_dir, native_summary = Path(output_dir), Path(native_summary)
    try:
        result = assess_coverage(root, json.loads(native_summary.read_text(encoding='utf-8')),
                                 json.loads((native_summary.parent / 'native.json').read_text(encoding='utf-8')),
                                 json.loads(Path(python_json).read_text(encoding='utf-8')), minimum,
                                 source_manifest=json.loads((native_summary.parent / 'source-manifest.json').read_text(encoding='utf-8')))
    except (CoverageError, OSError, ValueError, KeyError, TypeError) as error:
        result = {'passed': False, 'error': str(error), 'minimum_line_percent': minimum}
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / 'summary.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    details = html.escape(json.dumps(result, indent=2))
    (output_dir / 'index.html').write_text(
        '<!doctype html><meta charset="utf-8"><title>RE4 test coverage</title>'
        '<h1>RE4 test coverage</h1><p>' + ('PASS' if result['passed'] else 'FAIL') + '</p>'
        '<p><a href="native.html">C++ detail</a> · <a href="python/html/index.html">Python detail</a></p>'
        '<pre>' + details + '</pre>', encoding='utf-8')
    if 'error' in result:
        print('Coverage FAIL:', result['error'])
    else:
        for label in ('cpp', 'python', 'combined'):
            value = result[label]
            print(f"{label}: {value['covered_lines']}/{value['total_lines']} executable lines ({value['line_percent']:.2f}%)")
        print('Coverage', 'PASS' if result['passed'] else 'FAIL', f'(minimum {minimum:g}% in each language)')
    return 0 if result['passed'] else 1
