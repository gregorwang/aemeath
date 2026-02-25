from __future__ import annotations

import json
import sys
import unittest
from datetime import datetime, timedelta
from pathlib import Path
from tempfile import TemporaryDirectory

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from core.asset_manager import AssetManager


class AssetManagerPhase2Test(unittest.TestCase):
    def setUp(self) -> None:
        self.manager = AssetManager(ROOT / "characters" / "default")

    def test_loads_idle_and_panic_events(self) -> None:
        self.assertGreaterEqual(len(self.manager.idle_scripts), 1)
        self.assertGreaterEqual(len(self.manager.panic_scripts), 1)

    def test_time_range_matching_default_and_wrap(self) -> None:
        late = datetime(2026, 2, 10, 23, 30)
        noon = datetime(2026, 2, 10, 12, 30)
        self.assertTrue(self.manager._match_time_range("default", noon))
        self.assertTrue(self.manager._match_time_range("22:00-06:00", late))
        self.assertTrue(self.manager._match_time_range("22:00-06:00", datetime(2026, 2, 10, 2, 0)))
        self.assertFalse(self.manager._match_time_range("12:00-13:00", late))
        self.assertTrue(self.manager._match_time_range("12:00-13:00", datetime(2026, 2, 10, 12, 0)))
        self.assertFalse(self.manager._match_time_range("12:00-13:00", datetime(2026, 2, 10, 13, 0)))

    def test_cooldown_blocks_recent_script(self) -> None:
        script = self.manager.idle_scripts[0]
        script.cooldown_minutes = 10
        now = datetime.now()
        self.manager._last_triggered_at[script.id] = now
        self.assertTrue(self.manager._in_cooldown(script, now + timedelta(minutes=5)))
        self.assertFalse(self.manager._in_cooldown(script, now + timedelta(minutes=11)))

    def test_reload_refreshes_scripts_from_disk(self) -> None:
        with TemporaryDirectory() as td:
            root = Path(td)
            scripts_path = root / "scripts.json"
            scripts_path.write_text(
                json.dumps(
                    {
                        "scripts": [{"id": "idle_1", "text": "old text", "time_range": "default"}],
                        "panic_events": [{"id": "panic_1", "text": "panic old", "time_range": "default"}],
                    },
                    ensure_ascii=False,
                ),
                encoding="utf-8",
            )
            manager = AssetManager(root)
            self.assertEqual(manager.scripts_path, scripts_path)
            self.assertEqual(manager.idle_scripts[0].text, "old text")

            scripts_path.write_text(
                json.dumps(
                    {
                        "scripts": [{"id": "idle_1", "text": "new text", "time_range": "default"}],
                        "panic_events": [{"id": "panic_1", "text": "panic new", "time_range": "default"}],
                    },
                    ensure_ascii=False,
                ),
                encoding="utf-8",
            )
            manager.reload()

            self.assertEqual(manager.idle_scripts[0].text, "new text")
            self.assertEqual(manager.panic_scripts[0].text, "panic new")


if __name__ == "__main__":
    unittest.main()
