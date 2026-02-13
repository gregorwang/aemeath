"""
System Audio Output Monitor.

Uses Windows Audio Session API (WASAPI) via pycaw/comtypes to detect
when any application is producing audio output (music, video, etc.).

Emits:
    audio_playing_started  – when system audio output is detected
    audio_playing_stopped  – when system audio output stops
"""

from __future__ import annotations

from dataclasses import dataclass
import logging
import os
import time

from PySide6.QtCore import QObject, QThread, QTimer, Signal, Slot

logger = logging.getLogger("CyberCompanion")


@dataclass(slots=True)
class ActiveAudioSession:
    pid: int | None
    process_name: str
    display_name: str
    peak: float


_MEDIA_PROCESS_KEYWORDS = (
    "chrome",
    "msedge",
    "firefox",
    "vlc",
    "potplayer",
    "mpv",
    "spotify",
    "qqmusic",
    "cloudmusic",
    "music",
    "wmplayer",
    "kodi",
    "bilibili",
    "youku",
)

_NON_MEDIA_PROCESS_KEYWORDS = (
    "cybercompanion",
    "python",
    "pythonw",
    "ffmpeg",
    "audiodg",
    "svchost",
)


def _build_session_summary(sessions: list[ActiveAudioSession], *, limit: int = 3) -> str:
    if not sessions:
        return "-"
    parts: list[str] = []
    for item in sessions[: max(1, limit)]:
        label = item.process_name or item.display_name or "unknown"
        if item.pid is not None:
            label = f"{label}(pid={item.pid})"
        parts.append(f"{label}:{item.peak:.3f}")
    return ", ".join(parts)


def _looks_like_media_session(session: ActiveAudioSession) -> bool:
    blob = f"{session.process_name} {session.display_name}".lower().strip()
    if not blob:
        return False
    if any(keyword in blob for keyword in _NON_MEDIA_PROCESS_KEYWORDS):
        return False
    return any(keyword in blob for keyword in _MEDIA_PROCESS_KEYWORDS)


def _extract_session_identity(session) -> tuple[int | None, str, str]:
    pid: int | None = None
    process_name = ""
    display_name = ""
    try:
        display_name = str(getattr(session, "DisplayName", "") or "").strip().lower()
    except Exception:
        display_name = ""

    process = getattr(session, "Process", None)
    if process is not None:
        try:
            raw_pid = getattr(process, "pid", None)
            pid = int(raw_pid) if raw_pid is not None else None
        except Exception:
            pid = None
        try:
            process_name = str(process.name() or "").strip().lower()
        except Exception:
            process_name = ""

    if pid is None:
        try:
            raw_pid = getattr(session, "ProcessId", None)
            if raw_pid is not None:
                pid = int(raw_pid)
        except Exception:
            pid = None

    return pid, process_name, display_name


def _collect_active_sessions(
    *,
    ignore_pid: int | None,
    min_peak: float = 0.001,
) -> list[ActiveAudioSession]:
    import comtypes  # noqa: F401
    from pycaw.pycaw import AudioUtilities, IAudioMeterInformation

    active: list[ActiveAudioSession] = []
    sessions = AudioUtilities.GetAllSessions()
    for session in sessions:
        try:
            pid, process_name, display_name = _extract_session_identity(session)
            if ignore_pid is not None and pid == ignore_pid:
                continue
            meter = session._ctl.QueryInterface(IAudioMeterInformation)
            peak = float(meter.GetPeakValue() or 0.0)
            if peak <= min_peak:
                continue
            active.append(
                ActiveAudioSession(
                    pid=pid,
                    process_name=process_name,
                    display_name=display_name,
                    peak=peak,
                )
            )
        except Exception:
            continue
    return active


def _check_system_audio_playing(
    *,
    ignore_pid: int | None = None,
    include_master_peak: bool = False,
    prefer_media_sessions: bool = True,
) -> tuple[bool, list[ActiveAudioSession], list[ActiveAudioSession]]:
    """Return (is_playing, matched_sessions, all_active_sessions)."""
    try:
        import comtypes  # noqa: F401
        from comtypes import CLSCTX_ALL
        from pycaw.pycaw import AudioUtilities, IAudioMeterInformation

        active = _collect_active_sessions(ignore_pid=ignore_pid)
        matched = [item for item in active if _looks_like_media_session(item)] if prefer_media_sessions else list(active)
        if matched:
            return True, matched, active

        if include_master_peak:
            # Optional fallback for machines where session meters are unavailable.
            try:
                devices = AudioUtilities.GetSpeakers()
                interface = devices.Activate(
                    IAudioMeterInformation._iid_, CLSCTX_ALL, None
                )
                meter = interface.QueryInterface(IAudioMeterInformation)
                peak = meter.GetPeakValue()
                if peak > 0.005:
                    return True, [], active
            except Exception:
                pass

        return False, [], active
    except ImportError:
        return False, [], []
    except Exception:
        return False, [], []


