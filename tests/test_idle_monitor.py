from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from core.idle_monitor import IdleMonitor


class _User32Stub:
    def __init__(self, dw_time: int) -> None:
        self._dw_time = int(dw_time) & 0xFFFFFFFF

    def GetLastInputInfo(self, info_ptr) -> bool:  # noqa: N802 - Windows API style
        info_ptr._obj.dwTime = self._dw_time
        return True


class _Kernel32Tick32Stub:
    def __init__(self, tick: int) -> None:
        self._tick = int(tick) & 0xFFFFFFFF

    def GetTickCount(self) -> int:  # noqa: N802 - Windows API style
        return self._tick


class _Kernel32Tick64Stub:
    def __init__(self, tick64: int) -> None:
        self._tick64 = int(tick64)

    def GetTickCount64(self) -> int:  # noqa: N802 - Windows API style
        return self._tick64


class IdleMonitorTests(unittest.TestCase):
    def _build_monitor(self) -> IdleMonitor:
        monitor = IdleMonitor(threshold_ms=1000)
        monitor._is_windows = True
        return monitor

    def test_idle_time_uses_32bit_wraparound_when_tick32(self) -> None:
        monitor = self._build_monitor()
        monitor._user32 = _User32Stub(dw_time=0xFFFFFFF0)
        monitor._kernel32 = _Kernel32Tick32Stub(tick=0x10)
        monitor._has_tick64 = False

        self.assertEqual(monitor._get_idle_time_ms(), 0x20)

    def test_idle_time_uses_low32_alignment_when_tick64_gap_is_huge(self) -> None:
        monitor = self._build_monitor()
        monitor._user32 = _User32Stub(dw_time=0x00000FF0)
        monitor._kernel32 = _Kernel32Tick64Stub(tick64=0x0000000300001000)
        monitor._has_tick64 = True

        self.assertEqual(monitor._get_idle_time_ms(), 0x10)

    def test_idle_time_returns_zero_when_api_call_fails(self) -> None:
        monitor = self._build_monitor()
        monitor._kernel32 = _Kernel32Tick32Stub(tick=1234)
        monitor._has_tick64 = False

        class _FailingUser32:
            def GetLastInputInfo(self, _info_ptr) -> bool:  # noqa: N802
                return False

        monitor._user32 = _FailingUser32()
        self.assertEqual(monitor._get_idle_time_ms(), 0)


if __name__ == "__main__":
    unittest.main()
