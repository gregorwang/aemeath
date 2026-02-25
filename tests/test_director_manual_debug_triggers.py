from __future__ import annotations

import logging
import sys
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from core.director import Director
from core.state_machine import EntityState


class _StateMachineStub:
    def __init__(self, state: EntityState) -> None:
        self.current_state = state


class _ManualDebugSubject:
    def __init__(self, *, state: EntityState = EntityState.ENGAGED, voice_trajectory_playing: bool = False) -> None:
        self.LOGGER = logging.getLogger("CyberCompanionTest")
        self._voice_trajectory_playing = voice_trajectory_playing
        self._state_machine = _StateMachineStub(state)
        self._suppress_engaged_script_once = False
        self.sad_comfort_calls = 0
        self.no_face_calls = 0
        self.summon_calls = 0

    def _trigger_sad_comfort(self) -> None:
        self.sad_comfort_calls += 1

    def _trigger_no_face_test(self) -> None:
        self.no_face_calls += 1

    def summon_now(self) -> bool:
        self.summon_calls += 1
        if self._state_machine.current_state == EntityState.HIDDEN:
            self._state_machine.current_state = EntityState.ENGAGED
        return True


class DirectorManualDebugTriggerTest(unittest.TestCase):
    def test_trigger_sad_comfort_debug_schedules_callback(self) -> None:
        subject = _ManualDebugSubject()

        with patch("core.director.QTimer.singleShot") as single_shot:
            result = Director.trigger_sad_comfort_debug(subject, source="test")

        self.assertTrue(result)
        single_shot.assert_called_once()
        delay, callback = single_shot.call_args.args
        self.assertEqual(delay, 0)
        callback()
        self.assertEqual(subject.sad_comfort_calls, 1)

    def test_trigger_sad_comfort_debug_returns_false_when_fleeing(self) -> None:
        subject = _ManualDebugSubject(state=EntityState.FLEEING)

        with patch("core.director.QTimer.singleShot") as single_shot:
            result = Director.trigger_sad_comfort_debug(subject, source="test")

        self.assertFalse(result)
        single_shot.assert_not_called()
        self.assertEqual(subject.sad_comfort_calls, 0)

    def test_trigger_no_face_test_debug_schedules_callback(self) -> None:
        subject = _ManualDebugSubject()

        with patch("core.director.QTimer.singleShot") as single_shot:
            result = Director.trigger_no_face_test_debug(subject, source="test")

        self.assertTrue(result)
        single_shot.assert_called_once()
        delay, callback = single_shot.call_args.args
        self.assertEqual(delay, 0)
        callback()
        self.assertEqual(subject.no_face_calls, 1)

    def test_trigger_no_face_test_debug_returns_false_when_voice_trajectory_active(self) -> None:
        subject = _ManualDebugSubject(voice_trajectory_playing=True)

        with patch("core.director.QTimer.singleShot") as single_shot:
            result = Director.trigger_no_face_test_debug(subject, source="test")

        self.assertFalse(result)
        single_shot.assert_not_called()
        self.assertEqual(subject.no_face_calls, 0)

    def test_trigger_sad_comfort_debug_summons_when_hidden(self) -> None:
        subject = _ManualDebugSubject(state=EntityState.HIDDEN)

        with patch("core.director.QTimer.singleShot") as single_shot:
            result = Director.trigger_sad_comfort_debug(subject, source="test")

        self.assertTrue(result)
        self.assertEqual(subject.summon_calls, 1)
        single_shot.assert_called_once()

    def test_trigger_no_face_test_debug_summons_when_hidden(self) -> None:
        subject = _ManualDebugSubject(state=EntityState.HIDDEN)

        with patch("core.director.QTimer.singleShot") as single_shot:
            result = Director.trigger_no_face_test_debug(subject, source="test")

        self.assertTrue(result)
        self.assertEqual(subject.summon_calls, 1)
        single_shot.assert_called_once()


if __name__ == "__main__":
    unittest.main()
