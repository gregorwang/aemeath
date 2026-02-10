from __future__ import annotations

import sys
from pathlib import Path

from PySide6.QtCore import QTimer
from PySide6.QtWidgets import QApplication

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from core.asset_manager import Script
from core.audio_manager import AudioManager


if __name__ == "__main__":
    app = QApplication(sys.argv)
    cache_dir = ROOT / ".cache" / "audio_test"
    audio_manager = AudioManager(cache_dir=cache_dir)

    script_a = Script(id="audio_low_1", text="这是一条低优先级语音，用于队列测试。", priority=3)
    script_b = Script(id="audio_low_2", text="这是第二条低优先级语音，应排队等待播放。", priority=3)
    script_c = Script(id="audio_high_1", text="高优先级语音打断测试。", priority=1)

    print("enqueue: low #1")
    audio_manager.play_script(script_a, priority=3)
    QTimer.singleShot(300, lambda: (print("enqueue: low #2"), audio_manager.play_script(script_b, priority=3)))
    QTimer.singleShot(900, lambda: (print("enqueue: HIGH interrupt"), audio_manager.play_script(script_c, priority=1, interrupt=True)))
    QTimer.singleShot(4500, lambda: (print("interrupt and exit"), audio_manager.stop(), app.quit()))

    raise SystemExit(app.exec())
