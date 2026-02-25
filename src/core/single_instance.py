from __future__ import annotations

import ctypes
from dataclasses import dataclass


@dataclass
class SingleInstanceGuard:
    """Windows named-mutex based single-instance guard."""

    mutex_name: str
    _handle: int | None = None

    ERROR_ALREADY_EXISTS = 183

    def acquire(self) -> bool:
        if self._handle:
            return True
        if not hasattr(ctypes, "WinDLL"):
            return True
        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        handle = kernel32.CreateMutexW(None, False, self.mutex_name)
        if not handle:
            return True
        self._handle = int(handle)
        last_error = int(ctypes.get_last_error() or 0)
        if last_error == self.ERROR_ALREADY_EXISTS:
            return False
        return True

    def release(self) -> None:
        handle = self._handle
        self._handle = None
        if not handle:
            return
        try:
            kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
            kernel32.CloseHandle(ctypes.c_void_p(handle))
        except Exception:
            return
