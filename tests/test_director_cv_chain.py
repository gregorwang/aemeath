from __future__ import annotations

import sys
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from ai.gaze_tracker import GazeData
from core.director import BehaviorMode, Director
from core.presence_detector import PresenceState
from core.state_machine import EntityState


class _StateMachineStub:
    def __init__(self, state: EntityState) -> None:
        self.current_state = state
        self.transitions: list[EntityState] = []

    def transition_to(self, new_state: EntityState) -> bool:
        self.transitions.append(new_state)
        self.current_state = new_state
        return True


class _PresenceDetectorStub:
    def __init__(self, result: PresenceState) -> None:
        self._result = result
        self.calls: list[tuple[int, GazeData | None]] = []

    def determine_presence(self, idle_time_ms: int, gaze_data: GazeData | None) -> PresenceState:
        self.calls.append((int(idle_time_ms), gaze_data))
        return self._result


class _PresenceRefreshSubject:
    def __init__(self) -> None:
        self._latest_idle_time_ms = 0
        self.refresh_calls = 0

    def _refresh_presence_state(self) -> None:
        self.refresh_calls += 1


class _GazeUpdateSubject:
    def __init__(self) -> None:
        self._latest_gaze_data = GazeData(face_detected=False)
        self._eye_tracking_enabled = False
        self._ascii_renderer = None
        self._state_machine = _StateMachineStub(EntityState.ENGAGED)
        self._current_ascii_template = ""
        self.refresh_calls = 0
        self.no_face_calls = 0
        self.expression_calls = 0
        self.sad_comfort_calls = 0

    def _refresh_presence_state(self) -> None:
        self.refresh_calls += 1

    def _maybe_trigger_no_face_test(self, _gaze_data: GazeData) -> None:
        self.no_face_calls += 1

    def _track_expression_state(self, _gaze_data: GazeData) -> None:
        self.expression_calls += 1

    def _maybe_trigger_sad_comfort(self, _gaze_data: GazeData) -> None:
        self.sad_comfort_calls += 1


class _GazeFollowWindowStub:
    def __init__(self) -> None:
        self.calls: list[tuple[float, float, bool, float]] = []

    def apply_gaze_follow(
        self,
        face_x: float,
        face_y: float,
        *,
        face_detected: bool,
        confidence: float,
    ) -> None:
        self.calls.append((float(face_x), float(face_y), bool(face_detected), float(confidence)))


class _GazeFollowSubject(_GazeUpdateSubject):
    def __init__(self) -> None:
        super().__init__()
        self._camera_enabled = True
        self._eye_tracking_enabled = True
        self._entity_window = _GazeFollowWindowStub()


class _PresenceFlowSubject:
    def __init__(self, *, camera_enabled: bool, result: PresenceState) -> None:
        self._camera_enabled = bool(camera_enabled)
        self._latest_idle_time_ms = 360_000
        self._latest_gaze_data = GazeData(face_detected=True)
        self._latest_presence_state = PresenceState.UNKNOWN
        self._presence_detector = _PresenceDetectorStub(result)
        self.applied_states: list[PresenceState] = []
        self.LOGGER = SimpleNamespace(debug=lambda *_args, **_kwargs: None)

    def _apply_presence_state(self, state: PresenceState) -> None:
        self.applied_states.append(state)


class _PresenceBehaviorSubject:
    def __init__(self) -> None:
        self._voice_trajectory_playing = False
        self.passive_calls = 0
        self.deep_sleep_calls = 0
        self.active_calls = 0

    def _enter_passive_companion(self) -> None:
        self.passive_calls += 1

    def _enter_deep_sleep(self) -> None:
        self.deep_sleep_calls += 1

    def _on_presence_back_active(self) -> None:
        self.active_calls += 1


class _MoodStub:
    def __init__(self) -> None:
        self.deltas: list[float] = []

    def apply_delta(self, delta: float) -> None:
        self.deltas.append(float(delta))


