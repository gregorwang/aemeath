from __future__ import annotations

import sys
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

from PySide6.QtCore import QCoreApplication

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from core.voice_runtime import VoiceRuntimeController


class _SignalStub:
    def __init__(self) -> None:
        self._handlers = []

    def connect(self, handler) -> None:
        self._handlers.append(handler)

    def emit(self, *args, **kwargs) -> None:
        for handler in list(self._handlers):
            handler(*args, **kwargs)


class _FakeVoiceWakeupListener:
    created: list["_FakeVoiceWakeupListener"] = []
    transcribe_return = "测试语音"

    def __init__(self, **kwargs) -> None:
        self.kwargs = kwargs
        self.wake_phrase_detected = _SignalStub()
        self.listener_error = _SignalStub()
        self.transcript_updated = _SignalStub()
        self.started = False
        self.stopped = False
        self.deleted = False
        self.__class__.created.append(self)

    @staticmethod
    def transcribe_once(**_kwargs) -> str:
        return _FakeVoiceWakeupListener.transcribe_return

    def start_listening(self) -> None:
        self.started = True

    def stop_listening(self) -> None:
        self.stopped = True

    def deleteLater(self) -> None:
        self.deleted = True


class _ImmediateThread:
    def __init__(self, *, target, **_kwargs) -> None:
        self._target = target

    def start(self) -> None:
        self._target()


class _LoggerStub:
    def info(self, *_args, **_kwargs) -> None:
        return

    def warning(self, *_args, **_kwargs) -> None:
        return

    def debug(self, *_args, **_kwargs) -> None:
        return


def _build_config(*, microphone: bool, mode: str, wakeup: bool, debug_mode: bool):
    return SimpleNamespace(
        audio=SimpleNamespace(
            microphone_enabled=microphone,
            voice_input_mode=mode,
            asr_provider="zhipu_asr",
            asr_model="glm-asr-2512",
            asr_prompt="",
            asr_temperature=0.0,
        ),
        wakeup=SimpleNamespace(
            enabled=wakeup,
            language="zh-CN",
            phrases=("小爱同学",),
        ),
        behavior=SimpleNamespace(debug_mode=debug_mode),
    )


class VoiceRuntimeControllerTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls._app = QCoreApplication.instance() or QCoreApplication([])

    def test_push_to_talk_honors_mode_and_emits_transcript(self) -> None:
        notifications: list[tuple[str, str, int]] = []
        command_calls: list[tuple[str, str]] = []
        config = _build_config(microphone=True, mode="push_to_talk", wakeup=False, debug_mode=False)

        def _notify(title: str, message: str, timeout_ms: int) -> None:
            notifications.append((title, message, int(timeout_ms)))

        with patch("core.voice_runtime.VoiceWakeupListener", _FakeVoiceWakeupListener), patch(
            "core.voice_runtime.threading.Thread", _ImmediateThread
        ):
            subject = VoiceRuntimeController(
                logger=_LoggerStub(),
                get_config=lambda: config,
                resolve_asr_runtime=lambda _cfg: ("k", "https://asr"),
                notify=_notify,
                execute_voice_command=lambda text, source: command_calls.append((text, source)) or False,
                summon_now_or_notify=lambda _source: True,
                request_screen_commentary=lambda _source: None,
            )
            subject.start_push_to_talk_once()

        self.assertEqual(command_calls, [("测试语音", "push_to_talk")])
        self.assertTrue(any("开始收音" in msg for _, msg, _ in notifications))
        self.assertTrue(any(msg == "测试语音" for _, msg, _ in notifications))
        self.assertTrue(any("未匹配到动作" in msg for _, msg, _ in notifications))

    def test_voice_listener_handles_wakeup_intent_and_error(self) -> None:
        notifications: list[tuple[str, str, int]] = []
        summon_calls: list[str] = []
        commentary_calls: list[str] = []
        command_calls: list[tuple[str, str]] = []
        config = _build_config(microphone=True, mode="continuous", wakeup=True, debug_mode=True)
        _FakeVoiceWakeupListener.created.clear()

        def _notify(title: str, message: str, timeout_ms: int) -> None:
            notifications.append((title, message, int(timeout_ms)))

        with patch("core.voice_runtime.VoiceWakeupListener", _FakeVoiceWakeupListener), patch(
            "core.voice_runtime.QTimer.singleShot", side_effect=lambda _ms, fn: fn()
        ):
            subject = VoiceRuntimeController(
                logger=_LoggerStub(),
                get_config=lambda: config,
                resolve_asr_runtime=lambda _cfg: ("k", "https://asr"),
                notify=_notify,
                execute_voice_command=lambda text, source: command_calls.append((text, source)) or False,
                summon_now_or_notify=lambda source: summon_calls.append(source) or True,
                request_screen_commentary=lambda source: commentary_calls.append(source),
            )
            subject.start_voice_listener()
            listener = _FakeVoiceWakeupListener.created[-1]
            listener.wake_phrase_detected.emit("看看屏幕")
            listener.transcript_updated.emit("实时文本")
            listener.listener_error.emit("网络故障")

        self.assertEqual(command_calls, [("看看屏幕", "wakeup")])
        self.assertIn("wakeup", summon_calls)
        self.assertIn("voice:wakeup_intent", commentary_calls)
        self.assertTrue(any(title == "语音唤醒" for title, _, _ in notifications))
        self.assertTrue(any(title == "语音转写" and msg == "实时文本" for title, msg, _ in notifications))
        self.assertTrue(any(title == "语音降级" for title, _, _ in notifications))


if __name__ == "__main__":
    unittest.main()
