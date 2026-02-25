from __future__ import annotations

from typing import Callable, Iterable, Mapping

from PySide6.QtCore import QObject, Signal
from PySide6.QtGui import QAction, QIcon
from PySide6.QtWidgets import QApplication, QMenu, QStyle, QSystemTrayIcon


class SystemTrayManager(QObject):
    """System tray icon and context-menu manager."""

    summon_requested = Signal()
    invasion_debug_requested = Signal()
    sad_comfort_debug_requested = Signal()
    no_face_debug_requested = Signal()
    settings_requested = Signal()
    status_requested = Signal()
    commentary_requested = Signal()
    guide_requested = Signal()
    open_logs_requested = Signal()
    edit_scripts_requested = Signal()
    reload_scripts_requested = Signal()
    copy_recent_logs_requested = Signal()
    feedback_requested = Signal()
    dnd_toggled = Signal(bool)
    quit_requested = Signal()
    character_switch_requested = Signal(str)
    toggle_requested = Signal()

    def __init__(self, app: QApplication, icon_path: str | None = None):
        """Initialize tray icon, menu, and actions."""
        super().__init__(app)
        icon = QIcon(icon_path) if icon_path else app.style().standardIcon(QStyle.StandardPixmap.SP_ComputerIcon)
        self._tray = QSystemTrayIcon(icon, app)
        self._menu = QMenu()
        self._character_menu = self._menu.addMenu("切换角色")
        self._dnd_action: QAction | None = None
        self._dnd_enabled = False
        self._build_actions()
        self._tray.setContextMenu(self._menu)
        self._tray.activated.connect(self._on_activated)
        self._refresh_tooltip()

    def _add_action(
        self,
        text: str,
        handler: Callable[[], None],
        *,
        tooltip: str = "",
        menu: QMenu | None = None,
        enabled: bool = True,
    ) -> QAction:
        """Create an action with consistent trigger wiring and optional tooltip."""
        target_menu = menu or self._menu
        action = target_menu.addAction(text)
        action.setEnabled(enabled)
        if tooltip:
            action.setToolTip(tooltip)
        action.triggered.connect(lambda _checked=False, cb=handler: cb())
        return action

    def _build_actions(self) -> None:
        """Build static tray actions."""
        self._add_action("立即召唤", self.summon_requested.emit, tooltip="立即显示角色并触发互动。")
        self._add_action("调试空闲入侵", self.invasion_debug_requested.emit, tooltip="立即触发空闲入侵效果用于调试。")
        self._add_action("调试悲伤安慰", self.sad_comfort_debug_requested.emit, tooltip="立即触发一次悲伤安慰语音。")
        self._add_action("调试无人脸提醒", self.no_face_debug_requested.emit, tooltip="立即触发一次无人脸提醒语音。")
        self._add_action("你在看什么？", self.commentary_requested.emit, tooltip="手动触发一次屏幕解读。")
        self._add_action("使用指南", self.guide_requested.emit, tooltip="打开项目快速上手指南。")
        self._add_action("编辑台词", self.edit_scripts_requested.emit, tooltip="打开当前角色 scripts.json。")
        self._add_action("重载台词", self.reload_scripts_requested.emit, tooltip="重新加载当前角色台词，无需重启。")
        self._dnd_action = self._menu.addAction("请勿打扰")
        self._dnd_action.setCheckable(True)
        self._dnd_action.setChecked(False)
        self._dnd_action.setToolTip("暂停自动出场/自动解读/空闲入侵。")
        self._dnd_action.toggled.connect(self._on_dnd_toggled)
        self._add_action("设置", self.settings_requested.emit, tooltip="打开设置面板。")

        self._menu.addSeparator()

        self._add_action("状态", self.status_requested.emit, tooltip="查看当前运行状态。")
        self._add_action("打开日志目录", self.open_logs_requested.emit, tooltip="打开 app.log 所在目录。")
        self._add_action("复制最近日志", self.copy_recent_logs_requested.emit, tooltip="复制 app.log 最近 50 行到剪贴板。")
        self._add_action("反馈问题", self.feedback_requested.emit, tooltip="打开问题反馈页面。")

        self._menu.addSeparator()
        self._add_action("退出", self.quit_requested.emit, tooltip="退出 Cyber Companion。")

    def update_characters(self, manifests: Iterable[Mapping[str, object]]) -> None:
        """Rebuild character switch submenu based on manifests."""
        self._character_menu.clear()
        for item in manifests:
            character_id = str(item.get("id", "")).strip()
            if not character_id:
                continue
            name = str(item.get("name", character_id))
            self._add_action(
                name,
                lambda cid=character_id: self.character_switch_requested.emit(cid),
                tooltip=f"切换到角色: {name}",
                menu=self._character_menu,
            )
        if not self._character_menu.actions():
            self._add_action(
                "无可用角色",
                lambda: None,
                tooltip="当前未检测到可切换角色。",
                menu=self._character_menu,
                enabled=False,
            )

    def show(self) -> None:
        """Show tray icon."""
        self._tray.show()

    def hide(self) -> None:
        """Hide tray icon."""
        self._tray.hide()

    def show_message(self, title: str, message: str, timeout_ms: int = 3000) -> None:
        """Show a transient tray notification."""
        self._tray.showMessage(title, message, QSystemTrayIcon.MessageIcon.Information, timeout_ms)

    def set_dnd_checked(self, enabled: bool) -> None:
        target = bool(enabled)
        if self._dnd_action is None:
            self._dnd_enabled = target
            self._refresh_tooltip()
            return
        if self._dnd_action.isChecked() != target:
            old_block = self._dnd_action.blockSignals(True)
            self._dnd_action.setChecked(target)
            self._dnd_action.blockSignals(old_block)
        self._dnd_enabled = target
        self._refresh_tooltip()

    def is_dnd_checked(self) -> bool:
        return bool(self._dnd_enabled)

    def _refresh_tooltip(self) -> None:
        if self._dnd_enabled:
            self._tray.setToolTip("Cyber Companion (请勿打扰)")
        else:
            self._tray.setToolTip("Cyber Companion")

    def _on_dnd_toggled(self, enabled: bool) -> None:
        self._dnd_enabled = bool(enabled)
        self._refresh_tooltip()
        self.dnd_toggled.emit(self._dnd_enabled)

    def _on_activated(self, reason: QSystemTrayIcon.ActivationReason) -> None:
        """Toggle companion window on tray double-click."""
        if reason == QSystemTrayIcon.ActivationReason.DoubleClick:
            self.toggle_requested.emit()
