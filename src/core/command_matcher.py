from __future__ import annotations

import re
from dataclasses import dataclass
from difflib import SequenceMatcher

try:
    from rapidfuzz import fuzz, process  # type: ignore
except Exception:  # pragma: no cover - fallback path covered by unit tests
    fuzz = None
    process = None


@dataclass(slots=True)
class CommandMatch:
    action: str
    score: int
    phrase: str
    transcript: str


class VoiceCommandMatcher:
    """Fuzzy command matcher for short spoken commands."""

    _COMMAND_PHRASES: dict[str, tuple[str, ...]] = {
        "summon": (
            "出来",
            "出来吧",
            "召唤",
            "现身",
            "过来",
            "出来一下",
            "出现",
        ),
        "screen_commentary": (
            "看屏幕",
            "看看屏幕",
            "你在看什么",
            "屏幕上是什么",
            "解读屏幕",
            "看一下屏幕",
        ),
        "hide": (
            "隐藏",
            "躲起来",
            "退下",
            "回去",
            "消失",
        ),
        "toggle_visibility": (
            "显示隐藏",
            "切换显示",
            "切换可见",
            "切换状态",
        ),
        "status": (
            "状态",
            "报告状态",
            "查看状态",
        ),
    }

    _NORMALIZE_RE = re.compile(r"[\s,.!?;:'\"`~@#$%^&*()_+\-=\[\]{}|\\<>/，。！？；：、（）【】《》“”‘’]+")

    @classmethod
    def normalize_text(cls, text: str) -> str:
        lowered = (text or "").strip().lower()
        if not lowered:
            return ""
        return cls._NORMALIZE_RE.sub("", lowered)

    @classmethod
    def match(cls, transcript: str, *, min_score: int = 68) -> CommandMatch | None:
        normalized = cls.normalize_text(transcript)
        if not normalized:
            return None

        for action, phrases in cls._COMMAND_PHRASES.items():
            for phrase in phrases:
                normalized_phrase = cls.normalize_text(phrase)
                if normalized_phrase and normalized_phrase in normalized:
                    return CommandMatch(action=action, score=100, phrase=phrase, transcript=transcript)

        candidates: list[tuple[str, str]] = []
        for action, phrases in cls._COMMAND_PHRASES.items():
            for phrase in phrases:
                candidates.append((action, phrase))
        if not candidates:
            return None

        if process is not None and fuzz is not None:
            phrases_only = [phrase for _, phrase in candidates]
            best = process.extractOne(normalized, phrases_only, scorer=fuzz.WRatio)
            if best is None:
                return None
            phrase, score, _ = best
            matched_action = next((action for action, item in candidates if item == phrase), "")
            final_score = int(score)
        else:
            final_score = -1
            matched_action = ""
            phrase = ""
            for action, candidate in candidates:
                ratio = int(SequenceMatcher(None, normalized, cls.normalize_text(candidate)).ratio() * 100)
                if ratio > final_score:
                    final_score = ratio
                    matched_action = action
                    phrase = candidate

        if final_score < int(min_score) or not matched_action:
            return None
        return CommandMatch(
            action=matched_action,
            score=final_score,
            phrase=phrase,
            transcript=transcript,
        )
