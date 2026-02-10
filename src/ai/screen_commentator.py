from __future__ import annotations

import asyncio
import re
from typing import Callable

import numpy as np

from .llm_provider import LLMProvider, LLMRequest

try:
    from core.audio_manager import AudioManager, AudioPriority
except ModuleNotFoundError:
    from ..core.audio_manager import AudioManager, AudioPriority


class ScreenCommentator:
    """
    Screenshot -> OCR -> privacy filter -> LLM commentary -> TTS.
    """

    def __init__(
        self,
        llm_provider: LLMProvider,
        audio_manager: AudioManager,
        ocr_callable: Callable[[np.ndarray], str] | None = None,
    ):
        self._llm = llm_provider
        self._audio = audio_manager
        self._ocr_callable = ocr_callable

    async def comment_on_screen(self, mood_value: float = 0.5) -> str:
        screenshot = self._capture_active_window()
        text = self._extract_text(screenshot)
        text = self._filter_privacy(text)

        if len(text.strip()) < 10:
            fallback = "哼，屏幕上什么都没有，好无聊。"
            self._audio.speak(fallback, priority=AudioPriority.HIGH)
            return fallback

        request = LLMRequest(
            system_prompt=self._build_system_prompt(mood_value),
            user_message=f"用户当前屏幕上显示以下文字：\n{text[:500]}",
            max_tokens=80,
            temperature=0.8,
        )
        response = await self._llm.generate(request)
        speak_text = response.text.strip() or "我一时语塞了。"
        self._audio.speak(speak_text, priority=AudioPriority.HIGH)
        return speak_text

    def comment_on_screen_sync(self, mood_value: float = 0.5) -> str:
        return asyncio.run(self.comment_on_screen(mood_value=mood_value))

    def _capture_active_window(self) -> np.ndarray:
        try:
            import mss
        except Exception:
            return np.zeros((1, 1, 4), dtype=np.uint8)

        with mss.mss() as sct:
            monitor = sct.monitors[1]
            screenshot = sct.grab(monitor)
            return np.array(screenshot)

    def _extract_text(self, image: np.ndarray) -> str:
        if self._ocr_callable is not None:
            try:
                return self._ocr_callable(image)
            except Exception:
                return ""

        # Optional OCR backend: PaddleOCR if available, else empty text.
        try:
            from paddleocr import PaddleOCR  # type: ignore
        except Exception:
            return ""

        try:
            ocr = PaddleOCR(use_angle_cls=True, lang="ch", show_log=False)
            result = ocr.ocr(image, cls=True)
            chunks: list[str] = []
            for item in result or []:
                for line in item or []:
                    if len(line) >= 2 and isinstance(line[1], (list, tuple)) and line[1]:
                        text = str(line[1][0]).strip()
                        if text:
                            chunks.append(text)
            return "\n".join(chunks)
        except Exception:
            return ""

    def _filter_privacy(self, text: str) -> str:
        text = re.sub(r"[\w.-]+@[\w.-]+\.\w+", "[邮箱已隐藏]", text)
        text = re.sub(r"1[3-9]\d{9}", "[手机号已隐藏]", text)
        text = re.sub(r"\d{1,3}(?:\.\d{1,3}){3}", "[IP已隐藏]", text)
        text = re.sub(r"\b\d{16}\b", "[银行卡号已隐藏]", text)
        return text

    def _build_system_prompt(self, mood_value: float) -> str:
        if mood_value < 0.3:
            personality = "你现在很生气，不太想理用户。回复简短冷淡。"
        elif mood_value > 0.7:
            personality = "你现在心情很好，有点粘人。回复活泼可爱，会用颜文字。"
        else:
            personality = "你是一个傲娇毒舌的风格。回复犀利但关心对方。"

        return (
            f"你是一个赛博朋克风格的桌面伴侣 AI 少女。{personality}\n"
            "规则：\n"
            "1. 根据屏幕文字内容，用简短吐槽的语气评论用户正在做的事\n"
            "2. 回复不超过 30 个字\n"
            "3. 如果是代码报错，可以给出简短建议\n"
            "4. 如果是聊天记录，装作没看到\n"
            "5. 使用中文回复"
        )
