from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from core.director import Director


class _TimerStub:
    def __init__(self, active: bool = True) -> None:
        self._active = active
        self.stop_calls = 0

    def isActive(self) -> bool:
        return self._active

    def stop(self) -> None:
        self.stop_calls += 1
        self._active = False


class _StopStub:
    def __init__(self) -> None:
        self.stop_calls = 0

    def stop(self) -> None:
        self.stop_calls += 1


class _ShutdownStub:
    def __init__(self) -> None:
        self.shutdown_calls = 0

    def shutdown(self) -> None:
        self.shutdown_calls += 1


class _ShutdownSubject:
    def __init__(self) -> None:
        self.voice_stop_calls = 0
        self.auto_dismiss_stop_calls = 0
        self.camera_stop_calls = 0
        self.autonomous_calls: list[bool] = []

        self._auto_screen_commentary_timer = _TimerStub(active=True)
        self._mood_decay_timer = _TimerStub(active=True)
        self._prolonged_idle_timer = _TimerStub(active=True)
        self._audio_output_monitor = _StopStub()
        self._gif_state_mapper = _ShutdownStub()
        self._idle_invasion_controller = _ShutdownStub()

    def _stop_voice_scripted_entrance(self) -> None:
        self.voice_stop_calls += 1

    def _stop_auto_dismiss_timer(self) -> None:
        self.auto_dismiss_stop_calls += 1

    def _stop_camera_tracking(self) -> None:
        self.camera_stop_calls += 1

    def _set_entity_autonomous(self, enabled: bool) -> None:
        self.autonomous_calls.append(bool(enabled))


class DirectorShutdownTest(unittest.TestCase):
    def test_shutdown_stops_idle_invasion_controller(self) -> None:
        subject = _ShutdownSubject()

        Director.shutdown(subject)

        self.assertEqual(subject.voice_stop_calls, 1)
        self.assertEqual(subject.auto_dismiss_stop_calls, 1)
        self.assertEqual(subject.camera_stop_calls, 1)
        self.assertEqual(subject._auto_screen_commentary_timer.stop_calls, 1)
        self.assertEqual(subject._mood_decay_timer.stop_calls, 1)
        self.assertEqual(subject._prolonged_idle_timer.stop_calls, 1)
        self.assertEqual(subject._audio_output_monitor.stop_calls, 1)
        self.assertEqual(subject._gif_state_mapper.shutdown_calls, 1)
        self.assertEqual(subject._idle_invasion_controller.shutdown_calls, 1)
        self.assertEqual(subject.autonomous_calls, [False])


if __name__ == "__main__":
    unittest.main()
