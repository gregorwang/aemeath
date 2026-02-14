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


class _GifStateMapperStub:
    def __init__(self) -> None:
        self.on_summoned_calls = 0

    def on_summoned(self) -> None:
        self.on_summoned_calls += 1


class _ScriptedEntranceSubject:
    def __init__(self, *, state: EntityState) -> None:
        self._state_machine = _StateMachineStub(state)
        self._gif_state_mapper = _GifStateMapperStub()
        self.behavior_calls: list[tuple[BehaviorMode, bool]] = []
        self.autonomous_calls: list[bool] = []
        self.stop_auto_dismiss_calls = 0

    def _stop_auto_dismiss_timer(self) -> None:
        self.stop_auto_dismiss_calls += 1

    def _set_entity_autonomous(self, enabled: bool) -> None:
        self.autonomous_calls.append(bool(enabled))

    def _set_behavior_mode(self, mode: BehaviorMode, *, apply_visual: bool = True) -> None:
        self.behavior_calls.append((mode, bool(apply_visual)))


class DirectorScriptedEntranceTest(unittest.TestCase):
    def test_hidden_stays_hidden_after_scripted_entrance(self) -> None:
        subject = _ScriptedEntranceSubject(state=EntityState.HIDDEN)

        Director._complete_voice_scripted_entrance(subject)

        self.assertEqual(subject._state_machine.transitions, [])
        self.assertEqual(subject._gif_state_mapper.on_summoned_calls, 1)
        self.assertEqual(subject.stop_auto_dismiss_calls, 1)
        self.assertEqual(subject.autonomous_calls, [False])
        self.assertEqual(subject.behavior_calls, [(BehaviorMode.BUSY, False)])

    def test_peeking_transitions_to_hidden_after_scripted_entrance(self) -> None:
        subject = _ScriptedEntranceSubject(state=EntityState.PEEKING)

        Director._complete_voice_scripted_entrance(subject)

        self.assertEqual(subject._state_machine.transitions, [EntityState.HIDDEN])

    def test_engaged_transitions_to_hidden_after_scripted_entrance(self) -> None:
        subject = _ScriptedEntranceSubject(state=EntityState.ENGAGED)

        Director._complete_voice_scripted_entrance(subject)

        self.assertEqual(subject._state_machine.transitions, [EntityState.HIDDEN])


if __name__ == "__main__":
    unittest.main()
