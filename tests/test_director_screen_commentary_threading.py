from __future__ import annotations

import sys
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from core.director import Director


class _SignalStub:
    def __init__(self) -> None:
        self.calls: list[tuple[str, bool]] = []

    def emit(self, state_name: str, as_base: bool) -> None:
        self.calls.append((state_name, bool(as_base)))


class _StateDispatchSubject:
    def __init__(self, owner_thread: object) -> None:
        self._owner_thread = owner_thread
        self._entity_state_change_requested = _SignalStub()
        self.state_calls: list[tuple[str, bool]] = []

    def thread(self) -> object:
        return self._owner_thread

    def _set_entity_state(self, state_name: str, *, as_base: bool = True) -> bool:
        self.state_calls.append((state_name, bool(as_base)))
        return True


class DirectorScreenCommentaryThreadingTest(unittest.TestCase):
    def test_set_entity_state_threadsafe_direct_when_on_owner_thread(self) -> None:
        owner_thread = object()
        subject = _StateDispatchSubject(owner_thread=owner_thread)

        with patch("core.director.QThread.currentThread", return_value=owner_thread):
            ok = Director._set_entity_state_threadsafe(subject, "state5", as_base=False)

        self.assertTrue(ok)
        self.assertEqual(subject.state_calls, [("state5", False)])
        self.assertEqual(subject._entity_state_change_requested.calls, [])

    def test_set_entity_state_threadsafe_emits_when_on_worker_thread(self) -> None:
        owner_thread = object()
        worker_thread = object()
        subject = _StateDispatchSubject(owner_thread=owner_thread)

        with patch("core.director.QThread.currentThread", return_value=worker_thread):
            ok = Director._set_entity_state_threadsafe(subject, "state1", as_base=False)

        self.assertTrue(ok)
        self.assertEqual(subject.state_calls, [])
        self.assertEqual(subject._entity_state_change_requested.calls, [("state1", False)])

    def test_state_change_signal_slot_applies_state(self) -> None:
        subject = _StateDispatchSubject(owner_thread=object())

        Director._on_entity_state_change_requested(subject, "state3", True)

        self.assertEqual(subject.state_calls, [("state3", True)])


if __name__ == "__main__":
    unittest.main()
