"""
Sprite sheet animator widget.

Loads a sprite sheet PNG + JSON metadata and provides frame-level controls.
"""

from __future__ import annotations

import json
from pathlib import Path

from PySide6.QtCore import QRect, QSize, QTimer, Qt, Signal
from PySide6.QtGui import QPixmap, QTransform
from PySide6.QtWidgets import QLabel, QWidget


class SpriteAnimator(QWidget):
    """A lightweight sprite-sheet animation player."""

    frame_changed = Signal(int)
    animation_finished = Signal()
    loop_completed = Signal()

    def __init__(self, parent: QWidget | None = None):
        super().__init__(parent)
        self._sheet_pixmap: QPixmap | None = None
        self._frame_width = 0
        self._frame_height = 0
        self._frame_count = 0
        self._columns = 1
        self._rows = 1
        self._padding = 0

        self._current_frame = 0
        self._fps = 10.0
        self._loop = True
        self._playing = False
        self._flipped_h = False
        self._scale_factor = 1.0

        self._frame_cache: dict[int, QPixmap] = {}
        self._flipped_cache: dict[int, QPixmap] = {}

        self._label = QLabel(self)
        self._label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self._label.setStyleSheet("background: transparent;")

        self._timer = QTimer(self)
        self._timer.timeout.connect(self._advance_frame)

    def resizeEvent(self, event) -> None:  # noqa: N802
        super().resizeEvent(event)
        self._label.setGeometry(self.rect())

    def load_from_files(self, sheet_path: str, meta_path: str = "") -> bool:
        """Load animation from sprite sheet image + metadata JSON."""
        sheet_file = Path(sheet_path)
        if not sheet_file.exists():
            return False

        sheet = QPixmap(str(sheet_file))
        if sheet.isNull():
            return False

        if meta_path:
            meta_file = Path(meta_path)
        else:
            meta_file = sheet_file.with_suffix(".json")

        meta: dict = {}
        if meta_file.exists():
            try:
                meta = json.loads(meta_file.read_text(encoding="utf-8"))
            except Exception:
                meta = {}

        frame_width = int(meta.get("frame_width", 0))
        frame_height = int(meta.get("frame_height", 0))
        frame_count = int(meta.get("frame_count", 0))
        columns = int(meta.get("columns", 0))
        rows = int(meta.get("rows", 0))
        padding = int(meta.get("padding", 0))

        if frame_width <= 0:
            frame_width = sheet.height() if sheet.height() > 0 else sheet.width()
        if frame_height <= 0:
            frame_height = sheet.height() if sheet.height() > 0 else frame_width
        if frame_count <= 0:
            frame_count = max(1, sheet.width() // max(1, frame_width))
        if columns <= 0:
            columns = max(1, frame_count)
        if rows <= 0:
            rows = max(1, (frame_count + columns - 1) // columns)

        self._sheet_pixmap = sheet
        self._frame_width = max(1, frame_width)
        self._frame_height = max(1, frame_height)
        self._frame_count = max(1, frame_count)
        self._columns = max(1, columns)
        self._rows = max(1, rows)
        self._padding = max(0, padding)

        self._fps = max(0.1, float(meta.get("default_fps", 10.0)))
        self._loop = bool(meta.get("loop", True))
        self._current_frame = 0
        self._playing = False
        self._timer.stop()
        self._frame_cache.clear()
        self._flipped_cache.clear()

        self._precut_frames()
        if not self._frame_cache:
            return False

        self._update_size()
        self._show_current_frame()
        return True

    def _precut_frames(self) -> None:
        """Cut all frames up front to avoid per-frame crop cost while playing."""
        if self._sheet_pixmap is None:
            return

        for idx in range(self._frame_count):
            col = idx % self._columns
            row = idx // self._columns
            x = col * (self._frame_width + self._padding)
            y = row * (self._frame_height + self._padding)
            rect = QRect(x, y, self._frame_width, self._frame_height)
            frame = self._sheet_pixmap.copy(rect)
            if frame.isNull():
                continue
            self._frame_cache[idx] = frame

    def play(self) -> None:
        if self._frame_count <= 1:
            self._show_current_frame()
            return
        self._playing = True
        self._timer.start(max(1, int(round(1000.0 / max(0.1, self._fps)))))

    def pause(self) -> None:
        self._playing = False
        self._timer.stop()

    def stop(self) -> None:
        self._playing = False
        self._timer.stop()
        self._current_frame = 0
        self._show_current_frame()

    def set_fps(self, fps: float) -> None:
        self._fps = max(0.1, float(fps))
        if self._playing:
            self._timer.setInterval(max(1, int(round(1000.0 / self._fps))))

    def set_frame(self, index: int) -> None:
        if self._frame_count <= 0:
            return
        self._current_frame = max(0, min(int(index), self._frame_count - 1))
        self._show_current_frame()

    def set_flipped(self, flipped: bool) -> None:
        flipped = bool(flipped)
        if self._flipped_h == flipped:
            return
        self._flipped_h = flipped
        self._show_current_frame()

    def set_scale(self, factor: float) -> None:
        self._scale_factor = max(0.1, float(factor))
        self._update_size()
        self._show_current_frame()

    @property
    def frame_size(self) -> QSize:
        return QSize(self._frame_width, self._frame_height)

    @property
    def fps(self) -> float:
        return self._fps

    @property
    def current_frame(self) -> int:
        return self._current_frame

    def _advance_frame(self) -> None:
        if self._frame_count <= 0:
            return

        next_frame = self._current_frame + 1
        if next_frame >= self._frame_count:
            if self._loop:
                next_frame = 0
                self.loop_completed.emit()
            else:
                self._timer.stop()
                self._playing = False
                self.animation_finished.emit()
                return

        self._current_frame = next_frame
        self._show_current_frame()
        self.frame_changed.emit(self._current_frame)

    def _show_current_frame(self) -> None:
        frame = self._get_display_frame(self._current_frame)
        if frame is None:
            self._label.clear()
            return

        output = frame
        if abs(self._scale_factor - 1.0) > 1e-6:
            target_w = max(1, int(round(frame.width() * self._scale_factor)))
            target_h = max(1, int(round(frame.height() * self._scale_factor)))
            output = frame.scaled(
                target_w,
                target_h,
                Qt.AspectRatioMode.KeepAspectRatio,
                Qt.TransformationMode.SmoothTransformation,
            )
        self._label.setPixmap(output)

    def _get_display_frame(self, index: int) -> QPixmap | None:
        base = self._frame_cache.get(index)
        if base is None:
            return None
        if not self._flipped_h:
            return base

        cached = self._flipped_cache.get(index)
        if cached is not None:
            return cached

        flipped = base.transformed(QTransform().scale(-1, 1))
        self._flipped_cache[index] = flipped
        return flipped

    def _update_size(self) -> None:
        width = max(1, int(round(self._frame_width * self._scale_factor)))
        height = max(1, int(round(self._frame_height * self._scale_factor)))
        self.setFixedSize(width, height)
        self._label.setFixedSize(width, height)
