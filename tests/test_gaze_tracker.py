from __future__ import annotations

import sys
import types
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

try:
    import PySide6  # type: ignore  # noqa: F401
except ModuleNotFoundError:
    pyside_module = types.ModuleType("PySide6")
    qtcore_module = types.ModuleType("PySide6.QtCore")

    class _StubQThread:
        def __init__(self, parent=None):
            self._parent = parent

    class _StubSignal:
        def __init__(self, *args, **kwargs):
            self._args = args
            self._kwargs = kwargs

    qtcore_module.QThread = _StubQThread
    qtcore_module.Signal = _StubSignal
    pyside_module.QtCore = qtcore_module
    sys.modules["PySide6"] = pyside_module
    sys.modules["PySide6.QtCore"] = qtcore_module

from ai.gaze_tracker import GazeTracker


class _FakePoint:
    def __init__(self, x: float, y: float, visibility: float = 1.0):
        self.x = x
        self.y = y
        self.visibility = visibility


class _FakeLandmarks:
    def __init__(self, x: float, y: float):
        self.landmark = [_FakePoint(0.0, 0.0) for _ in range(500)]
        self.landmark[1] = _FakePoint(x, y, 0.9)
        self.landmark[61] = _FakePoint(0.40, 0.55)
        self.landmark[291] = _FakePoint(0.60, 0.55)
        self.landmark[13] = _FakePoint(0.50, 0.50)
        self.landmark[14] = _FakePoint(0.50, 0.54)
        self.landmark[234] = _FakePoint(0.30, 0.50)
        self.landmark[454] = _FakePoint(0.70, 0.50)
        self.landmark[33] = _FakePoint(0.38, 0.42)
        self.landmark[263] = _FakePoint(0.62, 0.42)
        self.landmark[105] = _FakePoint(0.38, 0.36)
        self.landmark[334] = _FakePoint(0.62, 0.36)


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
        self.assertIn(gaze.emotion_label, {"happy", "neutral", "angry"})


if __name__ == "__main__":
    unittest.main()
