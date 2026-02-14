from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from core.director import Director
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


class _GifStateMapperStub:
    def __init__(self) -> None:
        self.on_summoned_calls = 0

    def on_summoned(self) -> None:
        self.on_summoned_calls += 1


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
        scripted_result: bool = False,
        scripted_error: Exception | None = None,
        transition_result: bool = True,
    ) -> None:
        self._state_machine = _StateMachineStub(state, transition_result=transition_result)
        self._gif_state_mapper = _GifStateMapperStub()
        self._auto_dismiss_timer = _AutoDismissTimerStub()
        self._auto_dismiss_ms = 4321
        self._silent_presence_mode = True

        self._scripted_result = scripted_result
        self._scripted_error = scripted_error
        self.scripted_attempts = 0

    def _try_start_voice_scripted_entrance(self) -> bool:
        self.scripted_attempts += 1
        if self._scripted_error is not None:
            raise self._scripted_error
        return self._scripted_result


class DirectorSummonTest(unittest.TestCase):
    def test_hidden_prefers_scripted_entrance(self) -> None:
        subject = _SummonSubject(state=EntityState.HIDDEN, scripted_result=True)

        result = Director.summon_now(subject)

        self.assertTrue(result)
        self.assertEqual(subject.scripted_attempts, 1)
        self.assertEqual(subject._state_machine.transitions, [])
        self.assertEqual(subject._gif_state_mapper.on_summoned_calls, 0)

    def test_hidden_raises_when_scripted_entrance_fails(self) -> None:
        subject = _SummonSubject(state=EntityState.HIDDEN, scripted_error=RuntimeError("boom"))

        with self.assertRaisesRegex(RuntimeError, "boom"):
            Director.summon_now(subject)

        self.assertEqual(subject.scripted_attempts, 1)
        self.assertEqual(subject._state_machine.transitions, [])
        self.assertEqual(subject._gif_state_mapper.on_summoned_calls, 0)
        self.assertTrue(subject._silent_presence_mode)

    def test_engaged_restarts_timer_without_scripted_entrance(self) -> None:
        subject = _SummonSubject(state=EntityState.ENGAGED, scripted_result=True)

        result = Director.summon_now(subject)

        self.assertTrue(result)
        self.assertEqual(subject.scripted_attempts, 0)
        self.assertEqual(subject._auto_dismiss_timer.start_calls, [subject._auto_dismiss_ms])


if __name__ == "__main__":
    unittest.main()
