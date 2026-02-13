from __future__ import annotations

import logging

try:
    from PyQt6.QtCore import QObject, QTimer, pyqtSignal as Signal  # type: ignore[attr-defined]
except ImportError:  # pragma: no cover
    try:
        from PyQt5.QtCore import QObject, QTimer, pyqtSignal as Signal  # type: ignore[attr-defined]
    except ImportError:
        from PySide6.QtCore import QObject, QTimer, Signal

logger = logging.getLogger("CyberCompanion")


class AudioDetector(QObject):
    """
    Detect system audio output by reading default render device peak level.

    Requirements:
    - Timer interval: 200ms
    - Threshold: > 0.01 means audio is playing
    - Debounce to avoid rapid toggling
    """

    audio_started = Signal()
    audio_stopped = Signal()
    peak_level_changed = Signal(float)

    # Backward-compatible signals for existing monitor wiring.
    audio_playing_started = Signal()
    audio_playing_stopped = Signal()
    audio_state_changed = Signal(bool)

    DEFAULT_POLL_INTERVAL_MS = 200
    DEFAULT_THRESHOLD = 0.01

    def __init__(
        self,
        *,
        poll_interval_ms: int = DEFAULT_POLL_INTERVAL_MS,
        threshold: float = DEFAULT_THRESHOLD,
        start_debounce_polls: int = 2,
        stop_debounce_polls: int = 3,
        parent: QObject | None = None,
        **_legacy_kwargs,
    ) -> None:
        super().__init__(parent)
        self._poll_interval_ms = max(100, int(poll_interval_ms))
        self._threshold = max(0.0, float(threshold))
        self._start_debounce_polls = max(1, int(start_debounce_polls))
        self._stop_debounce_polls = max(1, int(stop_debounce_polls))

        self._running = False
        self._is_playing = False
        self._sound_counter = 0
        self._silence_counter = 0
        self._last_peak = 0.0

        self._meter = None
        self._dependencies_available = self._init_default_output_meter()

        self._timer = QTimer(self)
        self._timer.setInterval(self._poll_interval_ms)
        self._timer.timeout.connect(self._poll_once)

    @property
    def is_available(self) -> bool:
        return self._dependencies_available

    @property
    def is_playing(self) -> bool:
        return self._is_playing

    @property
    def last_peak(self) -> float:
        return float(self._last_peak)

    def start_monitoring(self) -> None:
        if not self._dependencies_available:
            logger.warning("[AudioDetector] 依赖缺失，无法启动音频检测。")
            return
        if self._running:
            return
        self._running = True
        self._sound_counter = 0
        self._silence_counter = 0
        self._timer.start()
        logger.info("[AudioDetector] 音频检测已启动 interval=%sms threshold=%.3f", self._poll_interval_ms, self._threshold)

    def stop_monitoring(self) -> None:
        if not self._running:
            return
        self._running = False
        self._timer.stop()
        self._sound_counter = 0
        self._silence_counter = 0
        self._last_peak = 0.0
        self.peak_level_changed.emit(0.0)
        if self._is_playing:
            self._set_playing_state(False)
        logger.info("[AudioDetector] 音频检测已停止")

    # Compatibility with existing call sites.
    def start(self) -> None:
        self.start_monitoring()

    def stop(self) -> None:
        self.stop_monitoring()

    def _init_default_output_meter(self) -> bool:
        try:
            import comtypes  # noqa: F401
            from comtypes import CLSCTX_ALL
            from pycaw.pycaw import AudioUtilities, IAudioMeterInformation

            speakers = AudioUtilities.GetSpeakers()
            interface = speakers.Activate(IAudioMeterInformation._iid_, CLSCTX_ALL, None)
            self._meter = interface.QueryInterface(IAudioMeterInformation)
            return True
        except ImportError:
            logger.warning("[AudioDetector] 缺少 pycaw/comtypes，音频检测不可用。请安装: pip install pycaw comtypes")
            self._meter = None
            return False
        except Exception as exc:
            logger.warning("[AudioDetector] 初始化默认输出设备失败: %s", exc)
            self._meter = None
            return False

    def _read_peak_level(self) -> float:
        if self._meter is None and not self._init_default_output_meter():
            return 0.0
        try:
            return float(self._meter.GetPeakValue() or 0.0)
        except Exception:
            # Device reinit once on transient COM/device changes.
            if self._init_default_output_meter() and self._meter is not None:
                try:
                    return float(self._meter.GetPeakValue() or 0.0)
                except Exception:
                    return 0.0
            return 0.0

    def _poll_once(self) -> None:
        if not self._running:
            return
        peak = self._read_peak_level()
        self._last_peak = float(peak)
        self.peak_level_changed.emit(self._last_peak)
        audible = peak > self._threshold
        self._apply_debounce(audible)

    def _apply_debounce(self, audible: bool) -> None:
        if audible:
            self._silence_counter = 0
            self._sound_counter += 1
            if not self._is_playing and self._sound_counter >= self._start_debounce_polls:
                self._set_playing_state(True)
            return

        self._sound_counter = 0
        if not self._is_playing:
            self._silence_counter = 0
            return

        self._silence_counter += 1
        if self._silence_counter >= self._stop_debounce_polls:
            self._set_playing_state(False)
            self._silence_counter = 0

    def _set_playing_state(self, playing: bool) -> None:
        if self._is_playing == playing:
            return
        self._is_playing = playing
        if playing:
            self.audio_started.emit()
            self.audio_playing_started.emit()
            self.audio_state_changed.emit(True)
        else:
            self.audio_stopped.emit()
            self.audio_playing_stopped.emit()
            self.audio_state_changed.emit(False)
