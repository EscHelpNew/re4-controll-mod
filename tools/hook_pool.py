"""Apply guarded exlaunch hook-pool fixes only to the disposable build tree."""
from pathlib import Path
import re


_CONSTANTS = '''        constexpr size_t HookPoolSize = setting::JitSize;
        constexpr s64 MaxInstructions = 5;
        constexpr size_t TrampolineSize = MaxInstructions * 10;
        constexpr u64 HookMax = HookPoolSize / (TrampolineSize * sizeof(uint32_t));'''

_ALLOCATOR = '''    static Result AllocForTrampoline(uint32_t** rx, uint32_t** rw) {
        static_assert((TrampolineSize * sizeof(uint32_t)) % 8 == 0, "8-byte align");
        static volatile s32 index = -1;

        uint32_t i = __atomic_increase(&index);
\x20\x20\x20\x20\x20\x20\x20\x20
        if(i > HookMax)
            return result::HookTrampolineAllocFail;

        HookPool* rwptr = (HookPool*)s_HookJit.GetRw();
        HookPool* rxptr = (HookPool*)s_HookJit.GetRo();
        *rw = (*rwptr)[i];
        *rx = (*rxptr)[i];

        return result::Success;
    }'''


def _replace_once(text: str, before: str, after: str, description: str) -> str:
    if text.count(before) != 1:
        raise ValueError(f'Unexpected exlaunch {description} revision')
    return text.replace(before, after, 1)


def stage_hook_pool(stage: Path, root: Path) -> dict:
    """Validate source/budget first, then enlarge and bounds-check staged hooks.

    The upstream allocation has 200-byte slots. The original 4096-byte pool
    exhausted at our 22nd hook because its limit comparison admitted index20.
    A second page provides40 valid slots; the native assertion tracks installs.
    """
    stage, root = Path(stage), Path(root)
    setting_path = stage / 'source/program/setting.hpp'
    hook_path = stage / 'source/lib/hook/nx64/hook_impl.cpp'
    setting = setting_path.read_text(encoding='utf-8')
    hook = hook_path.read_text(encoding='utf-8')
    setting = _replace_once(setting, 'constexpr size_t JitSize = 0x1000;',
                            'constexpr size_t JitSize = 0x2000;', 'JIT size')
    hook = _replace_once(hook, _CONSTANTS, _CONSTANTS, 'hook-slot constants')
    hook = _replace_once(hook, _ALLOCATOR,
                         _ALLOCATOR.replace('if(i > HookMax)', 'if(i >= HookMax)'),
                         'trampoline allocator')
    required = 0
    for name in ('module.cpp', 'modern_move.inc'):
        source = (root / 'source/game' / name).read_text(encoding='utf-8')
        source = re.sub(r'//[^\n]*|/\*.*?\*/', '', source, flags=re.DOTALL)
        required += len(re.findall(r'\bInstallAt(?:FuncPtr|Offset|Ptr)\s*\(', source))
    jit_bytes, slot_bytes = 0x2000, 5 * 10 * 4
    capacity = jit_bytes // slot_bytes
    if jit_bytes % 0x1000 or required <= 0 or required > capacity:
        raise ValueError(f'Invalid hook-pool budget: {required} hooks, {capacity} slots')
    assertions = f'''
        static_assert(sizeof(uint32_t) == 4, "Hook slot instruction size changed");
        static_assert(setting::JitSize % 0x1000 == 0, "Hook pool must be page aligned");
        static_assert(HookMax >= {required}, "RE4 trampoline hook budget exceeds pool");'''
    hook = hook.replace(_CONSTANTS, _CONSTANTS + assertions, 1)
    # No staged or vendor file is modified until every input has been validated.
    setting_path.write_text(setting, encoding='utf-8')
    hook_path.write_text(hook, encoding='utf-8')
    return dict(required_hooks=required, capacity=capacity, slot_bytes=slot_bytes, jit_bytes=jit_bytes)
