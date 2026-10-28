"""Build locally; deployment is exclusively through Switch Debug Kit's client."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent
KIT = ROOT / 'third_party/switch-debug-kit'
sys.path.insert(0, str(KIT / 'host'))
from sdk import SDK, tool, sdk_tool
from project import load, header, prepare

OUT = ROOT / 'build'


def run(args, **kwargs):
    subprocess.run([str(a) for a in args], check=True, cwd=ROOT, **kwargs)


def tests():
    run([sys.executable, '-m', 'unittest', 'discover', '-s', 'tests', '-v'])
    host = os.environ.get('CXX', 'C:/msys64/mingw64/bin/g++.exe')
    run([host, '-std=c++17', '-Wall', '-Wextra', '-Werror', '-static', '-Iinclude',
         'tests/input_tests.cpp', 'source/input.cpp', 'source/settings.cpp',
         '-o', OUT / 'input_tests.exe'])
    run([OUT / 'input_tests.exe'])
    run([host, '-std=c++17', '-Wall', '-Wextra', '-Werror', '-static', '-Iinclude',
         'tests/settings_file_tests.cpp', 'source/settings.cpp', 'source/settings_file.cpp',
         '-o', OUT / 'settings_file_tests.exe'])
    run([OUT / 'settings_file_tests.exe'])
    run([host, '-std=c++17', '-Wall', '-Wextra', '-Werror', '-static', '-Iinclude',
         'tests/install_tests.cpp', 'source/install.cpp', '-o', OUT / 'install_tests.exe'])
    run([OUT / 'install_tests.exe'])


def diagnostic():
    cfg = prepare(ROOT)
    config = OUT / 'config'
    config.mkdir(exist_ok=True)
    (config / 'reload_project.h').write_text(header(cfg), encoding='utf-8')
    receiver = KIT / 'receiver'
    includes = ['-I' + str(p) for p in (ROOT / 'include', ROOT / 'source/overlay', receiver, config)]
    includes += ['-isystem', str(SDK / 'libnx/include')]
    common = ['-O2', '-g', '-Wall', '-Wextra', '-Werror', '-Wno-misleading-indentation',
              '-march=armv8-a+crc+crypto', '-mtune=cortex-a57', '-ffixed-x18',
              '-mtp=soft', '-fPIE', '-D__SWITCH__', *includes]
    objects = []
    for name in ('reload_core', 'reload_bridge', 'remote_reload', 'reload_launch',
                 'reload_reports', 'reload_main'):
        dest = OUT / (name + '.o')
        run([tool('gcc'), '-std=gnu11', *common, '-c', receiver / (name + '.c'), '-o', dest])
        objects.append(dest)
    for source in ('source/input.cpp', 'source/settings.cpp', 'source/settings_file.cpp', 'source/install.cpp',
                   'source/overlay/game_link.cpp',
                   'source/diagnostic/main.cpp'):
        dest = OUT / (Path(source).stem + '.o')
        run([tool('g++'), '-std=gnu++17', '-fno-exceptions', '-fno-rtti', *common,
             '-c', source, '-o', dest])
        objects.append(dest)
    elf, nacp, nro = [OUT / ('re4-controls.' + ext) for ext in ('elf', 'nacp', 'nro')]
    env = os.environ.copy()
    env['DEVKITPRO'] = SDK.as_posix()
    run([tool('g++'), '-specs=' + str(SDK / 'libnx/switch.specs'), *common,
         *objects, '-L' + str(SDK / 'libnx/lib'), '-lnx', '-lm', '-o', elf], env=env)
    run([sdk_tool('nacptool'), '--create', cfg['name'], cfg['author'], '0.1.6', nacp,
         '--titleid=' + cfg['title_id']])
    payload = OUT / 'diagnostic-romfs'
    payload.mkdir(exist_ok=True)
    base = OUT / 'experimental/atmosphere/contents/010099A00BC1E000/exefs'
    for name, source in {'subsdk9': base / 'subsdk9', 'main.npdm': base / 'main.npdm',
                         're4-controls.ovl': OUT / 'experimental/switch/.overlays/re4-controls.ovl'}.items():
        if not source.is_file():
            raise RuntimeError('Build --module --overlay before the bundled diagnostic NRO')
        shutil.copy2(source, payload / name)
    run([sdk_tool('elf2nro'), elf, nro, '--nacp=' + str(nacp), '--romfsdir=' + str(payload)])
    digest = hashlib.sha256(nro.read_bytes()).hexdigest()
    (OUT / 'diagnostic.json').write_text(json.dumps({'file': nro.name,
        'bytes': nro.stat().st_size, 'sha256': digest, 'title_id': cfg['title_id']}, indent=2) + '\n')
    print('Built', nro, 'SHA256', digest)
    if cfg['title_id'] == '0000000000000000':
        print('Own forwarder not configured: automatic RUN remains disabled.')


def module():
    stage = OUT / 'game-sdk'
    vendor = ROOT / 'third_party/exlaunch'
    stage.mkdir(exist_ok=True)
    for name in ('source', 'misc'):
        shutil.copytree(vendor / name, stage / name, dirs_exist_ok=True)
    for name in ('Makefile', 'config.mk'):
        shutil.copy2(vendor / name, stage / name)
    profile = ROOT / 'profiles/010099A00BC1E000/npdm.json'
    (stage / 'config.json').write_text(profile.read_text(encoding='utf-8'), encoding='utf-8')
    shutil.copy2(ROOT / 'source/game/module.cpp', stage / 'source/program/main.cpp')
    core = stage / 'source/re4-core'
    core.mkdir(exist_ok=True)
    for name in ('input.cpp', 'settings.cpp'):
        shutil.copy2(ROOT / 'source' / name, core / name)
    # RE4's SDK is 5.4. Its loader has the old ModuleObject layout and does not
    # export the SDK 6 manual-symbol callback. Apply this guarded compatibility
    # change in the disposable build tree, retaining pristine upstream files.
    loader_source = stage / 'source/rtld/ModuleObject.cpp'
    loader_text = loader_source.read_text(encoding='utf-8')
    manual_lookup = '''        if (address == 0 && ro::g_LookupGlobalManualFunctionPointer) {
            address = ro::g_LookupGlobalManualFunctionPointer(name);
        }'''
    if manual_lookup not in loader_text:
        raise RuntimeError('Unexpected exlaunch loader revision')
    loader_source.write_text(loader_text.replace(manual_lookup,
        '#ifdef __RTLD_6XX__\n' + manual_lookup + '\n#endif'), encoding='utf-8')
    # Upstream makefiles use the installed devkitPro toolchain. Build a staged
    # tree so public vendor sources remain unchanged.
    temporary = OUT / 'tmp'
    temporary.mkdir(exist_ok=True)
    def msys_path(path):
        path = path.resolve().as_posix()
        return '/' + path[0].lower() + path[2:] if len(path) > 1 and path[1] == ':' else path
    env = os.environ.copy()
    env['DEVKITPRO'] = '/opt/devkitpro'
    env['DEVKITA64'] = '/opt/devkitpro/devkitA64'
    env['TMPDIR'] = msys_path(temporary)
    env['PATH'] = os.pathsep.join(['C:/msys64/usr/bin', str(SDK / 'devkitA64/bin'),
                                  str(SDK / 'tools/bin'), env.get('PATH', '')])
    flags = '-Wno-non-c-typedef-for-linkage -U__RTLD_6XX__ -I' + msys_path(ROOT / 'include')
    flag_stamp = stage / 'build-flags.txt'
    if not flag_stamp.exists() or flag_stamp.read_text() != flags:
        object_dir = (stage / 'build').resolve()
        object_dir.relative_to(OUT.resolve())
        if object_dir.is_dir():
            for obj in object_dir.glob('*.o'):
                obj.unlink()
        flag_stamp.write_text(flags)
    command = ['C:/msys64/usr/bin/make.exe', '-j4', 'PROGRAM_ID=010099A00BC1E000',
               'CXX_FLAGS=' + flags]
    subprocess.run(command, cwd=stage, env=env, check=True)
    run([sys.executable, ROOT / 'tools/check_hook_abi.py', stage / 'build/inline_asm.o'])
    package = OUT / 'experimental/atmosphere/contents/010099A00BC1E000/exefs'
    package.mkdir(parents=True, exist_ok=True)
    shutil.copy2(stage / 'deploy/subsdk9', package / 'subsdk9')
    result = subprocess.run([sdk_tool('npdmtool'), str(profile), str(package / 'main.npdm')],
                            capture_output=True, text=True, check=True)
    if 'Failed' in result.stdout + result.stderr:
        raise RuntimeError('NPDM generation rejected fields: ' + result.stdout + result.stderr)
    print('Built EXPERIMENTAL module:', package)


def overlay():
    includes = ['-I' + str(ROOT / 'include'), '-I' + str(ROOT / 'source/overlay'),
                '-isystem', str(ROOT / 'third_party/libtesla/include'),
                '-isystem', str(SDK / 'libnx/include')]
    flags = ['-std=gnu++20', '-O2', '-g', '-Wall', '-Wextra', '-Werror',
             '-Wno-unused-parameter', '-fno-exceptions', '-ffixed-x18',
             '-march=armv8-a+crc+crypto', '-mtune=cortex-a57', '-mtp=soft', '-fPIE',
             '-D__SWITCH__', *includes]
    elf = OUT / 're4-controls-overlay.elf'
    env = os.environ.copy(); env['DEVKITPRO'] = SDK.as_posix()
    run([tool('g++'), *flags, 'source/overlay/main.cpp', 'source/overlay/game_link.cpp',
         'source/settings.cpp', 'source/settings_file.cpp',
         '-specs=' + str(SDK / 'libnx/switch.specs'), '-L' + str(SDK / 'libnx/lib'),
         '-lnx', '-lm', '-o', elf], env=env)
    output = OUT / 'experimental/switch/.overlays/re4-controls.ovl'
    output.parent.mkdir(parents=True, exist_ok=True)
    nacp = OUT / 're4-controls-overlay.nacp'
    run([sdk_tool('nacptool'), '--create', 'RE4 Controls', 're4-controls', '0.1.0', nacp])
    run([sdk_tool('elf2nro'), elf, output, '--nacp=' + str(nacp)])
    print('Built EXPERIMENTAL overlay:', output)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--test', action='store_true')
    parser.add_argument('--nro', action='store_true')
    parser.add_argument('--module', action='store_true')
    parser.add_argument('--overlay', action='store_true')
    args = parser.parse_args()
    OUT.mkdir(exist_ok=True)
    if args.test:
        tests()
    if args.module:
        module()
    if args.overlay:
        overlay()
    if args.nro:
        diagnostic()
    if not (args.test or args.nro or args.module or args.overlay):
        parser.print_help()
