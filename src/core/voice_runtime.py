from __future__ import annotations

import threading
from typing import Any, Callable

from PySide6.QtCore import QObject, QTimer, Signal

try:
    from .voice_wakeup import VoiceWakeupListener
except ModuleNotFoundError:
    from core.voice_wakeup import VoiceWakeupListener


class _PushToTalkBridge(QObject):
    result = Signal(str)
    error = Signal(str)


class VoiceRuntimeController(QObject):
    def __init__(
        self,
        *,
        logger,
        get_config: Callable[[], Any],
        resolve_asr_runtime: Callable[[Any], tuple[str, str]],
        notify: Callable[[str, str, int], None],
        execute_voice_command: Callable[[str, str], bool],
        summon_now_or_notify: Callable[[str], bool],
        request_screen_commentary: Callable[[str], None],
        parent: QObject | None = None,
    ) -> None:
        super().__init__(parent)
        self._logger = logger
        self._get_config = get_config
        self._resolve_asr_runtime = resolve_asr_runtime
        self._notify = notify
        self._execute_voice_command = execute_voice_command
        self._summon_now_or_notify = summon_now_or_notify
        self._request_screen_commentary = request_screen_commentary
        self._voice_listener: VoiceWakeupListener | None = None
        self._ptt_busy = False
        self._ptt_lock = threading.Lock()
        self._ptt_bridge = _PushToTalkBridge()
        self._ptt_bridge.result.connect(self._on_ptt_result)
        self._ptt_bridge.error.connect(self._on_ptt_error)

    def stop_voice_listener(self) -> None:
        listener = self._voice_listener
        if listener is None:
            return
        listener.stop_listening()
        listener.deleteLater()
        self._voice_listener = None

    def start_voice_listener(self) -> None:
        self.stop_voice_listener()
        config = self._get_config()
        if (
            not bool(config.audio.microphone_enabled)
            or not bool(config.wakeup.enabled)
            or str(config.audio.voice_input_mode or "").lower() != "continuous"
        ):
            return

        asr_key, asr_base_url = self._resolve_asr_runtime(config)
        listener = VoiceWakeupListener(
            phrases=config.wakeup.phrases,
            language=config.wakeup.language,
            recognition_provider=config.audio.asr_provider,
            openai_api_key=asr_key,
            openai_base_url=asr_base_url,
            openai_model=config.audio.asr_model,
            openai_prompt=config.audio.asr_prompt,
            openai_temperature=config.audio.asr_temperature,
        )
        listener.wake_phrase_detected.connect(self._on_wakeup_hit)
        listener.listener_error.connect(self._on_wakeup_error)
        listener.transcript_updated.connect(self._on_wakeup_transcript)
        listener.start_listening()
        self._voice_listener = listener
        self._logger.info("Voice wakeup listener started successfully.")

    def start_push_to_talk_once(self) -> None:
        config = self._get_config()
        if not bool(config.audio.microphone_enabled):
            self._notify("语音转写", "麦克风未启用，请在设置中打开。", 2400)
            return
        if str(config.audio.voice_input_mode or "").lower() != "push_to_talk":
            self._notify("语音转写", "当前是连续唤醒模式，Ctrl+B 单次转写未启用。", 2200)
            return

        with self._ptt_lock:
            if self._ptt_busy:
                self._logger.debug("[PushToTalk] 忽略重复触发：当前仍在转写中。")
                return
            self._ptt_busy = True

        self._notify("语音转写", "开始收音，请说话…", 1200)

        def _worker() -> None:
            try:
                current_config = self._get_config()
                asr_key, asr_base_url = self._resolve_asr_runtime(current_config)
                text = VoiceWakeupListener.transcribe_once(
                    language=current_config.wakeup.language,
                    recognition_provider=current_config.audio.asr_provider,
                    openai_api_key=asr_key,
                    openai_base_url=asr_base_url,
                    openai_model=current_config.audio.asr_model,
                    openai_prompt=current_config.audio.asr_prompt,
                    openai_temperature=current_config.audio.asr_temperature,
                    listen_timeout_seconds=6.0,
                    phrase_time_limit_seconds=12.0,
                )
                self._ptt_bridge.result.emit(text)
            except Exception as exc:
                self._ptt_bridge.error.emit(str(exc))
            finally:
                with self._ptt_lock:
                    self._ptt_busy = False

        threading.Thread(target=_worker, daemon=True, name="PushToTalk").start()

    def _on_ptt_result(self, payload: str) -> None:
        text = (payload or "").strip()
        if not text:
            self._notify("语音转写", "未识别到有效语音，请重试。", 2200)
            return
        self._logger.info("[PushToTalk] transcript=%s", text)
        self._notify("语音转写", text, 1800)
        if not self._execute_voice_command(text, "push_to_talk"):
            self._notify("语音命令", f"未匹配到动作：{text}", 2600)

    def _on_ptt_error(self, message: str) -> None:
        self._notify("语音转写失败", message, 3500)

    def _on_wakeup_hit(self, heard: str) -> None:
        self._logger.info("Wake phrase detected: %s", heard)
        matched = self._execute_voice_command(heard, "wakeup")
        if not matched:
            self._summon_now_or_notify("wakeup")
        lowered = (heard or "").strip().lower()
        if (not matched) and any(keyword in lowered for keyword in ("看屏幕", "看看屏幕", "你在看什么", "屏幕上", "screen")):
            self._logger.info("[VoiceWakeup] Wake phrase includes screen intent, auto request commentary")
            QTimer.singleShot(1200, lambda: self._request_screen_commentary("voice:wakeup_intent"))
        self._notify("语音唤醒", f"已识别: {heard}", 2000)

    def _on_wakeup_error(self, message: str) -> None:
        self._logger.warning("Voice wakeup degraded (session only): %s", message)
        self.stop_voice_listener()
        self._notify("语音降级", f"{message}\n重启应用可重新尝试。", 5000)

    def _on_wakeup_transcript(self, text: str) -> None:
        self._logger.info("[VoiceWakeup] 实时转写: \"%s\"", text)
        config = self._get_config()
        if bool(config.behavior.debug_mode):
            self._notify("语音转写", text, 1500)
