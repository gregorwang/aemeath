from __future__ import annotations

import sys
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from core.idle_invasion import IdleInvasionController


def _subject(
    *,
    idle_ms: int,
    start_delay_ms: int = 180_000,
    initial_spawn_interval_ms: int = 10_000,
    min_spawn_interval_ms: int = 2_000,
):
    return SimpleNamespace(
        _idle_time_ms=idle_ms,
        _config=SimpleNamespace(
            start_delay_ms=start_delay_ms,
            initial_spawn_interval_ms=initial_spawn_interval_ms,
            min_spawn_interval_ms=min_spawn_interval_ms,
        ),
    )


class IdleInvasionIntervalsTest(unittest.TestCase):
    def test_stage1_interval_bounds(self) -> None:
        subject = _subject(idle_ms=180_000 + 60_000)
        with patch("core.idle_invasion.random.randint", return_value=9_123) as mocked:
            value = IdleInvasionController._current_spawn_interval(subject)
        self.assertEqual(value, 9_123)
        mocked.assert_called_once_with(8_000, 12_000)

    def test_stage2_interval_bounds(self) -> None:
        subject = _subject(idle_ms=180_000 + 4 * 60_000)
        with patch("core.idle_invasion.random.randint", return_value=6_543) as mocked:
            value = IdleInvasionController._current_spawn_interval(subject)
        self.assertEqual(value, 6_543)
        mocked.assert_called_once_with(5_000, 8_000)

    def test_stage3_interval_bounds(self) -> None:
        subject = _subject(idle_ms=180_000 + 7 * 60_000)
        with patch("core.idle_invasion.random.randint", return_value=4_321) as mocked:
            value = IdleInvasionController._current_spawn_interval(subject)
        self.assertEqual(value, 4_321)
        mocked.assert_called_once_with(3_000, 5_000)

    def test_stage4_interval_bounds_respect_min(self) -> None:
        subject = _subject(idle_ms=180_000 + 15 * 60_000)
        with patch("core.idle_invasion.random.randint", return_value=2_345) as mocked:
            value = IdleInvasionController._current_spawn_interval(subject)
        self.assertEqual(value, 2_345)
        mocked.assert_called_once_with(2_000, 3_000)

    def test_stage4_supports_one_second_floor(self) -> None:
        subject = _subject(
            idle_ms=180_000 + 15 * 60_000,
            min_spawn_interval_ms=1_000,
        )
        with patch("core.idle_invasion.random.randint", return_value=1_678) as mocked:
            value = IdleInvasionController._current_spawn_interval(subject)
        self.assertEqual(value, 1_678)
        mocked.assert_called_once_with(1_000, 3_000)


if __name__ == "__main__":
    unittest.main()
