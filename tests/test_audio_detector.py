from __future__ import annotations

import unittest
import sys
from pathlib import Path

from PySide6.QtCore import QCoreApplication

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from core.audio_detector import AudioDetector


class _TestableAudioDetector(AudioDetector):
    def __init__(
        self,
        levels: list[float],
        *,
        threshold: float = 0.01,
        start_debounce_polls: int = 2,
        stop_debounce_polls: int = 2,
    ) -> None:
        self._levels = list(levels)
        self._cursor = 0
        super().__init__(
            poll_interval_ms=200,
            threshold=threshold,
            start_debounce_polls=start_debounce_polls,
            stop_debounce_polls=stop_debounce_polls,
        )

    def _init_default_output_meter(self) -> bool:
        return True

    def _read_peak_level(self) -> float:
        if self._cursor >= len(self._levels):
            return 0.0
        value = float(self._levels[self._cursor])
        self._cursor += 1
        return value


class AudioDetectorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls._app = QCoreApplication.instance() or QCoreApplication([])

    def test_audio_started_with_threshold_and_debounce(self) -> None:
        detector = _TestableAudioDetector([0.006, 0.012, 0.013], start_debounce_polls=2, stop_debounce_polls=2)
        started: list[bool] = []
        detector.audio_started.connect(lambda: started.append(True))

        detector.start_monitoring()
        detector._poll_once()
        detector._poll_once()
        detector._poll_once()
        detector.stop_monitoring()

        self.assertEqual(len(started), 1)

    def test_audio_stopped_after_silence_debounce(self) -> None:
        detector = _TestableAudioDetector([0.02, 0.03, 0.0, 0.0], start_debounce_polls=2, stop_debounce_polls=2)
        started: list[bool] = []
        stopped: list[bool] = []
        detector.audio_started.connect(lambda: started.append(True))
        detector.audio_stopped.connect(lambda: stopped.append(True))

        detector.start_monitoring()
        detector._poll_once()
        detector._poll_once()
        detector._poll_once()
        detector._poll_once()

        self.assertEqual(len(started), 1)
        self.assertEqual(len(stopped), 1)
        self.assertFalse(detector.is_playing)

    def test_stop_monitoring_emits_stopped_when_playing(self) -> None:
        detector = _TestableAudioDetector([0.02, 0.03], start_debounce_polls=2, stop_debounce_polls=2)
        stopped: list[bool] = []
        detector.audio_stopped.connect(lambda: stopped.append(True))

        detector.start_monitoring()
        detector._poll_once()
        detector._poll_once()
        detector.stop_monitoring()

        self.assertEqual(len(stopped), 1)
        self.assertFalse(detector.is_playing)


if __name__ == "__main__":
    unittest.main()
