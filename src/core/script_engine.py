from __future__ import annotations

import random
from datetime import datetime, timedelta

from .asset_manager import Script
from .time_range import is_default_time_range, matches_time_range


class ScriptEngine:
    """
    Context-aware script selector.

    - Time-range filtering
    - Cooldown filtering
    - Avoid immediate repetition when possible
    - Weighted random by probability
    """

    def __init__(self, idle_scripts: list[Script], panic_scripts: list[Script] | None = None):
        self._idle_scripts = list(idle_scripts)
        self._panic_scripts = list(panic_scripts or [])
        self._last_played: dict[str, datetime] = {}
        self._last_script_id: str | None = None

    def refresh(self, idle_scripts: list[Script], panic_scripts: list[Script] | None = None) -> None:
        self._idle_scripts = list(idle_scripts)
        self._panic_scripts = list(panic_scripts or [])

    def select_idle_script(self, now: datetime | None = None) -> Script | None:
        timestamp = now or datetime.now()
        return self._select_script(
            source=self._idle_scripts,
            now=timestamp,
            avoid_repeat=True,
            honor_cooldown=True,
        )

    def select_panic_script(self, now: datetime | None = None) -> Script | None:
        timestamp = now or datetime.now()
        candidates = self._panic_scripts or self._idle_scripts
        return self._select_script(
            source=candidates,
            now=timestamp,
            avoid_repeat=False,
            honor_cooldown=False,
        )

    def _select_script(
        self,
        *,
        source: list[Script],
        now: datetime,
        avoid_repeat: bool,
        honor_cooldown: bool,
    ) -> Script | None:
        if not source:
            return None

        exact_matches = [s for s in source if self._is_time_match(s, now) and not self._is_default_range(s)]
        default_matches = [s for s in source if self._is_default_range(s)]
        primary_pool = exact_matches or default_matches or source

        candidates = self._filter_candidates(
            source=primary_pool,
            now=now,
            avoid_repeat=avoid_repeat,
            honor_cooldown=honor_cooldown,
            total_source_size=len(source),
        )

        if not candidates and exact_matches and default_matches:
            candidates = self._filter_candidates(
                source=default_matches,
                now=now,
                avoid_repeat=avoid_repeat,
                honor_cooldown=honor_cooldown,
                total_source_size=len(source),
            )

        if not candidates:
            candidates = self._filter_candidates(
                source=default_matches if (exact_matches and default_matches) else primary_pool,
                now=now,
                avoid_repeat=False,
                honor_cooldown=honor_cooldown,
                total_source_size=len(source),
            )

        if not candidates:
            return None

        selected = self._weighted_random(candidates)
        self._last_played[selected.id] = now
        self._last_script_id = selected.id
        return selected

    def _filter_candidates(
        self,
        *,
        source: list[Script],
        now: datetime,
        avoid_repeat: bool,
        honor_cooldown: bool,
        total_source_size: int,
    ) -> list[Script]:
        result: list[Script] = []
        for script in source:
            if not self._is_time_match(script, now):
                continue
            if honor_cooldown and self._is_cooling_down(script, now):
                continue
            if avoid_repeat and script.id == self._last_script_id and total_source_size > 1:
                continue
            result.append(script)
        return result

    def _is_time_match(self, script: Script, now: datetime) -> bool:
        return matches_time_range(script.time_range, now)

    def _is_cooling_down(self, script: Script, now: datetime) -> bool:
        if script.cooldown_minutes <= 0:
            return False
        last_time = self._last_played.get(script.id)
        if last_time is None:
            return False
        return now < last_time + timedelta(minutes=script.cooldown_minutes)

    @staticmethod
    def _weighted_random(candidates: list[Script]) -> Script:
        weights = [max(script.probability, 0.01) for script in candidates]
        return random.choices(candidates, weights=weights, k=1)[0]

    @staticmethod
    def _is_default_range(script: Script) -> bool:
        return is_default_time_range(script.time_range)
