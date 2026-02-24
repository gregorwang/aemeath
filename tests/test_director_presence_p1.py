from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

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


class _TimerStub:
    def __init__(self, *, active: bool = False) -> None:
        self._active = bool(active)
        self.stop_calls = 0
        self.start_calls: list[int] = []

    def isActive(self) -> bool:
        return self._active

    def stop(self) -> None:
        self.stop_calls += 1
        self._active = False

    def start(self, ms: int) -> None:
        self.start_calls.append(int(ms))
        self._active = True


class _MoodStub:
    def __init__(self) -> None:
        self.dismiss_calls = 0

    def on_dismissed(self) -> None:
        self.dismiss_calls += 1


class _UserActiveSubject:
    def __init__(self) -> None:
        self._passive_presence_active = True
        self._deep_sleep_active = False
        self._passive_presence_timer = _TimerStub(active=True)
        self._state_machine = _StateMachineStub(EntityState.ENGAGED)
        self._mood_system = _MoodStub()
        self.behavior_calls: list[tuple[BehaviorMode, bool]] = []

    def _set_behavior_mode(self, mode: BehaviorMode, *, apply_visual: bool = True) -> None:
        self.behavior_calls.append((mode, apply_visual))


class _PresenceRouteSubject:
    def __init__(self) -> None:
        self._voice_trajectory_playing = False
        self.passive_calls = 0
        self.deep_sleep_calls = 0
        self.back_active_calls = 0

    def _enter_passive_companion(self) -> None:
        self.passive_calls += 1

    def _enter_deep_sleep(self) -> None:
        self.deep_sleep_calls += 1

    def _on_presence_back_active(self) -> None:
        self.back_active_calls += 1


class _DeepSleepSubject:
    def __init__(self) -> None:
        self._state_machine = _StateMachineStub(EntityState.ENGAGED)
        self._passive_presence_active = True
        self._passive_presence_timer = _TimerStub(active=True)
        self._deep_sleep_active = False
        self.behavior_calls: list[tuple[BehaviorMode, bool]] = []
        self.autonomous_calls: list[bool] = []
        self.state_calls: list[tuple[str, bool]] = []
        self.stop_auto_dismiss_calls = 0

    def _stop_auto_dismiss_timer(self) -> None:
        self.stop_auto_dismiss_calls += 1

    def _set_entity_autonomous(self, enabled: bool) -> None:
        self.autonomous_calls.append(bool(enabled))

    def _set_behavior_mode(self, mode: BehaviorMode, *, apply_visual: bool = True) -> None:
        self.behavior_calls.append((mode, apply_visual))

    def _set_entity_state(self, state_name: str, *, as_base: bool = True) -> bool:
        self.state_calls.append((state_name, as_base))
        return state_name == "state5"


class _WakeSubject:
    def __init__(self) -> None:
        self._deep_sleep_active = True
        self._passive_presence_active = True
        self._passive_presence_timer = _TimerStub(active=True)
        self._state_machine = _StateMachineStub(EntityState.ENGAGED)
        self._auto_dismiss_ms = 12345
        self._auto_dismiss_timer = _TimerStub(active=False)
        self.state_calls: list[tuple[str, bool]] = []
        self.autonomous_calls: list[bool] = []

    def _resolve_effective_behavior_mode(self) -> BehaviorMode:
        return BehaviorMode.IDLE

    def _set_entity_state(self, state_name: str, *, as_base: bool = True) -> bool:
        self.state_calls.append((state_name, as_base))
        return state_name == "state2"

    def _set_entity_autonomous(self, enabled: bool) -> None:
        self.autonomous_calls.append(bool(enabled))


class _PassiveTimeoutSubject:
    def __init__(self) -> None:
        self._passive_presence_active = True
        self._state_machine = _StateMachineStub(EntityState.ENGAGED)


