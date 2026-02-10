from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any


@dataclass(slots=True)
class TriggerConfig:
    idle_threshold_seconds: int = 180
    jitter_range_seconds: tuple[int, int] = (-30, 60)
    auto_dismiss_seconds: int = 30


@dataclass(slots=True)
class AppearanceConfig:
    theme: str = "default"
    position: str = "right"
    ascii_width: int = 60
    font_size_px: int = 8


@dataclass(slots=True)
class AudioConfig:
    tts_voice: str = "zh-CN-XiaoxiaoNeural"
    tts_rate: str = "+0%"
    volume: float = 0.8
    cache_enabled: bool = True


@dataclass(slots=True)
class BehaviorConfig:
    full_screen_pause: bool = True
    auto_start_on_login: bool = False
    debug_mode: bool = False


@dataclass(slots=True)
class VisionConfig:
    camera_enabled: bool = False
    camera_consent_granted: bool = False
    camera_index: int = 0
    target_fps: int = 15
    eye_tracking_enabled: bool = True


@dataclass(slots=True)
class LLMConfig:
    provider: str = "none"  # none / ollama / openai
    model: str = "gpt-4o-mini"
    base_url: str = "https://api.openai.com/v1"
    ollama_base_url: str = "http://localhost:11434"


@dataclass(slots=True)
class AppConfig:
    version: str = "1.0.0"
    trigger: TriggerConfig = field(default_factory=TriggerConfig)
    appearance: AppearanceConfig = field(default_factory=AppearanceConfig)
    audio: AudioConfig = field(default_factory=AudioConfig)
    behavior: BehaviorConfig = field(default_factory=BehaviorConfig)
    vision: VisionConfig = field(default_factory=VisionConfig)
    llm: LLMConfig = field(default_factory=LLMConfig)


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
        llm = self._build_llm(raw.get("llm"))
        version = str(raw.get("version", "1.0.0"))
        return AppConfig(
            version=version,
            trigger=trigger,
            appearance=appearance,
            audio=audio,
            behavior=behavior,
            vision=vision,
            llm=llm,
        )

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
            position=str(payload.get("position", "right")),
            ascii_width=max(20, int(payload.get("ascii_width", 60))),
            font_size_px=max(6, int(payload.get("font_size_px", 8))),
        )

    @staticmethod
    def _build_audio(payload: Any) -> AudioConfig:
        if not isinstance(payload, dict):
            return AudioConfig()
        return AudioConfig(
            tts_voice=str(payload.get("tts_voice", "zh-CN-XiaoxiaoNeural")),
            tts_rate=str(payload.get("tts_rate", "+0%")),
            volume=float(payload.get("volume", 0.8)),
            cache_enabled=bool(payload.get("cache_enabled", True)),
        )

    @staticmethod
    def _build_behavior(payload: Any) -> BehaviorConfig:
        if not isinstance(payload, dict):
            return BehaviorConfig()
        return BehaviorConfig(
            full_screen_pause=bool(payload.get("full_screen_pause", True)),
            auto_start_on_login=bool(payload.get("auto_start_on_login", False)),
            debug_mode=bool(payload.get("debug_mode", False)),
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
    def _build_llm(payload: Any) -> LLMConfig:
        if not isinstance(payload, dict):
            return LLMConfig()
        return LLMConfig(
            provider=str(payload.get("provider", "none")).lower(),
            model=str(payload.get("model", "gpt-4o-mini")),
            base_url=str(payload.get("base_url", "https://api.openai.com/v1")),
            ollama_base_url=str(payload.get("ollama_base_url", "http://localhost:11434")),
        )
