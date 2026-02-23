from __future__ import annotations

import sys
import unittest
from pathlib import Path
from types import SimpleNamespace

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from core.idle_invasion import IdleInvasionController, InvasionState


class _TimerStub:
    def __init__(self) -> None:
        self.stop_calls = 0
        self.start_calls = 0
        self.started_with: list[int] = []

    def stop(self) -> None:
        self.stop_calls += 1

    def start(self, interval_ms: int) -> None:
        self.start_calls += 1
        self.started_with.append(interval_ms)


class _DebugSubject:
    def __init__(self) -> None:
        self._debug_force_timer = _TimerStub()
        self._spawn_timer = _TimerStub()
        self._retreat_timer = _TimerStub()
        self._state = InvasionState.SPAWNING
        self._invasion_started = False
        self._debug_force_mode = False
        self._idle_time_ms = 0
        self._config = SimpleNamespace(start_delay_ms=30_000)
        self._gif_paths: list[str] = []
        self.dismiss_calls = 0
        self.refresh_calls = 0
        self.spawn_calls = 0

    def _dismiss_all_immediate(self) -> None:
        self.dismiss_calls += 1

    def _resolve_gif_paths(self) -> list[str]:
        return ["state1.gif"]

    def _refresh_gif_sizes(self) -> None:
        self.refresh_calls += 1

    def _begin_spawning(self) -> None:
        self.spawn_calls += 1
        self._state = InvasionState.SPAWNING


class _NoStartSubject(_DebugSubject):
    def _begin_spawning(self) -> None:
        self.spawn_calls += 1
        self._state = InvasionState.INACTIVE


class _RetreatStub:
    def __init__(self) -> None:
        self._debug_force_mode = False
        self._idle_time_ms = 0
        self._invasion_started = False
        self._state = InvasionState.SPAWNING
        self._config = SimpleNamespace(enabled=True, start_delay_ms=30_000)
        self.retreat_calls = 0

    def _begin_retreat(self) -> None:
        self.retreat_calls += 1


class IdleInvasionDebugTriggerTest(unittest.TestCase):
    def test_trigger_debug_invasion_starts_spawning(self) -> None:
        subject = _DebugSubject()

        result = IdleInvasionController.trigger_debug_invasion(subject)

        self.assertTrue(result)
        self.assertEqual(subject._spawn_timer.stop_calls, 1)
        self.assertEqual(subject._retreat_timer.stop_calls, 1)
        self.assertEqual(subject._debug_force_timer.stop_calls, 1)
        self.assertEqual(subject._debug_force_timer.start_calls, 1)
        self.assertEqual(subject._debug_force_timer.started_with[-1], 120_000)
        self.assertEqual(subject.dismiss_calls, 1)
        self.assertEqual(subject.refresh_calls, 1)
        self.assertEqual(subject.spawn_calls, 1)
        self.assertTrue(subject._invasion_started)
        self.assertTrue(subject._debug_force_mode)
        self.assertGreaterEqual(subject._idle_time_ms, subject._config.start_delay_ms)
        self.assertEqual(subject._state, InvasionState.SPAWNING)
        self.assertEqual(subject._gif_paths, ["state1.gif"])

    def test_trigger_debug_invasion_returns_false_when_not_started(self) -> None:
        subject = _NoStartSubject()

        result = IdleInvasionController.trigger_debug_invasion(subject)

        self.assertFalse(result)
        self.assertEqual(subject._state, InvasionState.INACTIVE)

    def test_debug_mode_ignores_active_idle_drop(self) -> None:
        subject = _RetreatStub()
        subject._debug_force_mode = True

        IdleInvasionController._on_idle_time_updated(subject, 0)
        IdleInvasionController._on_user_active(subject)

        self.assertEqual(subject.retreat_calls, 0)
        self.assertGreaterEqual(subject._idle_time_ms, subject._config.start_delay_ms + 10 * 60_000)


if __name__ == "__main__":
    unittest.main()
