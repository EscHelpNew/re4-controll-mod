import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

try:
    from tools.hook_pool import stage_hook_pool
except ModuleNotFoundError:
    stage_hook_pool = None


ROOT = Path(__file__).resolve().parents[1]
SETTING = Path('source/program/setting.hpp')
HOOK = Path('source/lib/hook/nx64/hook_impl.cpp')


class HookPoolTests(unittest.TestCase):
    def setUp(self):
        self.assertTrue(callable(stage_hook_pool), 'stage_hook_pool implementation is missing')
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.stage = Path(self.temp.name) / 'stage'
        self.root = Path(self.temp.name) / 'project'
        for name in (SETTING, HOOK):
            target = self.stage / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / 'third_party/exlaunch' / name, target)
        for name in ('module.cpp', 'modern_move.inc'):
            target = self.root / 'source/game' / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / 'source/game' / name, target)

    def test_capacity_and_native_compile_guard(self):
        before = {p: (ROOT / 'third_party/exlaunch' / p).read_bytes() for p in (SETTING, HOOK)}
        report = stage_hook_pool(self.stage, self.root)
        self.assertEqual(report, dict(required_hooks=27, capacity=40, slot_bytes=200, jit_bytes=8192))
        self.assertIn('constexpr size_t JitSize = 0x2000;', (self.stage / SETTING).read_text())
        hook = (self.stage / HOOK).read_text()
        self.assertIn('if(i >= HookMax)', hook)
        self.assertRegex(hook, r'static_assert\(HookMax\s*>=\s*27\b')
        for p, data in before.items():
            self.assertEqual((ROOT / 'third_party/exlaunch' / p).read_bytes(), data)

    def test_derived_hook_budget_tracks_new_install_and_ignores_comments(self):
        source = self.root / 'source/game/module.cpp'
        with source.open('a') as stream:
            stream.write('\nExtra::InstallAtPtr(pointer); // Fake::InstallAtOffset(1);\n')
            stream.write('/* Fake::InstallAtFuncPtr(pointer); */\n')
        self.assertEqual(stage_hook_pool(self.stage, self.root)['required_hooks'], 28)
        self.assertRegex((self.stage / HOOK).read_text(), r'HookMax\s*>=\s*28\b')

    def test_unknown_revisions_or_over_budget_leave_both_files_unchanged(self):
        cases = [(SETTING, 'JitSize = 0x1000', 'JitSize = 0x3000'),
                 (HOOK, 'MaxInstructions = 5', 'MaxInstructions = 6'),
                 (HOOK, 'MaxInstructions * 10', 'MaxInstructions * 11'),
                 (HOOK, 'if(i > HookMax)', 'if(i >= HookMax)'),
                 (HOOK, 'sizeof(uint32_t)', 'sizeof(uint64_t)')]
        originals = {p: (self.stage / p).read_text() for p in (SETTING, HOOK)}
        for name, old, new in cases:
            with self.subTest(pattern=old):
                for p, data in originals.items():
                    (self.stage / p).write_text(data)
                (self.stage / name).write_text(originals[name].replace(old, new))
                before = {p: (self.stage / p).read_bytes() for p in originals}
                with self.assertRaises(ValueError):
                    stage_hook_pool(self.stage, self.root)
                self.assertEqual(before, {p: (self.stage / p).read_bytes() for p in originals})
        for p, data in originals.items():
            (self.stage / p).write_text(data)
        with (self.root / 'source/game/module.cpp').open('a') as stream:
            stream.write('\nExtra::InstallAtOffset(1);' * 18)
        before = {p: (self.stage / p).read_bytes() for p in originals}
        with self.assertRaises(ValueError):
            stage_hook_pool(self.stage, self.root)
        self.assertEqual(before, {p: (self.stage / p).read_bytes() for p in originals})

    def compile_allocator(self, text, capacity, fixed):
        start = text.index('    static Result AllocForTrampoline(')
        end = text.index('\n    }', start) + len('\n    }')
        function = text[start:end]
        compiler = os.environ.get('CXX') or shutil.which('g++')
        if not compiler:
            compiler = next((str(p) for p in (Path('C:/msys64/ucrt64/bin/g++.exe'),
                                              Path('C:/msys64/mingw64/bin/g++.exe')) if p.exists()), None)
        self.assertIsNotNone(compiler, 'A C++ compiler is required for the actual allocator regression')
        source = self.stage / ('fixed.cpp' if fixed else 'old.cpp')
        source.write_text(r'''
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
using Result=unsigned;using s32=std::int32_t;
namespace result {constexpr Result Success=0,HookTrampolineAllocFail=1;}
#define __atomic_increase(p) __sync_add_and_fetch(p,1)
constexpr unsigned TrampolineSize=50;
constexpr unsigned HookMax=CAPACITY;
using HookPool=uint32_t[HookMax][TrampolineSize];
struct Jit {
 std::array<uint32_t,(HookMax*200+1024)/4> area{};
 uintptr_t GetRw(){return reinterpret_cast<uintptr_t>(area.data());}
 uintptr_t GetRo(){return GetRw();}
} s_HookJit;
''' .replace('CAPACITY', str(capacity)) + function + r'''
int main(){
 auto* base=s_HookJit.area.data();
 constexpr unsigned pool_bytes=POOL_BYTES;
 std::memset(reinterpret_cast<unsigned char*>(base)+pool_bytes,0xA5,512);
 for(unsigned i=0;i<HookMax;++i){
  uint32_t* rx=nullptr;uint32_t* rw=nullptr;
  assert(AllocForTrampoline(&rx,&rw)==0);
  assert(rx==base+i*50&&rw==rx);
  std::memset(rw,0x11,200);
 }
 auto* sentinel=reinterpret_cast<uint32_t*>(uintptr_t(0x1234));
 uint32_t* rx=sentinel;uint32_t* rw=sentinel;
 const auto code=AllocForTrampoline(&rx,&rw);
 if(FIXED){
  assert(code==result::HookTrampolineAllocFail&&rx==sentinel&&rw==sentinel);
  const auto* guard=reinterpret_cast<unsigned char*>(base)+pool_bytes;
  for(unsigned i=0;i<512;++i)assert(guard[i]==0xA5);
 }else{
  assert(code==0&&rw==base+HookMax*50); // Old21st invalid slot is accepted.
  assert(reinterpret_cast<uintptr_t>(rw)+200>reinterpret_cast<uintptr_t>(base)+pool_bytes);
  std::memset(rw,0x22,200); // Padded witness keeps this deliberate overflow bounded.
  assert(*(reinterpret_cast<unsigned char*>(base)+pool_bytes)==0x22);
  rx=rw=sentinel;
  assert(AllocForTrampoline(&rx,&rw)==result::HookTrampolineAllocFail);
  assert(rx==sentinel&&rw==sentinel); // Old22nd allocation abort condition.
 }
}
'''.replace('POOL_BYTES', '8192' if fixed else '4096').replace('FIXED', 'true' if fixed else 'false'))
        binary = source.with_suffix('.exe')
        compiled = subprocess.run([compiler, '-std=c++20', '-Wall', '-Wextra', '-Werror', '-static',
                                   str(source), '-o', str(binary)], capture_output=True, text=True, timeout=20)
        self.assertEqual(compiled.returncode, 0, compiled.stdout + compiled.stderr)
        executed = subprocess.run([str(binary)], capture_output=True, text=True, timeout=20)
        self.assertEqual(executed.returncode, 0, executed.stdout + executed.stderr)

    def test_actual_allocator_boundary_and_old_crash_witness(self):
        original = (self.stage / HOOK).read_text()
        self.compile_allocator(original, 20, False)
        stage_hook_pool(self.stage, self.root)
        self.compile_allocator((self.stage / HOOK).read_text(), 40, True)


if __name__ == '__main__':
    unittest.main()
