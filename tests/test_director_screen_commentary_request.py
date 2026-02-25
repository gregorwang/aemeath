from __future__ import annotations

import sys
import threading
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from core.audio_manager import AudioPriority
from core.director import Director
from core.state_machine import EntityState


class _LoggerStub:
    def info(self, *_args, **_kwargs) -> None:
        return

    def warning(self, *_args, **_kwargs) -> None:
        return

    def debug(self, *_args, **_kwargs) -> None:
        return

    def error(self, *_args, **_kwargs) -> None:
        return


class _MoodStub:
    def __init__(self) -> None:
        self.mood = 0.55
        self.on_engaged_calls = 0

    def on_engaged(self) -> None:
        self.on_engaged_calls += 1


class _ScreenCommentatorStub:
    def __init__(self) -> None:
        self.cancel_calls = 0

    def cancel_current_session(self) -> None:
        self.cancel_calls += 1


class _AudioManagerStub:
    def __init__(self) -> None:
        self.speak_calls: list[tuple[str, AudioPriority]] = []

    def speak(self, text: str, *, priority: AudioPriority) -> None:
        self.speak_calls.append((text, priority))


class _RequestSubject:
    def __init__(self, *, state: EntityState = EntityState.ENGAGED, summon_result: bool = True) -> None:
        self.LOGGER = _LoggerStub()
        self._screen_commentator = _ScreenCommentatorStub()
        self._audio_manager = _AudioManagerStub()
        self._mood_system = _MoodStub()
        self._screen_commentary_state_lock = threading.Lock()
        self._screen_commentary_active_count = 0
        self._screen_commentary_summon_pending = False
        self._full_screen_pause = False
        self._dnd_mode = False
        self._suppress_engaged_script_once = False
        self._suppress_camera_once = False
        self._resource_scheduler = object()
        self._state_machine = SimpleNamespace(current_state=state)
        self._summon_result = bool(summon_result)
        self.summon_calls = 0
        self.state_calls: list[tuple[str, bool]] = []
        self.worker_starts: list[str] = []

    def _set_entity_state_threadsafe(self, state_name: str, *, as_base: bool = True) -> bool:
        self.state_calls.append((state_name, bool(as_base)))
        return True

    def _start_screen_commentary_worker(self, *, source_name: str) -> None:
        self.worker_starts.append(source_name)

    def _is_fullscreen_app_running(self) -> bool:
        return False

    def summon_now(self) -> bool:
        self.summon_calls += 1
        if self._summon_result and self._state_machine.current_state == EntityState.HIDDEN:
            self._state_machine.current_state = EntityState.ENGAGED
        return self._summon_result


class DirectorScreenCommentaryRequestTest(unittest.TestCase):
    def test_request_skips_when_previous_request_still_running(self) -> None:
        subject = _RequestSubject()
        subject._screen_commentary_active_count = 1

        Director.request_screen_commentary(subject, source="manual")

        self.assertEqual(subject._mood_system.on_engaged_calls, 0)
        self.assertEqual(subject._screen_commentator.cancel_calls, 0)
        self.assertEqual(subject.state_calls, [])
        self.assertEqual(subject.worker_starts, [])

    def test_timer_request_skips_when_fullscreen(self) -> None:
        subject = _RequestSubject()
        subject._full_screen_pause = True
        subject._is_fullscreen_app_running = lambda: True  # type: ignore[method-assign]

        Director.request_screen_commentary(subject, source="timer")

        self.assertEqual(subject._mood_system.on_engaged_calls, 0)
        self.assertEqual(subject.worker_starts, [])

    def test_timer_request_skips_when_dnd_enabled(self) -> None:
        subject = _RequestSubject()
        subject._dnd_mode = True

        Director.request_screen_commentary(subject, source="timer")

        self.assertEqual(subject._mood_system.on_engaged_calls, 0)
        self.assertEqual(subject.worker_starts, [])

    def test_manual_request_still_allowed_when_dnd_enabled(self) -> None:
        subject = _RequestSubject()
        subject._dnd_mode = True

        Director.request_screen_commentary(subject, source="manual")

        self.assertEqual(subject._mood_system.on_engaged_calls, 1)
        self.assertEqual(subject.worker_starts, ["manual"])

    def test_request_starts_worker_and_marks_active(self) -> None:
        subject = _RequestSubject()

        Director.request_screen_commentary(subject, source="Tray")

        self.assertEqual(subject._mood_system.on_engaged_calls, 1)
        self.assertEqual(subject._screen_commentator.cancel_calls, 1)
        self.assertEqual(subject._screen_commentary_active_count, 1)
        self.assertEqual(subject.state_calls, [("state5", False)])
        self.assertEqual(subject.worker_starts, ["tray"])

    def test_hidden_request_summons_then_starts_commentary(self) -> None:
        subject = _RequestSubject(state=EntityState.HIDDEN, summon_result=True)

        with patch("core.director.QTimer.singleShot", side_effect=lambda _ms, cb: cb()):
            Director.request_screen_commentary(subject, source="manual")

        self.assertEqual(subject.summon_calls, 1)
        self.assertEqual(subject._state_machine.current_state, EntityState.ENGAGED)
        self.assertEqual(subject.worker_starts, ["manual"])
        self.assertFalse(subject._screen_commentary_summon_pending)
        self.assertTrue(subject._suppress_camera_once)

    def test_hidden_request_stops_when_summon_fails(self) -> None:
        subject = _RequestSubject(state=EntityState.HIDDEN, summon_result=False)

        with patch("core.director.QTimer.singleShot") as single_shot:
            Director.request_screen_commentary(subject, source="manual")

        self.assertEqual(subject.summon_calls, 1)
        self.assertEqual(subject.worker_starts, [])
        self.assertEqual(subject._screen_commentary_active_count, 0)
        self.assertEqual(subject._mood_system.on_engaged_calls, 0)
        self.assertFalse(subject._suppress_camera_once)
        single_shot.assert_not_called()

    def test_done_restores_state_and_decrements_active_count(self) -> None:
        subject = _RequestSubject()
        subject._screen_commentary_active_count = 2

        Director._on_screen_commentary_done(subject)

        self.assertEqual(subject._screen_commentary_active_count, 1)
        self.assertEqual(subject.state_calls, [("state1", False)])

    def test_skipped_by_plan_speaks_power_save_hint(self) -> None:
        subject = _RequestSubject()

        Director._on_screen_commentary_skipped(subject, "llm_running=false")

        self.assertEqual(len(subject._audio_manager.speak_calls), 1)
        text, priority = subject._audio_manager.speak_calls[0]
        self.assertIn("省电模式", text)
        self.assertEqual(priority, AudioPriority.HIGH)

    def test_failed_speaks_error_hint(self) -> None:
        subject = _RequestSubject()

        Director._on_screen_commentary_failed(subject, "network timeout")

        self.assertEqual(len(subject._audio_manager.speak_calls), 1)
        text, priority = subject._audio_manager.speak_calls[0]
        self.assertIn("看屏幕失败", text)
        self.assertEqual(priority, AudioPriority.HIGH)

    def test_thread_finished_clears_worker_references(self) -> None:
        subject = SimpleNamespace(
            _screen_commentary_thread=object(),
            _screen_commentary_worker=object(),
        )

        Director._on_screen_commentary_thread_finished(subject)

        self.assertIsNone(subject._screen_commentary_thread)
        self.assertIsNone(subject._screen_commentary_worker)


if __name__ == "__main__":
    unittest.main()
