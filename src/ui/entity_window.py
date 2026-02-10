from __future__ import annotations

from dataclasses import dataclass

from PySide6.QtCore import QEasingCurve, QPoint, QPropertyAnimation, QSequentialAnimationGroup, Qt, Signal
from PySide6.QtGui import QFont, QGuiApplication
from PySide6.QtWidgets import QLabel, QVBoxLayout, QWidget


@dataclass(slots=True)
class EntityPositions:
    hidden: int
    peeking: int
    full: int

    @staticmethod
    def calculate(
        *,
        screen_x: int,
        screen_width: int,
        window_width: int,
        margin: int = 50,
        edge: str = "right",
    ) -> "EntityPositions":
        if edge == "left":
            return EntityPositions(
                hidden=screen_x - window_width,
                peeking=screen_x - window_width + 100,
                full=screen_x + margin,
            )

        right = screen_x + screen_width
        return EntityPositions(
            hidden=right,
            peeking=right - 100,
            full=right - window_width - margin,
        )


class EntityWindow(QWidget):
    """
    Transparent top-most companion window that displays ASCII HTML.
    """

    flee_completed = Signal()
    peek_completed = Signal()
    enter_completed = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self._current_edge = "right"
        self._current_speed_scale = 1.0
        self._active_sequence: QSequentialAnimationGroup | None = None
        self._peek_animation: QPropertyAnimation | None = None
        self._enter_animation: QPropertyAnimation | None = None
        self._flee_animation: QPropertyAnimation | None = None
        self._last_positions: EntityPositions | None = None
        self._last_y: int = 0
        self._setup_window_flags()
        self._setup_ui()
        self._setup_animations()
        self.hide()

    def _setup_window_flags(self) -> None:
        self.setWindowFlags(
            Qt.WindowType.FramelessWindowHint
            | Qt.WindowType.WindowStaysOnTopHint
            | Qt.WindowType.Tool
            | Qt.WindowType.WindowDoesNotAcceptFocus
        )
        self.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground, True)
        self.setAttribute(Qt.WidgetAttribute.WA_ShowWithoutActivating, True)
        self.setAttribute(Qt.WidgetAttribute.WA_TransparentForMouseEvents, True)

    def _setup_ui(self) -> None:
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        self._label = QLabel(self)
        self._label.setTextFormat(Qt.TextFormat.RichText)
        self._label.setFont(QFont("Consolas", 8))
        self._label.setStyleSheet(
            "QLabel { background: transparent; color: white; padding: 0px; margin: 0px; }"
        )
        layout.addWidget(self._label)
        self.setMinimumSize(120, 60)

    def _setup_animations(self) -> None:
        self._flee_animation = None

    def set_ascii_content(self, html: str) -> None:
        self._label.setText(html)
        self._label.adjustSize()
        self.adjustSize()

    def peek(self, edge: str, y_position: int, script=None) -> None:
        screen = QGuiApplication.primaryScreen()
        if screen is None:
            return

        self._cancel_animations()
        self._current_edge = edge if edge in {"left", "right"} else "right"
        geometry = screen.availableGeometry()
        width = max(self.width(), 1)
        height = max(self.height(), 1)
        self._last_y = max(geometry.y(), min(y_position, geometry.y() + geometry.height() - height))
        self._last_positions = EntityPositions.calculate(
            screen_x=geometry.x(),
            screen_width=geometry.width(),
            window_width=width,
            margin=24,
            edge=self._current_edge,
        )

        speed_name = getattr(script, "anim_speed", "normal") if script is not None else "normal"
        self._current_speed_scale = {"slow": 1.2, "normal": 1.0, "fast": 0.8}.get(str(speed_name).lower(), 1.0)

        self.move(QPoint(self._last_positions.hidden, self._last_y))
        self.show()
        self.raise_()

        self._peek_animation = self._create_slide_animation(
            start_x=self._last_positions.hidden,
            end_x=self._last_positions.peeking,
            y=self._last_y,
            duration_ms=int(1500 * self._current_speed_scale),
            curve=QEasingCurve.Type.OutBack,
            parent=self,
        )
        self._peek_animation.finished.connect(self.peek_completed.emit)
        self._peek_animation.start()

    def enter(self, script=None) -> None:
        if self._last_positions is None:
            self._rebuild_cached_positions()
        if self._last_positions is None:
            return

        speed_name = getattr(script, "anim_speed", "normal") if script is not None else None
        if speed_name:
            self._current_speed_scale = {"slow": 1.2, "normal": 1.0, "fast": 0.8}.get(
                str(speed_name).lower(),
                self._current_speed_scale,
            )
        self._enter_animation = self._create_slide_animation(
            start_x=self.x(),
            end_x=self._last_positions.full,
            y=self.y(),
            duration_ms=int(800 * self._current_speed_scale),
            curve=QEasingCurve.Type.OutBounce,
            parent=self,
        )
        self._enter_animation.finished.connect(self.enter_completed.emit)
        self._enter_animation.start()

    def summon(self, edge: str, y_position: int, script=None) -> None:
        screen = QGuiApplication.primaryScreen()
        if screen is None:
            return

        self._cancel_animations()
        self._current_edge = edge if edge in {"left", "right"} else "right"

        geometry = screen.availableGeometry()
        width = max(self.width(), 1)
        height = max(self.height(), 1)
        y_clamped = max(geometry.y(), min(y_position, geometry.y() + geometry.height() - height))
        positions = EntityPositions.calculate(
            screen_x=geometry.x(),
            screen_width=geometry.width(),
            window_width=width,
            margin=24,
            edge=self._current_edge,
        )

        speed_name = getattr(script, "anim_speed", "normal") if script is not None else "normal"
        speed_scale = {"slow": 1.2, "normal": 1.0, "fast": 0.8}.get(str(speed_name).lower(), 1.0)
        self._current_speed_scale = speed_scale
        self._last_positions = positions
        self._last_y = y_clamped

        self.move(QPoint(positions.hidden, y_clamped))
        self.show()
        self.raise_()

        sequence = QSequentialAnimationGroup(self)

        peek_anim = self._create_slide_animation(
            start_x=positions.hidden,
            end_x=positions.peeking,
            y=y_clamped,
            duration_ms=int(1500 * speed_scale),
            curve=QEasingCurve.Type.OutBack,
            parent=sequence,
        )

        enter_anim = self._create_slide_animation(
            start_x=positions.peeking,
            end_x=positions.full,
            y=y_clamped,
            duration_ms=int(800 * speed_scale),
            curve=QEasingCurve.Type.OutBounce,
            parent=sequence,
        )

        sequence.addAnimation(peek_anim)
        sequence.addPause(int(2000 * speed_scale))
        sequence.addAnimation(enter_anim)
        self._active_sequence = sequence
        self._active_sequence.start()

    def flee(self) -> None:
        if not self.isVisible():
            if self._active_sequence:
                self._active_sequence.stop()
                self._active_sequence = None
            self.flee_completed.emit()
            return

        self._cancel_animations()
        screen = QGuiApplication.primaryScreen()
        if screen is None:
            self.hide()
            self.flee_completed.emit()
            return

        geometry = screen.availableGeometry()
        current = self.pos()
        width = max(self.width(), 1)
        positions = EntityPositions.calculate(
            screen_x=geometry.x(),
            screen_width=geometry.width(),
            window_width=width,
            margin=24,
            edge=self._current_edge,
        )
        self._flee_animation = self._create_slide_animation(
            start_x=current.x(),
            end_x=positions.hidden,
            y=current.y(),
            duration_ms=300,
            curve=QEasingCurve.Type.InExpo,
            parent=self,
        )
        self._flee_animation.finished.connect(self._on_flee_finished)
        self._flee_animation.start()

    def hide_now(self) -> None:
        self._cancel_animations()
        self.hide()

    def _create_slide_animation(
        self,
        *,
        start_x: int,
        end_x: int,
        y: int,
        duration_ms: int,
        curve: QEasingCurve.Type,
        parent,
    ) -> QPropertyAnimation:
        anim = QPropertyAnimation(self, b"pos", parent)
        anim.setDuration(max(1, duration_ms))
        anim.setStartValue(QPoint(start_x, y))
        anim.setEndValue(QPoint(end_x, y))
        anim.setEasingCurve(curve)
        return anim

    def _cancel_animations(self) -> None:
        if self._active_sequence is not None:
            self._active_sequence.stop()
            self._active_sequence.deleteLater()
            self._active_sequence = None
        if self._peek_animation is not None and self._peek_animation.state() == QPropertyAnimation.State.Running:
            self._peek_animation.stop()
        if self._enter_animation is not None and self._enter_animation.state() == QPropertyAnimation.State.Running:
            self._enter_animation.stop()
        if self._flee_animation is not None and self._flee_animation.state() == QPropertyAnimation.State.Running:
            self._flee_animation.stop()

    def _on_flee_finished(self) -> None:
        self.hide()
        self.flee_completed.emit()

    def _rebuild_cached_positions(self) -> None:
        screen = QGuiApplication.primaryScreen()
        if screen is None:
            self._last_positions = None
            return
        geometry = screen.availableGeometry()
        self._last_positions = EntityPositions.calculate(
            screen_x=geometry.x(),
            screen_width=geometry.width(),
            window_width=max(self.width(), 1),
            margin=24,
            edge=self._current_edge,
        )
