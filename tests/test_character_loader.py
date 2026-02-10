from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from core.character_loader import CharacterLoader


class CharacterLoaderTest(unittest.TestCase):
    def setUp(self) -> None:
        self.loader = CharacterLoader(ROOT / "characters")

    def test_scan_and_load_default_character(self) -> None:
        manifests = self.loader.scan_characters()
        ids = {m.get("id") for m in manifests if isinstance(m, dict)}
        self.assertIn("default", ids)

        pkg = self.loader.load_character("default")
        self.assertIsNotNone(pkg)
        assert pkg is not None
        self.assertEqual(pkg.character_id, "default")
        self.assertTrue(pkg.dialogue_path.exists())
        self.assertTrue((pkg.sprites_dir / "peek.png").exists())


if __name__ == "__main__":
    unittest.main()

