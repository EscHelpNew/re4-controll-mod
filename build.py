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
from tools.prepare_graphics import prepare as prepare_graphics
from tools.payload import stage_payload
from tools.hook_pool import stage_hook_pool
from tools.shared_layout import stage_shared_layout

OUT = ROOT / 'build'
VERSION = re.search(r'kVersion="([^"]+)"',(ROOT/'include/re4/version.hpp').read_text()).group(1)


def run(args, **kwargs):
    subprocess.run([str(a) for a in args], check=True, cwd=ROOT, **kwargs)


def tests(jobs=4, coverage=False):
    from tools.test_runner import run_tests
    result = run_tests(ROOT, jobs=jobs, coverage=coverage)
    if result:
        raise SystemExit(result)
    return result


def diagnostic():
    prepare_graphics(ROOT)
    cfg = prepare(ROOT)
    config = OUT / 'config'
    config.mkdir(exist_ok=True)
    (config / 'reload_project.h').write_text(header(cfg), encoding='utf-8')
    receiver = KIT / 'receiver'
    includes = ['-I' + str(p) for p in (ROOT / 'include', ROOT / 'source/overlay', receiver, config,ROOT/'build/graphics')]
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
    for source in ('source/input.cpp', 'source/settings.cpp', 'source/control_menu.cpp', 'source/settings_file.cpp','source/profiles.cpp','source/launch_mode.cpp', 'source/install.cpp','source/update.cpp',
                   'source/overlay/game_link.cpp','source/overlay/live_diagnostics.cpp',
                   'source/overlay/storage.cpp',
                   'source/diagnostic/installer_model.cpp','source/diagnostic/installer_renderer.cpp',
                   'source/diagnostic/home_policy.cpp','source/diagnostic/home_launcher.cpp',
                   'source/diagnostic/update_worker.cpp','source/re6/log_server.cpp','source/diagnostic/support_report.cpp',
                   'source/diagnostic/main.cpp','source/re6/settings.cpp','source/re6/settings_file.cpp'):
        dest = OUT / (source.removesuffix('.cpp').replace('/','_') + '.o')
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
    prepare_bitmap(ROOT/'assets/icon-re6.png',OUT/'app-logo-re6.bmp')
    base = OUT / 'experimental/atmosphere/contents/010099A00BC1E000/exefs'
    payload_files={'app-logo.bmp':OUT/'app-logo.bmp','app-logo-re6.bmp':OUT/'app-logo-re6.bmp',
        'home-launcher.bin':ROOT/'assets/home-launcher.bin','subsdk9':base/'subsdk9','main.npdm':base/'main.npdm',
        're4-controls.ovl':OUT/'experimental/switch/.overlays/re4-controls.ovl',
        'liberation-serif-OFL.txt':ROOT/'assets/fonts/liberation-serif/LICENSE'}
    re6_base=OUT/'experimental/atmosphere/contents/01002A000CD48000/exefs'
    payload_files.update({'re6-subsdk9':re6_base/'subsdk9','re6-main.npdm':re6_base/'main.npdm',
        're6-telemetry.ovl':OUT/'experimental/switch/.overlays/re6-telemetry.ovl',
        're6-controls.ovl':OUT/'experimental/switch/.overlays/re6-controls.ovl'})
    payload=stage_payload(ROOT,payload_files)
    icon = prepare_app_icon(ROOT / 'assets/icon-re.png', OUT / 'icon.jpg')
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
    stage_shared_layout(stage)
    print('Trampoline budget:',budget)
    profile = ROOT / 'profiles/010099A00BC1E000/npdm.json'
    (stage / 'config.json').write_text(profile.read_text(encoding='utf-8'), encoding='utf-8')
    shutil.copy2(ROOT / 'source/game/module.cpp', stage / 'source/program/main.cpp')
    for name in ('modern_move.inc','character_appearance.inc','character_pack_budget.inc','aim_raise.inc','scoped_aim.inc','enemy_health.inc','grenade_spot.inc','pickup_confirmation.inc','startup_logos.inc','dpad_weapons.inc','weapon_popup.inc','weapon_popup_native_text.inc','inventory_damage.inc','inventory_damage_native_text.inc','extended_upgrade.inc','ammo_counter.inc','health_name.inc'):
        shutil.copy2(ROOT / 'source/game' / name, stage / 'source/program' / name)
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
    from tools.inspect_nso import load_nso
    image = load_nso(package / 'subsdk9')
    data = image.segments[2]
    if data.size < 488 or image.memory[data.memory_offset:data.memory_offset+16] != b'RE4CTL_SHARED23\0':
        raise RuntimeError('RE4 IPC block is outside the first writable data page')
    print('RE4 IPC discovery: shared block at writable data offset0 PASS')
    result = subprocess.run([sdk_tool('npdmtool'), str(profile), str(package / 'main.npdm')],
                            capture_output=True, text=True, check=True)
    if 'Failed' in result.stdout + result.stderr:
        raise RuntimeError('NPDM generation rejected fields: ' + result.stdout + result.stderr)
    print('Built EXPERIMENTAL module:', package)


