import json
import unittest
from pathlib import Path


class GamePermissionsTests(unittest.TestCase):
    def test_saved_controls_are_readable_without_debug_mode(self):
        profile = Path(__file__).resolve().parents[1] / 'profiles/010099A00BC1E000/npdm.json'
        permissions = int(json.loads(profile.read_text())['filesystem_access']['permissions'], 0)
        # Atmosphere AccessControlBits::SdCard is bit21. Debug-only bit62
        # is masked out in ordinary user mode and cannot load saved controls.
        self.assertTrue(permissions & (1 << 21), 'Persisted controls need ordinary SD permission')
        self.assertEqual(permissions & ~(1 << 62), 1 << 21, 'Grant only the additional SD capability')