class AudioOutputMonitor(QObject):
    """
    Monitors system-wide audio output and emits signals on state changes.

    Polling-based approach checking audio meter levels every `poll_interval_ms`.
    Uses a debounce mechanism to avoid rapid toggling.
    """

    audio_playing_started = Signal()
    audio_playing_stopped = Signal()
    audio_state_changed = Signal(bool)  # True = playing, False = stopped
    _poll_requested = Signal(int, bool, bool)

    # How many consecutive "silent" polls before declaring stopped
    SILENCE_DEBOUNCE_COUNT = 5

    def __init__(
        self,
        poll_interval_ms: int = 500,
        ignore_current_process_audio: bool = True,
        include_master_peak_fallback: bool = False,
        prefer_media_sessions: bool = True,
        parent: QObject | None = None,
    ):
        super().__init__(parent)
        self._poll_interval_ms = max(100, poll_interval_ms)
        self._is_playing = False
        self._silence_counter = 0
        self._running = False
        self._dependencies_available = self._check_dependencies()
        self._ignore_current_process_audio = bool(ignore_current_process_audio)
        self._include_master_peak_fallback = bool(include_master_peak_fallback)
        self._prefer_media_sessions = bool(prefer_media_sessions)
        self._current_pid = os.getpid() if self._ignore_current_process_audio else None
        self._last_rejected_log_monotonic = 0.0
        self._last_session_summary = "-"

        self._timer = QTimer(self)
        self._timer.setInterval(self._poll_interval_ms)
        self._timer.timeout.connect(self._poll_async)

        self._worker_thread: QThread | None = None
        self._worker: _AudioOutputPollWorker | None = None
        self._poll_inflight = False

    @staticmethod
    def _check_dependencies() -> bool:
        """Check if pycaw and comtypes are available."""
        try:
            import comtypes  # noqa: F401
            from pycaw.pycaw import AudioUtilities  # noqa: F401
            return True
        except ImportError:
            logger.warning("[AudioOutputMonitor] pycaw/comtypes 未安装，音频输出检测不可用。")
            logger.warning("[AudioOutputMonitor] 安装依赖: pip install pycaw comtypes")
            return False

    @property
    def is_playing(self) -> bool:
        """Whether system audio is currently detected."""
        return self._is_playing

    @property
    def is_available(self) -> bool:
        """Whether the required dependencies are installed."""
        return self._dependencies_available

    def set_prefer_media_sessions(self, enabled: bool) -> None:
        """Enable or disable media-process filtering at runtime."""
        self._prefer_media_sessions = bool(enabled)

    def start(self) -> None:
        """Start monitoring audio output."""
        if not self._dependencies_available:
            logger.warning("[AudioOutputMonitor] 依赖缺失，无法启动监控。")
            return
        if self._running:
            return
        self._running = True
        self._silence_counter = 0
        self._poll_inflight = False
        self._ensure_worker()
        self._timer.start()
        logger.info("[AudioOutputMonitor] 音频输出监控已启动")

    def stop(self) -> None:
        """Stop monitoring audio output."""
        if not self._running:
            return
        self._running = False
        self._timer.stop()
        self._shutdown_worker()
        self._poll_inflight = False
        if self._is_playing:
            self._is_playing = False
            self.audio_playing_stopped.emit()
            self.audio_state_changed.emit(False)
        logger.info("[AudioOutputMonitor] 音频输出监控已停止")

    def __del__(self) -> None:
        try:
            self._shutdown_worker()
        except Exception:
            pass

    def _ensure_worker(self) -> None:
        if self._worker_thread is not None and self._worker is not None and self._worker_thread.isRunning():
            return

        thread = QThread(self)
        worker = _AudioOutputPollWorker()
        worker.moveToThread(thread)
        self._poll_requested.connect(worker.poll)
        worker.poll_finished.connect(self._on_poll_result)
        thread.finished.connect(worker.deleteLater)
        thread.start()

        self._worker_thread = thread
        self._worker = worker

    def _shutdown_worker(self) -> None:
        worker = self._worker
        thread = self._worker_thread
        if worker is not None:
            try:
                self._poll_requested.disconnect(worker.poll)
            except Exception:
                pass
            try:
                worker.poll_finished.disconnect(self._on_poll_result)
            except Exception:
                pass
        self._worker = None
        self._worker_thread = None
        if thread is not None:
            thread.quit()
            thread.wait(1000)

    def _poll_async(self) -> None:
        if not self._running:
            return
        if self._worker_thread is None or self._worker is None or not self._worker_thread.isRunning():
            self._poll()
            return
        if self._poll_inflight:
            return
        self._poll_inflight = True
        ignore_pid = -1 if self._current_pid is None else int(self._current_pid)
        self._poll_requested.emit(ignore_pid, self._include_master_peak_fallback, self._prefer_media_sessions)

    # Compatibility entrypoint when direct synchronous polling is needed.
    def _poll(self) -> None:
        """Check current audio output status."""
        if not self._running:
            return
        try:
            currently_playing, matched_sessions, all_active_sessions = _check_system_audio_playing(
                ignore_pid=self._current_pid,
                include_master_peak=self._include_master_peak_fallback,
                prefer_media_sessions=self._prefer_media_sessions,
            )
        except Exception:
            return

        matched_summary = _build_session_summary(matched_sessions) if matched_sessions else ""
        all_summary = _build_session_summary(all_active_sessions) if all_active_sessions else ""
        self._apply_poll_result(
            currently_playing=bool(currently_playing),
            matched_summary=matched_summary,
            has_all_active=bool(all_active_sessions),
            all_summary=all_summary,
        )

    @Slot(bool, str, bool, str)
    def _on_poll_result(
        self,
        currently_playing: bool,
        matched_summary: str,
        has_all_active: bool,
        all_summary: str,
    ) -> None:
        self._poll_inflight = False
        if not self._running:
            return
        self._apply_poll_result(
            currently_playing=bool(currently_playing),
            matched_summary=matched_summary,
            has_all_active=bool(has_all_active),
            all_summary=all_summary,
        )

    def _apply_poll_result(
        self,
        *,
        currently_playing: bool,
        matched_summary: str,
        has_all_active: bool,
        all_summary: str,
    ) -> None:
        if matched_summary:
            self._last_session_summary = matched_summary
        elif all_summary:
            self._last_session_summary = all_summary

        if self._prefer_media_sessions and not currently_playing and has_all_active:
            now = time.monotonic()
            if now - self._last_rejected_log_monotonic >= 5.0:
                self._last_rejected_log_monotonic = now
                logger.debug("[AudioOutputMonitor] 检测到非媒体音频，会话已忽略: %s", all_summary)

        if currently_playing:
            self._silence_counter = 0
            if not self._is_playing:
                self._is_playing = True
                self.audio_playing_started.emit()
                self.audio_state_changed.emit(True)
                logger.debug("[AudioOutputMonitor] 检测到音频输出: %s", self._last_session_summary)
        else:
            if self._is_playing:
                self._silence_counter += 1
                if self._silence_counter >= self.SILENCE_DEBOUNCE_COUNT:
                    self._is_playing = False
                    self._silence_counter = 0
                    self.audio_playing_stopped.emit()
                    self.audio_state_changed.emit(False)
                    logger.debug("[AudioOutputMonitor] 音频输出已停止 (last=%s)", self._last_session_summary)


class _AudioOutputPollWorker(QObject):
    """Background worker for WASAPI session polling."""

    poll_finished = Signal(bool, str, bool, str)

    @Slot(int, bool, bool)
    def poll(self, ignore_pid: int, include_master_peak: bool, prefer_media_sessions: bool) -> None:
        normalized_pid = None if ignore_pid < 0 else ignore_pid
        try:
            currently_playing, matched_sessions, all_active_sessions = _check_system_audio_playing(
                ignore_pid=normalized_pid,
                include_master_peak=bool(include_master_peak),
                prefer_media_sessions=bool(prefer_media_sessions),
            )
        except Exception:
            self.poll_finished.emit(False, "", False, "")
            return

        matched_summary = _build_session_summary(matched_sessions) if matched_sessions else ""
        all_summary = _build_session_summary(all_active_sessions) if all_active_sessions else ""
        self.poll_finished.emit(bool(currently_playing), matched_summary, bool(all_active_sessions), all_summary)
