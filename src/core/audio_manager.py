from __future__ import annotations

import asyncio
import hashlib
import heapq
import queue
import threading
from dataclasses import dataclass
from enum import IntEnum
from pathlib import Path
from typing import Final

from PySide6.QtCore import QObject, QUrl, Signal, Slot
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


class AudioManager(QObject):
    """
    Cache-first audio manager with queue and priority interrupt.

    - Uses script-provided local audio when available
    - Falls back to edge-tts generation into local cache
    - Synthesizes in a background worker thread
    - Supports playback queue and high-priority interruption
    """

    _audio_ready = Signal(str, int, bool, int, int)
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
        self._audio_ready.connect(self._on_audio_ready)
        self._player.mediaStatusChanged.connect(self._on_media_status_changed)
        self._state_lock = threading.Lock()
        self._token = 0
        self._task_seq = 0
        self._playback_heap: list[tuple[int, int, str, int]] = []
        self._task_queue: queue.PriorityQueue[tuple[int, int, AudioManager._SpeechTask | None]] = queue.PriorityQueue()
        self._running = True
        self._worker = threading.Thread(target=self._worker_loop, daemon=True)
        self._worker.start()

    def speak(
        self,
        text: str,
        *,
        priority: AudioPriority = AudioPriority.NORMAL,
        cached_path: str | None = None,
        interrupt: bool = False,
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
        )

    def interrupt(self, *, clear_pending_playback: bool = True, clear_pending_tts: bool = True) -> None:
        """
        Stop current audio immediately.

        - clear_pending_playback: clear already-resolved pending files
        - clear_pending_tts: invalidate pending/in-flight TTS synthesis tasks
        """
        self._player.stop()
        with self._state_lock:
            if clear_pending_playback:
                self._playback_heap.clear()
            if clear_pending_tts:
                self._token += 1
        self.playback_finished.emit()

    def stop(self) -> None:
        """Shutdown background worker gracefully."""
        self.interrupt()
        self._running = False
        with self._state_lock:
            self._task_seq += 1
            self._task_queue.put((self.LOW_PRIORITY, self._task_seq, None))
        if self._worker.is_alive():
            self._worker.join(timeout=2.0)

    def _cache_path(self, text: str, voice: str) -> Path:
        digest = hashlib.md5(f"{voice}:{text}".encode("utf-8")).hexdigest()
        return self._cache_dir / f"{digest}.mp3"

    async def _synthesize(self, text: str, output_path: Path) -> None:
        communicator = edge_tts.Communicate(text, self._voice, rate=self._voice_rate)
        await communicator.save(str(output_path))

    @Slot(str, int, bool, int, int)
    def _on_audio_ready(self, file_path: str, priority: int, interrupt: bool, token: int, order: int) -> None:
        if token != self._current_token():
            return
        path = Path(file_path)
        if not path.exists():
            return

        if interrupt or priority == self.CRITICAL_PRIORITY:
            self._player.stop()
            with self._state_lock:
                self._playback_heap.clear()
            self._start_playback(path)
            return

        if self._player.playbackState() == QMediaPlayer.PlaybackState.PlayingState:
            with self._state_lock:
                heapq.heappush(self._playback_heap, (priority, order, str(path), token))
            return

        self._start_playback(path)

    @Slot(QMediaPlayer.MediaStatus)
    def _on_media_status_changed(self, status: QMediaPlayer.MediaStatus) -> None:
        if status in (QMediaPlayer.MediaStatus.EndOfMedia, QMediaPlayer.MediaStatus.InvalidMedia):
            self._play_next()

    def _play_next(self) -> None:
        next_path: str | None = None
        token: int | None = None
        with self._state_lock:
            if self._playback_heap:
                _, _, next_path, token = heapq.heappop(self._playback_heap)
        if next_path and token == self._current_token():
            self._start_playback(Path(next_path))
            return
        self.playback_finished.emit()

    def _start_playback(self, path: Path) -> None:
        self._player.stop()
        self._player.setSource(QUrl.fromLocalFile(str(path.resolve())))
        self._player.play()
        self.playback_started.emit(str(path))

    def _worker_loop(self) -> None:
        while self._running:
            try:
                _, _, task = self._task_queue.get(timeout=0.2)
            except queue.Empty:
                continue

            if task is None:
                continue
            if task.token != self._current_token():
                continue

            target = self._cache_path(text=task.text, voice=self._voice)
            if self._cache_enabled and target.exists():
                self._audio_ready.emit(str(target), task.priority, task.interrupt, task.token, task.order)
                continue

            if edge_tts is None:
                continue

            if not self._cache_enabled and target.exists():
                try:
                    target.unlink(missing_ok=True)
                except OSError:
                    pass

            try:
                asyncio.run(self._synthesize(text=task.text, output_path=target))
            except Exception:
                # Offline or synthesis failure should be silent and non-fatal.
                continue

            self._audio_ready.emit(str(target), task.priority, task.interrupt, task.token, task.order)

    def _current_token(self) -> int:
        with self._state_lock:
            return self._token

    def set_voice(self, voice: str) -> None:
        self._voice = voice.strip() or self._voice

    def set_rate(self, rate: str) -> None:
        self._voice_rate = rate.strip() or self._voice_rate

    def set_volume(self, volume: float) -> None:
        self._audio_output.setVolume(min(max(volume, 0.0), 1.0))

    def _enqueue_request(self, *, text: str, audio_path: str | None, priority: int, interrupt: bool) -> None:
        clean_text = text.strip()
        if not clean_text:
            return
        if interrupt:
            self.interrupt(clear_pending_playback=True, clear_pending_tts=True)

        token = self._current_token()
        with self._state_lock:
            self._task_seq += 1
            order = self._task_seq

        if audio_path:
            local_audio = Path(audio_path)
            if local_audio.exists():
                self._audio_ready.emit(str(local_audio), priority, interrupt, token, order)
                return

        target = self._cache_path(text=clean_text, voice=self._voice)
        if self._cache_enabled and target.exists():
            self._audio_ready.emit(str(target), priority, interrupt, token, order)
            return

        if edge_tts is None:
            return

        with self._state_lock:
            task = self._SpeechTask(
                text=clean_text,
                priority=priority,
                interrupt=interrupt,
                token=token,
                order=order,
            )
            self._task_queue.put((priority, order, task))

    def _has_pending_playback(self) -> bool:
        with self._state_lock:
            return bool(self._playback_heap)

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
