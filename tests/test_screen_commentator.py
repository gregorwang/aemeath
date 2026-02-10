from __future__ import annotations

import sys
import unittest
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from ai.llm_provider import DummyProvider
from ai.screen_commentator import ScreenCommentator


class _FakeAudio:
    def __init__(self):
        self.calls: list[tuple[str, object]] = []

    def speak(self, text: str, priority=None):
        self.calls.append((text, priority))


class ScreenCommentatorTest(unittest.IsolatedAsyncioTestCase):
    async def test_privacy_filter_masks_sensitive_values(self) -> None:
        audio = _FakeAudio()
        commentator = ScreenCommentator(
            llm_provider=DummyProvider(),
            audio_manager=audio,  # type: ignore[arg-type]
            ocr_callable=lambda _img: "mail test@example.com phone 13812345678 ip 192.168.1.1",
        )
        text = await commentator.comment_on_screen(mood_value=0.6)
        self.assertTrue(text)
        self.assertTrue(audio.calls)

        masked = commentator._filter_privacy("a@test.com 13812345678 10.0.0.1 1234567812345678")
        self.assertIn("[邮箱已隐藏]", masked)
        self.assertIn("[手机号已隐藏]", masked)
        self.assertIn("[IP已隐藏]", masked)
        self.assertIn("[银行卡号已隐藏]", masked)

    async def test_fallback_when_text_too_short(self) -> None:
        audio = _FakeAudio()
        commentator = ScreenCommentator(
            llm_provider=DummyProvider(),
            audio_manager=audio,  # type: ignore[arg-type]
            ocr_callable=lambda _img: "短",
        )
        text = await commentator.comment_on_screen(mood_value=0.5)
        self.assertIn("好无聊", text)
        self.assertTrue(audio.calls)

    def test_prompt_changes_with_mood(self) -> None:
        audio = _FakeAudio()
        commentator = ScreenCommentator(
            llm_provider=DummyProvider(),
            audio_manager=audio,  # type: ignore[arg-type]
            ocr_callable=lambda _img: "",
        )
        low = commentator._build_system_prompt(0.1)
        high = commentator._build_system_prompt(0.9)
        self.assertIn("生气", low)
        self.assertIn("粘人", high)


if __name__ == "__main__":
    unittest.main()

