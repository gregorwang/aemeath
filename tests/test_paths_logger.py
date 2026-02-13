from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from core.logger import setup_logger
from core.paths import get_base_dir, get_cache_dir, get_log_dir, get_user_data_dir


class PathsLoggerTest(unittest.TestCase):
    def test_paths_exist(self) -> None:
        base = get_base_dir()
        self.assertTrue(base.exists())
        user_dir = get_user_data_dir()
        cache_dir = get_cache_dir()
        log_dir = get_log_dir()
        self.assertTrue(user_dir.exists())
        self.assertTrue(cache_dir.exists())
        self.assertTrue(log_dir.exists())

    def test_logger_writes_file(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            log_dir = Path(tmp)
            logger = setup_logger(log_dir, debug=True)
            logger.info("phase5 logger test")
            for handler in logger.handlers:
                handler.flush()
            log_file = log_dir / "app.log"
            self.assertTrue(log_file.exists())
            content = log_file.read_text(encoding="utf-8")
            self.assertIn("phase5 logger test", content)
            # Release file handle on Windows.
            for handler in list(logger.handlers):
                handler.close()
                logger.removeHandler(handler)

    def test_windows_user_data_dir_is_stable(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            local_appdata = Path(tmp)
            qt_path = str(local_appdata / "SomeQtDerivedPath")
            with patch("core.paths.sys.platform", "win32"), patch.dict(
                "core.paths.os.environ",
                {"LOCALAPPDATA": str(local_appdata)},
                clear=False,
            ), patch("core.paths.QStandardPaths.writableLocation", return_value=qt_path):
                user_dir = get_user_data_dir()

            self.assertEqual(user_dir, local_appdata / "CyberCompanion")
            self.assertTrue(user_dir.exists())


if __name__ == "__main__":
    unittest.main()
