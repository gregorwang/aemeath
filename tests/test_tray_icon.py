from __future__ import annotations

import os
import sys
import unittest
from pathlib import Path

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

from PySide6.QtCore import QCoreApplication
from PySide6.QtWidgets import QApplication, QSystemTrayIcon

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from ui.tray_icon import SystemTrayManager


def _get_or_create_app() -> QApplication | None:
    current = QCoreApplication.instance()
    if current is not None:
        if isinstance(current, QApplication):
            return current
        return None
    return QApplication(sys.argv)


class SystemTrayManagerTest(unittest.TestCase):
    def setUp(self) -> None:
        self._app = _get_or_create_app()
        if self._app is None:
            self.skipTest("QCoreApplication already exists; tray tests require QApplication.")
        self._manager = SystemTrayManager(self._app, icon_path=":/icons/test.png")

    def tearDown(self) -> None:
        self._manager.hide()
        self._manager.deleteLater()

    def test_static_actions_emit_signals(self) -> None:
        fired: list[str] = []
        self._manager.summon_requested.connect(lambda: fired.append("summon"))
        self._manager.commentary_requested.connect(lambda: fired.append("commentary"))
        self._manager.settings_requested.connect(lambda: fired.append("settings"))
        self._manager.status_requested.connect(lambda: fired.append("status"))
        self._manager.open_logs_requested.connect(lambda: fired.append("open_logs"))
        self._manager.quit_requested.connect(lambda: fired.append("quit"))

        actions_by_text = {action.text(): action for action in self._manager._menu.actions() if action.text()}
        actions_by_text["立即召唤"].trigger()
        actions_by_text["你在看什么？"].trigger()
        actions_by_text["设置"].trigger()
        actions_by_text["状态"].trigger()
        actions_by_text["打开日志目录"].trigger()
        actions_by_text["退出"].trigger()

        self.assertEqual(
            fired,
            ["summon", "commentary", "settings", "status", "open_logs", "quit"],
        )

    def test_update_characters_builds_submenu_and_emits_switch(self) -> None:
        switched: list[str] = []
        self._manager.character_switch_requested.connect(switched.append)

        manifests = [
            {"id": "cat", "name": "Cat"},
            {"id": "fox", "name": "Fox"},
        ]
        self._manager.update_characters(manifests)

        labels = [action.text() for action in self._manager._character_menu.actions()]
        self.assertEqual(labels, ["Cat", "Fox"])

        self._manager._character_menu.actions()[1].trigger()
        self.assertEqual(switched, ["fox"])

    def test_update_characters_with_empty_data_uses_disabled_placeholder(self) -> None:
        self._manager.update_characters([])
        actions = self._manager._character_menu.actions()

        self.assertEqual(len(actions), 1)
        self.assertEqual(actions[0].text(), "无可用角色")
        self.assertFalse(actions[0].isEnabled())

    def test_double_click_activation_emits_toggle(self) -> None:
        fired: list[str] = []
        self._manager.toggle_requested.connect(lambda: fired.append("toggle"))

        self._manager._on_activated(QSystemTrayIcon.ActivationReason.DoubleClick)
        self.assertEqual(fired, ["toggle"])


if __name__ == "__main__":
    unittest.main()
