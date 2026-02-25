from __future__ import annotations

import sys
import unittest
from pathlib import Path
from types import SimpleNamespace

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from core.director import BehaviorMode, Director, ScriptedEntranceError
from core.state_machine import EntityState


class _StateMachineStub:
    def __init__(self, state: EntityState, transition_result: bool = True) -> None:
        self.current_state = state
        self.transition_result = transition_result
        self.transitions: list[EntityState] = []

    def transition_to(self, new_state: EntityState) -> bool:
        self.transitions.append(new_state)
        if self.transition_result:
            self.current_state = new_state
        return self.transition_result


class _AutoDismissTimerStub:
    def __init__(self) -> None:
        self.start_calls: list[int] = []

    def start(self, interval_ms: int) -> None:
        self.start_calls.append(int(interval_ms))


class _SummonSubject:
    def __init__(
        self,
        *,
        state: EntityState,
        transition_result: bool = True,
        scripted_entrance_result: bool = False,
        scripted_entrance_error: bool = False,
    ) -> None:
        self._state_machine = _StateMachineStub(state, transition_result=transition_result)
        self._auto_dismiss_timer = _AutoDismissTimerStub()
        self._auto_dismiss_ms = 4321
        self.behavior_calls: list[tuple[BehaviorMode, bool]] = []
        self._scripted_entrance_result = bool(scripted_entrance_result)
        self._scripted_entrance_error = bool(scripted_entrance_error)
        self._scripted_entrance_enabled = False
        self.scripted_entrance_calls = 0

    def _set_behavior_mode(self, mode: BehaviorMode, *, apply_visual: bool = True) -> None:
        self.behavior_calls.append((mode, bool(apply_visual)))

    def _try_start_voice_scripted_entrance(self) -> bool:
        self.scripted_entrance_calls += 1
        if self._scripted_entrance_error:
            raise ScriptedEntranceError("test error")
        return self._scripted_entrance_result


class _AssetManagerReloadStub:
    def __init__(self) -> None:
        self.reload_calls = 0
        self.idle_scripts = [SimpleNamespace(id="idle_1")]
        self.panic_scripts = [SimpleNamespace(id="panic_1")]
        self.idle_pick = SimpleNamespace(id="idle_pick")

    def reload(self) -> None:
        self.reload_calls += 1

    def get_idle_script_for_time(self, _now) -> object:
        return self.idle_pick


class _ScriptEngineReloadStub:
    def __init__(self) -> None:
        self.refresh_calls: list[tuple[list[object], list[object]]] = []

    def refresh(self, idle_scripts, panic_scripts) -> None:
        self.refresh_calls.append((list(idle_scripts), list(panic_scripts)))

    def select_idle_script(self, *, now):
        return None


class _ReloadSubject:
    def __init__(self, *, state: EntityState) -> None:
        self._asset_manager = _AssetManagerReloadStub()
        self._script_engine = _ScriptEngineReloadStub()
        self._pending_idle_script = object()
        self._state_machine = SimpleNamespace(current_state=state)
        self.visual_calls: list[object] = []

    def _set_visual_from_script(self, script) -> None:
        self.visual_calls.append(script)


class DirectorSummonTest(unittest.TestCase):
    def test_hidden_transitions_to_engaged(self) -> None:
        subject = _SummonSubject(state=EntityState.HIDDEN)

        result = Director.summon_now(subject)

        self.assertTrue(result)
        self.assertEqual(subject.behavior_calls, [(BehaviorMode.IDLE, False)])
        self.assertEqual(subject._state_machine.transitions, [EntityState.ENGAGED])
        self.assertEqual(subject.scripted_entrance_calls, 0)

    def test_hidden_uses_scripted_entrance_when_available(self) -> None:
        subject = _SummonSubject(state=EntityState.HIDDEN, scripted_entrance_result=True)
        subject._scripted_entrance_enabled = True

        result = Director.summon_now(subject)

        self.assertTrue(result)
        self.assertEqual(subject.scripted_entrance_calls, 1)
        self.assertEqual(subject._state_machine.transitions, [])

    def test_hidden_falls_back_when_scripted_entrance_errors(self) -> None:
        subject = _SummonSubject(state=EntityState.HIDDEN, scripted_entrance_error=True)
        subject._scripted_entrance_enabled = True

        result = Director.summon_now(subject)

        self.assertTrue(result)
        self.assertEqual(subject.scripted_entrance_calls, 1)
        self.assertEqual(subject._state_machine.transitions, [EntityState.ENGAGED])

    def test_engaged_restarts_timer(self) -> None:
        subject = _SummonSubject(state=EntityState.ENGAGED)

        result = Director.summon_now(subject)

        self.assertTrue(result)
        self.assertEqual(subject.behavior_calls, [])
        self.assertEqual(subject._auto_dismiss_timer.start_calls, [subject._auto_dismiss_ms])

    def test_fleeing_ignores_summon(self) -> None:
        subject = _SummonSubject(state=EntityState.FLEEING)

        result = Director.summon_now(subject)

        self.assertFalse(result)
        self.assertEqual(subject.behavior_calls, [])
        self.assertEqual(subject._state_machine.transitions, [])
        self.assertEqual(subject._auto_dismiss_timer.start_calls, [])

    def test_reload_scripts_refreshes_engine(self) -> None:
        subject = _ReloadSubject(state=EntityState.HIDDEN)

        Director.reload_scripts(subject)

        self.assertEqual(subject._asset_manager.reload_calls, 1)
        self.assertEqual(len(subject._script_engine.refresh_calls), 1)
        self.assertIsNone(subject._pending_idle_script)
        self.assertEqual(subject.visual_calls, [])

    def test_reload_scripts_updates_visual_when_engaged(self) -> None:
        subject = _ReloadSubject(state=EntityState.ENGAGED)

        Director.reload_scripts(subject)

        self.assertEqual(subject._asset_manager.reload_calls, 1)
        self.assertEqual(len(subject._script_engine.refresh_calls), 1)
        self.assertEqual(subject.visual_calls, [subject._asset_manager.idle_pick])


if __name__ == "__main__":
    unittest.main()
