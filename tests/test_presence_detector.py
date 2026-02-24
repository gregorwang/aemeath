from __future__ import annotations

import sys
import unittest
from pathlib import Path
from unittest.mock import patch

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

    def test_absent_when_no_face_and_motion_still_for_long(self) -> None:
        detector = PresenceDetector()
        detector.FACE_ABSENT_FRAMES = 999
        gaze = GazeData(face_detected=False, motion_score=0.5, brightness=120.0)
        for _ in range(detector.STILL_NO_FACE_FRAMES):
            state = detector.determine_presence(360_000, gaze)
        self.assertEqual(state, PresenceState.ABSENT)

    def test_absent_when_no_face_and_dark_for_long(self) -> None:
        detector = PresenceDetector()
        detector.FACE_ABSENT_FRAMES = 999
        gaze = GazeData(face_detected=False, motion_score=10.0, brightness=10.0)
        max_frames = max(detector.DARK_NO_FACE_FRAMES, 500)
        for _ in range(max_frames):
            state = detector.determine_presence(360_000, gaze)
        self.assertEqual(state, PresenceState.ABSENT)

    def test_stillness_threshold_can_trigger_earlier_than_face_absent_baseline(self) -> None:
        detector = PresenceDetector()
        detector.FACE_ABSENT_FRAMES = 999
        gaze = GazeData(face_detected=False, motion_score=0.2, brightness=120.0)
        state = PresenceState.UNKNOWN
        for _ in range(detector.STILL_NO_FACE_FRAMES):
            state = detector.determine_presence(360_000, gaze)
        self.assertEqual(state, PresenceState.ABSENT)

    def test_absence_threshold_uses_elapsed_time_under_low_fps(self) -> None:
        detector = PresenceDetector(target_fps=15)
        gaze = GazeData(face_detected=False, motion_score=10.0, brightness=200.0)
        fake_time_points = [i * 0.2 for i in range(12)]  # ~5 FPS runtime

        state = PresenceState.UNKNOWN
        with patch("core.presence_detector.time.monotonic", side_effect=fake_time_points):
            for _ in fake_time_points:
                state = detector.determine_presence(360_000, gaze)

        self.assertEqual(state, PresenceState.ABSENT)


if __name__ == "__main__":
    unittest.main()
