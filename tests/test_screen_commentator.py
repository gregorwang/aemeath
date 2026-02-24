from __future__ import annotations

import sys
import types
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from ai.llm_provider import DummyProvider, LLMResponse
from ai.screen_commentator import ScreenCommentator


class _FakeAudio:
    def __init__(self):
        self.calls: list[tuple[str, object]] = []
        self.interrupt_calls = 0

    def speak(self, text: str, priority=None):
        self.calls.append((text, priority))

    def interrupt(self, *, clear_pending_playback=True, clear_pending_tts=True):
        _ = (clear_pending_playback, clear_pending_tts)
        self.interrupt_calls += 1


class _VisionProvider(DummyProvider):
    async def generate_with_image(
        self,
        system_prompt: str,
        image_base64: str,
        user_message: str = "",
        max_tokens: int = 100,
        temperature: float = 0.7,
    ) -> LLMResponse:
        _ = (system_prompt, image_base64, user_message, max_tokens, temperature)
        return LLMResponse(text="你在写代码，我看见终端了。", tokens_used=12, latency_ms=50, provider="openai")


class _StreamVisionProvider(DummyProvider):
    async def generate_with_image_stream(
        self,
        system_prompt: str,
        image_base64: str,
        user_message: str = "",
        max_tokens: int = 100,
        temperature: float = 0.7,
    ):
        _ = (system_prompt, image_base64, user_message, max_tokens, temperature)
        for token in ("你在", "调试", "日志。"):
            yield token


class ScreenCommentatorTest(unittest.IsolatedAsyncioTestCase):
    async def test_vision_success(self) -> None:
        audio = _FakeAudio()
        commentator = ScreenCommentator(
            llm_provider=_VisionProvider(),
            audio_manager=audio,  # type: ignore[arg-type]
        )
        commentator._capture_active_window = lambda: np.zeros((10, 10, 3), dtype=np.uint8)  # type: ignore[method-assign]
        commentator._image_to_base64 = lambda _img: "base64-image"  # type: ignore[method-assign]

        text = await commentator.comment_on_screen(mood_value=0.6)
        self.assertIn("写代码", text)
        self.assertGreaterEqual(len(audio.calls), 2)  # preamble + response

    async def test_streaming_vision_success(self) -> None:
        audio = _FakeAudio()
        commentator = ScreenCommentator(
            llm_provider=_StreamVisionProvider(),
            audio_manager=audio,  # type: ignore[arg-type]
            streaming_enabled=True,
            stream_chunk_chars=2,
        )
        commentator._capture_active_window = lambda: np.zeros((10, 10, 3), dtype=np.uint8)  # type: ignore[method-assign]
        commentator._image_to_base64 = lambda _img: "base64-image"  # type: ignore[method-assign]

        text = await commentator.comment_on_screen(mood_value=0.5)
        self.assertIn("日志", text)
        self.assertTrue(any("调试" in spoken for spoken, _ in audio.calls))

    async def test_fallback_when_vision_unavailable(self) -> None:
        audio = _FakeAudio()
        commentator = ScreenCommentator(
            llm_provider=DummyProvider(),
            audio_manager=audio,  # type: ignore[arg-type]
        )
        commentator._capture_active_window = lambda: np.zeros((10, 10, 3), dtype=np.uint8)  # type: ignore[method-assign]
        commentator._image_to_base64 = lambda _img: "base64-image"  # type: ignore[method-assign]

        text = await commentator.comment_on_screen(mood_value=0.5)
        self.assertIn("没看清你的屏幕", text)
        self.assertTrue(audio.calls)

    def test_prompt_changes_with_mood(self) -> None:
        audio = _FakeAudio()
        commentator = ScreenCommentator(
            llm_provider=DummyProvider(),
            audio_manager=audio,  # type: ignore[arg-type]
        )
        low = commentator._build_vision_system_prompt(0.1)
        high = commentator._build_vision_system_prompt(0.9)
        self.assertIn("生气", low)
        self.assertIn("粘人", high)

    def test_image_to_base64_prefers_cv2_resize_and_imencode(self) -> None:
        audio = _FakeAudio()
        commentator = ScreenCommentator(
            llm_provider=DummyProvider(),
            audio_manager=audio,  # type: ignore[arg-type]
        )
        large_bgr = np.zeros((2200, 1800, 3), dtype=np.uint8)
        calls = {"resize": 0, "imencode": 0}

        fake_cv2 = types.ModuleType("cv2")
        fake_cv2.IMWRITE_JPEG_QUALITY = 1
        fake_cv2.INTER_AREA = 3
        fake_cv2.COLOR_GRAY2BGR = 8

        def _resize(image, size, interpolation=None):
            _ = interpolation
            calls["resize"] += 1
            w, h = int(size[0]), int(size[1])
            return np.zeros((h, w, 3), dtype=np.uint8)

        def _imencode(ext, image, params=None):
            _ = (ext, image, params)
            calls["imencode"] += 1
            return True, np.frombuffer(b"jpeg-bytes", dtype=np.uint8)

        def _cvt_color(image, _code):
            if image.ndim == 2:
                return np.stack([image, image, image], axis=-1)
            return image

        fake_cv2.resize = _resize  # type: ignore[attr-defined]
        fake_cv2.imencode = _imencode  # type: ignore[attr-defined]
        fake_cv2.cvtColor = _cvt_color  # type: ignore[attr-defined]

        with patch.dict(sys.modules, {"cv2": fake_cv2}):
            b64 = commentator._image_to_base64(large_bgr)

        self.assertTrue(b64)
        self.assertEqual(calls["resize"], 1)
        self.assertEqual(calls["imencode"], 1)


class ScreenCommentatorSyncLoopTest(unittest.TestCase):
    def test_comment_on_screen_sync_reuses_background_loop(self) -> None:
        audio = _FakeAudio()
        commentator = ScreenCommentator(
            llm_provider=_VisionProvider(),
            audio_manager=audio,  # type: ignore[arg-type]
        )
        commentator._capture_active_window = lambda: np.zeros((10, 10, 3), dtype=np.uint8)  # type: ignore[method-assign]
        commentator._image_to_base64 = lambda _img: "base64-image"  # type: ignore[method-assign]

        try:
            text_a = commentator.comment_on_screen_sync(mood_value=0.5)
            first_thread = commentator._sync_loop_thread
            text_b = commentator.comment_on_screen_sync(mood_value=0.6)
            second_thread = commentator._sync_loop_thread
        finally:
            commentator.shutdown()

        self.assertIn("写代码", text_a)
        self.assertIn("写代码", text_b)
        self.assertIsNotNone(first_thread)
        self.assertIs(first_thread, second_thread)


if __name__ == "__main__":
    unittest.main()
