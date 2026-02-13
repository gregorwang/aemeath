from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

try:
    from PyQt6.QtWidgets import QApplication, QLabel, QPushButton, QVBoxLayout, QWidget
except ImportError:
    try:
        from PyQt5.QtWidgets import QApplication, QLabel, QPushButton, QVBoxLayout, QWidget
    except ImportError:
        from PySide6.QtWidgets import QApplication, QLabel, QPushButton, QVBoxLayout, QWidget

from core.audio_detector import AudioDetector


class AudioDetectorDemo(QWidget):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("Audio Detector Demo")
        self.resize(360, 180)

        self._detector = AudioDetector(poll_interval_ms=200, threshold=0.01, start_debounce_polls=2, stop_debounce_polls=3)

        self._status_label = QLabel("状态: 未启动")
        self._peak_label = QLabel("峰值: 0.0000")
        self._hint_label = QLabel("阈值: 0.01  轮询: 200ms")

        self._start_btn = QPushButton("开始检测")
        self._stop_btn = QPushButton("停止检测")
        self._stop_btn.setEnabled(False)

        layout = QVBoxLayout(self)
        layout.addWidget(self._status_label)
        layout.addWidget(self._peak_label)
        layout.addWidget(self._hint_label)
        layout.addWidget(self._start_btn)
        layout.addWidget(self._stop_btn)

        self._start_btn.clicked.connect(self._on_start_clicked)
        self._stop_btn.clicked.connect(self._on_stop_clicked)
        self._detector.audio_started.connect(self._on_audio_started)
        self._detector.audio_stopped.connect(self._on_audio_stopped)
        self._detector.peak_level_changed.connect(self._on_peak_changed)

    def _on_start_clicked(self) -> None:
        self._detector.start_monitoring()
        self._status_label.setText("状态: 监听中（无声）")
        self._start_btn.setEnabled(False)
        self._stop_btn.setEnabled(True)

    def _on_stop_clicked(self) -> None:
        self._detector.stop_monitoring()
        self._status_label.setText("状态: 已停止")
        self._start_btn.setEnabled(True)
        self._stop_btn.setEnabled(False)

    def _on_audio_started(self) -> None:
        self._status_label.setText("状态: 有声音（显示小人）")

    def _on_audio_stopped(self) -> None:
        self._status_label.setText("状态: 无声音（隐藏小人）")

    def _on_peak_changed(self, peak: float) -> None:
        self._peak_label.setText(f"峰值: {peak:.4f}")


def main() -> int:
    app = QApplication(sys.argv)
    window = AudioDetectorDemo()
    window.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
