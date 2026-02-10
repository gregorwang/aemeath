from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from core.mood_system import MoodSystem


class MoodSystemTest(unittest.TestCase):
    def test_mood_changes_and_bounds(self) -> None:
        mood = MoodSystem(initial_mood=0.5)
        mood.on_interacted()
        self.assertAlmostEqual(mood.mood, 0.6, places=3)
        mood.on_engaged()
        self.assertAlmostEqual(mood.mood, 0.75, places=3)
        for _ in range(20):
            mood.on_dismissed()
        self.assertGreaterEqual(mood.mood, 0.0)
        self.assertLessEqual(mood.mood, 1.0)

    def test_natural_decay_towards_mid(self) -> None:
        high = MoodSystem(initial_mood=0.9)
        high.natural_decay()
        self.assertLess(high.mood, 0.9)

        low = MoodSystem(initial_mood=0.1)
        low.natural_decay()
        self.assertGreater(low.mood, 0.1)

    def test_mood_label(self) -> None:
        mood = MoodSystem(initial_mood=0.85)
        self.assertEqual(mood.mood_label, "兴奋")


if __name__ == "__main__":
    unittest.main()

