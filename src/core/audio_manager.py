from __future__ import annotations

import asyncio
import hashlib
import heapq
from dataclasses import dataclass
from enum import IntEnum
from pathlib import Path
from typing import Final

from PySide6.QtCore import QObject, QThread, QTimer, QUrl, Signal, Slot
from PySide6.QtMultimedia import QAudioOutput, QMediaPlayer

from .asset_manager import Script

try:
    import edge_tts
except ModuleNotFoundError:
    edge_tts = None


class AudioPriority(IntEnum):
    """Smaller number means higher priority."""

    CRITICAL = 0
    HIGH = 1
    NORMAL = 2
    LOW = 3


class _SynthesisWorker(QObject):
    """Qt worker running in dedicated thread for TTS synthesis."""

    audio_ready = Signal(str, int, bool, int, int)

    def __init__(self) -> None:
        super().__init__()
        self._running = True
        self._processing = False
        self._active_token = 0
        self._queue: list[tuple[int, int, dict]] = []

    @Slot(object)
    def enqueue_task(self, payload: object) -> None:
        if not self._running or not isinstance(payload, dict):
            return
        try:
            priority = int(payload["priority"])
            order = int(payload["order"])
            token = int(payload["token"])
        except Exception:
            return

        if token > self._active_token:
            self._active_token = token
            self._queue.clear()
        if token < self._active_token:
            return

        heapq.heappush(self._queue, (priority, order, payload))
        QTimer.singleShot(0, self._drain_once)

    @Slot(int)
    def invalidate(self, token: int) -> None:
        if token > self._active_token:
            self._active_token = token
        self._queue.clear()

    @Slot()
    def stop(self) -> None:
        self._running = False
        self._queue.clear()

    def _drain_once(self) -> None:
        if not self._running or self._processing:
            return
        if not self._queue:
            return

        _, _, task = heapq.heappop(self._queue)
        token = int(task.get("token", -1))
        if token != self._active_token:
            QTimer.singleShot(0, self._drain_once)
            return

        self._processing = True
        try:
            target = Path(str(task["target_path"]))
            if bool(task.get("cache_enabled", True)) and target.exists():
                self.audio_ready.emit(
                    str(target),
                    int(task["priority"]),
                    bool(task.get("interrupt", False)),
                    token,
                    int(task["order"]),
                )
                return

            if edge_tts is None:
                return

            if not bool(task.get("cache_enabled", True)) and target.exists():
                try:
                    target.unlink(missing_ok=True)
                except OSError:
                    pass

            target.parent.mkdir(parents=True, exist_ok=True)
            asyncio.run(
                self._synthesize_edge(
                    text=str(task["text"]),
                    voice=str(task["voice"]),
                    voice_rate=str(task["voice_rate"]),
                    output_path=target,
                )
            )
            self.audio_ready.emit(
                str(target),
                int(task["priority"]),
                bool(task.get("interrupt", False)),
                token,
                int(task["order"]),
            )
        except Exception:
            # Keep silent on synthesis errors, same behavior as legacy implementation.
            pass
        finally:
            self._processing = False
            if self._queue:
                QTimer.singleShot(0, self._drain_once)

    @staticmethod
    async def _synthesize_edge(*, text: str, voice: str, voice_rate: str, output_path: Path) -> None:
        communicator = edge_tts.Communicate(text, voice, rate=voice_rate)  # type: ignore[union-attr]
        await communicator.save(str(output_path))


