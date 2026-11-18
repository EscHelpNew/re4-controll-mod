"""Build the guarded RE5 controls module in its own disposable SDK6 staging tree."""
from pathlib import Path
import copy
import json
import os
import shutil
import subprocess
import sys

TITLE = '010018100CD46000'
# Source identity for the permissions-only profile, not a bundled game asset.
# Generated locally with hactool -t npdm --json from hash-checked update v196608.
SOURCE_NPDM_SHA256 = '58cf04db2699bec007055580659bf63582d457aedb98982a0dbc7fb849c28188'
SDK_IMPORTS = {'_ZN2nn2ro6detail15g_pAutoLoadListE',
               '_ZN2nn2ro6detail36g_pLookupGlobalManualFunctionPointerE',
               '_ZN2nn2fs11GetFileSizeEPlNS0_10FileHandleE',
               '_ZN2nn2fs19MountSdCardForDebugEPKc',
               '_ZN2nn2fs8OpenFileEPNS0_10FileHandleEPKci',
               '_ZN2nn2fs8ReadFileENS0_10FileHandleElPvm',
               '_ZN2nn2fs9CloseFileENS0_10FileHandleE'}


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
        raise ValueError('RE5 source permissions have the wrong title identity')
    capabilities = result.get('kernel_capabilities', [])
    calls = [c['value'] for c in capabilities if c.get('type') == 'syscalls']
    debug = [c['value'] for c in capabilities if c.get('type') == 'debug_flags']
    if len(calls) != 1 or len(debug) != 1 or not result.get('service_access') or not result.get('filesystem_access'):
        raise ValueError('Incomplete original RE5 process permissions')
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

def graphics_permissions(original):
    """Retain native services and add only SD access for the mod's settings."""
    result=observer_permissions(original)
    permissions=int(result['filesystem_access']['permissions'],16)
    # Atmosphere AccessControlBits::Bits::SdCard is bit21. No save/system expansion.
    result['filesystem_access']['permissions']=hex(permissions|(1<<21))
    return result


def stage_layout(stage):
    """Reserve 61 trampoline slots and pin ABI1 into the discovery window."""
    from tools.hook_pool import _replace_once, _CONSTANTS
    stage = Path(stage)
    setting = stage/'source/program/setting.hpp'
    setting.write_text(_replace_once(setting.read_text(), 'constexpr size_t JitSize = 0x1000;',
                       'constexpr size_t JitSize = 0x3000;', 'RE5 JIT size'), encoding='utf-8')
    hook = stage/'source/lib/hook/nx64/hook_impl.cpp'
    text = _replace_once(hook.read_text(), _CONSTANTS,
                        _CONSTANTS + '\n        static_assert(HookMax >= 60, "RE5 trampoline budget");', 'RE5 hook constants')
    hook.write_text(text, encoding='utf-8')
    assembly = stage/'source/lib/hook/nx64/inline_asm.s'
    text = _replace_once(assembly.read_text(), '    stp x28, x29, [sp, #0xE0]\n.endm',
                        '    stp x28, x29, [sp, #0xE0]\n    mrs x16, nzcv\n    str x16, [sp, #0xF8]\n.endm', 'RE5 NZCV save')
    text = _replace_once(text, '.macro armRecoverRegisters\n',
                        '.macro armRecoverRegisters\n    ldr x16, [sp, #0xF8]\n    msr nzcv, x16\n', 'RE5 NZCV restore')
    assembly.write_text(text, encoding='utf-8')
    linker = stage/'misc/link.ld'
    original = '  .data : {\n    *(.data .data.*)\n  } :data'
    replacement = '  .data : {\n    __re5_control_start__ = .;\n    KEEP(*(.data.re5controls))\n    __re5_control_end__ = .;\n    ASSERT(__re5_control_end__ - __re5_control_start__ == 384, "RE5 IPC ABI1 size changed");\n    . = ALIGN(16);\n    *(.data .data.*)\n  } :data'
    replacement=replacement.replace('    . = ALIGN(16);',
        '    __re5_identity_start__ = .;\n'
        '    KEEP(*(.data.re5identity))\n'
        '    __re5_identity_end__ = .;\n'
        '    ASSERT(__re5_identity_start__ - __re5_control_end__ == 0, "RE5 identity adjacency changed");\n'
        '    ASSERT(__re5_identity_end__ - __re5_identity_start__ == 112, "RE5 identity size changed");\n'
        '    __re5_runtime_start__ = .;\n'
        '    KEEP(*(.data.re5runtime))\n'
        '    __re5_runtime_end__ = .;\n'
        '    ASSERT(__re5_runtime_start__ - __re5_identity_end__ == 0, "RE5 runtime adjacency changed");\n'
        '    ASSERT(__re5_runtime_end__ - __re5_runtime_start__ == 192, "RE5 runtime ABI1 size changed");\n'
        '    __re5_camera_start__ = .;\n'
        '    KEEP(*(.data.re5camera))\n'
        '    __re5_camera_end__ = .;\n'
        '    ASSERT(__re5_camera_start__ - __re5_control_start__ == 688, "RE5 camera offset changed");\n'
        '    ASSERT(__re5_camera_end__ - __re5_camera_start__ == 5808, "RE5 camera ABI1 size changed");\n'
        '    . = ALIGN(16);')
    linker.write_text(_replace_once(linker.read_text(), original, replacement, 'RE5 shared layout'), encoding='utf-8')