class _PassiveCompanionSubject:
    PASSIVE_COMPANION_DURATION_MS = Director.PASSIVE_COMPANION_DURATION_MS

    def __init__(self) -> None:
        self._state_machine = _StateMachineStub(EntityState.ENGAGED)
        self._deep_sleep_active = True
        self._passive_presence_active = False
        self._passive_presence_timer = _TimerStub(active=False)
        self.autonomous_calls: list[bool] = []
        self.state_calls: list[tuple[str, bool]] = []
        self.stop_auto_dismiss_calls = 0

    def _resolve_effective_behavior_mode(self) -> BehaviorMode:
        return BehaviorMode.IDLE

    def _set_behavior_mode(self, _mode: BehaviorMode, *, apply_visual: bool = True) -> None:
        return

    def _stop_auto_dismiss_timer(self) -> None:
        self.stop_auto_dismiss_calls += 1

    def _set_entity_autonomous(self, enabled: bool) -> None:
        self.autonomous_calls.append(bool(enabled))

    def _set_entity_state(self, state_name: str, *, as_base: bool = True) -> bool:
        self.state_calls.append((state_name, as_base))
        return True


class DirectorPresenceP1Test(unittest.TestCase):
    def test_user_active_hides_passive_presence_without_flee(self) -> None:
        subject = _UserActiveSubject()

        Director.on_user_active(subject)

        self.assertEqual(subject.behavior_calls, [(BehaviorMode.BUSY, True)])
        self.assertEqual(subject._state_machine.transitions, [EntityState.HIDDEN])
        self.assertEqual(subject._mood_system.dismiss_calls, 0)
        self.assertEqual(subject._passive_presence_timer.stop_calls, 1)
        self.assertFalse(subject._passive_presence_active)
        self.assertFalse(subject._deep_sleep_active)

    def test_apply_presence_state_routes_by_presence(self) -> None:
        subject = _PresenceRouteSubject()

        Director._apply_presence_state(subject, PresenceState.PRESENT_PASSIVE)
        Director._apply_presence_state(subject, PresenceState.ABSENT)
        Director._apply_presence_state(subject, PresenceState.PRESENT_ACTIVE)

        self.assertEqual(subject.passive_calls, 1)
        self.assertEqual(subject.deep_sleep_calls, 1)
        self.assertEqual(subject.back_active_calls, 1)

    def test_enter_deep_sleep_sets_sleep_fallback(self) -> None:
        subject = _DeepSleepSubject()

        Director._enter_deep_sleep(subject)

        self.assertTrue(subject._deep_sleep_active)
        self.assertFalse(subject._passive_presence_active)
        self.assertEqual(subject._passive_presence_timer.stop_calls, 1)
        self.assertEqual(subject.stop_auto_dismiss_calls, 1)
        self.assertEqual(subject.autonomous_calls, [False])
        self.assertEqual(subject.behavior_calls, [(BehaviorMode.BUSY, False)])
        self.assertEqual(subject.state_calls, [("sleep", False), ("state5", False)])

    def test_presence_back_active_wakes_from_deep_sleep(self) -> None:
        subject = _WakeSubject()

        Director._on_presence_back_active(subject)

        self.assertFalse(subject._deep_sleep_active)
        self.assertFalse(subject._passive_presence_active)
        self.assertEqual(subject._passive_presence_timer.stop_calls, 1)
        self.assertEqual(subject.state_calls, [("state2", False)])
        self.assertEqual(subject.autonomous_calls, [True])
        self.assertEqual(subject._auto_dismiss_timer.start_calls, [12345])

    def test_passive_timeout_hides_entity(self) -> None:
        subject = _PassiveTimeoutSubject()

        Director._on_passive_presence_timeout(subject)

        self.assertFalse(subject._passive_presence_active)
        self.assertEqual(subject._state_machine.transitions, [EntityState.HIDDEN])

    def test_enter_passive_companion_arms_silent_timer(self) -> None:
        subject = _PassiveCompanionSubject()

        Director._enter_passive_companion(subject)

        self.assertFalse(subject._deep_sleep_active)
        self.assertTrue(subject._passive_presence_active)
        self.assertEqual(subject.stop_auto_dismiss_calls, 1)
        self.assertEqual(subject.autonomous_calls, [False])
        self.assertEqual(subject.state_calls, [("state5", False)])
        self.assertEqual(
            subject._passive_presence_timer.start_calls,
            [Director.PASSIVE_COMPANION_DURATION_MS],
        )


if __name__ == "__main__":
    unittest.main()
