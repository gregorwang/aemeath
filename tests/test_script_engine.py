from __future__ import annotations

import sys
import unittest
from datetime import datetime, timedelta
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from core.asset_manager import Script
from core.script_engine import ScriptEngine


class ScriptEngineTest(unittest.TestCase):
    def setUp(self) -> None:
        self.idle_scripts = [
            Script(id="night", text="night", time_range="22:00-06:00", probability=1.0, cooldown_minutes=30),
            Script(id="default", text="default", time_range="default", probability=1.0, cooldown_minutes=0),
        ]
        self.panic_scripts = [
            Script(id="panic", text="panic", time_range="default", probability=1.0, cooldown_minutes=0, event_type="panic")
        ]
        self.engine = ScriptEngine(self.idle_scripts, self.panic_scripts)

    def test_time_match_wraps_midnight(self) -> None:
        script = self.engine.select_idle_script(now=datetime(2026, 2, 10, 23, 0))
        self.assertIsNotNone(script)
        self.assertEqual(script.id, "night")

    def test_cooldown_prevents_repeat(self) -> None:
        now = datetime(2026, 2, 10, 23, 0)
        first = self.engine.select_idle_script(now=now)
        self.assertIsNotNone(first)
        second = self.engine.select_idle_script(now=now + timedelta(minutes=10))
        self.assertIsNotNone(second)
        self.assertNotEqual(first.id, second.id)

    def test_select_panic(self) -> None:
        script = self.engine.select_panic_script(now=datetime(2026, 2, 10, 12, 0))
        self.assertIsNotNone(script)
        self.assertEqual(script.id, "panic")


if __name__ == "__main__":
    unittest.main()

