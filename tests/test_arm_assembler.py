"""Host ABI probes must assemble ARM64 even without the Switch SDK."""
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest.mock import patch
import unittest

from tools.arm_assembler import assembler_command


class AssemblerSelectionTests(unittest.TestCase):
    def test_installed_sdk_is_preferred_to_host_clang(self):
        with TemporaryDirectory() as temporary:
            sdk = Path(temporary)
            compiler = sdk / 'devkitA64/bin/aarch64-none-elf-gcc.exe'
            compiler.parent.mkdir(parents=True)
            compiler.touch()
            with patch('tools.arm_assembler.shutil.which', return_value='host-clang'):
                self.assertEqual(assembler_command(sdk), [str(compiler)])

    def test_missing_sdk_uses_clang_with_explicit_arm64_elf_target(self):
        with TemporaryDirectory() as temporary, patch('tools.arm_assembler.shutil.which', return_value='host-clang'):
            command = assembler_command(Path(temporary) / 'absent-sdk')
            self.assertEqual(command, ['host-clang', '--target=aarch64-none-elf'])

    def test_missing_assemblers_fail_with_installation_guidance(self):
        with TemporaryDirectory() as temporary, patch('tools.arm_assembler.shutil.which', return_value=None):
            with self.assertRaisesRegex(RuntimeError, 'devkitA64.*Clang.*PATH'):
                assembler_command(Path(temporary) / 'absent-sdk')
