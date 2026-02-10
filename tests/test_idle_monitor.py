from __future__ import annotations

import sys
from pathlib import Path

from PySide6.QtWidgets import QApplication

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from core.idle_monitor import IdleMonitor


if __name__ == "__main__":
    app = QApplication(sys.argv)
    monitor = IdleMonitor(threshold_ms=10_000)

    def _print_idle(ms: int) -> None:
        print(f"空闲: {ms / 1000:.1f}s")

    monitor.idle_time_updated.connect(_print_idle)
    monitor.user_idle_confirmed.connect(lambda: print(">>> 用户已离开！"))
    monitor.user_active_detected.connect(lambda: print(">>> 用户回来了！"))
    monitor.start()

    def _shutdown() -> None:
        monitor.stop()

    app.aboutToQuit.connect(_shutdown)
    raise SystemExit(app.exec())

