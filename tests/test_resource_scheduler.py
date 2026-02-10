from __future__ import annotations

import sys
import unittest
from datetime import datetime, timedelta
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from core.resource_scheduler import ResourceScheduler


class ResourceSchedulerTest(unittest.TestCase):
    def test_fullscreen_suspends_cv_and_llm(self) -> None:
        scheduler = ResourceScheduler()
        plan = scheduler.resolve_plan(is_fullscreen=True, user_dialog_active=False)
        self.assertTrue(plan.gui_running)
        self.assertFalse(plan.cv_running)
        self.assertFalse(plan.llm_running)

    def test_dialog_activates_llm(self) -> None:
        scheduler = ResourceScheduler()
        plan = scheduler.resolve_plan(is_fullscreen=False, user_dialog_active=True)
        self.assertTrue(plan.llm_running)

    def test_llm_terminates_after_idle_window(self) -> None:
        scheduler = ResourceScheduler()
        now = datetime.now()
        scheduler.mark_dialog_activity(now)
        plan_active = scheduler.resolve_plan(
            is_fullscreen=False,
            user_dialog_active=False,
            now=now + timedelta(minutes=3),
        )
        self.assertTrue(plan_active.llm_running)

        plan_sleep = scheduler.resolve_plan(
            is_fullscreen=False,
            user_dialog_active=False,
            now=now + timedelta(minutes=8),
        )
        self.assertFalse(plan_sleep.llm_running)


if __name__ == "__main__":
    unittest.main()

