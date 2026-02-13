from __future__ import annotations

from typing import Iterable

from PySide6.QtCore import QObject, Signal
from PySide6.QtGui import QAction, QIcon
from PySide6.QtWidgets import QMenu, QStyle, QSystemTrayIcon


class SystemTrayManager(QObject):
    """
    System tray icon and context-menu manager.
    """

    summon_requested = Signal()
    settings_requested = Signal()
    status_requested = Signal()
    commentary_requested = Signal()
    open_logs_requested = Signal()
    quit_requested = Signal()
    character_switch_requested = Signal(str)
    toggle_requested = Signal()

    def __init__(self, app, icon_path: str | None = None):
        super().__init__(app)
        icon = QIcon(icon_path) if icon_path else app.style().standardIcon(QStyle.StandardPixmap.SP_ComputerIcon)
        self._tray = QSystemTrayIcon(icon, app)
        self._menu = QMenu()
        self._character_menu = self._menu.addMenu("切换角色")
        self._build_actions()
        self._tray.setContextMenu(self._menu)
        self._tray.activated.connect(self._on_activated)
        self._tray.setToolTip("Cyber Companion")

    def _build_actions(self) -> None:
        summon_action = self._menu.addAction("立即召唤")
        summon_action.triggered.connect(self.summon_requested.emit)

        commentary_action = self._menu.addAction("你在看什么？")
        commentary_action.triggered.connect(self.commentary_requested.emit)

        settings_action = self._menu.addAction("设置")
        settings_action.triggered.connect(self.settings_requested.emit)

        self._menu.addSeparator()

        status_action = self._menu.addAction("状态")
        status_action.triggered.connect(self.status_requested.emit)

        open_logs_action = self._menu.addAction("打开日志目录")
        open_logs_action.triggered.connect(self.open_logs_requested.emit)

        self._menu.addSeparator()

        quit_action = self._menu.addAction("退出")
        quit_action.triggered.connect(self.quit_requested.emit)

    def update_characters(self, manifests: Iterable[dict]) -> None:
        self._character_menu.clear()
        for item in manifests:
            character_id = str(item.get("id", "")).strip()
            if not character_id:
                continue
            name = str(item.get("name", character_id))
            action = QAction(name, self._character_menu)
            action.triggered.connect(lambda checked=False, cid=character_id: self.character_switch_requested.emit(cid))
            self._character_menu.addAction(action)
        if not self._character_menu.actions():
            no_data = QAction("无可用角色", self._character_menu)
            no_data.setEnabled(False)
            self._character_menu.addAction(no_data)

    def show(self) -> None:
        self._tray.show()

    def hide(self) -> None:
        self._tray.hide()

    def show_message(self, title: str, message: str, timeout_ms: int = 3000) -> None:
        self._tray.showMessage(title, message, QSystemTrayIcon.MessageIcon.Information, timeout_ms)

    def _on_activated(self, reason: QSystemTrayIcon.ActivationReason) -> None:
        if reason == QSystemTrayIcon.ActivationReason.DoubleClick:
            self.toggle_requested.emit()
