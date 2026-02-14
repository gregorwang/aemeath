from __future__ import annotations

import sys
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from ai.gaze_tracker import GazeData
from core.director import Director
from core.state_machine import EntityState


class _StateMachineStub:
    def __init__(self, state: EntityState) -> None:
        self.current_state = state


class _NoFaceSubject:
    NO_FACE_TEST_MIN_ABSENCE_SECONDS = Director.NO_FACE_TEST_MIN_ABSENCE_SECONDS
    NO_FACE_TEST_COOLDOWN_SECONDS = Director.NO_FACE_TEST_COOLDOWN_SECONDS

    def __init__(self, state: EntityState = EntityState.ENGAGED) -> None:
        self._camera_enabled = True
        self._state_machine = _StateMachineStub(state)
        self._voice_trajectory_playing = False
        self._no_face_absent_since: float | None = None
        self._last_no_face_test_at = 0.0
        self._no_face_streak_triggered = False

    def _trigger_no_face_test(self) -> None:
        return

    def _reset_no_face_tracker(self) -> None:
        self._no_face_absent_since = None
        self._no_face_streak_triggered = False


class DirectorNoFaceTest(unittest.TestCase):
    def test_face_detected_resets_tracker(self) -> None:
        subject = _NoFaceSubject()
        subject._no_face_absent_since = 10.0
        subject._no_face_streak_triggered = True

        Director._maybe_trigger_no_face_test(subject, GazeData(face_detected=True))

        self.assertIsNone(subject._no_face_absent_since)
        self.assertFalse(subject._no_face_streak_triggered)

    def test_no_face_beyond_threshold_schedules_trigger(self) -> None:
        subject = _NoFaceSubject()
        subject._no_face_absent_since = 90.0

        with patch("core.director.time.monotonic", return_value=100.0):
            with patch("core.director.QTimer.singleShot") as single_shot:
                Director._maybe_trigger_no_face_test(subject, GazeData(face_detected=False))

        self.assertTrue(subject._no_face_streak_triggered)
        self.assertEqual(subject._last_no_face_test_at, 100.0)
        single_shot.assert_called_once()

    def test_cooldown_blocks_retrigger(self) -> None:
        subject = _NoFaceSubject()
        subject._no_face_absent_since = 90.0
        subject._last_no_face_test_at = 95.0

        with patch("core.director.time.monotonic", return_value=100.0):
            with patch("core.director.QTimer.singleShot") as single_shot:
                Director._maybe_trigger_no_face_test(subject, GazeData(face_detected=False))

        self.assertFalse(subject._no_face_streak_triggered)
        single_shot.assert_not_called()


if __name__ == "__main__":
    unittest.main()