def overlay():
    prepare_graphics(ROOT)
    includes = ['-I' + str(ROOT / 'include'), '-I' + str(ROOT/'build/graphics'), '-I' + str(ROOT / 'source/overlay'),
                '-isystem', str(ROOT / 'third_party/libtesla/include'),
                '-isystem', str(SDK / 'libnx/include')]
    flags = ['-std=gnu++20', '-O2', '-g', '-Wall', '-Wextra', '-Werror',
             '-Wno-unused-parameter', '-fno-exceptions', '-ffixed-x18',
             '-march=armv8-a+crc+crypto', '-mtune=cortex-a57', '-mtp=soft', '-fPIE',
             '-D__SWITCH__', *includes]
    elf = OUT / 're4-controls-overlay.elf'
    env = os.environ.copy(); env['DEVKITPRO'] = SDK.as_posix()
    run([tool('g++'), *flags, 'source/overlay/main.cpp', 'source/overlay/game_link.cpp',
         'source/overlay/storage.cpp','source/overlay/live_debug.cpp','source/overlay/live_debug_server.cpp','source/overlay/live_diagnostics.cpp',
         'source/settings.cpp', 'source/settings_file.cpp', 'source/control_menu.cpp','source/profiles.cpp','source/launch_mode.cpp',
         '-specs=' + str(SDK / 'libnx/switch.specs'), '-L' + str(SDK / 'libnx/lib'),
         '-lnx', '-lm', '-o', elf], env=env)
    output = OUT / 'experimental/switch/.overlays/re4-controls.ovl'
    output.parent.mkdir(parents=True, exist_ok=True)
    nacp = OUT / 're4-controls-overlay.nacp'
    run([sdk_tool('nacptool'), '--create', 'RE4 Controls', 're4-controls', VERSION, nacp])
    icon = prepare_app_icon(ROOT / 'assets/icon-re.png', OUT / 'icon.jpg')
    run([sdk_tool('elf2nro'), elf, output, '--nacp=' + str(nacp), '--icon=' + str(icon)])
    print('Built EXPERIMENTAL overlay:', output)


def telemetry_overlay():
    includes=['-I'+str(ROOT/'include'),'-I'+str(ROOT/'source/re6_overlay'),
              '-isystem',str(ROOT/'third_party/libtesla/include'),'-isystem',str(SDK/'libnx/include')]
    flags=['-std=gnu++20','-O2','-g','-Wall','-Wextra','-Werror','-Wno-unused-parameter',
           '-fno-exceptions','-ffixed-x18','-march=armv8-a+crc+crypto','-mtune=cortex-a57',
           '-mtp=soft','-fPIE','-D__SWITCH__',*includes]
    elf=OUT/'re6-telemetry-overlay.elf'
    env=os.environ.copy();env['DEVKITPRO']=SDK.as_posix()
    run([tool('g++'),*flags,'source/re6_overlay/main.cpp','source/re6_overlay/worker.cpp',
         '-specs='+str(SDK/'libnx/switch.specs'),'-L'+str(SDK/'libnx/lib'),'-lnx','-lm','-o',elf],env=env)
    output=OUT/'experimental/switch/.overlays/re6-telemetry.ovl';output.parent.mkdir(parents=True,exist_ok=True)
    nacp=OUT/'re6-telemetry-overlay.nacp'
    run([sdk_tool('nacptool'),'--create','RE4 / RE6 Telemetry','re4-controls',VERSION,nacp])
    icon=prepare_app_icon(ROOT/'assets/icon-re6.png',OUT/'icon-re6.jpg')
    run([sdk_tool('elf2nro'),elf,output,'--nacp='+str(nacp),'--icon='+str(icon)])
    print('Built read-only telemetry overlay:',output)


def re6_graphics_overlay():
    prepare_graphics(ROOT)
    includes=['-I'+str(ROOT/'include'),'-I'+str(ROOT/'build/graphics'),'-I'+str(ROOT/'source/re6_controls'),
              '-isystem',str(ROOT/'third_party/libtesla/include'),'-isystem',str(SDK/'libnx/include')]
    flags=['-std=gnu++20','-O2','-g','-Wall','-Wextra','-Werror','-Wno-unused-parameter',
           '-fno-exceptions','-ffixed-x18','-march=armv8-a+crc+crypto','-mtune=cortex-a57',
           '-mtp=soft','-fPIE','-D__SWITCH__',*includes]
    elf=OUT/'re6-graphics-overlay.elf'
    env=os.environ.copy();env['DEVKITPRO']=SDK.as_posix()
    run([tool('g++'),*flags,'source/re6_controls/main.cpp','source/re6_controls/game_link.cpp',
         'source/re6_controls/storage.cpp','source/re6/settings.cpp','source/re6/settings_file.cpp',
         '-specs='+str(SDK/'libnx/switch.specs'),'-L'+str(SDK/'libnx/lib'),'-lnx','-lm','-o',elf],env=env)
    output=OUT/'experimental/switch/.overlays/re6-controls.ovl';output.parent.mkdir(parents=True,exist_ok=True)
    nacp=OUT/'re6-graphics-overlay.nacp'
    run([sdk_tool('nacptool'),'--create','RE6 Graphics','re4-controls',VERSION,nacp])
    icon=prepare_app_icon(ROOT/'assets/icon-re6.png',OUT/'icon-re6.jpg')
    run([sdk_tool('elf2nro'),elf,output,'--nacp='+str(nacp),'--icon='+str(icon)])
    print('Built RE6 graphics overlay:',output)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--test', action='store_true')
    parser.add_argument('--jobs', type=int, choices=(2, 3, 4), default=4,
                        help='parallel host-test workers (2 to 4; default: 4)')
    parser.add_argument('--coverage', action='store_true')
    parser.add_argument('--nro', action='store_true')
    parser.add_argument('--module', action='store_true')
    parser.add_argument('--overlay', action='store_true')
    args = parser.parse_args()
    if args.coverage and not args.test:
        parser.error('--coverage requires --test')
    OUT.mkdir(exist_ok=True)
    if args.test:
        tests(jobs=args.jobs, coverage=args.coverage)
    if args.module:
        module()
        from tools.re6_module import build as build_re6
        build_re6(ROOT,SDK,run)
    if args.overlay:
        overlay()
        telemetry_overlay()
        re6_graphics_overlay()
    if args.nro:
        diagnostic()
    if not (args.test or args.nro or args.module or args.overlay):
        parser.print_help()
