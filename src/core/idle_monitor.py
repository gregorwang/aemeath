from __future__ import annotations

import ctypes
import ctypes.wintypes
import platform
from dataclasses import dataclass

from PySide6.QtCore import QThread, Signal


class LASTINPUTINFO(ctypes.Structure):
    """Windows API structure for GetLastInputInfo."""

    _fields_ = [
        ("cbSize", ctypes.wintypes.UINT),
        ("dwTime", ctypes.wintypes.DWORD),
    ]


class IdleState:
    """Idle state machine states."""

    STANDBY = "STANDBY"
    PRE_IDLE = "PRE_IDLE"
    IDLE_TRIGGERED = "IDLE_TRIGGERED"
    ACTIVE = "ACTIVE"


@dataclass(slots=True)
class IdleSnapshot:
    """Optional debug snapshot for state transitions."""

    idle_ms: int
    state: str


class IdleMonitor(QThread):
    """
    Background idle monitor thread based on Windows GetLastInputInfo.

    Signals:
    - user_idle_confirmed: user crossed idle threshold
    - user_active_detected: input activity after idle state
    - idle_time_updated(int): live idle time in milliseconds
    """

    user_idle_confirmed = Signal()
    user_active_detected = Signal()
    idle_time_updated = Signal(int)
    state_changed = Signal(str)

    POLL_INTERVAL_MS: int = 100
    DEFAULT_THRESHOLD_MS: int = 180_000
    PRE_IDLE_RATIO: float = 0.8
    ACTIVE_RESET_MS: int = 1_000

    def __init__(self, threshold_ms: int = DEFAULT_THRESHOLD_MS, parent=None):
        super().__init__(parent)
        self._threshold_ms = max(threshold_ms, 1)
        self._state = IdleState.STANDBY
        self._running = False
        self._is_windows = platform.system() == "Windows"
        self._user32 = None
        self._kernel32 = None
        self._has_tick64 = False
        if self._is_windows:
            self._user32 = ctypes.windll.user32
            self._kernel32 = ctypes.windll.kernel32
            self._user32.GetLastInputInfo.argtypes = [ctypes.POINTER(LASTINPUTINFO)]
            self._user32.GetLastInputInfo.restype = ctypes.wintypes.BOOL
            self._kernel32.GetTickCount.restype = ctypes.wintypes.DWORD
            self._has_tick64 = hasattr(self._kernel32, "GetTickCount64")
            if self._has_tick64:
                self._kernel32.GetTickCount64.restype = ctypes.c_ulonglong

    @property
    def state(self) -> str:
        return self._state

    def run(self) -> None:
        self._running = True
        while self._running:
            idle_ms = self._get_idle_time_ms()
            self.idle_time_updated.emit(idle_ms)
            self._update_state(idle_ms)
            self.msleep(self.POLL_INTERVAL_MS)

    def stop(self) -> None:
        self._running = False
        if self.isRunning():
            self.wait(2000)

    def set_threshold_ms(self, threshold_ms: int) -> None:
        self._threshold_ms = max(1, int(threshold_ms))

    def reset_to_standby(self) -> None:
        self._set_state(IdleState.STANDBY)

    def _get_idle_time_ms(self) -> int:
        if not self._is_windows or self._user32 is None or self._kernel32 is None:
            return 0

        lii = LASTINPUTINFO()
        lii.cbSize = ctypes.sizeof(LASTINPUTINFO)
        if not self._user32.GetLastInputInfo(ctypes.byref(lii)):
            return 0

        try:
            if self._has_tick64:
                current_tick = int(self._kernel32.GetTickCount64()) & 0xFFFFFFFF
            else:
                current_tick = int(self._kernel32.GetTickCount())
        except Exception:
            return 0

        idle_time = current_tick - int(lii.dwTime)
        if idle_time < 0:
            idle_time = (idle_time + 0x100000000) & 0xFFFFFFFF
        return max(0, int(idle_time))

    def _update_state(self, idle_ms: int) -> None:
        match self._state:
            case IdleState.STANDBY:
                if idle_ms >= self._threshold_ms:
                    self._set_state(IdleState.IDLE_TRIGGERED)
                    self.user_idle_confirmed.emit()
                elif idle_ms >= int(self._threshold_ms * self.PRE_IDLE_RATIO):
                    self._set_state(IdleState.PRE_IDLE)

            case IdleState.PRE_IDLE:
                if idle_ms >= self._threshold_ms:
                    self._set_state(IdleState.IDLE_TRIGGERED)
                    self.user_idle_confirmed.emit()
                elif idle_ms < self.ACTIVE_RESET_MS:
                    self._set_state(IdleState.STANDBY)

            case IdleState.IDLE_TRIGGERED:
                if idle_ms < self.ACTIVE_RESET_MS:
                    self._set_state(IdleState.ACTIVE)
                    self.user_active_detected.emit()

            case IdleState.ACTIVE:
                # ACTIVE state must be reset by external call reset_to_standby().
                pass

    def _set_state(self, state: str) -> None:
        if self._state == state:
            return
        self._state = state
        self.state_changed.emit(state)
