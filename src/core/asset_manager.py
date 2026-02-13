from __future__ import annotations

import json
import random
from dataclasses import dataclass, field
from datetime import datetime, timedelta
from pathlib import Path
from typing import Any

from PySide6.QtCore import QFile, QResource

from .time_range import matches_time_range

try:
    import yaml
except ModuleNotFoundError:
    yaml = None


@dataclass(slots=True)
class Script:
    """Script line data model."""

    id: str
    text: str
    audio_path: str | None = None
    anim_speed: str = "normal"
    priority: int = 2
    time_range: str = "default"
    probability: float = 1.0
    sprite_path: str | None = None
    cooldown_minutes: int = 0
    tags: list[str] = field(default_factory=list)
    event_type: str = "idle"


class AssetManager:
    """Load and select scripts and static resources from a character pack."""

    SCRIPTS_FILENAME = "scripts.json"
    DIALOGUE_RELATIVE_PATH = Path("scripts") / "dialogue.yaml"

    def __init__(self, character_dir: Path):
        self._character_dir = character_dir
        self._idle_scripts: list[Script] = []
        self._panic_scripts: list[Script] = []
        self._last_triggered_at: dict[str, datetime] = {}
        self._load_scripts()

    @property
    def scripts(self) -> list[Script]:
        return self.idle_scripts

    @property
    def idle_scripts(self) -> list[Script]:
        return list(self._idle_scripts)

    @property
    def panic_scripts(self) -> list[Script]:
        return list(self._panic_scripts)

    def _load_scripts(self) -> None:
        scripts_path = self._character_dir / self.SCRIPTS_FILENAME
        dialogue_path = self._character_dir / self.DIALOGUE_RELATIVE_PATH

        idle_loaded: list[Script] = []
        panic_loaded: list[Script] = []

        if dialogue_path.exists():
            idle_loaded = self._load_idle_from_dialogue_yaml(dialogue_path)

        if scripts_path.exists():
            panic_loaded = self._load_panic_from_json(scripts_path)

        if idle_loaded or panic_loaded:
            builtin_idle, builtin_panic = self._build_builtin_scripts()
            self._idle_scripts = idle_loaded or self._load_idle_from_json(scripts_path) or builtin_idle
            self._panic_scripts = panic_loaded or builtin_panic
            return

        if not scripts_path.exists():
            self._idle_scripts, self._panic_scripts = self._build_builtin_scripts()
            return

        try:
            raw_data = json.loads(scripts_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            self._idle_scripts, self._panic_scripts = self._build_builtin_scripts()
            return

        idle_loaded: list[Script] = []
        panic_loaded: list[Script] = []

        if isinstance(raw_data, dict) and ("idle_events" in raw_data or "panic_events" in raw_data):
            for item in raw_data.get("idle_events", []):
                script = self._parse_script(item, event_type="idle")
                if script:
                    idle_loaded.append(script)
            for item in raw_data.get("panic_events", []):
                script = self._parse_script(item, event_type="panic")
                if script:
                    panic_loaded.append(script)
        else:
            source_items = raw_data.get("scripts", raw_data) if isinstance(raw_data, dict) else raw_data
            if isinstance(source_items, list):
                for item in source_items:
                    script = self._parse_script(item, event_type="idle")
                    if script:
                        idle_loaded.append(script)

        builtin_idle, builtin_panic = self._build_builtin_scripts()
        self._idle_scripts = idle_loaded or builtin_idle
        self._panic_scripts = panic_loaded or builtin_panic

    def _load_idle_from_dialogue_yaml(self, dialogue_path: Path) -> list[Script]:
        if yaml is None:
            return []
        try:
            raw = yaml.safe_load(dialogue_path.read_text(encoding="utf-8"))
        except Exception:
            return []
        if not isinstance(raw, dict):
            return []
        source = raw.get("scripts", [])
        if not isinstance(source, list):
            return []
        loaded: list[Script] = []
        for item in source:
            script = self._parse_dialogue_script(item)
            if script is not None:
                loaded.append(script)
        return loaded

    def _load_idle_from_json(self, scripts_path: Path) -> list[Script]:
        if not scripts_path.exists():
            return []
        try:
            raw_data = json.loads(scripts_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            return []
        source_items = raw_data.get("scripts", raw_data) if isinstance(raw_data, dict) else raw_data
        loaded: list[Script] = []
        if isinstance(source_items, list):
            for item in source_items:
                script = self._parse_script(item, event_type="idle")
                if script:
                    loaded.append(script)
        return loaded

    def _load_panic_from_json(self, scripts_path: Path) -> list[Script]:
        if not scripts_path.exists():
            return []
        try:
            raw_data = json.loads(scripts_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            return []
        if not isinstance(raw_data, dict):
            return []
        loaded: list[Script] = []
        for item in raw_data.get("panic_events", []):
            script = self._parse_script(item, event_type="panic")
            if script:
                loaded.append(script)
        return loaded

    def _parse_dialogue_script(self, item: Any) -> Script | None:
        if not isinstance(item, dict):
            return None
        script_id = str(item.get("id", "")).strip()
        text = str(item.get("text", "")).strip()
        if not script_id or not text:
            return None

        conditions = item.get("conditions", {})
        if not isinstance(conditions, dict):
            conditions = {}
        time_start = str(conditions.get("time_start", "default")).strip().lower()
        time_end = str(conditions.get("time_end", "default")).strip().lower()
        time_range = "default" if "default" in {time_start, time_end} else f"{time_start}-{time_end}"

        probability = float(conditions.get("probability", 1.0) or 1.0)
        cooldown_minutes = int(conditions.get("cooldown_minutes", 0) or 0)

        tts = item.get("tts", {})
        if not isinstance(tts, dict):
            tts = {}
        animation = item.get("animation", {})
        if not isinstance(animation, dict):
            animation = {}

        sprite_name = str(animation.get("sprite", "")).strip()
        sprite_path = self._resolve_optional_path(f"assets/sprites/{sprite_name}") if sprite_name else None
        audio_path = self._resolve_optional_path(tts.get("audio_cache"))

        return Script(
            id=script_id,
            text=text,
            audio_path=audio_path,
            anim_speed=str(animation.get("speed", "normal")).strip() or "normal",
            priority=2,
            time_range=time_range,
            probability=max(probability, 0.01),
            sprite_path=sprite_path,
            cooldown_minutes=max(cooldown_minutes, 0),
            tags=[],
            event_type="idle",
        )

    def _parse_script(self, item: Any, event_type: str) -> Script | None:
        if not isinstance(item, dict):
            return None
        script_id = str(item.get("id", "")).strip()
        text = str(item.get("text", "")).strip()
        if not script_id or not text:
            return None

        time_range = self._resolve_time_range(item)

        probability = float(item.get("probability", 1.0) or 1.0)
        priority_default = 1 if event_type == "panic" else 2
        priority = int(item.get("priority", priority_default) or priority_default)
        cooldown_minutes = int(item.get("cooldown_minutes", 0) or 0)
        tags = item.get("tags", [])
        if not isinstance(tags, list):
            tags = []
        tags = [str(tag) for tag in tags if str(tag).strip()]

        audio_path = self._resolve_optional_path(item.get("audio_cache") or item.get("audio_path"))
        sprite_path = self._resolve_optional_path(item.get("sprite"))
        anim_speed = str(item.get("anim_speed", "normal")).strip() or "normal"

        return Script(
            id=script_id,
            text=text,
            audio_path=audio_path,
            anim_speed=anim_speed,
            priority=priority,
            time_range=time_range,
            probability=max(probability, 0.01),
            sprite_path=sprite_path,
            cooldown_minutes=max(cooldown_minutes, 0),
            tags=tags,
            event_type=event_type,
        )

    @staticmethod
    def _resolve_time_range(item: dict[str, Any]) -> str:
        value = item.get("time_range")
        if isinstance(value, str) and value.strip():
            return value.strip().lower()

        # Backward compatibility for Phase 1 `time_ranges`.
        legacy = item.get("time_ranges")
        if isinstance(legacy, list):
            legacy_values = [str(tag).strip().lower() for tag in legacy if str(tag).strip()]
            if "default" in legacy_values:
                return "default"
            if "morning" in legacy_values:
                return "05:00-11:00"
            if "afternoon" in legacy_values:
                return "11:00-18:00"
            if "evening" in legacy_values:
                return "18:00-22:00"
            if "night" in legacy_values:
                return "22:00-06:00"
        return "default"

    def _resolve_optional_path(self, path_value: Any) -> str | None:
        if not path_value:
            return None
        text = str(path_value).strip()
        if not text:
            return None
        if text.startswith(":/"):
            # Qt resource path
            if QFile.exists(text) or QResource(text).isValid():
                return text
            return None
        path = Path(text)
        if not path.is_absolute():
            path = self._character_dir / path
        return str(path)

    def _build_builtin_scripts(self) -> tuple[list[Script], list[Script]]:
        idle = [
            Script(
                id="morning_default",
                text="早上好，要不要先喝口水？",
                time_range="05:00-11:00",
                priority=2,
                probability=1.0,
                cooldown_minutes=10,
                event_type="idle",
            ),
            Script(
                id="afternoon_default",
                text="午后效率时间到了，继续推进吧。",
                time_range="11:00-18:00",
                priority=2,
                probability=1.0,
                cooldown_minutes=10,
                event_type="idle",
            ),
            Script(
                id="night_default",
                text="已经很晚了，注意休息。",
                time_range="22:00-06:00",
                priority=1,
                probability=1.0,
                cooldown_minutes=20,
                event_type="idle",
            ),
            Script(
                id="fallback_default",
                text="我在屏幕边缘看着你。",
                time_range="default",
                priority=3,
                probability=1.0,
                cooldown_minutes=5,
                event_type="idle",
            ),
        ]
        panic = [
            Script(
                id="panic_default",
                text="哇！被发现了！",
                time_range="default",
                priority=1,
                probability=0.6,
                cooldown_minutes=0,
                event_type="panic",
            ),
            Script(
                id="panic_shy",
                text="才...才没有在偷看你...",
                time_range="default",
                priority=1,
                probability=0.4,
                cooldown_minutes=0,
                event_type="panic",
            ),
        ]
        return idle, panic

    def get_script_for_time(self, now: datetime) -> Script:
        return self.get_idle_script_for_time(now)

    def get_idle_script_for_time(self, now: datetime) -> Script:
        candidates = [s for s in self._idle_scripts if self._match_time_range(s.time_range, now)]
        if not candidates:
            candidates = [s for s in self._idle_scripts if s.time_range == "default"] or self._idle_scripts
        selected = self._pick_script(candidates, now=now)
        self._last_triggered_at[selected.id] = now
        return selected

    def get_panic_script(self, now: datetime | None = None) -> Script:
        timestamp = now or datetime.now()
        candidates = self._panic_scripts or self._idle_scripts
        selected = self._pick_script(candidates, now=timestamp, honor_cooldown=False)
        self._last_triggered_at[selected.id] = timestamp
        return selected

    def _pick_script(self, candidates: list[Script], now: datetime, honor_cooldown: bool = True) -> Script:
        if not candidates:
            fallback = self._build_builtin_scripts()[0][0]
            return fallback

        available = candidates
        if honor_cooldown:
            no_cooldown = [s for s in candidates if not self._in_cooldown(s, now)]
            if no_cooldown:
                available = no_cooldown

        top_priority = min(script.priority for script in available)
        top_candidates = [script for script in available if script.priority == top_priority]
        weights = [max(script.probability, 0.01) for script in top_candidates]
        return random.choices(top_candidates, weights=weights, k=1)[0]

    def _in_cooldown(self, script: Script, now: datetime) -> bool:
        if script.cooldown_minutes <= 0:
            return False
        last = self._last_triggered_at.get(script.id)
        if last is None:
            return False
        return now < (last + timedelta(minutes=script.cooldown_minutes))

    @staticmethod
    def _match_time_range(time_range: str, now: datetime) -> bool:
        return matches_time_range(time_range, now)
