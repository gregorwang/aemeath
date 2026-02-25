from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from core.director import BehaviorMode, Director
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

    def isActive(self) -> bool:
        return self._active

    def stop(self) -> None:
        self.stop_calls += 1
        self._active = False


class _IdleMonitorStub:
    def __init__(self) -> None:
        self.reset_calls = 0

    def reset_to_standby(self) -> None:
        self.reset_calls += 1


class _ResidentVisibilitySubject:
    def __init__(self, *, state: EntityState, fullscreen_running: bool = False) -> None:
        self._resident_mode = True
        self._dnd_mode = False
        self._voice_trajectory_playing = False
        self._full_screen_pause = bool(fullscreen_running)
        self._state_machine = _StateMachineStub(state)
        self._suppress_engaged_script_once = False
        self.summon_calls = 0
        self.stop_timer_calls = 0
        self.autonomous_calls: list[bool] = []

    def _is_fullscreen_app_running(self) -> bool:
        return bool(self._full_screen_pause)

    def summon_now(self) -> bool:
        self.summon_calls += 1
        self._state_machine.current_state = EntityState.ENGAGED
        return True

    def _stop_auto_dismiss_timer(self) -> None:
        self.stop_timer_calls += 1

    def _set_entity_autonomous(self, enabled: bool) -> None:
        self.autonomous_calls.append(bool(enabled))


class _ResidentActiveSubject:
    def __init__(self) -> None:
        self._resident_mode = True
        self._dnd_mode = False
        self._voice_trajectory_playing = False
        self._full_screen_pause = False
        self._state_machine = _StateMachineStub(EntityState.ENGAGED)
        self._passive_presence_active = True
        self._deep_sleep_active = True
        self._passive_presence_timer = _TimerStub(active=True)
        self._idle_monitor = _IdleMonitorStub()
        self.behavior_calls: list[tuple[BehaviorMode, bool]] = []
        self.stop_timer_calls = 0
        self.autonomous_calls: list[bool] = []

    def _is_fullscreen_app_running(self) -> bool:
        return False

    def _set_behavior_mode(self, mode: BehaviorMode, *, apply_visual: bool = True) -> None:
        self.behavior_calls.append((mode, bool(apply_visual)))

    def _stop_auto_dismiss_timer(self) -> None:
        self.stop_timer_calls += 1

    def _set_entity_autonomous(self, enabled: bool) -> None:
        self.autonomous_calls.append(bool(enabled))


class DirectorResidentModeTest(unittest.TestCase):
    def test_sync_resident_visibility_summons_when_hidden(self) -> None:
        subject = _ResidentVisibilitySubject(state=EntityState.HIDDEN, fullscreen_running=False)

        Director._sync_resident_visibility(subject)

        self.assertEqual(subject.summon_calls, 1)
        self.assertEqual(subject._state_machine.transitions, [])
        self.assertEqual(subject.stop_timer_calls, 1)
        self.assertEqual(subject.autonomous_calls, [False])

    def test_sync_resident_visibility_hides_when_fullscreen(self) -> None:
        subject = _ResidentVisibilitySubject(state=EntityState.ENGAGED, fullscreen_running=True)

        Director._sync_resident_visibility(subject)

        self.assertEqual(subject.summon_calls, 0)
        self.assertEqual(subject._state_machine.transitions, [EntityState.HIDDEN])

    def test_user_active_keeps_resident_visible_without_flee(self) -> None:
        subject = _ResidentActiveSubject()

        Director.on_user_active(subject)

        self.assertEqual(subject.behavior_calls, [(BehaviorMode.IDLE, False)])
        self.assertEqual(subject._state_machine.transitions, [])
        self.assertFalse(subject._passive_presence_active)
        self.assertFalse(subject._deep_sleep_active)
        self.assertEqual(subject._passive_presence_timer.stop_calls, 1)
        self.assertEqual(subject.stop_timer_calls, 1)
        self.assertEqual(subject.autonomous_calls, [False])
        self.assertEqual(subject._idle_monitor.reset_calls, 1)


if __name__ == "__main__":
    unittest.main()
