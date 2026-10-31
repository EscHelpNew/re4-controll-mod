"""Build the guarded RE6 observer in its own disposable SDK6 staging tree."""
from pathlib import Path
import copy
import json
import os
import shutil
import subprocess
import sys

TITLE = '01002A000CD48000'
# Source identity for the permissions-only profile, not a bundled game asset.
# Generated locally with hactool -t npdm --json from hash-checked update v131072.
SOURCE_NPDM_SHA256 = '2d9c805a64bd0a4b585082084e667920064da22715d2a15fb8ea379580c05c65'
SDK_IMPORTS = {'_ZN2nn2ro6detail15g_pAutoLoadListE',
               '_ZN2nn2ro6detail36g_pLookupGlobalManualFunctionPointerE'}


def sdk6_header(original):
    # The verified SDK6.6 export has a "p" that upstream exlaunch omits.
    # Keep the C++ variable/API spelling while binding the actual ELF symbol.
    declaration = 'extern rtld::lookup_global_t g_LookupGlobalManualFunctionPointer;'
    if original.count(declaration) != 1:
        raise ValueError('Unexpected exlaunch SDK6 manual lookup declaration')
    return original.replace(declaration, declaration[:-1] +
        ' asm("_ZN2nn2ro6detail36g_pLookupGlobalManualFunctionPointerE");')


def observer_permissions(original):
    """Keep native game permissions; add only exlaunch's writable-code aliases."""
    result = copy.deepcopy(original)
    if int(result.get('title_id', '0'), 16) != int(TITLE, 16):
        raise ValueError('RE6 source permissions have the wrong title identity')
    capabilities = result.get('kernel_capabilities', [])
    calls = [c['value'] for c in capabilities if c.get('type') == 'syscalls']
    debug = [c['value'] for c in capabilities if c.get('type') == 'debug_flags']
    if len(calls) != 1 or len(debug) != 1 or not result.get('service_access') or not result.get('filesystem_access'):
        raise ValueError('Incomplete original RE6 process permissions')
    calls[0].update(svcMapProcessMemory='0x74', svcUnmapProcessMemory='0x75')
    debug[0]['allow_debug'] = True
    debug[0]['force_debug_prod'] = False
    title = '0x' + TITLE.lower()
    for prefix in ('title_id', 'program_id'):
        result[prefix] = result[prefix + '_range_min'] = result[prefix + '_range_max'] = title
    # Modern npdmtool names for the original META header's zero fields and
    # MMU flags0x07. There is no filesystem or service expansion.
    result.update(process_category=0, prevent_code_reads=False, signature_key_generation=0,
                  system_resource_size='0x0', optimize_memory_allocation=False,
                  disable_device_address_space_merge=False, enable_alias_region_extra_size=False,
                  force_debug_prod=False)
    return result


def _msys(path):
    value = Path(path).resolve().as_posix()
    return '/' + value[0].lower() + value[2:] if len(value) > 1 and value[1] == ':' else value


def build(root, sdk, run):
    """Entry point for build.py module_re6(); return the generated ExeFS path."""
    root, sdk = Path(root).resolve(), Path(sdk).resolve()
    stage = root / 'build/re6-game-sdk'
    vendor = root / 'third_party/exlaunch'
    stage.mkdir(parents=True, exist_ok=True)
    for name in ('source', 'misc'):
        shutil.copytree(vendor / name, stage / name, dirs_exist_ok=True)
    for name in ('Makefile', 'config.mk'):
        shutil.copy2(vendor / name, stage / name)
    profile = root / ('profiles/' + TITLE + '/npdm.json')
    permissions = json.loads(profile.read_text(encoding='utf-8'))
    if int(permissions['program_id'], 16) != int(TITLE, 16):
        raise ValueError('Invalid RE6 module profile')
    shutil.copy2(profile, stage / 'config.json')
    shutil.copy2(root / 'source/re6/module.cpp', stage / 'source/program/main.cpp')
    # SDK6.6 uses the upstream0xD0 ModuleObject with soname+B8/NROsize+C0.
    # Retain manual lookup, unlike the isolated RE4 SDK5 staging compatibility.
    loader = (stage / 'source/rtld/ModuleObject.hpp').read_text(encoding='utf-8')
    common = (stage / 'misc/mk/common.mk').read_text(encoding='utf-8')
    if 'sizeof(ModuleObject) == 0xD0' not in loader or '-D__RTLD_6XX__' not in common:
        raise ValueError('Unexpected exlaunch SDK6 loader layout/flags')
    rtld_header = stage / 'source/rtld.hpp'
    rtld_header.write_text(sdk6_header(rtld_header.read_text(encoding='utf-8')), encoding='utf-8')
    hook_source = stage / 'source/lib/hook/nx64/hook_impl.cpp'
    hook = hook_source.read_text(encoding='utf-8')
    if hook.count('if(i > HookMax)') != 1:
        raise ValueError('Unexpected exlaunch trampoline allocator revision')
    hook_source.write_text(hook.replace('if(i > HookMax)', 'if(i >= HookMax)'), encoding='utf-8')
    # Four hooks fit the default4096-byte pool (20 slots). No RE4 modules or
    # RE4 hook-count/pool substitutions are staged into this build.
    temporary = root / 'build/re6-tmp'
    temporary.mkdir(exist_ok=True)
    env = os.environ.copy()
    env.update(DEVKITPRO='/opt/devkitpro', DEVKITA64='/opt/devkitpro/devkitA64', TMPDIR=_msys(temporary))
    env['PATH'] = os.pathsep.join(['C:/msys64/usr/bin', str(sdk / 'devkitA64/bin'),
                                  str(sdk / 'tools/bin'), env.get('PATH', '')])
    flags = '-Wno-non-c-typedef-for-linkage -D__RTLD_6XX__ -I' + _msys(root / 'include')
    stamp = stage / 'build-flags.txt'
    if not stamp.exists() or stamp.read_text() != flags:
        object_dir = (stage / 'build').resolve()
        object_dir.relative_to((root / 'build').resolve())
        if object_dir.is_dir():
            for obj in object_dir.glob('*.o'):
                obj.unlink()
        stamp.write_text(flags)
    subprocess.run(['C:/msys64/usr/bin/make.exe', '-j4', 'PROGRAM_ID=' + TITLE,
                    'CXX_FLAGS=' + flags], cwd=stage, env=env, check=True)
    imports = subprocess.run([str(sdk / 'devkitA64/bin/aarch64-none-elf-nm.exe'), '-u',
                              str(stage / 're6-game-sdk.elf')], capture_output=True, text=True, check=True)
    undefined = {line.split()[-1] for line in imports.stdout.splitlines() if line.strip()}
    if undefined != SDK_IMPORTS:
        raise RuntimeError('RE6 loader import mismatch: ' + repr(sorted(undefined)))
    run([sys.executable, root / 'tools/check_hook_abi.py', stage / 'build/inline_asm.o'])
    package = root / ('build/experimental/atmosphere/contents/' + TITLE + '/exefs')
    package.mkdir(parents=True, exist_ok=True)
    shutil.copy2(stage / 'deploy/subsdk9', package / 'subsdk9')
    result = subprocess.run([str(sdk / 'tools/bin/npdmtool.exe'), str(profile), str(package / 'main.npdm')],
                            capture_output=True, text=True, check=True)
    if 'Failed' in result.stdout + result.stderr:
        raise RuntimeError('RE6 NPDM generation rejected fields: ' + result.stdout + result.stderr)
    print('Built guarded RE6 observer:', package)
    return package
