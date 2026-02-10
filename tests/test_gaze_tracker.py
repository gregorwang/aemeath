from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from ai.gaze_tracker import GazeTracker


class _FakePoint:
    def __init__(self, x: float, y: float, visibility: float = 1.0):
        self.x = x
        self.y = y
        self.visibility = visibility


class _FakeLandmarks:
    def __init__(self, x: float, y: float):
        self.landmark = [_FakePoint(0.0, 0.0) for _ in range(10)]
        self.landmark[1] = _FakePoint(x, y, 0.9)


class GazeTrackerTest(unittest.TestCase):
    def test_calculate_gaze_normalization(self) -> None:
        tracker = GazeTracker()
        landmarks = _FakeLandmarks(0.75, 0.25)
        gaze = tracker._calculate_gaze(landmarks)
        self.assertTrue(gaze.face_detected)
        self.assertGreaterEqual(gaze.face_x, -1.0)
        self.assertLessEqual(gaze.face_x, 1.0)
        self.assertGreaterEqual(gaze.face_y, -1.0)
        self.assertLessEqual(gaze.face_y, 1.0)
        self.assertGreater(gaze.confidence, 0.0)


if __name__ == "__main__":
    unittest.main()

