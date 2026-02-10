from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime, timedelta


@dataclass(slots=True)
class ResourcePlan:
    gui_running: bool = True
    cv_running: bool = True
    llm_running: bool = False


class ResourceScheduler:
    """
    Lightweight scheduler for GUI/CV/LLM lifecycle decisions.
    """

    LLM_IDLE_TERMINATE_MINUTES = 5

    def __init__(self):
        self._last_dialog_at: datetime | None = None

    def mark_dialog_activity(self, now: datetime | None = None) -> None:
        self._last_dialog_at = now or datetime.now()

    def resolve_plan(self, *, is_fullscreen: bool, user_dialog_active: bool, now: datetime | None = None) -> ResourcePlan:
        timestamp = now or datetime.now()
        if is_fullscreen:
            return ResourcePlan(gui_running=True, cv_running=False, llm_running=False)

        if user_dialog_active:
            self.mark_dialog_activity(timestamp)
            return ResourcePlan(gui_running=True, cv_running=True, llm_running=True)

        if self._last_dialog_at and timestamp <= self._last_dialog_at + timedelta(minutes=self.LLM_IDLE_TERMINATE_MINUTES):
            return ResourcePlan(gui_running=True, cv_running=True, llm_running=True)

        return ResourcePlan(gui_running=True, cv_running=True, llm_running=False)

