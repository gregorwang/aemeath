from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from PySide6.QtCore import QIODevice, QSaveFile


@dataclass(slots=True)
class TriggerConfig:
    idle_threshold_seconds: int = 180
    jitter_range_seconds: tuple[int, int] = (-30, 60)
    auto_dismiss_seconds: int = 30


@dataclass(slots=True)
class AppearanceConfig:
    theme: str = "default"
    position: str = "auto"
    ascii_width: int = 60
    font_size_px: int = 8


@dataclass(slots=True)
class AudioConfig:
    tts_provider: str = "edge"
    tts_voice: str = "zh-CN-XiaoxiaoNeural"
    tts_rate: str = "+0%"
    volume: float = 0.8
    cache_enabled: bool = True
    microphone_enabled: bool = False
    voice_input_mode: str = "push_to_talk"  # continuous / push_to_talk
    asr_provider: str = "zhipu_asr"  # openai_whisper / google / xai_realtime / zhipu_asr
    asr_api_key: str = ""
    asr_model: str = "glm-asr-2512"
    asr_base_url: str = "https://open.bigmodel.cn/api/paas/v4/audio/transcriptions"
    asr_temperature: float = 0.0
    asr_prompt: str = ""


@dataclass(slots=True)
class BehaviorConfig:
    full_screen_pause: bool = True
    auto_start_on_login: bool = False
    debug_mode: bool = False
    offline_mode: bool = False
    audio_output_reactive: bool = True


@dataclass(slots=True)
class VisionConfig:
    camera_enabled: bool = False
    camera_consent_granted: bool = False
    camera_index: int = 0
    target_fps: int = 15
    eye_tracking_enabled: bool = True


@dataclass(slots=True)
class WakeupConfig:
    enabled: bool = False
    phrases: tuple[str, ...] = ("小爱同学请你出来", "小爱同学出来", "小爱同学")
    language: str = "zh-CN"


@dataclass(slots=True)
class LLMConfig:
    provider: str = "xai"  # none / openai / xai / deepseek
    model: str = "grok-4-fast-reasoning"
    api_key: str = ""
    base_url: str = "https://api.x.ai/v1"


@dataclass(slots=True)
class ScreenCommentaryConfig:
    streaming_enabled: bool = True
    ocr_fallback_enabled: bool = False
    stream_chunk_chars: int = 22
    max_response_chars: int = 90
    preamble_text: str = "正在看你的屏幕内容，让我看看你在做什么。"
    auto_enabled: bool = False
    auto_interval_minutes: int = 60


@dataclass(slots=True)
class AppConfig:
    version: str = "1.0.0"
    trigger: TriggerConfig = field(default_factory=TriggerConfig)
    appearance: AppearanceConfig = field(default_factory=AppearanceConfig)
    audio: AudioConfig = field(default_factory=AudioConfig)
    behavior: BehaviorConfig = field(default_factory=BehaviorConfig)
    vision: VisionConfig = field(default_factory=VisionConfig)
    wakeup: WakeupConfig = field(default_factory=WakeupConfig)
    llm: LLMConfig = field(default_factory=LLMConfig)
    screen_commentary: ScreenCommentaryConfig = field(default_factory=ScreenCommentaryConfig)


