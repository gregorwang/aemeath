from __future__ import annotations

from datetime import datetime


def is_default_time_range(time_range: str) -> bool:
    return (time_range or "default").strip().lower() == "default"


def matches_time_range(time_range: str, now: datetime) -> bool:
    """
    Match HH:MM-HH:MM ranges using end-exclusive semantics.

    Examples:
    - 12:00-13:00 matches 12:00 but not 13:00
    - 22:00-06:00 wraps midnight
    """
    value = (time_range or "default").strip().lower()
    if value == "default":
        return True
    if "-" not in value:
        return False

    start_text, end_text = value.split("-", 1)
    try:
        start_minutes = _to_minutes(start_text)
        end_minutes = _to_minutes(end_text)
    except ValueError:
        return False

    current = now.hour * 60 + now.minute
    if start_minutes <= end_minutes:
        return start_minutes <= current < end_minutes
    return current >= start_minutes or current < end_minutes


def _to_minutes(value: str) -> int:
    hh_str, mm_str = value.strip().split(":")
    hh = int(hh_str)
    mm = int(mm_str)
    if not (0 <= hh < 24 and 0 <= mm < 60):
        raise ValueError("Invalid HH:MM")
    return hh * 60 + mm
