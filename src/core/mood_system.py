from __future__ import annotations


class MoodSystem:
    """
    Mood range: 0.0 (angry) -> 1.0 (excited), default 0.5.
    """

    def __init__(self, initial_mood: float = 0.5):
        self._mood = max(0.0, min(1.0, float(initial_mood)))

    @property
    def mood(self) -> float:
        return self._mood

    @property
    def mood_label(self) -> str:
        if self._mood < 0.2:
            return "愤怒"
        if self._mood < 0.4:
            return "不满"
        if self._mood < 0.6:
            return "平静"
        if self._mood < 0.8:
            return "开心"
        return "兴奋"

    def on_dismissed(self) -> None:
        self._mood = max(0.0, self._mood - 0.05)

    def on_interacted(self) -> None:
        self._mood = min(1.0, self._mood + 0.1)

    def on_engaged(self) -> None:
        self._mood = min(1.0, self._mood + 0.15)

    def natural_decay(self) -> None:
        if self._mood > 0.5:
            self._mood = max(0.5, self._mood - 0.02)
        elif self._mood < 0.5:
            self._mood = min(0.5, self._mood + 0.02)