class ConfigManager:
    """Load app runtime configuration from JSON with safe defaults."""

    def __init__(self, config_path: Path):
        self._config_path = config_path

    def load(self) -> AppConfig:
        if not self._config_path.exists():
            return AppConfig()
        try:
            raw = json.loads(self._config_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            return AppConfig()
        if not isinstance(raw, dict):
            return AppConfig()

        trigger = self._build_trigger(raw.get("trigger"))
        appearance = self._build_appearance(raw.get("appearance"))
        audio = self._build_audio(raw.get("audio"))
        behavior = self._build_behavior(raw.get("behavior"))
        vision = self._build_vision(raw.get("vision"))
        wakeup = self._build_wakeup(raw.get("wakeup"))
        llm = self._build_llm(raw.get("llm"))
        screen_commentary = self._build_screen_commentary(raw.get("screen_commentary"))
        version = str(raw.get("version", "1.0.0"))
        return AppConfig(
            version=version,
            trigger=trigger,
            appearance=appearance,
            audio=audio,
            behavior=behavior,
            vision=vision,
            wakeup=wakeup,
            llm=llm,
            screen_commentary=screen_commentary,
        )

    def save(self, config: AppConfig) -> bool:
        payload = self.to_dict(config)
        self._config_path.parent.mkdir(parents=True, exist_ok=True)
        content = json.dumps(payload, ensure_ascii=False, indent=2) + "\n"
        try:
            saver = QSaveFile(str(self._config_path))
            if not saver.open(QIODevice.OpenModeFlag.WriteOnly | QIODevice.OpenModeFlag.Truncate):
                return False
            raw = content.encode("utf-8")
            written = saver.write(raw)
            if written != len(raw):
                saver.cancelWriting()
                return False
            if not saver.commit():
                return False
        except Exception:
            return False
        return True

    @staticmethod
    def to_dict(config: AppConfig) -> dict[str, Any]:
        return {
            "version": str(config.version),
            "trigger": {
                "idle_threshold_seconds": int(config.trigger.idle_threshold_seconds),
                "jitter_range_seconds": [
                    int(config.trigger.jitter_range_seconds[0]),
                    int(config.trigger.jitter_range_seconds[1]),
                ],
                "auto_dismiss_seconds": int(config.trigger.auto_dismiss_seconds),
            },
            "appearance": {
                "theme": str(config.appearance.theme),
                "position": str(config.appearance.position),
                "ascii_width": int(config.appearance.ascii_width),
                "font_size_px": int(config.appearance.font_size_px),
            },
            "audio": {
                "tts_provider": str(config.audio.tts_provider).lower(),
                "tts_voice": str(config.audio.tts_voice),
                "tts_rate": str(config.audio.tts_rate),
                "volume": float(config.audio.volume),
                "cache_enabled": bool(config.audio.cache_enabled),
                "microphone_enabled": bool(config.audio.microphone_enabled),
                "voice_input_mode": str(config.audio.voice_input_mode).lower(),
                "asr_provider": str(config.audio.asr_provider).lower(),
                "asr_api_key": str(config.audio.asr_api_key),
                "asr_model": str(config.audio.asr_model),
                "asr_base_url": str(config.audio.asr_base_url),
                "asr_temperature": float(config.audio.asr_temperature),
                "asr_prompt": str(config.audio.asr_prompt),
            },
            "behavior": {
                "full_screen_pause": bool(config.behavior.full_screen_pause),
                "auto_start_on_login": bool(config.behavior.auto_start_on_login),
                "debug_mode": bool(config.behavior.debug_mode),
                "offline_mode": bool(config.behavior.offline_mode),
                "audio_output_reactive": bool(config.behavior.audio_output_reactive),
            },
            "vision": {
                "camera_enabled": bool(config.vision.camera_enabled),
                "camera_consent_granted": bool(config.vision.camera_consent_granted),
                "camera_index": int(config.vision.camera_index),
                "target_fps": int(config.vision.target_fps),
                "eye_tracking_enabled": bool(config.vision.eye_tracking_enabled),
            },
            "wakeup": {
                "enabled": bool(config.wakeup.enabled),
                "phrases": [str(item) for item in config.wakeup.phrases if str(item).strip()],
                "language": str(config.wakeup.language),
            },
            "llm": {
                "provider": str(config.llm.provider).lower(),
                "model": str(config.llm.model),
                "api_key": str(config.llm.api_key),
                "base_url": str(config.llm.base_url),
            },
            "screen_commentary": {
                "streaming_enabled": bool(config.screen_commentary.streaming_enabled),
                "ocr_fallback_enabled": bool(config.screen_commentary.ocr_fallback_enabled),
                "stream_chunk_chars": int(config.screen_commentary.stream_chunk_chars),
                "max_response_chars": int(config.screen_commentary.max_response_chars),
                "preamble_text": str(config.screen_commentary.preamble_text),
                "auto_enabled": bool(config.screen_commentary.auto_enabled),
                "auto_interval_minutes": int(config.screen_commentary.auto_interval_minutes),
            },
        }

    @staticmethod
    def _build_trigger(payload: Any) -> TriggerConfig:
        if not isinstance(payload, dict):
            return TriggerConfig()
        jitter = payload.get("jitter_range_seconds", [-30, 60])
        if isinstance(jitter, list) and len(jitter) == 2:
            try:
                jitter_pair = (int(jitter[0]), int(jitter[1]))
            except (TypeError, ValueError):
                jitter_pair = (-30, 60)
        else:
            jitter_pair = (-30, 60)
        return TriggerConfig(
            idle_threshold_seconds=max(1, int(payload.get("idle_threshold_seconds", 180))),
            jitter_range_seconds=jitter_pair,
            auto_dismiss_seconds=max(1, int(payload.get("auto_dismiss_seconds", 30))),
        )

    @staticmethod
    def _build_appearance(payload: Any) -> AppearanceConfig:
        if not isinstance(payload, dict):
            return AppearanceConfig()
        return AppearanceConfig(
            theme=str(payload.get("theme", "default")),
            position=str(payload.get("position", "auto")),
            ascii_width=max(20, int(payload.get("ascii_width", 60))),
            font_size_px=max(6, int(payload.get("font_size_px", 8))),
        )

    @staticmethod
    def _build_audio(payload: Any) -> AudioConfig:
        if not isinstance(payload, dict):
            return AudioConfig()
        voice_input_mode = str(payload.get("voice_input_mode", "push_to_talk")).lower()
        if voice_input_mode not in {"continuous", "push_to_talk"}:
            voice_input_mode = "push_to_talk"
        asr_provider = str(payload.get("asr_provider", "zhipu_asr")).lower()
        if asr_provider not in {"openai_whisper", "google", "xai_realtime", "zhipu_asr"}:
            asr_provider = "zhipu_asr"
        default_base_by_provider = {
            "openai_whisper": "https://api.openai.com/v1",
            "xai_realtime": "https://api.x.ai/v1",
            "zhipu_asr": "https://open.bigmodel.cn/api/paas/v4/audio/transcriptions",
            "google": "",
        }
        asr_base_url = str(payload.get("asr_base_url", default_base_by_provider[asr_provider]))
        if asr_provider == "openai_whisper" and "x.ai" in asr_base_url.lower():
            asr_provider = "xai_realtime"
            if not asr_base_url.strip():
                asr_base_url = default_base_by_provider["xai_realtime"]
        if not asr_base_url.strip() and asr_provider in default_base_by_provider:
            asr_base_url = default_base_by_provider[asr_provider]
        default_model_by_provider = {
            "openai_whisper": "whisper-1",
            "xai_realtime": "grok-2-mini-transcribe",
            "zhipu_asr": "glm-asr-2512",
            "google": "",
        }
        asr_model = str(payload.get("asr_model", default_model_by_provider.get(asr_provider, "")))
        if asr_provider == "xai_realtime" and (not asr_model.strip() or asr_model.strip() == "whisper-1"):
            asr_model = "grok-2-mini-transcribe"
        if asr_provider == "zhipu_asr" and (
            not asr_model.strip() or asr_model.strip() in {"grok-2-mini-transcribe", "whisper-1"}
        ):
            asr_model = "glm-asr-2512"
            if not asr_base_url.strip() or "x.ai" in asr_base_url.lower():
                asr_base_url = default_base_by_provider["zhipu_asr"]
        try:
            asr_temperature = float(payload.get("asr_temperature", 0.0))
        except (TypeError, ValueError):
            asr_temperature = 0.0
        asr_temperature = min(max(asr_temperature, 0.0), 1.0)
        return AudioConfig(
            tts_provider="edge",
            tts_voice=str(payload.get("tts_voice", "zh-CN-XiaoxiaoNeural")),
            tts_rate=str(payload.get("tts_rate", "+0%")),
            volume=float(payload.get("volume", 0.8)),
            cache_enabled=bool(payload.get("cache_enabled", True)),
            microphone_enabled=bool(payload.get("microphone_enabled", False)),
            voice_input_mode=voice_input_mode,
            asr_provider=asr_provider,
            asr_api_key=str(payload.get("asr_api_key", "")),
            asr_model=asr_model,
            asr_base_url=asr_base_url,
            asr_temperature=asr_temperature,
            asr_prompt=str(payload.get("asr_prompt", "")),
        )

    @staticmethod
    def _build_behavior(payload: Any) -> BehaviorConfig:
        if not isinstance(payload, dict):
            return BehaviorConfig()
        return BehaviorConfig(
            full_screen_pause=bool(payload.get("full_screen_pause", True)),
            auto_start_on_login=bool(payload.get("auto_start_on_login", False)),
            debug_mode=bool(payload.get("debug_mode", False)),
            offline_mode=bool(payload.get("offline_mode", False)),
            audio_output_reactive=bool(payload.get("audio_output_reactive", True)),
        )

    @staticmethod
    def _build_vision(payload: Any) -> VisionConfig:
        if not isinstance(payload, dict):
            return VisionConfig()
        return VisionConfig(
            camera_enabled=bool(payload.get("camera_enabled", False)),
            camera_consent_granted=bool(payload.get("camera_consent_granted", False)),
            camera_index=max(0, int(payload.get("camera_index", 0))),
            target_fps=max(1, min(30, int(payload.get("target_fps", 15)))),
            eye_tracking_enabled=bool(payload.get("eye_tracking_enabled", True)),
        )

    @staticmethod
    def _build_wakeup(payload: Any) -> WakeupConfig:
        if not isinstance(payload, dict):
            return WakeupConfig()
        raw_phrases = payload.get("phrases", WakeupConfig().phrases)
        phrases: list[str] = []
        if isinstance(raw_phrases, list):
            phrases = [str(item).strip() for item in raw_phrases if str(item).strip()]
        elif isinstance(raw_phrases, str) and raw_phrases.strip():
            phrases = [raw_phrases.strip()]
        if not phrases:
            phrases = list(WakeupConfig().phrases)
        return WakeupConfig(
            enabled=bool(payload.get("enabled", False)),
            phrases=tuple(phrases),
            language=str(payload.get("language", "zh-CN")).strip() or "zh-CN",
        )

    @staticmethod
    def _build_llm(payload: Any) -> LLMConfig:
        if not isinstance(payload, dict):
            return LLMConfig()
        provider = str(payload.get("provider", "xai")).lower()
        if provider not in {"none", "openai", "xai", "deepseek"}:
            provider = "xai"
        return LLMConfig(
            provider=provider,
            model=str(payload.get("model", "grok-4-fast-reasoning")),
            api_key=str(payload.get("api_key", "")),
            base_url=str(payload.get("base_url", "https://api.x.ai/v1")),
        )

    @staticmethod
    def _build_screen_commentary(payload: Any) -> ScreenCommentaryConfig:
        if not isinstance(payload, dict):
            return ScreenCommentaryConfig()
        return ScreenCommentaryConfig(
            streaming_enabled=bool(payload.get("streaming_enabled", True)),
            ocr_fallback_enabled=bool(payload.get("ocr_fallback_enabled", False)),
            stream_chunk_chars=max(8, min(80, int(payload.get("stream_chunk_chars", 22)))),
            max_response_chars=max(20, min(300, int(payload.get("max_response_chars", 90)))),
            preamble_text=str(
                payload.get("preamble_text", "正在看你的屏幕内容，让我看看你在做什么。")
            ),
            auto_enabled=bool(payload.get("auto_enabled", False)),
            auto_interval_minutes=max(1, min(1440, int(payload.get("auto_interval_minutes", 60)))),
        )