def build(root, sdk, run):
    """Entry point for build.py module_re5(); return the generated ExeFS path."""
    root, sdk = Path(root).resolve(), Path(sdk).resolve()
    stage = root / 'build/re5-game-sdk'
    vendor = root / 'third_party/exlaunch'
    stage.mkdir(parents=True, exist_ok=True)
    for name in ('source', 'misc'):
        shutil.copytree(vendor / name, stage / name, dirs_exist_ok=True)
    for name in ('Makefile', 'config.mk'):
        shutil.copy2(vendor / name, stage / name)
    profile = root / ('profiles/' + TITLE + '/npdm.json')
    permissions = json.loads(profile.read_text(encoding='utf-8'))
    if int(permissions['program_id'], 16) != int(TITLE, 16):
        raise ValueError('Invalid RE5 module profile')
    build_profile=stage/'config.json'
    build_profile.write_text(json.dumps(graphics_permissions(permissions),indent=2)+'\n',encoding='utf-8')
    shutil.copy2(root / 'source/re5/module.cpp', stage / 'source/program/main.cpp')
    core=stage/'source/re5-core';core.mkdir(exist_ok=True)
    shutil.copy2(root/'source/re5/settings.cpp',core/'settings.cpp')
    shutil.copy2(root/'source/settings.cpp',core/'re4_settings.cpp')
    shutil.copy2(root/'source/input.cpp',core/'input.cpp')
    for source in (root/'source/re5').glob('*.inc'):
        shutil.copy2(source,stage/'source/program'/source.name)
    stage_layout(stage)
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
    # RE5 uses its own three-page pool; RE4/RE6 staged trees are untouched.
    temporary = root / 'build/re5-tmp'
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
                              str(stage / 're5-game-sdk.elf')], capture_output=True, text=True, check=True)
    undefined = {line.split()[-1] for line in imports.stdout.splitlines() if line.strip()}
    if undefined != SDK_IMPORTS:
        raise RuntimeError('RE5 loader import mismatch: ' + repr(sorted(undefined)))
    run([sys.executable, root / 'tools/check_hook_abi.py', stage / 'build/inline_asm.o', '--nzcv'])
    package = root / ('build/experimental/atmosphere/contents/' + TITLE + '/exefs')
    package.mkdir(parents=True, exist_ok=True)
    shutil.copy2(stage / 'deploy/subsdk9', package / 'subsdk9')
    from tools.inspect_nso import load_nso
    image = load_nso(package / 'subsdk9')
    data = image.segments[2]
    symbols = subprocess.run([str(sdk / 'devkitA64/bin/aarch64-none-elf-nm.exe'), '-S',
                              str(stage / 're5-game-sdk.elf')], capture_output=True, text=True, check=True)
    shared = [line.split() for line in symbols.stdout.splitlines() if line.endswith('shared_controlE')]
    if len(shared) != 1 or int(shared[0][0],16) != data.memory_offset or int(shared[0][1],16) != 384:
        raise RuntimeError('RE5 IPC block outside first writable page or wrong ABI size')
    result = subprocess.run([str(sdk / 'tools/bin/npdmtool.exe'), str(build_profile), str(package / 'main.npdm')],
                            capture_output=True, text=True, check=True)
    if 'Failed' in result.stdout + result.stderr:
        raise RuntimeError('RE5 NPDM generation rejected fields: ' + result.stdout + result.stderr)
    print('Built guarded RE5 controls and graphics:', package)
    return package
