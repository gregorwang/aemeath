from __future__ import annotations

import sys
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from core.hotkey_manager import setup_global_hotkeys


class _LoggerStub:
    def __init__(self) -> None:
        self.info_calls: list[tuple] = []
        self.warning_calls: list[tuple] = []
        self.error_calls: list[tuple] = []

    def info(self, *args) -> None:
        self.info_calls.append(args)

    def warning(self, *args) -> None:
        self.warning_calls.append(args)

    def error(self, *args) -> None:
        self.error_calls.append(args)

    def debug(self, *_args) -> None:
        return


class _AppStub:
    def __init__(self) -> None:
        self.installed_filters: list[object] = []
        self.removed_filters: list[object] = []

    def installNativeEventFilter(self, event_filter: object) -> None:
        self.installed_filters.append(event_filter)

    def removeNativeEventFilter(self, event_filter: object) -> None:
        self.removed_filters.append(event_filter)


class _User32Stub:
    def __init__(self, *, summon_ok: bool = True, push_ok: bool = True) -> None:
        self._summon_ok = bool(summon_ok)
        self._push_ok = bool(push_ok)
        self.register_calls: list[tuple[int, int, int]] = []
        self.unregister_calls: list[int] = []

    def RegisterHotKey(self, _hwnd, hotkey_id: int, modifiers: int, vk: int) -> bool:  # noqa: N802
        self.register_calls.append((int(hotkey_id), int(modifiers), int(vk)))
        if hotkey_id == 1:
            return self._summon_ok
        if hotkey_id == 2:
            return self._push_ok
        return False

    def UnregisterHotKey(self, _hwnd, hotkey_id: int) -> bool:  # noqa: N802
        self.unregister_calls.append(int(hotkey_id))
        return True


class _WindllStub:
    def __init__(self, user32: _User32Stub) -> None:
        self.user32 = user32


class HotkeyManagerTest(unittest.TestCase):
    def test_non_windows_returns_no_registrations(self) -> None:
        app = _AppStub()
        logger = _LoggerStub()

        with patch("core.hotkey_manager.sys.platform", "linux"):
            result = setup_global_hotkeys(app, logger, on_summon=lambda: None, on_push_to_talk=lambda: None)

        self.assertFalse(result.summon_registered)
        self.assertFalse(result.push_to_talk_registered)
        self.assertEqual(app.installed_filters, [])
        result.unregister()

    def test_windows_register_and_unregister_success(self) -> None:
        app = _AppStub()
        logger = _LoggerStub()
        user32 = _User32Stub(summon_ok=True, push_ok=True)
        windll = _WindllStub(user32)

        with patch("core.hotkey_manager.sys.platform", "win32"), patch("core.hotkey_manager.ctypes.windll", windll):
            result = setup_global_hotkeys(app, logger, on_summon=lambda: None, on_push_to_talk=lambda: None)

            self.assertTrue(result.summon_registered)
            self.assertTrue(result.push_to_talk_registered)
            self.assertEqual(len(app.installed_filters), 1)

            result.unregister()

        self.assertIn(1, user32.unregister_calls)
        self.assertIn(2, user32.unregister_calls)
        self.assertEqual(len(app.removed_filters), 1)

    def test_windows_partial_registration(self) -> None:
        app = _AppStub()
        logger = _LoggerStub()
        user32 = _User32Stub(summon_ok=False, push_ok=True)
        windll = _WindllStub(user32)

        with patch("core.hotkey_manager.sys.platform", "win32"), patch("core.hotkey_manager.ctypes.windll", windll):
            result = setup_global_hotkeys(app, logger, on_summon=lambda: None, on_push_to_talk=lambda: None)

        self.assertFalse(result.summon_registered)
        self.assertTrue(result.push_to_talk_registered)


if __name__ == "__main__":
    unittest.main()
