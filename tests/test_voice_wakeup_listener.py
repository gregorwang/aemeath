from __future__ import annotations

import sys
import types
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

try:
    import PySide6  # type: ignore  # noqa: F401
except ModuleNotFoundError:
    pyside_module = types.ModuleType("PySide6")
    qtcore_module = types.ModuleType("PySide6.QtCore")

    class _StubQThread:
        def __init__(self, parent=None):
            self._parent = parent

    class _StubSignal:
        def __init__(self, *args, **kwargs):
            self._args = args
            self._kwargs = kwargs

    qtcore_module.QThread = _StubQThread
    qtcore_module.Signal = _StubSignal
    pyside_module.QtCore = qtcore_module
    sys.modules["PySide6"] = pyside_module
    sys.modules["PySide6.QtCore"] = qtcore_module

from core.voice_wakeup import VoiceWakeupListener


class VoiceWakeupListenerTest(unittest.TestCase):
    def test_normalize_base_url(self) -> None:
        self.assertEqual(VoiceWakeupListener._normalize_base_url("https://poloai.top"), "https://poloai.top/v1")
        self.assertEqual(VoiceWakeupListener._normalize_base_url("https://api.openai.com/v1"), "https://api.openai.com/v1")
        self.assertEqual(
            VoiceWakeupListener._normalize_base_url("https://poloai.top/v1/chat/completions"),
            "https://poloai.top/v1",
        )
        self.assertEqual(
            VoiceWakeupListener._normalize_base_url("https://poloai.top/v1/audio/transcriptions"),
            "https://poloai.top/v1",
        )
        self.assertEqual(
            VoiceWakeupListener._normalize_base_url("https://example.com/custom-prefix/v1/audio/transcriptions"),
            "https://example.com/custom-prefix/v1",
        )
        self.assertEqual(
            VoiceWakeupListener._normalize_base_url("not-a-url"),
            "https://api.openai.com/v1",
        )

    def test_normalize_whisper_language(self) -> None:
        self.assertEqual(VoiceWakeupListener._normalize_whisper_language("zh-CN"), "zh")
        self.assertEqual(VoiceWakeupListener._normalize_whisper_language("en"), "en")
        self.assertEqual(VoiceWakeupListener._normalize_whisper_language(""), "")
        self.assertEqual(VoiceWakeupListener._normalize_whisper_language("invalid-language"), "")

    def test_extract_transcription_text(self) -> None:
        self.assertEqual(VoiceWakeupListener._extract_transcription_text({"text": "hello"}), "hello")
        self.assertEqual(VoiceWakeupListener._extract_transcription_text("plain"), "plain")
        self.assertEqual(VoiceWakeupListener._extract_transcription_text({"text": 123}), "")

    def test_provider_fallback(self) -> None:
        self.assertEqual(VoiceWakeupListener._normalize_recognition_provider("openai_whisper"), "openai_whisper")
        self.assertEqual(VoiceWakeupListener._normalize_recognition_provider("openai"), "openai_whisper")
        self.assertEqual(VoiceWakeupListener._normalize_recognition_provider("whisper"), "openai_whisper")
        self.assertEqual(VoiceWakeupListener._normalize_recognition_provider("google"), "google")
        self.assertEqual(VoiceWakeupListener._normalize_recognition_provider("google_webspeech"), "google")
        self.assertEqual(VoiceWakeupListener._normalize_recognition_provider("zhipu"), "zhipu_asr")
        self.assertEqual(VoiceWakeupListener._normalize_recognition_provider("zhipu_asr"), "zhipu_asr")
        self.assertEqual(VoiceWakeupListener._normalize_recognition_provider("xai_realtime"), "xai_realtime")
        self.assertEqual(VoiceWakeupListener._normalize_recognition_provider("xai"), "xai_realtime")
        self.assertEqual(VoiceWakeupListener._normalize_recognition_provider("unknown"), "xai_realtime")

    def test_build_xai_realtime_endpoint(self) -> None:
        self.assertEqual(
            VoiceWakeupListener._build_xai_realtime_endpoint(
                base_url="https://api.x.ai/v1",
                model="grok-2-mini-transcribe",
            ),
            "wss://api.x.ai/v1/realtime?model=grok-2-mini-transcribe",
        )
        self.assertEqual(
            VoiceWakeupListener._build_xai_realtime_endpoint(
                base_url="https://api.x.ai/v1",
                model="whisper-1",
            ),
            "wss://api.x.ai/v1/realtime?model=grok-2-mini-transcribe",
        )

    def test_parse_realtime_event(self) -> None:
        event = VoiceWakeupListener._parse_realtime_event(
            b'{"type":"conversation.item.input_audio_transcription.completed","transcript":"hello"}'
        )
        self.assertIsInstance(event, dict)
        self.assertEqual(event["type"], "conversation.item.input_audio_transcription.completed")
        self.assertIsNone(VoiceWakeupListener._parse_realtime_event("not-json"))

    def test_extract_xai_transcription_text(self) -> None:
        self.assertEqual(
            VoiceWakeupListener._extract_xai_transcription_text({"transcript": "hello"}),
            "hello",
        )
        self.assertEqual(
            VoiceWakeupListener._extract_xai_transcription_text(
                {
                    "item": {
                        "content": [
                            {"type": "input_audio", "transcript": "world"},
                        ]
                    }
                }
            ),
            "world",
        )

    def test_detect_realtime_error_event(self) -> None:
        self.assertTrue(VoiceWakeupListener._is_realtime_error_event({"type": "error"}))
        self.assertTrue(
            VoiceWakeupListener._is_realtime_error_event(
                {"type": "conversation.item.input_audio_transcription.failed"}
            )
        )
        self.assertFalse(
            VoiceWakeupListener._is_realtime_error_event(
                {"type": "conversation.item.input_audio_transcription.completed"}
            )
        )

    def test_prefer_legacy_for_transcribe_model(self) -> None:
        self.assertTrue(VoiceWakeupListener._should_prefer_xai_legacy_first("grok-2-mini-transcribe"))
        self.assertFalse(VoiceWakeupListener._should_prefer_xai_legacy_first("grok-4-fast"))

    def test_http_status_hint_for_xai(self) -> None:
        hint = VoiceWakeupListener._build_http_status_hint(
            status_code=401,
            base_url="https://api.x.ai/v1",
            response_body='{"error":"invalid_api_key"}',
        )
        self.assertIn("API Key", hint)
        self.assertIn("xAI", hint)
        self.assertIn("invalid_api_key", hint)

    def test_http_status_hint_for_404(self) -> None:
        hint = VoiceWakeupListener._build_http_status_hint(
            status_code=404,
            base_url="https://example.com/v1",
            response_body="not found",
        )
        self.assertIn("/v1/audio/transcriptions", hint)

    def test_normalize_zhipu_base_url(self) -> None:
        self.assertEqual(
            VoiceWakeupListener._normalize_zhipu_base_url(""),
            "https://open.bigmodel.cn/api/paas/v4/audio/transcriptions",
        )
        self.assertEqual(
            VoiceWakeupListener._normalize_zhipu_base_url("https://open.bigmodel.cn/api/paas/v4"),
            "https://open.bigmodel.cn/api/paas/v4/audio/transcriptions",
        )
        self.assertEqual(
            VoiceWakeupListener._normalize_zhipu_base_url("https://open.bigmodel.cn/api/paas/v4/audio/transcriptions"),
            "https://open.bigmodel.cn/api/paas/v4/audio/transcriptions",
        )

    def test_extract_transcription_text_from_nested_data(self) -> None:
        self.assertEqual(
            VoiceWakeupListener._extract_transcription_text({"data": {"text": "nested"}}),
            "nested",
        )


if __name__ == "__main__":
    unittest.main()
