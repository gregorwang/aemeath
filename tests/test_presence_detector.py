from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from ai.gaze_tracker import GazeData
from core.presence_detector import PresenceDetector, PresenceState


class PresenceDetectorTest(unittest.TestCase):
    def test_present_active_when_idle_short(self) -> None:
        detector = PresenceDetector()
        state = detector.determine_presence(30_000, GazeData(face_detected=False))
        self.assertEqual(state, PresenceState.PRESENT_ACTIVE)

    def test_present_passive_when_face_detected_and_idle_long(self) -> None:
        detector = PresenceDetector()
        state = detector.determine_presence(360_000, GazeData(face_detected=True))
        self.assertEqual(state, PresenceState.PRESENT_PASSIVE)

    def test_absent_when_face_missing_for_many_frames(self) -> None:
        detector = PresenceDetector()
        for _ in range(detector.FACE_ABSENT_FRAMES):
            state = detector.determine_presence(360_000, GazeData(face_detected=False))
        self.assertEqual(state, PresenceState.ABSENT)

    def test_unknown_when_no_camera_data(self) -> None:
        detector = PresenceDetector()
        state = detector.determine_presence(100_000, None)
        self.assertEqual(state, PresenceState.UNKNOWN)


if __name__ == "__main__":
    unittest.main()

