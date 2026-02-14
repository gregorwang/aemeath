from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from ui._file_helpers import normalize_asset_path, path_exists


class UIFileHelpersTest(unittest.TestCase):
    def test_normalize_asset_path_for_filesystem_path(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            rel_path = Path(tmp_dir) / "asset.gif"
            expected = str(rel_path.resolve())
            self.assertEqual(normalize_asset_path(str(rel_path)), expected)

    def test_normalize_asset_path_keeps_qt_resource_path(self) -> None:
        self.assertEqual(normalize_asset_path(":/characters/state1.gif"), ":/characters/state1.gif")

    def test_path_exists_for_filesystem_file(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            file_path = Path(tmp_dir) / "sample.txt"
            file_path.write_text("ok", encoding="utf-8")
            self.assertTrue(path_exists(str(file_path)))

    def test_path_exists_for_empty_path(self) -> None:
        self.assertFalse(path_exists(""))
        self.assertFalse(path_exists("   "))


if __name__ == "__main__":
    unittest.main()

