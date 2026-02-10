from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any


@dataclass(slots=True)
class CharacterPackage:
    character_id: str
    root_dir: Path
    manifest: dict[str, Any]
    config: dict[str, Any]
    dialogue_path: Path
    sprites_dir: Path
    sounds_dir: Path
    voice_cache_dir: Path


class CharacterLoader:
    """Scan, validate, and load character packs."""

    REQUIRED_FILES = [
        "manifest.json",
        "config.json",
        "scripts/dialogue.yaml",
        "assets/sprites/peek.png",
    ]
    IDLE_CANDIDATES = [
        "assets/sprites/idle.gif",
        "assets/sprites/idle.png",
    ]

    def __init__(self, characters_dir: Path):
        self._characters_dir = characters_dir
        self._loaded_characters: dict[str, CharacterPackage] = {}

    def scan_characters(self) -> list[dict[str, Any]]:
        result: list[dict[str, Any]] = []
        if not self._characters_dir.exists():
            return result

        for char_dir in self._characters_dir.iterdir():
            if not char_dir.is_dir():
                continue
            if not self._validate_character(char_dir):
                continue
            manifest_path = char_dir / "manifest.json"
            try:
                manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            except (OSError, json.JSONDecodeError):
                continue
            if isinstance(manifest, dict):
                result.append(manifest)
        return result

    def list_characters(self) -> list[dict[str, Any]]:
        return self.scan_characters()

    def load_character(self, character_id: str) -> CharacterPackage | None:
        if character_id in self._loaded_characters:
            return self._loaded_characters[character_id]

        char_dir = self._characters_dir / character_id
        if not self._validate_character(char_dir):
            return None

        try:
            manifest = json.loads((char_dir / "manifest.json").read_text(encoding="utf-8"))
            config = json.loads((char_dir / "config.json").read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            return None

        package = CharacterPackage(
            character_id=character_id,
            root_dir=char_dir,
            manifest=manifest if isinstance(manifest, dict) else {},
            config=config if isinstance(config, dict) else {},
            dialogue_path=char_dir / "scripts" / "dialogue.yaml",
            sprites_dir=char_dir / "assets" / "sprites",
            sounds_dir=char_dir / "assets" / "sounds",
            voice_cache_dir=char_dir / "voice_cache",
        )
        self._loaded_characters[character_id] = package
        return package

    def _validate_character(self, char_dir: Path) -> bool:
        if not char_dir.exists() or not char_dir.is_dir():
            return False
        for required in self.REQUIRED_FILES:
            if not (char_dir / required).exists():
                return False
        if not any((char_dir / candidate).exists() for candidate in self.IDLE_CANDIDATES):
            return False
        return True

