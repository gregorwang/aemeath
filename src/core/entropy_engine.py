from __future__ import annotations

import random


class EntropyEngine:
    """Randomness helpers to avoid deterministic behavior."""

    @staticmethod
    def jitter_threshold(base_threshold_ms: int, jitter_range_seconds: tuple[int, int] = (-30, 60)) -> int:
        low_s, high_s = jitter_range_seconds
        low_ms = int(low_s * 1000)
        high_ms = int(high_s * 1000)
        if low_ms > high_ms:
            low_ms, high_ms = high_ms, low_ms
        jitter_ms = random.randint(low_ms, high_ms)
        return max(60_000, int(base_threshold_ms) + jitter_ms)

    @staticmethod
    def random_y_position(screen_top: int, screen_height: int) -> int:
        return random.randint(
            int(screen_top + screen_height * 0.2),
            int(screen_top + screen_height * 0.8),
        )

