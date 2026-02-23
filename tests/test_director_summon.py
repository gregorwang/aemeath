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
    def __init__(self, *, state: EntityState, transition_result: bool = True) -> None:
        self._state_machine = _StateMachineStub(state, transition_result=transition_result)
        self._auto_dismiss_timer = _AutoDismissTimerStub()
        self._auto_dismiss_ms = 4321
        self.behavior_calls: list[tuple[BehaviorMode, bool]] = []

    def _set_behavior_mode(self, mode: BehaviorMode, *, apply_visual: bool = True) -> None:
        self.behavior_calls.append((mode, bool(apply_visual)))


class DirectorSummonTest(unittest.TestCase):
    def test_hidden_transitions_to_engaged(self) -> None:
        subject = _SummonSubject(state=EntityState.HIDDEN)

        result = Director.summon_now(subject)

        self.assertTrue(result)
        self.assertEqual(subject.behavior_calls, [(BehaviorMode.IDLE, False)])
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


if __name__ == "__main__":
    unittest.main()
