from __future__ import annotations

import ctypes
import ctypes.wintypes
import sys
from dataclasses import dataclass
from typing import Callable

from PySide6.QtCore import QAbstractNativeEventFilter, QTimer
from PySide6.QtWidgets import QApplication


@dataclass(slots=True)
class HotkeySetupResult:
    summon_registered: bool
    push_to_talk_registered: bool
    unregister: Callable[[], None]


class _WindowsHotkeyFilter(QAbstractNativeEventFilter):
    WM_HOTKEY = 0x0312

    def __init__(
        self,
        logger,
        *,
        summon_hotkey_id: int,
        push_to_talk_hotkey_id: int,
        on_summon: Callable[[], None],
        on_push_to_talk: Callable[[], None],
    ) -> None:
        super().__init__()
        self._logger = logger
        self._summon_hotkey_id = int(summon_hotkey_id)
        self._push_to_talk_hotkey_id = int(push_to_talk_hotkey_id)
        self._on_summon = on_summon
        self._on_push_to_talk = on_push_to_talk

    def nativeEventFilter(self, event_type, message):
        try:
            if event_type not in ("windows_generic_MSG", "windows_dispatcher_MSG"):
                return False, 0
            msg = ctypes.wintypes.MSG.from_address(int(message))
            if int(msg.message) != self.WM_HOTKEY:
                return False, 0
            hotkey_id = int(msg.wParam)
            if hotkey_id == self._summon_hotkey_id:
                self._logger.info("[Hotkey] Ctrl+Shift+S pressed")
                QTimer.singleShot(0, self._on_summon)
                return True, 0
            if hotkey_id == self._push_to_talk_hotkey_id:
                self._logger.info("[Hotkey] Ctrl+B pressed")
                QTimer.singleShot(0, self._on_push_to_talk)
                return True, 0
        except Exception as exc:
            self._logger.debug("[Hotkey] nativeEventFilter error: %s", exc)
        return False, 0


def setup_global_hotkeys(
    app: QApplication,
    logger,
    *,
    on_summon: Callable[[], None],
    on_push_to_talk: Callable[[], None],
) -> HotkeySetupResult:
    hotkey_filter: QAbstractNativeEventFilter | None = None
    registered_ids: list[int] = []

    HOTKEY_ID_SUMMON = 1
    HOTKEY_ID_PUSH_TO_TALK = 2
    MOD_CTRL = 0x0002  # MOD_CONTROL
    MOD_SHIFT = 0x0004
    MOD_CTRL_SHIFT = MOD_CTRL | MOD_SHIFT
    MOD_NOREPEAT = 0x4000
    VK_S = 0x53
    VK_B = 0x42

    def _unregister_hotkeys() -> None:
        nonlocal hotkey_filter
        if sys.platform != "win32":
            return
        if hotkey_filter is not None:
            try:
                app.removeNativeEventFilter(hotkey_filter)
            except Exception:
                pass
            hotkey_filter = None
        if not registered_ids:
            return
        try:
            user32 = ctypes.windll.user32
            for hotkey_id in list(registered_ids):
                try:
                    user32.UnregisterHotKey(None, hotkey_id)
                except Exception:
                    pass
        finally:
            registered_ids.clear()

    if sys.platform != "win32":
        logger.info("[Hotkey] Global hotkeys are only enabled on Windows")
        return HotkeySetupResult(False, False, _unregister_hotkeys)

    hotkey_filter = _WindowsHotkeyFilter(
        logger,
        summon_hotkey_id=HOTKEY_ID_SUMMON,
        push_to_talk_hotkey_id=HOTKEY_ID_PUSH_TO_TALK,
        on_summon=on_summon,
        on_push_to_talk=on_push_to_talk,
    )
    app.installNativeEventFilter(hotkey_filter)

    try:
        user32 = ctypes.windll.user32
        if user32.RegisterHotKey(None, HOTKEY_ID_SUMMON, MOD_CTRL_SHIFT, VK_S):
            registered_ids.append(HOTKEY_ID_SUMMON)
        else:
            logger.warning("[Hotkey] Failed to register Ctrl+Shift+S (may be in use by another app)")
        if user32.RegisterHotKey(None, HOTKEY_ID_PUSH_TO_TALK, MOD_CTRL | MOD_NOREPEAT, VK_B):
            registered_ids.append(HOTKEY_ID_PUSH_TO_TALK)
        elif user32.RegisterHotKey(None, HOTKEY_ID_PUSH_TO_TALK, MOD_CTRL, VK_B):
            registered_ids.append(HOTKEY_ID_PUSH_TO_TALK)
            logger.info("[Hotkey] Ctrl+B registered without MOD_NOREPEAT fallback")
        else:
            logger.warning("[Hotkey] Failed to register Ctrl+B for push-to-talk (may be in use by another app)")
        if registered_ids:
            logger.info("[Hotkey] ✅ 全局快捷键已注册: ids=%s", registered_ids)
        else:
            logger.warning("[Hotkey] 未成功注册任何全局快捷键")
    except Exception as exc:
        logger.error("[Hotkey] Failed to initialize native hotkey filter: %s", exc)
        _unregister_hotkeys()

    return HotkeySetupResult(
        summon_registered=HOTKEY_ID_SUMMON in registered_ids,
        push_to_talk_registered=HOTKEY_ID_PUSH_TO_TALK in registered_ids,
        unregister=_unregister_hotkeys,
    )
