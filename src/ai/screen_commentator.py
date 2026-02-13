from __future__ import annotations

import asyncio
import base64
import ctypes
import ctypes.wintypes
import io
import logging
import threading
from typing import Any, Callable

import numpy as np

from .llm_provider import LLMProvider
from .text_chunker import TextChunker

try:
    from core.audio_manager import AudioManager, AudioPriority
except Exception:
    try:
        from ..core.audio_manager import AudioManager, AudioPriority
    except Exception:
        AudioManager = Any  # type: ignore[assignment]

        class AudioPriority:  # type: ignore[no-redef]
            HIGH = 1


class ScreenCommentator:
    """
    Screenshot -> Vision LLM commentary -> TTS.

    Primary mode: send screenshot as image to a multimodal Vision LLM
    (GPT-4o, Gemini, etc.) and let the model directly "see" the screen.
    No OCR fallback: if vision request fails, return a short failure message.
    """

    # Reduce image to this max dimension to save tokens/bandwidth.
    VISION_MAX_DIMENSION = 1024

    def __init__(
        self,
        llm_provider: LLMProvider,
        audio_manager: AudioManager,
        ocr_callable: Callable[[np.ndarray], str] | None = None,
        on_llm_error: Callable[[str], None] | None = None,
        on_capture_error: Callable[[str], None] | None = None,
        *,
        streaming_enabled: bool = True,
        ocr_fallback_enabled: bool = False,
        stream_chunk_chars: int = 22,
        max_response_chars: int = 90,
        preamble_text: str = "正在看你的屏幕内容，让我看看你在做什么。",
    ):
        self._llm = llm_provider
        self._audio = audio_manager
        self._ocr_callable = ocr_callable
        self._on_llm_error = on_llm_error
        self._on_capture_error = on_capture_error
        self._last_capture_error = ""
        self._logger = logging.getLogger("CyberCompanion")
        self._streaming_enabled = bool(streaming_enabled)
        self._ocr_fallback_enabled = bool(ocr_fallback_enabled)
        self._stream_chunk_chars = max(8, int(stream_chunk_chars))
        self._max_response_chars = max(20, int(max_response_chars))
        self._preamble_text = (preamble_text or "").strip()
        self._session_lock = threading.Lock()
        self._active_session_id = 0

    def set_llm_provider(self, llm_provider: LLMProvider) -> None:
        self._llm = llm_provider

    def set_llm_error_callback(self, callback: Callable[[str], None] | None) -> None:
        self._on_llm_error = callback

    def set_capture_error_callback(self, callback: Callable[[str], None] | None) -> None:
        self._on_capture_error = callback

    def set_runtime_options(
        self,
        *,
        streaming_enabled: bool,
        ocr_fallback_enabled: bool,
        stream_chunk_chars: int,
        max_response_chars: int,
        preamble_text: str,
    ) -> None:
        self._streaming_enabled = bool(streaming_enabled)
        # OCR fallback is removed; keep argument for backward compatibility.
        self._ocr_fallback_enabled = False
        self._stream_chunk_chars = max(8, int(stream_chunk_chars))
        self._max_response_chars = max(20, int(max_response_chars))
        self._preamble_text = (preamble_text or "").strip()

    def cancel_current_session(self) -> None:
        with self._session_lock:
            self._active_session_id += 1

    async def comment_on_screen(self, mood_value: float = 0.5) -> str:
        session_id = self._start_new_session()
        self._logger.info(
            "[ScreenCommentary] Session start: id=%s mood=%.2f stream=%s provider=%s",
            session_id,
            mood_value,
            self._streaming_enabled,
            self._describe_provider(),
        )
        self._audio.interrupt(clear_pending_playback=True, clear_pending_tts=True)
        if self._preamble_text:
            self._audio.speak(self._preamble_text, priority=AudioPriority.HIGH)
            self._logger.debug("[ScreenCommentary] TTS preamble queued: %s", self._preamble_text[:48])

        screenshot = self._capture_active_window()
        if self._last_capture_error:
            if self._on_capture_error is not None:
                self._on_capture_error(self._last_capture_error)
            elif self._on_llm_error is not None:
                self._on_llm_error(self._last_capture_error)
        elif isinstance(screenshot, np.ndarray) and screenshot.ndim >= 2:
            h = int(screenshot.shape[0]) if screenshot.shape else 0
            w = int(screenshot.shape[1]) if len(screenshot.shape) > 1 else 0
            self._logger.debug("[ScreenCommentary] Screenshot captured: %dx%d", w, h)

        # ── Strategy 1: Vision LLM (send screenshot image directly) ──
        image_b64 = self._image_to_base64(screenshot)
        if image_b64:
            self._logger.debug(
                "[ScreenCommentary] Encoded screenshot: %d bytes(base64) ~ %.1f KB",
                len(image_b64),
                len(image_b64) * 3 / 4 / 1024.0,
            )
            speak_text = await self._try_vision_llm(image_b64, mood_value, session_id=session_id)
            if speak_text:
                return speak_text
        else:
            self._logger.warning("[ScreenCommentary] Screenshot encoding failed: empty image payload")

        fallback = "我这次没看清你的屏幕，稍后再试一次。"
        if self._is_session_active(session_id):
            self._audio.speak(fallback, priority=AudioPriority.HIGH)
            self._logger.info("[ScreenCommentary] Fallback TTS queued")
        return fallback

    def comment_on_screen_sync(self, mood_value: float = 0.5) -> str:
        return asyncio.run(self.comment_on_screen(mood_value=mood_value))

    # ─── Vision LLM ──────────────────────────────────────────────

    async def _try_vision_llm(self, image_b64: str, mood_value: float, *, session_id: int) -> str:
        """Send screenshot to a Vision-capable LLM and get commentary."""
        if self._streaming_enabled:
            try:
                self._logger.info("[ScreenCommentary] Sending screenshot to Vision LLM (stream mode)")
                text = await self._try_vision_llm_stream(image_b64=image_b64, mood_value=mood_value, session_id=session_id)
                if text:
                    return text
            except NotImplementedError:
                pass
            except Exception as exc:
                self._logger.debug("[ScreenCommentary] Vision stream failed: %s", exc)

        try:
            self._logger.info("[ScreenCommentary] Sending screenshot to Vision LLM (single-shot)")
            response = await self._llm.generate_with_image(
                system_prompt=self._build_vision_system_prompt(mood_value),
                image_base64=image_b64,
                user_message="看看我的屏幕，用你的风格吐槽一下我正在做什么。",
                max_tokens=self._max_response_chars,
                temperature=0.8,
            )
            text = response.text.strip()
            if text and response.provider != "dummy":
                self._logger.info(
                    "[ScreenCommentary] Vision response ok: provider=%s latency=%dms tokens=%d chars=%d",
                    response.provider,
                    response.latency_ms,
                    response.tokens_used,
                    len(text),
                )
                if self._is_session_active(session_id):
                    self._audio.speak(text[: self._max_response_chars], priority=AudioPriority.HIGH)
                    self._logger.debug("[ScreenCommentary] TTS reply queued: chars=%d", min(len(text), self._max_response_chars))
                return text
        except NotImplementedError:
            # Provider does not support vision, fall through to OCR.
            pass
        except Exception as exc:
            if self._on_llm_error is not None:
                self._on_llm_error(str(exc))
        return ""

    async def _try_vision_llm_stream(self, image_b64: str, mood_value: float, *, session_id: int) -> str:
        chunker = TextChunker(
            target_chunk_chars=self._stream_chunk_chars,
            max_chunk_chars=max(self._stream_chunk_chars + 14, self._stream_chunk_chars),
        )
        full_text_parts: list[str] = []
        spoken_any = False
        delta_count = 0
        spoken_chunks = 0

        async for delta in self._llm.generate_with_image_stream(
            system_prompt=self._build_vision_system_prompt(mood_value),
            image_base64=image_b64,
            user_message="看看我的屏幕，直接告诉我我正在做什么。",
            max_tokens=self._max_response_chars,
            temperature=0.7,
        ):
            if not self._is_session_active(session_id):
                return ""
            if not delta:
                continue
            delta_count += 1
            full_text_parts.append(delta)
            merged = "".join(full_text_parts)
            if len(merged) >= self._max_response_chars:
                merged = merged[: self._max_response_chars]
                full_text_parts = [merged]

            chunks = chunker.feed(delta)
            for chunk in chunks:
                if not self._is_session_active(session_id):
                    return ""
                cleaned = chunk.strip()
                if not cleaned:
                    continue
                self._audio.speak(cleaned, priority=AudioPriority.HIGH)
                spoken_any = True
                spoken_chunks += 1

            if len("".join(full_text_parts)) >= self._max_response_chars:
                break

        remaining = chunker.flush().strip()
        if remaining and self._is_session_active(session_id):
            self._audio.speak(remaining, priority=AudioPriority.HIGH)
            spoken_any = True
            spoken_chunks += 1

        full_text = "".join(full_text_parts).strip()
        if not full_text:
            return ""
        if not spoken_any and self._is_session_active(session_id):
            self._audio.speak(full_text[: self._max_response_chars], priority=AudioPriority.HIGH)
            spoken_chunks += 1
        self._logger.info(
            "[ScreenCommentary] Vision stream ok: deltas=%d chars=%d tts_chunks=%d",
            delta_count,
            len(full_text),
            spoken_chunks,
        )
        return full_text

    # ─── Image Encoding ──────────────────────────────────────────

    def _image_to_base64(self, image: np.ndarray) -> str:
        """Convert numpy screenshot to base64 JPEG for Vision LLM."""
        try:
            from PIL import Image
        except ImportError:
            return ""

        if image.size <= 4:
            return ""

        try:
            # Convert BGRA -> RGB
            if len(image.shape) == 3 and image.shape[2] == 4:
                rgb = image[:, :, [2, 1, 0]]
            elif len(image.shape) == 3 and image.shape[2] == 3:
                rgb = image[:, :, [2, 1, 0]]
            else:
                rgb = image

            pil_image = Image.fromarray(rgb)

            # Resize to save bandwidth while preserving enough detail
            w, h = pil_image.size
            max_dim = self.VISION_MAX_DIMENSION
            if max(w, h) > max_dim:
                scale = max_dim / max(w, h)
                pil_image = pil_image.resize(
                    (int(w * scale), int(h * scale)),
                    Image.Resampling.LANCZOS,
                )

            buf = io.BytesIO()
            pil_image.save(buf, format="JPEG", quality=75)
            return base64.b64encode(buf.getvalue()).decode("ascii")
        except Exception:
            return ""

    # ─── Screenshot Capture ───────────────────────────────────────

    def _capture_active_window(self) -> np.ndarray:
        self._last_capture_error = ""
        try:
            from PIL import ImageGrab
        except Exception as exc:
            self._last_capture_error = f"屏幕捕获失败：缺少 Pillow 依赖 ({exc})"
            self._logger.warning("[ScreenCommentary] %s", self._last_capture_error)
            return np.zeros((1, 1, 4), dtype=np.uint8)

        try:
            bbox = self._get_foreground_window_bbox()
            if bbox is not None:
                screenshot = ImageGrab.grab(bbox=bbox, all_screens=True)
            else:
                screenshot = ImageGrab.grab(all_screens=True)
            rgb = np.array(screenshot.convert("RGB"), dtype=np.uint8)
            if rgb.size <= 3:
                raise RuntimeError("截图结果为空")
            # Keep historical BGR order to stay compatible with existing pipeline.
            return rgb[:, :, ::-1]
        except Exception as exc:
            self._last_capture_error = f"屏幕捕获失败：{exc}"
            self._logger.warning("[ScreenCommentary] %s", self._last_capture_error)
            return np.zeros((1, 1, 4), dtype=np.uint8)

    @staticmethod
    def _get_foreground_window_bbox() -> tuple[int, int, int, int] | None:
        try:
            user32 = ctypes.windll.user32
            hwnd = user32.GetForegroundWindow()
            if not hwnd:
                return None
            rect = ctypes.wintypes.RECT()
            if not user32.GetWindowRect(hwnd, ctypes.byref(rect)):
                return None
            left = int(rect.left)
            top = int(rect.top)
            right = int(rect.right)
            bottom = int(rect.bottom)
            if right - left < 8 or bottom - top < 8:
                return None
            return left, top, right, bottom
        except Exception:
            return None

    def _build_vision_system_prompt(self, mood_value: float) -> str:
        personality = self._get_personality(mood_value)
        return (
            f"你是一个赛博朋克风格的桌面伴侣 AI 少女。{personality}\n"
            "规则：\n"
            "1. 用户会发给你一张他的屏幕截图，请你直接观察截图内容\n"
            "2. 必须先说结论，再补充一个细节，总共 1-2 句，适合语音播报\n"
            "3. 口语化、自然、简短，总字数不超过 60 字\n"
            "4. 如果看到代码报错，可以给出一句简短建议\n"
            "5. 如果看到聊天窗口或隐私内容，装作没看到\n"
            "6. 使用中文，不要输出 Markdown 或项目符号\n"
            "7. 直接对用户说话，不要解释你是模型"
        )

    @staticmethod
    def _get_personality(mood_value: float) -> str:
        if mood_value < 0.3:
            return "你现在很生气，不太想理用户。回复简短冷淡。"
        if mood_value > 0.7:
            return "你现在心情很好，有点粘人。回复活泼可爱，会用颜文字。"
        return "你是一个傲娇毒舌的风格。回复犀利但关心对方。"

    def _start_new_session(self) -> int:
        with self._session_lock:
            self._active_session_id += 1
            return self._active_session_id

    def _is_session_active(self, session_id: int) -> bool:
        with self._session_lock:
            return self._active_session_id == session_id

    def _describe_provider(self) -> str:
        name = self._llm.__class__.__name__
        model = str(getattr(self._llm, "_model", "") or "").strip()
        base_url = str(getattr(self._llm, "_base_url", "") or "").strip()
        parts = [name]
        if model:
            parts.append(f"model={model}")
        if base_url:
            parts.append(f"base={base_url}")
        return ", ".join(parts)
