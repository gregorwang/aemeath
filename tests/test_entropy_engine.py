from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from core.entropy_engine import EntropyEngine


class EntropyEngineTest(unittest.TestCase):
    def test_jitter_threshold_bounds(self) -> None:
        base = 180_000
        for _ in range(50):
            value = EntropyEngine.jitter_threshold(base, (-30, 60))
            self.assertGreaterEqual(value, 60_000)
            self.assertLessEqual(value, base + 60_000)

    def test_random_y_position_range(self) -> None:
        top = 100
        height = 1000
        for _ in range(50):
            y = EntropyEngine.random_y_position(top, height)
            self.assertGreaterEqual(y, int(top + height * 0.2))
            self.assertLessEqual(y, int(top + height * 0.8))


if __name__ == "__main__":
    unittest.main()

