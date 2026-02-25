from __future__ import annotations

import sys
import unittest
from pathlib import Path
from types import SimpleNamespace

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


class _IdleMonitorStub:
    def __init__(self) -> None:
        self.reset_calls = 0

    def reset_to_standby(self) -> None:
        self.reset_calls += 1


class _IdleBehaviorSubject:
    def __init__(
        self,
        *,
        idle_invasion_enabled: bool,
        state: EntityState = EntityState.HIDDEN,
        attach_idle_monitor: bool = False,
        fullscreen_pause: bool = False,
        fullscreen_running: bool = False,
        dnd_mode: bool = False,
    ) -> None:
        self._voice_trajectory_playing = False
        self._state_machine = _StateMachineStub(state)
        self._full_screen_pause = bool(fullscreen_pause)
        self._fullscreen_running = bool(fullscreen_running)
        self._dnd_mode = bool(dnd_mode)
        self._idle_monitor = _IdleMonitorStub() if attach_idle_monitor else None
        self._config = SimpleNamespace(
            idle_invasion=SimpleNamespace(enabled=bool(idle_invasion_enabled))
        )
        self.jitter_arm_calls = 0
        self.behavior_calls: list[tuple[BehaviorMode, bool]] = []
        self.LOGGER = SimpleNamespace(
            debug=lambda *_args, **_kwargs: None,
            info=lambda *_args, **_kwargs: None,
        )

    def _set_behavior_mode(self, mode: BehaviorMode, *, apply_visual: bool = True) -> None:
        self.behavior_calls.append((mode, apply_visual))

    def _arm_idle_threshold_with_jitter(self) -> None:
        self.jitter_arm_calls += 1

    def _is_fullscreen_app_running(self) -> bool:
        return self._fullscreen_running


class _ProlongedIdleTimerStub:
    def __init__(self) -> None:
        self.start_calls = 0

    def start(self) -> None:
        self.start_calls += 1


class _EntityWindowStub:
    def __init__(self) -> None:
        self.hide_calls = 0

    def hide(self) -> None:
        self.hide_calls += 1


class _HiddenEnterSubject:
    def __init__(self) -> None:
        self._pending_idle_script = object()
        self._idle_monitor = None
        self._entity_window = _EntityWindowStub()
        self._prolonged_idle_timer = _ProlongedIdleTimerStub()
        self._current_ascii_template = "placeholder"
        self.stop_auto_dismiss_calls = 0
        self.stop_camera_calls = 0
        self.reset_no_face_calls = 0
        self.autonomous_calls: list[bool] = []
        self.behavior_calls: list[tuple[BehaviorMode, bool]] = []

    def _stop_auto_dismiss_timer(self) -> None:
        self.stop_auto_dismiss_calls += 1

    def _stop_camera_tracking(self) -> None:
        self.stop_camera_calls += 1

    def _reset_no_face_tracker(self) -> None:
        self.reset_no_face_calls += 1

    def _set_entity_autonomous(self, enabled: bool) -> None:
        self.autonomous_calls.append(bool(enabled))

    def _set_behavior_mode(self, mode: BehaviorMode, *, apply_visual: bool = True) -> None:
        self.behavior_calls.append((mode, apply_visual))


class DirectorIdleBehaviorTest(unittest.TestCase):
    def test_skips_legacy_idle_summon_when_idle_invasion_enabled(self) -> None:
        subject = _IdleBehaviorSubject(idle_invasion_enabled=True)

        Director.on_user_idle(subject)

        self.assertEqual(subject._state_machine.transitions, [])
        self.assertEqual(subject.behavior_calls, [])

    def test_idle_invasion_path_rearms_idle_monitor(self) -> None:
        subject = _IdleBehaviorSubject(
            idle_invasion_enabled=True,
            attach_idle_monitor=True,
        )

        Director.on_user_idle(subject)

        self.assertIsNotNone(subject._idle_monitor)
        self.assertEqual(subject._state_machine.transitions, [])
        self.assertEqual(subject.behavior_calls, [])
        self.assertEqual(subject.jitter_arm_calls, 1)
        self.assertEqual(subject._idle_monitor.reset_calls, 1)

    def test_dnd_mode_blocks_auto_idle_summon(self) -> None:
        subject = _IdleBehaviorSubject(
            idle_invasion_enabled=False,
            attach_idle_monitor=True,
            dnd_mode=True,
        )

        Director.on_user_idle(subject)

        self.assertEqual(subject._state_machine.transitions, [])
        self.assertEqual(subject.behavior_calls, [])
        self.assertEqual(subject.jitter_arm_calls, 1)
        self.assertIsNotNone(subject._idle_monitor)
        self.assertEqual(subject._idle_monitor.reset_calls, 1)

    def test_enters_engaged_when_idle_trigger_fires(self) -> None:
        subject = _IdleBehaviorSubject(idle_invasion_enabled=False)

        Director.on_user_idle(subject)

        self.assertEqual(subject.behavior_calls, [(BehaviorMode.IDLE, False)])
        self.assertEqual(subject._state_machine.transitions, [EntityState.ENGAGED])

    def test_ignores_idle_when_not_hidden(self) -> None:
        subject = _IdleBehaviorSubject(
            idle_invasion_enabled=False,
            state=EntityState.ENGAGED,
        )

        Director.on_user_idle(subject)

        self.assertEqual(subject.behavior_calls, [])
        self.assertEqual(subject._state_machine.transitions, [])

    def test_enter_hidden_clears_pending_idle_script(self) -> None:
        subject = _HiddenEnterSubject()

        Director._enter_hidden(subject)

        self.assertIsNone(subject._pending_idle_script)
        self.assertEqual(subject.stop_auto_dismiss_calls, 1)
        self.assertEqual(subject.stop_camera_calls, 1)
        self.assertEqual(subject.reset_no_face_calls, 1)
        self.assertEqual(subject.autonomous_calls, [False])
        self.assertEqual(subject.behavior_calls, [(BehaviorMode.BUSY, False)])
        self.assertEqual(subject._entity_window.hide_calls, 1)
        self.assertEqual(subject._prolonged_idle_timer.start_calls, 1)


if __name__ == "__main__":
    unittest.main()
