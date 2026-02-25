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


class _LoggerStub:
    def info(self, *_args, **_kwargs) -> None:
        return

    def debug(self, *_args, **_kwargs) -> None:
        return

    def warning(self, *_args, **_kwargs) -> None:
        return


class _StateMachineStub:
    def __init__(self, state: EntityState) -> None:
        self.current_state = state


class _NoFaceSubject:
    NO_FACE_TEST_MIN_ABSENCE_SECONDS = Director.NO_FACE_TEST_MIN_ABSENCE_SECONDS
    NO_FACE_TEST_COOLDOWN_SECONDS = Director.NO_FACE_TEST_COOLDOWN_SECONDS
    USER_RETURN_MIN_SPEAK_SECONDS = Director.USER_RETURN_MIN_SPEAK_SECONDS

    def __init__(self, state: EntityState = EntityState.ENGAGED) -> None:
        self.LOGGER = _LoggerStub()
        self._camera_enabled = True
        self._state_machine = _StateMachineStub(state)
        self._voice_trajectory_playing = False
        self._no_face_absent_since: float | None = None
        self._user_left_at: float | None = None
        self._last_no_face_test_at = 0.0
        self._no_face_streak_triggered = False
        self.trigger_calls: list[float | None] = []

    def _trigger_no_face_test(self, away_seconds: float | None = None) -> None:
        self.trigger_calls.append(away_seconds)

    def _reset_no_face_tracker(self) -> None:
        self._no_face_absent_since = None
        self._user_left_at = None
        self._no_face_streak_triggered = False

    def _maybe_greet_user_return(self) -> None:
        Director._maybe_greet_user_return(self)


class DirectorNoFaceTest(unittest.TestCase):
    def test_face_detected_resets_tracker(self) -> None:
        subject = _NoFaceSubject()
        subject._no_face_absent_since = 10.0
        subject._user_left_at = None
        subject._no_face_streak_triggered = True

        Director._maybe_trigger_no_face_test(subject, GazeData(face_detected=True))

        self.assertIsNone(subject._no_face_absent_since)
        self.assertIsNone(subject._user_left_at)
        self.assertFalse(subject._no_face_streak_triggered)

    def test_no_face_beyond_threshold_marks_user_away_without_speaking(self) -> None:
        subject = _NoFaceSubject()
        subject._no_face_absent_since = 90.0

        with patch("core.director.time.monotonic", return_value=100.0):
            with patch("core.director.QTimer.singleShot") as single_shot:
                Director._maybe_trigger_no_face_test(subject, GazeData(face_detected=False))

        self.assertEqual(subject._user_left_at, 90.0)
        self.assertTrue(subject._no_face_streak_triggered)
        self.assertEqual(subject._last_no_face_test_at, 0.0)
        single_shot.assert_not_called()

    def test_return_under_30_seconds_skips_greeting(self) -> None:
        subject = _NoFaceSubject()
        subject._user_left_at = 90.0
        subject._no_face_streak_triggered = True

        with patch("core.director.time.monotonic", return_value=100.0):
            with patch("core.director.QTimer.singleShot") as single_shot:
                Director._maybe_trigger_no_face_test(subject, GazeData(face_detected=True))

        self.assertIsNone(subject._user_left_at)
        self.assertFalse(subject._no_face_streak_triggered)
        single_shot.assert_not_called()

    def test_return_after_threshold_schedules_greeting(self) -> None:
        subject = _NoFaceSubject()
        subject._user_left_at = 50.0
        subject._no_face_streak_triggered = True

        with patch("core.director.time.monotonic", return_value=100.0):
            with patch("core.director.QTimer.singleShot") as single_shot:
                Director._maybe_trigger_no_face_test(subject, GazeData(face_detected=True))

        self.assertFalse(subject._no_face_streak_triggered)
        self.assertEqual(subject._last_no_face_test_at, 100.0)
        single_shot.assert_called_once()
        _delay, callback = single_shot.call_args.args
        callback()
        self.assertEqual(subject.trigger_calls, [50.0])

    def test_cooldown_blocks_return_greeting(self) -> None:
        subject = _NoFaceSubject()
        subject._user_left_at = 40.0
        subject._no_face_streak_triggered = True
        subject._last_no_face_test_at = 95.0

        with patch("core.director.time.monotonic", return_value=100.0):
            with patch("core.director.QTimer.singleShot") as single_shot:
                Director._maybe_trigger_no_face_test(subject, GazeData(face_detected=True))

        self.assertEqual(subject._last_no_face_test_at, 95.0)
        self.assertEqual(subject.trigger_calls, [])
        single_shot.assert_not_called()


if __name__ == "__main__":
    unittest.main()
