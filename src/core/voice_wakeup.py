from __future__ import annotations

import base64
import json
import logging
import os
import threading
import time
from typing import Any, Iterable
from urllib.parse import urlencode, urlsplit, urlunsplit

from PySide6.QtCore import QThread, Signal

logger = logging.getLogger("CyberCompanion")


class WakeupRecognitionError(RuntimeError):
    """Raised when the ASR backend cannot complete recognition."""


class VoiceWakeupListener(QThread):
    """
    Optional microphone wake-word listener.

    This module is fail-safe:
    - Missing dependency -> emit error and stop.
    - Microphone unavailable -> emit error and stop.
    - Network recognition failure -> emit error and stop.
    """

    _XAI_REALTIME_TIMEOUT_SECONDS = 25.0
    _XAI_REALTIME_INPUT_SAMPLE_RATE = 16000
    _XAI_REALTIME_OUTPUT_SAMPLE_RATE = 24000
    _ZHIPU_ASR_ENDPOINT = "https://open.bigmodel.cn/api/paas/v4/audio/transcriptions"
    _ZHIPU_MAX_AUDIO_BYTES = 25 * 1024 * 1024
    _ZHIPU_MAX_AUDIO_SECONDS = 30.0
    _XAI_TRANSIENT_ERROR_KEYWORDS = (
        "timeout",
        "timed out",
        "connection to remote host was lost",
        "connection reset",
        "temporarily unavailable",
        "network",
        "1006",
        "503",
        "504",
    )
    _XAI_MAX_CONSECUTIVE_ERRORS = 4
    _LAST_WAKEUP_MONOTONIC = 0.0
    _LAST_WAKEUP_PHRASE = ""
    _LAST_WAKEUP_LOCK = threading.Lock()

    wake_phrase_detected = Signal(str)
    listener_state_changed = Signal(bool)
    listener_error = Signal(str)
    transcript_updated = Signal(str)

    def __init__(
        self,
        *,
        phrases: Iterable[str],
        language: str = "zh-CN",
        recognition_provider: str = "xai_realtime",
        openai_api_key: str = "",
        openai_base_url: str = "https://api.x.ai/v1",
        openai_model: str = "grok-2-mini-transcribe",
        openai_prompt: str = "",
        openai_temperature: float = 0.0,
        parent=None,
    ):
        super().__init__(parent)
        self._phrases = tuple(item.strip() for item in phrases if str(item).strip())
        self._language = language.strip() or "zh-CN"
        self._recognition_provider = self._normalize_recognition_provider(recognition_provider)
        self._openai_api_key = (
            openai_api_key.strip()
            or os.environ.get("OPENAI_API_KEY", "")
            or os.environ.get("POLOAI_API_KEY", "")
            or os.environ.get("XAI_API_KEY", "")
        )
        if self._recognition_provider == "zhipu_asr":
            self._openai_base_url = self._normalize_zhipu_base_url(openai_base_url)
        else:
            self._openai_base_url = self._normalize_base_url(openai_base_url)
        self._openai_model = openai_model.strip() or "grok-2-mini-transcribe"
        self._openai_prompt = openai_prompt.strip()
        try:
            self._openai_temperature = min(max(float(openai_temperature), 0.0), 1.0)
        except Exception:
            self._openai_temperature = 0.0
        self._running = False

    def start_listening(self) -> None:
        if self.isRunning():
            logger.debug("[VoiceWakeup] start_listening 被调用但线程已在运行，跳过")
            return
        if not self._phrases:
            logger.warning("[VoiceWakeup] 唤醒词为空，语音唤醒已停用")
            self.listener_error.emit("唤醒词为空，语音唤醒已停用。")
            return
        logger.info("[VoiceWakeup] 准备启动语音监听，唤醒词=%s, 语言=%s, 提供商=%s",
                    self._phrases, self._language, self._recognition_provider)
        self._running = True
        self.start()

    def stop_listening(self) -> None:
        self._running = False
        if self.isRunning():
            self.wait(3000)

    def run(self) -> None:
        logger.info("[VoiceWakeup] 后台线程已启动")
        try:
            import speech_recognition as sr  # type: ignore
            logger.debug("[VoiceWakeup] ✅ SpeechRecognition 导入成功")
        except Exception as exc:
            logger.error("[VoiceWakeup] ❌ 缺少 SpeechRecognition/PyAudio: %s", exc)
            self.listener_error.emit("缺少 SpeechRecognition/PyAudio，已降级为仅文字交互。")
            self.listener_state_changed.emit(False)
            return

        recognizer = sr.Recognizer()
        try:
            microphone = sr.Microphone()
            logger.debug("[VoiceWakeup] ✅ 麦克风初始化成功")
        except Exception as exc:
            logger.error("[VoiceWakeup] ❌ 麦克风不可用: %s", exc)
            self.listener_error.emit("麦克风不可用或权限不足，已降级为仅文字交互。")
            self.listener_state_changed.emit(False)
            return

        if self._recognition_provider in {"openai_whisper", "xai_realtime", "zhipu_asr"} and not self._openai_api_key.strip():
            provider_name = (
                "Whisper"
                if self._recognition_provider == "openai_whisper"
                else "xAI Realtime"
                if self._recognition_provider == "xai_realtime"
                else "Zhipu ASR"
            )
            logger.error("[VoiceWakeup] ❌ %s API Key 为空", provider_name)
            self.listener_error.emit(f"{provider_name} API Key 未填写，请在设置中配置。语音唤醒已停用。")
            self.listener_state_changed.emit(False)
            return

        logger.info("[VoiceWakeup] ✅ 所有检查通过，开始持续监听麦克风...")
        if self._recognition_provider == "xai_realtime":
            logger.info(
                "[VoiceWakeup] ASR Base URL: %s | Endpoint: %s (model=%s)",
                self._openai_base_url,
                self._build_xai_realtime_endpoint(base_url=self._openai_base_url, model=self._openai_model),
                self._openai_model,
            )
        elif self._recognition_provider == "zhipu_asr":
            logger.info(
                "[VoiceWakeup] ASR Endpoint: %s (model=%s)",
                self._openai_base_url,
                self._openai_model or "glm-asr-2512",
            )
        else:
            logger.info(
                "[VoiceWakeup] ASR Base URL: %s | Endpoint: %s/audio/transcriptions (model=%s)",
                self._openai_base_url,
                self._openai_base_url,
                self._openai_model,
            )
        self.listener_state_changed.emit(True)
        listen_count = 0
        consecutive_xai_failures = 0
        try:
            with microphone as source:
                logger.debug("[VoiceWakeup] 正在校准环境噪音 (0.8s)...")
                recognizer.adjust_for_ambient_noise(source, duration=0.8)
                logger.debug("[VoiceWakeup] 噪音校准完成，进入监听循环")
                while self._running:
                    try:
                        audio = recognizer.listen(source, timeout=2, phrase_time_limit=4)
                        listen_count += 1
                    except sr.WaitTimeoutError:
                        continue
                    except Exception as exc:
                        logger.debug("[VoiceWakeup] 录音异常 (非致命): %s", exc)
                        time.sleep(0.2)
                        continue

                    try:
                        heard = self._recognize(audio=audio, recognizer=recognizer).strip()
                        consecutive_xai_failures = 0
                    except sr.UnknownValueError:
                        logger.debug("[VoiceWakeup] 片段 #%d: 未识别到语音 (静音/噪音)", listen_count)
                        continue
                    except WakeupRecognitionError as exc:
                        if (
                            self._recognition_provider == "xai_realtime"
                            and self._is_transient_xai_error(str(exc))
                        ):
                            consecutive_xai_failures += 1
                            if consecutive_xai_failures < self._XAI_MAX_CONSECUTIVE_ERRORS:
                                retry_delay = min(8.0, 0.8 * (2 ** (consecutive_xai_failures - 1)))
                                logger.warning(
                                    "[VoiceWakeup] xAI Realtime 瞬时异常，%.1fs 后自动重试 (%d/%d): %s",
                                    retry_delay,
                                    consecutive_xai_failures,
                                    self._XAI_MAX_CONSECUTIVE_ERRORS - 1,
                                    exc,
                                )
                                time.sleep(retry_delay)
                                continue
                        logger.error("[VoiceWakeup] ❌ ASR 致命错误: %s", exc)
                        self.listener_error.emit(str(exc))
                        break
                    except sr.RequestError as exc:
                        logger.error("[VoiceWakeup] ❌ 语音识别网络不可用: %s", exc)
                        self.listener_error.emit("语音识别网络不可用，已降级为仅文字交互。")
                        break
                    except Exception as exc:
                        logger.debug("[VoiceWakeup] 识别异常 (非致命): %s", exc)
                        continue

                    if not heard:
                        logger.debug("[VoiceWakeup] 片段 #%d: 识别结果为空", listen_count)
                        continue

                    self.transcript_updated.emit(heard)
                    lowered = heard.lower()
                    matched = any(phrase.lower() in lowered for phrase in self._phrases)
                    logger.info("[VoiceWakeup] 片段 #%d 转写: \"%s\" → %s",
                                listen_count, heard, "✅ 匹配唤醒词!" if matched else "❌ 未匹配")
                    if matched:
                        self.mark_recent_wakeup(heard)
                        self.wake_phrase_detected.emit(heard)
        finally:
            logger.info("[VoiceWakeup] 监听线程退出 (共处理 %d 个音频片段)", listen_count)
            self._running = False
            self.listener_state_changed.emit(False)

    def _recognize(self, *, audio, recognizer) -> str:
        if self._recognition_provider == "openai_whisper":
            return self._recognize_openai_whisper(audio=audio)
        if self._recognition_provider == "xai_realtime":
            return self._recognize_xai_realtime(audio=audio)
        if self._recognition_provider == "zhipu_asr":
            return self._recognize_zhipu_asr(audio=audio)
        return recognizer.recognize_google(audio, language=self._language)

    def _recognize_xai_realtime(self, *, audio) -> str:
        try:
            import websocket  # type: ignore
        except Exception as exc:
            raise WakeupRecognitionError(
                "缺少 websocket-client 依赖，无法使用 xAI Realtime。请安装 websocket-client。"
            ) from exc

        pcm16_data = audio.get_raw_data(
            convert_rate=self._XAI_REALTIME_INPUT_SAMPLE_RATE,
            convert_width=2,
        )
        if not pcm16_data:
            return ""

        endpoint = self._build_xai_realtime_endpoint(base_url=self._openai_base_url, model=self._openai_model)
        audio_payload = base64.b64encode(pcm16_data).decode("ascii")

        modern_session_event: dict[str, Any] = {
            "type": "session.update",
            "session": {
                "turn_detection": {"type": None},
                "audio": {
                    "input": {
                        "format": {
                            "type": "audio/pcm",
                            "rate": self._XAI_REALTIME_INPUT_SAMPLE_RATE,
                        }
                    },
                    "output": {
                        "format": {
                            "type": "audio/pcm",
                            "rate": self._XAI_REALTIME_OUTPUT_SAMPLE_RATE,
                        }
                    },
                },
            },
        }
        legacy_session_event: dict[str, Any] = {
            "type": "session.update",
            "session": {
                "input_audio_format": "pcm16",
            },
        }
        prefer_legacy_first = self._should_prefer_xai_legacy_first(self._openai_model)
        if prefer_legacy_first:
            try:
                return self._run_xai_realtime_session(
                    websocket_module=websocket,
                    endpoint=endpoint,
                    audio_payload=audio_payload,
                    session_event=legacy_session_event,
                    commit_event_type="input_audio_buffer.commit",
                    send_response_create=True,
                )
            except WakeupRecognitionError as exc:
                logger.warning("[VoiceWakeup] xAI Realtime legacy 兼容流失败，回退现代事件流: %s", exc)
        try:
            return self._run_xai_realtime_session(
                websocket_module=websocket,
                endpoint=endpoint,
                audio_payload=audio_payload,
                session_event=modern_session_event,
                commit_event_type="conversation.item.commit",
                send_response_create=True,
            )
        except WakeupRecognitionError as exc:
            if prefer_legacy_first or not self._should_retry_xai_with_legacy(str(exc)):
                raise
            logger.warning("[VoiceWakeup] xAI Realtime 现代事件流失败，回退 legacy 兼容流: %s", exc)
            return self._run_xai_realtime_session(
                websocket_module=websocket,
                endpoint=endpoint,
                audio_payload=audio_payload,
                session_event=legacy_session_event,
                commit_event_type="input_audio_buffer.commit",
                send_response_create=True,
            )

    def _run_xai_realtime_session(
        self,
        *,
        websocket_module,
        endpoint: str,
        audio_payload: str,
        session_event: dict[str, Any],
        commit_event_type: str,
        send_response_create: bool,
    ) -> str:
        ws = None
        try:
            ws = websocket_module.create_connection(
                endpoint,
                header=[
                    f"Authorization: Bearer {self._openai_api_key}",
                ],
                timeout=self._XAI_REALTIME_TIMEOUT_SECONDS,
            )
            ws.send(json.dumps(session_event, ensure_ascii=False))
            ws.send(json.dumps({"type": "input_audio_buffer.append", "audio": audio_payload}))
            ws.send(json.dumps({"type": commit_event_type}))
            if send_response_create:
                ws.send(json.dumps({"type": "response.create"}))
            return self._receive_xai_transcription(ws=ws, websocket_module=websocket_module)
        except websocket_module.WebSocketBadStatusException as exc:  # type: ignore[attr-defined]
            status_code = getattr(exc, "status_code", "unknown")
            hint = self._build_http_status_hint(
                status_code=status_code,
                base_url=self._openai_base_url,
                response_body=str(exc),
                provider=self._recognition_provider,
            )
            raise WakeupRecognitionError(
                f"xAI Realtime 握手失败 (HTTP {status_code})。{hint} "
                f"(endpoint={endpoint}, model={self._openai_model})"
            ) from exc
        except websocket_module.WebSocketException as exc:  # type: ignore[attr-defined]
            raise WakeupRecognitionError(f"xAI Realtime 连接异常: {exc} (endpoint={endpoint})") from exc
        except OSError as exc:
            raise WakeupRecognitionError(f"xAI Realtime 网络异常: {exc} (endpoint={endpoint})") from exc
        finally:
            if ws is not None:
                try:
                    ws.close()
                except Exception:
                    pass

    def _receive_xai_transcription(self, *, ws, websocket_module) -> str:
        deadline = time.monotonic() + self._XAI_REALTIME_TIMEOUT_SECONDS
        streamed_text_parts: list[str] = []
        while time.monotonic() < deadline:
            remaining = max(0.1, deadline - time.monotonic())
            try:
                ws.settimeout(remaining)
                raw_event = ws.recv()
            except websocket_module.WebSocketTimeoutException:
                continue

            event = self._parse_realtime_event(raw_event)
            if not event:
                continue

            event_type = str(event.get("type", "")).strip()
            if not event_type:
                continue
            if event_type == "conversation.item.input_audio_transcription.completed":
                return self._extract_xai_transcription_text(event)
            if event_type == "conversation.item.added":
                transcript = self._extract_xai_transcription_text(event)
                if transcript:
                    return transcript
            if event_type in {"response.output_text.delta", "response.text.delta"}:
                delta = self._extract_realtime_response_delta(event)
                if delta:
                    streamed_text_parts.append(delta)
                continue
            if event_type in {"response.output_text.done", "response.text.done"}:
                done = self._extract_realtime_response_delta(event) or "".join(streamed_text_parts).strip()
                if done:
                    return done
            if event_type == "response.done":
                done = self._extract_realtime_response_text(event) or "".join(streamed_text_parts).strip()
                if done:
                    return done
            if self._is_realtime_error_event(event=event):
                detail = self._extract_realtime_error_message(event)
                raise WakeupRecognitionError(
                    f"xAI Realtime 返回错误事件: {detail} (event_type={event_type})"
                )

        raise WakeupRecognitionError("xAI Realtime 超时：未收到有效转写事件。")

    def _recognize_openai_whisper(self, *, audio) -> str:
        import httpx

        wav_data = audio.get_wav_data(convert_rate=16000, convert_width=2)
        endpoint = f"{self._openai_base_url}/audio/transcriptions"
        form_data: dict[str, str] = {
            "model": self._openai_model,
            "response_format": "json",
            "temperature": f"{self._openai_temperature:.2f}",
        }
        normalized_language = self._normalize_whisper_language(self._language)
        if normalized_language:
            form_data["language"] = normalized_language
        if self._openai_prompt:
            form_data["prompt"] = self._openai_prompt

        try:
            with httpx.Client(timeout=20.0) as client:
                headers = {
                    "Accept": "application/json",
                    "Authorization": f"Bearer {self._openai_api_key}",
                }
                response = client.post(
                    endpoint,
                    data=form_data,
                    files={"file": ("wakeup.wav", wav_data, "audio/wav")},
                    headers=headers,
                )
                if response.status_code in (401, 403):
                    fallback_headers = {
                        "Accept": "application/json",
                        "Authorization": self._openai_api_key,
                    }
                    fallback = client.post(
                        endpoint,
                        data=form_data,
                        files={"file": ("wakeup.wav", wav_data, "audio/wav")},
                        headers=fallback_headers,
                    )
                    if fallback.status_code < 400:
                        response = fallback
                response.raise_for_status()
        except httpx.HTTPStatusError as exc:
            status_code = exc.response.status_code if exc.response is not None else "unknown"
            body_text = ""
            if exc.response is not None:
                try:
                    body_text = str(exc.response.text or "").strip()
                except Exception:
                    body_text = ""
            hint = self._build_http_status_hint(
                status_code=status_code,
                base_url=self._openai_base_url,
                response_body=body_text,
            )
            raise WakeupRecognitionError(
                f"ASR 请求失败 (HTTP {status_code})。{hint} "
                f"(base_url={self._openai_base_url}, endpoint={endpoint}, model={self._openai_model})"
            ) from exc
        except httpx.HTTPError as exc:
            raise WakeupRecognitionError(f"Whisper 网络异常: {exc}") from exc

        return self._parse_whisper_text(response=response)

    def _recognize_zhipu_asr(self, *, audio) -> str:
        import httpx

        wav_data = audio.get_wav_data(convert_rate=16000, convert_width=2)
        if not wav_data:
            return ""

        approximate_duration = len(wav_data) / 32000.0
        if len(wav_data) > self._ZHIPU_MAX_AUDIO_BYTES:
            raise WakeupRecognitionError(
                f"Zhipu ASR 音频超过 25MB 限制（当前约 {len(wav_data) / (1024 * 1024):.1f}MB）。"
            )
        if approximate_duration > self._ZHIPU_MAX_AUDIO_SECONDS:
            raise WakeupRecognitionError(
                f"Zhipu ASR 音频超过 30 秒限制（当前约 {approximate_duration:.1f} 秒）。"
            )

        endpoint = self._normalize_zhipu_base_url(self._openai_base_url)
        model_name = (self._openai_model or "").strip() or "glm-asr-2512"
        form_data: dict[str, str] = {
            "model": model_name,
            "stream": "false",
        }

        try:
            with httpx.Client(timeout=20.0) as client:
                headers = {
                    "Accept": "application/json",
                    "Authorization": f"Bearer {self._openai_api_key}",
                }
                response = client.post(
                    endpoint,
                    data=form_data,
                    files={"file": ("wakeup.wav", wav_data, "audio/wav")},
                    headers=headers,
                )
                response.raise_for_status()
        except httpx.HTTPStatusError as exc:
            status_code = exc.response.status_code if exc.response is not None else "unknown"
            body_text = ""
            if exc.response is not None:
                try:
                    body_text = str(exc.response.text or "").strip()
                except Exception:
                    body_text = ""
            hint = self._build_http_status_hint(
                status_code=status_code,
                base_url=endpoint,
                response_body=body_text,
                provider=self._recognition_provider,
            )
            raise WakeupRecognitionError(
                f"Zhipu ASR 请求失败 (HTTP {status_code})。{hint} "
                f"(endpoint={endpoint}, model={model_name})"
            ) from exc
        except httpx.HTTPError as exc:
            raise WakeupRecognitionError(f"Zhipu ASR 网络异常: {exc}") from exc

        return self._parse_whisper_text(response=response)

    @staticmethod
    def _parse_whisper_text(*, response) -> str:
        content_type = (response.headers.get("content-type", "") or "").lower()
        if "application/json" in content_type:
            payload = response.json()
            return VoiceWakeupListener._extract_transcription_text(payload)
        return str(response.text or "").strip()

    @staticmethod
    def _extract_transcription_text(payload: object) -> str:
        if isinstance(payload, str):
            return payload.strip()
        if not isinstance(payload, dict):
            return ""
        data = payload.get("data")
        if isinstance(data, dict):
            nested_text = data.get("text")
            if isinstance(nested_text, str):
                return nested_text.strip()
        text = payload.get("text", "")
        if isinstance(text, str):
            return text.strip()
        return ""

    @staticmethod
    def _parse_realtime_event(payload: object) -> dict[str, Any] | None:
        if isinstance(payload, bytes):
            try:
                payload = payload.decode("utf-8")
            except UnicodeDecodeError:
                return None
        if not isinstance(payload, str):
            return None
        normalized = payload.strip()
        if not normalized:
            return None
        try:
            decoded = json.loads(normalized)
        except json.JSONDecodeError:
            return None
        if isinstance(decoded, dict):
            return decoded
        return None

    @staticmethod
    def _extract_xai_transcription_text(event: dict[str, Any]) -> str:
        for key in ("transcript", "text"):
            value = event.get(key)
            if isinstance(value, str) and value.strip():
                return value.strip()

        item = event.get("item")
        if isinstance(item, dict):
            for key in ("transcript", "text"):
                value = item.get(key)
                if isinstance(value, str) and value.strip():
                    return value.strip()
            content = item.get("content")
            if isinstance(content, list):
                for entry in content:
                    if not isinstance(entry, dict):
                        continue
                    for key in ("transcript", "text"):
                        value = entry.get(key)
                        if isinstance(value, str) and value.strip():
                            return value.strip()
        return ""

    @staticmethod
    def _is_realtime_error_event(event: dict[str, Any]) -> bool:
        event_type = str(event.get("type", "")).strip().lower()
        if event_type == "error":
            return True
        if event_type.endswith(".failed") or event_type.endswith(".error"):
            return True
        return "error" in event

    @staticmethod
    def _extract_realtime_error_message(event: dict[str, Any]) -> str:
        error = event.get("error")
        if isinstance(error, str) and error.strip():
            return error.strip()
        if isinstance(error, dict):
            code = str(error.get("code", "")).strip()
            message = str(error.get("message", "")).strip()
            event_name = str(error.get("event_type", "")).strip()
            param = str(error.get("param", "")).strip()
            fields = []
            if code:
                fields.append(code)
            if event_name:
                fields.append(f"event={event_name}")
            if param:
                fields.append(f"param={param}")
            if message and fields:
                return f"{message} ({', '.join(fields)})"
            if message:
                return message
            if fields:
                return ", ".join(fields)
        message = str(event.get("message", "")).strip()
        if message:
            return message
        return str(event)[:200]

    @staticmethod
    def _should_retry_xai_with_legacy(error_text: str) -> bool:
        normalized = (error_text or "").lower()
        return "invalid event" in normalized or "invalid event received" in normalized

    @staticmethod
    def _should_prefer_xai_legacy_first(model_name: str) -> bool:
        normalized = (model_name or "").strip().lower()
        return "transcribe" in normalized

    @classmethod
    def _is_transient_xai_error(cls, error_text: str) -> bool:
        normalized = (error_text or "").strip().lower()
        return any(keyword in normalized for keyword in cls._XAI_TRANSIENT_ERROR_KEYWORDS)

    @staticmethod
    def _extract_realtime_response_delta(event: dict[str, Any]) -> str:
        for key in ("delta", "text"):
            value = event.get(key)
            if isinstance(value, str) and value.strip():
                return value.strip()
        return ""

    @staticmethod
    def _extract_realtime_response_text(event: dict[str, Any]) -> str:
        response = event.get("response")
        if not isinstance(response, dict):
            return ""
        output = response.get("output")
        if not isinstance(output, list):
            return ""

        chunks: list[str] = []
        for item in output:
            if not isinstance(item, dict):
                continue
            content = item.get("content")
            if not isinstance(content, list):
                continue
            for entry in content:
                if not isinstance(entry, dict):
                    continue
                text_value = entry.get("text")
                if isinstance(text_value, str) and text_value.strip():
                    chunks.append(text_value.strip())
                    continue
                transcript = entry.get("transcript")
                if isinstance(transcript, str) and transcript.strip():
                    chunks.append(transcript.strip())
        return "".join(chunks).strip()

    @staticmethod
    def _normalize_recognition_provider(provider: str) -> str:
        normalized = (provider or "").strip().lower()
        if normalized in {"openai_whisper", "openai", "whisper"}:
            return "openai_whisper"
        if normalized in {"google", "google_webspeech"}:
            return "google"
        if normalized in {"zhipu_asr", "zhipu", "bigmodel"}:
            return "zhipu_asr"
        if normalized in {"xai_realtime", "xai", "xai-realtime", "xai realtime"}:
            return "xai_realtime"
        return "xai_realtime"

    @staticmethod
    def _build_http_status_hint(
        *,
        status_code: int | str,
        base_url: str,
        response_body: str,
        provider: str = "openai_whisper",
    ) -> str:
        status = str(status_code)
        body = (response_body or "").strip().replace("\r", " ").replace("\n", " ")
        body_preview = body[:180]
        if status in {"401", "403"}:
            hint = "API Key 无效、过期或无此模型权限。"
        elif status == "404":
            if provider == "zhipu_asr" or VoiceWakeupListener._is_zhipu_base_url(base_url):
                hint = "服务端不存在该接口或模型。请确认路径为 /api/paas/v4/audio/transcriptions。"
            else:
                hint = "服务端不存在该接口或模型。请确认服务支持 /v1/audio/transcriptions。"
        elif status == "429":
            hint = "请求过快或额度不足。请稍后重试。"
        else:
            hint = "请检查 API Key、模型名与网络连通性。"

        if VoiceWakeupListener._is_xai_base_url(base_url):
            if provider == "xai_realtime":
                hint += " 检测到 xAI 域名；请确认使用支持 Realtime 的模型，并检查 WebSocket 鉴权格式。"
            else:
                hint += " 检测到 xAI 域名；若使用 xAI，请将 ASR 提供商改为 xai_realtime。"
        if VoiceWakeupListener._is_zhipu_base_url(base_url):
            hint += " 检测到智谱域名；建议使用 glm-asr-2512，且路径应为 /api/paas/v4/audio/transcriptions。"
        if body_preview:
            hint += f" 服务端返回: {body_preview}"
        return hint

    @staticmethod
    def _is_xai_base_url(base_url: str) -> bool:
        parsed = urlsplit((base_url or "").strip())
        host = (parsed.netloc or "").lower()
        return host.endswith("x.ai")

    @staticmethod
    def _is_zhipu_base_url(base_url: str) -> bool:
        parsed = urlsplit((base_url or "").strip())
        host = (parsed.netloc or "").lower()
        return host.endswith("bigmodel.cn")

    @staticmethod
    def _normalize_base_url(base_url: str) -> str:
        normalized = (base_url or "").strip().rstrip("/")
        if not normalized:
            return "https://api.openai.com/v1"
        parsed = urlsplit(normalized)
        if not parsed.scheme or not parsed.netloc:
            return "https://api.openai.com/v1"

        path = (parsed.path or "").rstrip("/")
        for suffix in ("/chat/completions", "/audio/transcriptions", "/audio/translations"):
            if path.endswith(suffix):
                path = path[: -len(suffix)]
                break
        if "/v1/" in path:
            path = path.split("/v1/", maxsplit=1)[0] + "/v1"
        elif path.endswith("/v1"):
            pass
        elif not path:
            path = "/v1"
        else:
            path = f"{path}/v1"

        return urlunsplit((parsed.scheme, parsed.netloc, path, "", ""))

    @classmethod
    def _normalize_zhipu_base_url(cls, base_url: str) -> str:
        normalized = (base_url or "").strip().rstrip("/")
        if not normalized:
            return cls._ZHIPU_ASR_ENDPOINT
        parsed = urlsplit(normalized)
        if not parsed.scheme or not parsed.netloc:
            return cls._ZHIPU_ASR_ENDPOINT
        path = (parsed.path or "").rstrip("/")
        if not path:
            path = "/api/paas/v4/audio/transcriptions"
        elif not path.endswith("/audio/transcriptions"):
            path = f"{path}/audio/transcriptions"
        return urlunsplit((parsed.scheme, parsed.netloc, path, "", ""))

    @staticmethod
    def _build_xai_realtime_endpoint(*, base_url: str, model: str) -> str:
        normalized_base = VoiceWakeupListener._normalize_base_url(base_url)
        parsed = urlsplit(normalized_base)
        scheme = "wss" if parsed.scheme in {"https", "wss"} else "ws"
        path = (parsed.path or "/v1").rstrip("/")
        if not path:
            path = "/v1"
        if not path.endswith("/realtime"):
            path = f"{path}/realtime"
        model_name = (model or "").strip()
        if not model_name or model_name == "whisper-1":
            model_name = "grok-2-mini-transcribe"
        query = urlencode({"model": model_name})
        return urlunsplit((scheme, parsed.netloc, path, query, ""))

    @staticmethod
    def _normalize_whisper_language(language: str) -> str:
        normalized = (language or "").strip().lower().replace("_", "-")
        if len(normalized) == 2 and normalized.isalpha():
            return normalized
        if "-" in normalized:
            head = normalized.split("-", maxsplit=1)[0]
            if len(head) == 2 and head.isalpha():
                return head
        return ""

    @classmethod
    def transcribe_once(
        cls,
        *,
        language: str = "zh-CN",
        recognition_provider: str = "openai_whisper",
        openai_api_key: str = "",
        openai_base_url: str = "https://api.openai.com/v1",
        openai_model: str = "whisper-1",
        openai_prompt: str = "",
        openai_temperature: float = 0.0,
        listen_timeout_seconds: float = 6.0,
        phrase_time_limit_seconds: float = 12.0,
    ) -> str:
        try:
            import speech_recognition as sr  # type: ignore
        except Exception as exc:
            raise WakeupRecognitionError("缺少 SpeechRecognition/PyAudio，无法进行按键语音转写。") from exc

        recognizer = sr.Recognizer()
        try:
            microphone = sr.Microphone()
        except Exception as exc:
            raise WakeupRecognitionError("麦克风不可用或权限不足，无法进行按键语音转写。") from exc

        helper = cls(
            phrases=("__ptt__",),
            language=language,
            recognition_provider=recognition_provider,
            openai_api_key=openai_api_key,
            openai_base_url=openai_base_url,
            openai_model=openai_model,
            openai_prompt=openai_prompt,
            openai_temperature=openai_temperature,
        )
        if helper._recognition_provider in {"openai_whisper", "xai_realtime", "zhipu_asr"} and not helper._openai_api_key.strip():
            raise WakeupRecognitionError("ASR API Key 未填写，无法进行按键语音转写。")

        timeout_s = max(1.0, float(listen_timeout_seconds))
        phrase_limit_s = max(1.0, float(phrase_time_limit_seconds))
        with microphone as source:
            recognizer.adjust_for_ambient_noise(source, duration=0.5)
            try:
                audio = recognizer.listen(source, timeout=timeout_s, phrase_time_limit=phrase_limit_s)
            except sr.WaitTimeoutError as exc:
                raise WakeupRecognitionError("等待语音输入超时，请按 B 后尽快说话。") from exc

        try:
            return helper._recognize(audio=audio, recognizer=recognizer).strip()
        except sr.UnknownValueError as exc:
            raise WakeupRecognitionError("没有识别出清晰语音，请再试一次。") from exc
        except sr.RequestError as exc:
            raise WakeupRecognitionError(f"语音识别网络不可用: {exc}") from exc

    @classmethod
    def mark_recent_wakeup(cls, phrase: str) -> None:
        with cls._LAST_WAKEUP_LOCK:
            cls._LAST_WAKEUP_MONOTONIC = time.monotonic()
            cls._LAST_WAKEUP_PHRASE = (phrase or "").strip()

    @classmethod
    def consume_recent_wakeup(cls, *, window_seconds: float = 3.0) -> bool:
        now = time.monotonic()
        with cls._LAST_WAKEUP_LOCK:
            ts = float(cls._LAST_WAKEUP_MONOTONIC)
            if ts <= 0.0:
                return False
            if now - ts > max(0.1, float(window_seconds)):
                return False
            cls._LAST_WAKEUP_MONOTONIC = 0.0
            cls._LAST_WAKEUP_PHRASE = ""
            return True