class AudioManager(QObject):
    """
    Cache-first audio manager with queue and priority interrupt.

    - Uses script-provided local audio when available
    - Falls back to local/remote TTS generation into local cache
    - Synthesizes in a Qt background worker thread
    - Supports playback queue and high-priority interruption
    """

    _audio_ready = Signal(str, int, bool, int, int)
    _tts_enqueue_task = Signal(object)
    _tts_invalidate = Signal(int)
    _tts_stop = Signal()

    playback_started = Signal(str)
    playback_finished = Signal()

    CRITICAL_PRIORITY: Final[int] = int(AudioPriority.CRITICAL)
    HIGH_PRIORITY: Final[int] = int(AudioPriority.HIGH)
    NORMAL_PRIORITY: Final[int] = int(AudioPriority.NORMAL)
    LOW_PRIORITY: Final[int] = int(AudioPriority.LOW)

    @dataclass(slots=True)
    class _SpeechTask:
        """Speech synthesis request."""

        text: str
        priority: int
        interrupt: bool
        token: int
        order: int

    def __init__(
        self,
        cache_dir: Path,
        voice: str = "zh-CN-XiaoxiaoNeural",
        voice_rate: str = "+0%",
        volume: float = 0.8,
        cache_enabled: bool = True,
        parent=None,
    ):
        super().__init__(parent)
        self._cache_dir = cache_dir
        self._cache_dir.mkdir(parents=True, exist_ok=True)
        self._voice = voice
        self._voice_rate = voice_rate
        self._cache_enabled = cache_enabled
        self._player = QMediaPlayer(self)
        self._audio_output = QAudioOutput(self)
        self._audio_output.setVolume(min(max(volume, 0.0), 1.0))
        self._player.setAudioOutput(self._audio_output)

        self._token = 0
        self._task_seq = 0
        self._playback_heap: list[tuple[int, int, str, int]] = []
        self._running = True

        self._audio_ready.connect(self._on_audio_ready)
        self._player.mediaStatusChanged.connect(self._on_media_status_changed)

        self._worker_thread = QThread(self)
        self._worker = _SynthesisWorker()
        self._worker.moveToThread(self._worker_thread)
        self._worker.audio_ready.connect(self._audio_ready)
        self._tts_enqueue_task.connect(self._worker.enqueue_task)
        self._tts_invalidate.connect(self._worker.invalidate)
        self._tts_stop.connect(self._worker.stop)
        self._worker_thread.finished.connect(self._worker.deleteLater)
        self._worker_thread.start()

    def speak(
        self,
        text: str,
        *,
        priority: AudioPriority = AudioPriority.NORMAL,
        cached_path: str | None = None,
        interrupt: bool = False,
        voice_rate_override: str | None = None,
    ) -> None:
        """Speak one line with cache-first TTS and priority scheduling."""
        if not text.strip():
            return
        normalized_priority = self._normalize_priority(priority)
        should_interrupt = interrupt or normalized_priority == self.CRITICAL_PRIORITY
        if normalized_priority == self.LOW_PRIORITY and (
            self._player.playbackState() == QMediaPlayer.PlaybackState.PlayingState or self._has_pending_playback()
        ):
            return
        self._enqueue_request(
            text=text,
            audio_path=cached_path,
            priority=normalized_priority,
            interrupt=should_interrupt,
            voice_rate_override=voice_rate_override,
        )

    def play_script(self, script: Script, *, priority: int | None = None, interrupt: bool = False) -> None:
        """
        Enqueue one script audio request.

        - `priority`: lower number means higher priority
        - `interrupt`: force-stop current playback and clear older pending items
        """
        normalized_priority = self._normalize_priority(priority if priority is not None else script.priority)
        should_interrupt = interrupt or normalized_priority == self.CRITICAL_PRIORITY
        self._enqueue_request(
            text=script.text,
            audio_path=script.audio_path,
            priority=normalized_priority,
            interrupt=should_interrupt,
            voice_rate_override=None,
        )

    def interrupt(self, *, clear_pending_playback: bool = True, clear_pending_tts: bool = True) -> None:
        """
        Stop current audio immediately.

        - clear_pending_playback: clear already-resolved pending files
        - clear_pending_tts: invalidate pending/in-flight TTS synthesis tasks
        """
        self._player.stop()
        if clear_pending_playback:
            self._playback_heap.clear()
        if clear_pending_tts:
            self._token += 1
            self._tts_invalidate.emit(self._token)
        self.playback_finished.emit()

    def stop(self) -> None:
        """Shutdown synthesis worker gracefully."""
        if not self._running:
            return
        self._running = False
        self.interrupt()
        self._tts_stop.emit()
        if self._worker_thread.isRunning():
            self._worker_thread.quit()
            self._worker_thread.wait(1500)

    def __del__(self) -> None:
        try:
            self.stop()
        except Exception:
            pass

    def _cache_path(self, text: str, voice: str, voice_rate: str) -> Path:
        digest = hashlib.md5(f"edge:{voice}:{voice_rate}:{text}".encode("utf-8")).hexdigest()
        return self._cache_dir / f"{digest}.mp3"

    @Slot(str, int, bool, int, int)
    def _on_audio_ready(self, file_path: str, priority: int, interrupt: bool, token: int, order: int) -> None:
        if token != self._current_token():
            return
        path = Path(file_path)
        if not path.exists():
            return

        if interrupt or priority == self.CRITICAL_PRIORITY:
            self._player.stop()
            self._playback_heap.clear()
            self._start_playback(path)
            return

        if self._player.playbackState() == QMediaPlayer.PlaybackState.PlayingState:
            heapq.heappush(self._playback_heap, (priority, order, str(path), token))
            return

        self._start_playback(path)

    @Slot(QMediaPlayer.MediaStatus)
    def _on_media_status_changed(self, status: QMediaPlayer.MediaStatus) -> None:
        if status in (QMediaPlayer.MediaStatus.EndOfMedia, QMediaPlayer.MediaStatus.InvalidMedia):
            self._play_next()

    def _play_next(self) -> None:
        if not self._playback_heap:
            self.playback_finished.emit()
            return

        _, _, next_path, token = heapq.heappop(self._playback_heap)
        if token == self._current_token():
            self._start_playback(Path(next_path))
            return
        self._play_next()

    def _start_playback(self, path: Path) -> None:
        self._player.stop()
        self._player.setSource(QUrl.fromLocalFile(str(path.resolve())))
        self._player.play()
        self.playback_started.emit(str(path))

    def _current_token(self) -> int:
        return self._token

    def set_voice(self, voice: str) -> None:
        self._voice = voice.strip() or self._voice

    def set_rate(self, rate: str) -> None:
        self._voice_rate = rate.strip() or self._voice_rate

    def set_volume(self, volume: float) -> None:
        self._audio_output.setVolume(min(max(volume, 0.0), 1.0))

    def set_cache_enabled(self, enabled: bool) -> None:
        self._cache_enabled = bool(enabled)

    def _enqueue_request(
        self,
        *,
        text: str,
        audio_path: str | None,
        priority: int,
        interrupt: bool,
        voice_rate_override: str | None,
    ) -> None:
        clean_text = text.strip()
        if not clean_text:
            return
        if interrupt:
            self.interrupt(clear_pending_playback=True, clear_pending_tts=True)

        self._task_seq += 1
        order = self._task_seq
        token = self._current_token()

        if audio_path:
            local_audio = Path(audio_path)
            if local_audio.exists():
                self._audio_ready.emit(str(local_audio), priority, interrupt, token, order)
                return

        effective_voice_rate = (voice_rate_override or "").strip() or self._voice_rate
        target = self._cache_path(text=clean_text, voice=self._voice, voice_rate=effective_voice_rate)
        if self._cache_enabled and target.exists():
            self._audio_ready.emit(str(target), priority, interrupt, token, order)
            return

        if not self._can_synthesize():
            return

        task = self._SpeechTask(
            text=clean_text,
            priority=priority,
            interrupt=interrupt,
            token=token,
            order=order,
        )
        self._tts_enqueue_task.emit(
            {
                "text": task.text,
                "priority": task.priority,
                "interrupt": task.interrupt,
                "token": task.token,
                "order": task.order,
                "target_path": str(target),
                "voice": self._voice,
                "voice_rate": effective_voice_rate,
                "cache_enabled": self._cache_enabled,
            }
        )

    def _has_pending_playback(self) -> bool:
        return bool(self._playback_heap)

    def _can_synthesize(self) -> bool:
        return edge_tts is not None

    @staticmethod
    def _normalize_priority(priority: int | AudioPriority) -> int:
        try:
            numeric = int(priority)
        except Exception:
            numeric = int(AudioPriority.NORMAL)
        if numeric <= int(AudioPriority.CRITICAL):
            return int(AudioPriority.CRITICAL)
        if numeric >= int(AudioPriority.LOW):
            return int(AudioPriority.LOW)
        return numeric
