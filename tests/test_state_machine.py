from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from core.state_machine import EntityState, StateMachine


class StateMachineTest(unittest.TestCase):
    def test_legal_flow(self) -> None:
        fsm = StateMachine()
        self.assertEqual(fsm.current_state, EntityState.HIDDEN)
        self.assertTrue(fsm.transition_to(EntityState.PEEKING))
        self.assertTrue(fsm.transition_to(EntityState.ENGAGED))
        self.assertTrue(fsm.transition_to(EntityState.FLEEING))
        self.assertTrue(fsm.transition_to(EntityState.HIDDEN))
        self.assertEqual(fsm.current_state, EntityState.HIDDEN)

    def test_illegal_transition_rejected(self) -> None:
        fsm = StateMachine()
        self.assertFalse(fsm.transition_to(EntityState.FLEEING))
        self.assertEqual(fsm.current_state, EntityState.HIDDEN)

    def test_callbacks_execute(self) -> None:
        fsm = StateMachine()
        calls: list[str] = []

        fsm.register_state_handler(
            EntityState.PEEKING,
            on_enter=lambda: calls.append("enter_peeking"),
            on_exit=lambda: calls.append("exit_peeking"),
        )
        fsm.register_state_handler(
            EntityState.ENGAGED,
            on_enter=lambda: calls.append("enter_engaged"),
        )

        self.assertTrue(fsm.transition_to(EntityState.PEEKING))
        self.assertTrue(fsm.transition_to(EntityState.ENGAGED))
        self.assertEqual(calls, ["enter_peeking", "exit_peeking", "enter_engaged"])


if __name__ == "__main__":
    unittest.main()

