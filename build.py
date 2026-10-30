"""Build locally; deployment is exclusively through Switch Debug Kit's client."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import re

ROOT = Path(__file__).resolve().parent
KIT = ROOT / 'third_party/switch-debug-kit'
sys.path.insert(0, str(KIT / 'host'))
from sdk import SDK, tool, sdk_tool
from project import load, header, prepare
from tools.prepare_icon import prepare as prepare_app_icon, prepare_bitmap
from tools.payload import stage_payload
from tools.hook_pool import stage_hook_pool

OUT = ROOT / 'build'
VERSION = re.search(r'kVersion="([^"]+)"',(ROOT/'include/re4/version.hpp').read_text()).group(1)


def run(args, **kwargs):
    subprocess.run([str(a) for a in args], check=True, cwd=ROOT, **kwargs)


def tests():
    def native(name):
        # Windows CRT assertion dialogs must not leave an unattended test run
        # hanging indefinitely. Successful native cases finish in milliseconds.
        run([OUT / name], timeout=20)
    run([sys.executable, '-m', 'unittest', 'discover', '-s', 'tests', '-v'])
    host = os.environ.get('CXX', 'C:/msys64/mingw64/bin/g++.exe')
    run([host, '-std=c++17', '-Wall', '-Wextra', '-Werror', '-static', '-Iinclude',
         'tests/input_tests.cpp', 'source/input.cpp', 'source/settings.cpp',
         '-o', OUT / 'input_tests.exe'])
    native('input_tests.exe')
    for name,sources in [('button_map_tests',[]),('settings_v7_tests',['source/settings.cpp']),('camera_tests',[]),
                         ('graphics_tests',[]),('graphics_settings_tests',['source/settings.cpp']),('view_tests',[]),
                         ('view_settings_tests',['source/settings.cpp','source/profiles.cpp','source/settings_file.cpp'])]:
        run([host,'-std=c++17','-Wall','-Wextra','-Werror','-static','-Iinclude',
             'tests/'+name+'.cpp',*sources,'-o',OUT/(name+'.exe')])
        native(name+'.exe')
    run([host, '-std=c++17', '-Wall', '-Wextra', '-Werror', '-static', '-Iinclude',
         'tests/settings_file_tests.cpp', 'source/settings.cpp', 'source/settings_file.cpp',
         '-o', OUT / 'settings_file_tests.exe'])
    native('settings_file_tests.exe')
    run([host,'-std=c++17','-Wall','-Wextra','-Werror','-static','-Iinclude',
         'tests/profiles_tests.cpp','source/profiles.cpp','source/settings.cpp','source/settings_file.cpp',
         '-o',OUT/'profiles_tests.exe'])
    native('profiles_tests.exe')
    run([host, '-std=c++17', '-Wall', '-Wextra', '-Werror', '-static', '-Iinclude',
         'tests/install_tests.cpp', 'source/install.cpp', '-o', OUT / 'install_tests.exe'])
    native('install_tests.exe')
    run([host, '-std=c++17', '-Wall', '-Wextra', '-Werror', '-static',
         '-Itests/stubs', '-Isource/overlay', 'tests/overlay_storage_tests.cpp',
         'source/overlay/storage.cpp', '-o', OUT / 'overlay_storage_tests.exe'])
    native('overlay_storage_tests.exe')
    run([host, '-std=c++17', '-Wall', '-Wextra', '-Werror', '-static', '-Iinclude',
         'tests/sensor_tests.cpp', '-o', OUT / 'sensor_tests.exe'])
    native('sensor_tests.exe')
    run([host, '-std=c++20', '-Wall', '-Wextra', '-Werror', '-static',
         '-Itests/game_stubs', '-Iinclude', 'tests/game_module_tests.cpp',
         'source/input.cpp', 'source/settings.cpp', '-o', OUT / 'game_module_tests.exe'])
    native('game_module_tests.exe')
    run([host,'-std=c++20','-Wall','-Wextra','-Werror','-static','-Iinclude',
         'tests/modern_move_tests.cpp','source/input.cpp','-o',OUT/'modern_move_tests.exe'])
    native('modern_move_tests.exe')
    run([host, '-std=c++20', '-Wall', '-Wextra', '-Werror', '-static',
         '-Itests/overlay_stubs', '-Iinclude', '-Isource/overlay',
         'tests/overlay_ui_tests.cpp', 'source/settings.cpp', 'source/control_menu.cpp','source/profiles.cpp', '-o', OUT / 'overlay_ui_tests.exe'])
    native('overlay_ui_tests.exe')
    run([host,'-std=c++20','-Wall','-Wextra','-Werror','-static',
         '-Itests/overlay_stubs','-Iinclude','-Isource/overlay','tests/profile_ui_tests.cpp',
         'source/settings.cpp','source/control_menu.cpp','source/profiles.cpp','source/diagnostic/installer_model.cpp',
         '-o',OUT/'profile_ui_tests.exe'])
    native('profile_ui_tests.exe')
    run([host, '-std=c++20', '-Wall', '-Wextra', '-Werror', '-static',
         '-Itests/overlay_stubs', '-Iinclude', '-Isource/overlay',
         'tests/game_link_tests.cpp', 'source/settings.cpp', '-o', OUT / 'game_link_tests.exe'])
    native('game_link_tests.exe')
    run([host, '-std=c++17', '-Wall', '-Wextra', '-Werror', '-static', '-Iinclude',
         'tests/response_curve_tests.cpp', 'source/input.cpp', 'source/settings.cpp',
         '-o', OUT / 'response_curve_tests.exe'])
    native('response_curve_tests.exe')
    run([host,'-std=c++17','-Wall','-Wextra','-Werror','-static','-Iinclude',
         'tests/gyro_filter_tests.cpp','source/input.cpp','source/settings.cpp','-o',OUT/'gyro_filter_tests.exe'])
    native('gyro_filter_tests.exe')
    run([host,'-std=c++17','-Wall','-Wextra','-Werror','-static','-Iinclude',
         'tests/aim_input_tests.cpp','source/input.cpp','source/settings.cpp','-o',OUT/'aim_input_tests.exe'])
    native('aim_input_tests.exe')
    run([host,'-std=c++17','-Wall','-Wextra','-Werror','-static','-Iinclude',
         'tests/home_launcher_tests.cpp','source/diagnostic/home_policy.cpp','-o',OUT/'home_launcher_tests.exe'])
    native('home_launcher_tests.exe')
    run([host,'-std=c++17','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-static',
         '-Itests/home_stubs','-Iinclude','tests/actual_home_tests.cpp','source/diagnostic/home_policy.cpp',
         '-ladvapi32','-o',OUT/'actual_home_tests.exe'])
    native('actual_home_tests.exe')
    run([host.replace('g++','gcc'),'-std=c11','-Itests/update_stubs','-I'+str(KIT/'receiver'),
         '-c',KIT/'receiver/reload_core.c','-o',OUT/'reload_core_host.o'])
    for name,sources in [('update_tests',['source/update.cpp']),
                         ('update_worker_tests',['source/update.cpp','source/diagnostic/update_worker.cpp'])]:
        run([host,'-std=c++17','-Wall','-Wextra','-Werror','-static',
             '-Itests/update_stubs','-Iinclude','-I'+str(KIT/'receiver'),
             'tests/'+name+'.cpp',*sources,OUT/'reload_core_host.o','-ladvapi32','-o',OUT/(name+'.exe')])
        native(name+'.exe')
    run([host, '-std=c++17', '-Wall', '-Wextra', '-Werror', '-static', '-Iinclude',
         'tests/control_menu_tests.cpp', 'source/control_menu.cpp', 'source/settings.cpp','source/profiles.cpp','source/settings_file.cpp',
         '-o', OUT / 'control_menu_tests.exe'])
    native('control_menu_tests.exe')
    run([host, '-std=c++17', '-Wall', '-Wextra', '-Werror', '-static', '-Iinclude',
         'tests/installer_model_tests.cpp', 'source/diagnostic/installer_model.cpp',
         'source/control_menu.cpp', 'source/settings.cpp','source/profiles.cpp','source/settings_file.cpp', '-o', OUT / 'installer_model_tests.exe'])
    native('installer_model_tests.exe')
    run([host,'-std=c++17','-Wall','-Wextra','-Werror','-static','-Iinclude',
         'tests/calibration_tests.cpp','-o',OUT/'calibration_tests.exe'])
    native('calibration_tests.exe')
    prepare_bitmap(ROOT / 'assets/icon-original.png',OUT / 'app-logo.bmp')
    run([host,'-std=c++17','-O2','-Wall','-Wextra','-Werror','-static-libgcc','-static-libstdc++',
         '-Iinclude','-IC:/msys64/mingw64/include/SDL2','-isystem','third_party/libtesla/include',
         'tests/installer_preview.cpp','source/diagnostic/installer_renderer.cpp',
         'source/diagnostic/installer_model.cpp','source/control_menu.cpp','source/settings.cpp','source/input.cpp','source/profiles.cpp','source/settings_file.cpp',
         '-LC:/msys64/mingw64/lib','-lSDL2','-o',OUT/'installer_preview.exe'])
    preview_env=os.environ.copy()
    preview_env['PATH']='C:/msys64/mingw64/bin'+os.pathsep+preview_env.get('PATH','')
    preview_env['SDL_VIDEODRIVER']='dummy'
    run([OUT/'installer_preview.exe'],env=preview_env,timeout=20)
    from PIL import Image
    for bitmap in OUT.glob('installer-*.bmp'):
        with Image.open(bitmap) as frame:
            if frame.size!=(1280,720) or len(frame.getcolors(1280*720))<30:
                raise RuntimeError('Installer rendered a blank or malformed frame')
            frame.save(bitmap.with_suffix('.png'))


def diagnostic():
    cfg = prepare(ROOT)
    config = OUT / 'config'
    config.mkdir(exist_ok=True)
    (config / 'reload_project.h').write_text(header(cfg), encoding='utf-8')
    receiver = KIT / 'receiver'
    includes = ['-I' + str(p) for p in (ROOT / 'include', ROOT / 'source/overlay', receiver, config)]
    includes += ['-isystem', str(SDK / 'libnx/include')]
    includes += ['-isystem',str(SDK/'portlibs/switch/include/SDL2'),
                 '-isystem',str(SDK/'portlibs/switch/include'),
                 '-isystem',str(ROOT/'third_party/libtesla/include')]
    common = ['-O2', '-g', '-Wall', '-Wextra', '-Werror', '-Wno-misleading-indentation',
              '-march=armv8-a+crc+crypto', '-mtune=cortex-a57', '-ffixed-x18',
              '-mtp=soft', '-fPIE', '-D__SWITCH__', *includes]
    objects = []
    for name in ('reload_core', 'reload_bridge', 'remote_reload', 'reload_launch',
                 'reload_reports', 'reload_main'):
        dest = OUT / (name + '.o')
        run([tool('gcc'), '-std=gnu11', *common, '-c', receiver / (name + '.c'), '-o', dest])
        objects.append(dest)
    for source in ('source/input.cpp', 'source/settings.cpp', 'source/control_menu.cpp', 'source/settings_file.cpp','source/profiles.cpp', 'source/install.cpp','source/update.cpp',
                   'source/overlay/game_link.cpp',
                   'source/overlay/storage.cpp',
                   'source/diagnostic/installer_model.cpp','source/diagnostic/installer_renderer.cpp',
                   'source/diagnostic/home_policy.cpp','source/diagnostic/home_launcher.cpp',
                   'source/diagnostic/update_worker.cpp',
                   'source/diagnostic/main.cpp'):
        dest = OUT / (Path(source).stem + '.o')
        run([tool('g++'), '-std=gnu++17', '-fno-exceptions', '-fno-rtti', *common,
             '-c', source, '-o', dest])
        objects.append(dest)
    elf, nacp, nro = [OUT / ('re4-controls.' + ext) for ext in ('elf', 'nacp', 'nro')]
    env = os.environ.copy()
    env['DEVKITPRO'] = SDK.as_posix()
    run([tool('g++'), '-specs=' + str(SDK / 'libnx/switch.specs'), *common,
         *objects, '-L'+str(SDK/'portlibs/switch/lib'),'-lSDL2','-lEGL','-lglapi','-ldrm_nouveau','-lcurl','-lz',
         '-L' + str(SDK / 'libnx/lib'), '-lnx','-lpthread', '-lm', '-o', elf], env=env)
    run([sdk_tool('nacptool'), '--create', cfg['name'], cfg['author'], VERSION, nacp,
         '--titleid=' + cfg['title_id']])
    prepare_bitmap(ROOT/'assets/icon-original.png',OUT/'app-logo.bmp')
    base = OUT / 'experimental/atmosphere/contents/010099A00BC1E000/exefs'
    payload=stage_payload(ROOT,{'app-logo.bmp':OUT/'app-logo.bmp',
        'home-launcher.bin':ROOT/'assets/home-launcher.bin','subsdk9':base/'subsdk9','main.npdm':base/'main.npdm',
        're4-controls.ovl':OUT/'experimental/switch/.overlays/re4-controls.ovl'})
    icon = prepare_app_icon(ROOT / 'assets/icon-original.png', OUT / 'icon.jpg')
    run([sdk_tool('elf2nro'), elf, nro, '--nacp=' + str(nacp), '--icon=' + str(icon),
         '--romfsdir=' + str(payload)])
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
    budget=stage_hook_pool(stage,ROOT)
    print('Trampoline budget:',budget)
    profile = ROOT / 'profiles/010099A00BC1E000/npdm.json'
    (stage / 'config.json').write_text(profile.read_text(encoding='utf-8'), encoding='utf-8')
    shutil.copy2(ROOT / 'source/game/module.cpp', stage / 'source/program/main.cpp')
    shutil.copy2(ROOT / 'source/game/modern_move.inc', stage / 'source/program/modern_move.inc')
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
         'source/overlay/storage.cpp',
         'source/settings.cpp', 'source/settings_file.cpp', 'source/control_menu.cpp','source/profiles.cpp',
         '-specs=' + str(SDK / 'libnx/switch.specs'), '-L' + str(SDK / 'libnx/lib'),
         '-lnx', '-lm', '-o', elf], env=env)
    output = OUT / 'experimental/switch/.overlays/re4-controls.ovl'
    output.parent.mkdir(parents=True, exist_ok=True)
    nacp = OUT / 're4-controls-overlay.nacp'
    run([sdk_tool('nacptool'), '--create', 'RE4 Controls', 're4-controls', VERSION, nacp])
    icon = prepare_app_icon(ROOT / 'assets/icon-original.png', OUT / 'icon.jpg')
    run([sdk_tool('elf2nro'), elf, output, '--nacp=' + str(nacp), '--icon=' + str(icon)])
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