class _ExpressionSubject:
    EXPRESSION_STATE_MAP = Director.EXPRESSION_STATE_MAP
    EXPRESSION_MOOD_DELTA = Director.EXPRESSION_MOOD_DELTA

    def __init__(self) -> None:
        self._state_machine = _StateMachineStub(EntityState.PEEKING)
        self._expression_votes = {"happy": 0, "neutral": 0, "angry": 0, "sad": 0}
        self._stable_expression = "neutral"
        self._last_expression_visual_at = 0.0
        self._mood_system = _MoodStub()
        self.LOGGER = SimpleNamespace(debug=lambda *_args, **_kwargs: None)
        self.entity_state_calls: list[tuple[str, bool]] = []

    def _resolve_effective_behavior_mode(self) -> BehaviorMode:
        return BehaviorMode.IDLE

    def _set_entity_state(self, state_name: str, *, as_base: bool = True) -> bool:
        self.entity_state_calls.append((state_name, as_base))
        return True


class DirectorCvChainTest(unittest.TestCase):
    def test_idle_update_refreshes_presence(self) -> None:
        subject = _PresenceRefreshSubject()

        Director._on_idle_time_updated(subject, 12345)

        self.assertEqual(subject._latest_idle_time_ms, 12345)
        self.assertEqual(subject.refresh_calls, 1)

    def test_gaze_update_refreshes_presence_before_visual_hooks(self) -> None:
        subject = _GazeUpdateSubject()
        gaze_data = GazeData(face_detected=True, emotion_label="neutral", emotion_score=0.1)

        Director._on_gaze_updated(subject, gaze_data)

        self.assertIs(subject._latest_gaze_data, gaze_data)
        self.assertEqual(subject.refresh_calls, 1)
        self.assertEqual(subject.no_face_calls, 1)
        self.assertEqual(subject.expression_calls, 1)
        self.assertEqual(subject.sad_comfort_calls, 1)

    def test_gaze_update_applies_window_follow_when_enabled(self) -> None:
        subject = _GazeFollowSubject()
        gaze_data = GazeData(face_detected=True, face_x=0.4, face_y=-0.2, confidence=0.8)

        Director._on_gaze_updated(subject, gaze_data)

        self.assertEqual(len(subject._entity_window.calls), 1)
        follow_call = subject._entity_window.calls[0]
        self.assertAlmostEqual(follow_call[0], 0.4)
        self.assertAlmostEqual(follow_call[1], -0.2)
        self.assertTrue(follow_call[2])
        self.assertAlmostEqual(follow_call[3], 0.8)

    def test_refresh_presence_fuses_idle_and_gaze(self) -> None:
        subject = _PresenceFlowSubject(camera_enabled=True, result=PresenceState.PRESENT_PASSIVE)

        Director._refresh_presence_state(subject)
        Director._refresh_presence_state(subject)

        self.assertEqual(len(subject._presence_detector.calls), 2)
        self.assertEqual(subject._presence_detector.calls[0][0], 360_000)
        self.assertTrue(subject._presence_detector.calls[0][1].face_detected)
        self.assertEqual(subject.applied_states, [PresenceState.PRESENT_PASSIVE])

    def test_refresh_presence_uses_none_when_camera_disabled(self) -> None:
        subject = _PresenceFlowSubject(camera_enabled=False, result=PresenceState.UNKNOWN)

        Director._refresh_presence_state(subject)

        self.assertEqual(len(subject._presence_detector.calls), 1)
        self.assertIsNone(subject._presence_detector.calls[0][1])
        self.assertEqual(subject.applied_states, [])

    def test_apply_presence_state_triggers_expected_behavior(self) -> None:
        subject = _PresenceBehaviorSubject()

        Director._apply_presence_state(subject, PresenceState.PRESENT_PASSIVE)
        Director._apply_presence_state(subject, PresenceState.ABSENT)
        Director._apply_presence_state(subject, PresenceState.PRESENT_ACTIVE)

        self.assertEqual(subject.passive_calls, 1)
        self.assertEqual(subject.deep_sleep_calls, 1)
        self.assertEqual(subject.active_calls, 1)

    def test_expression_votes_apply_state_and_mood(self) -> None:
        subject = _ExpressionSubject()
        gaze_data = GazeData(face_detected=True, emotion_label="happy", emotion_score=0.8)

        Director._track_expression_state(subject, gaze_data)
        with patch("core.director.time.monotonic", return_value=100.0):
            Director._track_expression_state(subject, gaze_data)
        with patch("core.director.time.monotonic", return_value=100.2):
            Director._track_expression_state(subject, gaze_data)

        self.assertEqual(subject._stable_expression, "happy")
        self.assertEqual(subject.entity_state_calls, [("state6", False)])
        self.assertEqual(subject._mood_system.deltas, [0.05])


if __name__ == "__main__":
    unittest.main()
