from __future__ import annotations

import random
from dataclasses import dataclass
from functools import partial
from pathlib import Path

from PySide6.QtCore import (
    QAbstractAnimation,
    QPauseAnimation,
    QEasingCurve,
    QFile,
    QPoint,
    QPropertyAnimation,
    QResource,
    QSequentialAnimationGroup,
    QTimer,
    Qt,
    Signal,
)
from PySide6.QtGui import QEnterEvent, QFont, QGuiApplication, QMouseEvent, QMovie, QPixmap
from PySide6.QtWidgets import QApplication, QLabel, QStackedLayout, QVBoxLayout, QWidget
try:
    from PySide6.QtStateMachine import QState, QStateMachine
except Exception:  # pragma: no cover
    QState = None  # type: ignore[assignment]
    QStateMachine = None  # type: ignore[assignment]


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

    STATE_SEMANTICS = {
        "state1": "curious_idle",
        "state2": "excited_click",
        "state3": "roaming_move",
        "state4": "shy_flee",
        "state5": "hover_thinking",
        "state6": "greeting_click",
        "state7": "ambient_probe",
    }
    STATE_ALIASES = {
        "idle": "state1",
        "curious": "state1",
        "excited": "state2",
        "roam": "state3",
        "moving": "state3",
        "flee": "state4",
        "shy": "state4",
        "hover": "state5",
        "thinking": "state5",
        "greeting": "state6",
        "happy": "state6",
        "ambient": "state7",
        "probe": "state7",
    }
    ROAM_INTERVAL_MS = (10_000, 20_000)
    PROBE_INTERVAL_MS = (14_000, 24_000)
    CLICK_RESTORE_MS = 3_000

    flee_completed = Signal()
    peek_completed = Signal()
    enter_completed = Signal()
    context_menu_requested = Signal(object)
    double_clicked = Signal()
    _to_hidden = Signal()
    _to_peeking = Signal()
    _to_engaged = Signal()
    _to_fleeing = Signal()
    _to_roaming = Signal()
    _to_probing = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self._current_edge = "right"
        self._current_speed_scale = 1.0
        self._movie: QMovie | None = None
        self._movie_cache: dict[str, QMovie] = {}
        self._movie_key: str | None = None
        self._active_sequence: QSequentialAnimationGroup | None = None
        self._peek_animation: QPropertyAnimation | None = None
        self._enter_animation: QPropertyAnimation | None = None
        self._flee_animation: QPropertyAnimation | None = None
        self._last_positions: EntityPositions | None = None
        self._last_y: int = 0

        self._state_asset_root = Path.cwd() / "characters"
        self._state_paths: dict[str, str] = {}
        self._base_state_name = "state1"
        self._rendered_state_name = ""
        self._hover_active = False
        self._click_override_state: str | None = None
        self._probe_state_name: str | None = None
        self._moving_active = False

        self._interactive_enabled: bool = True
        self._drag_active: bool = False
        self._drag_started: bool = False
        self._drag_offset = QPoint(0, 0)
        self._left_press_global = QPoint(0, 0)
        self._consume_release_for_double_click = False

        self._single_click_timer = QTimer(self)
        self._single_click_timer.setSingleShot(True)
        self._single_click_timer.timeout.connect(self._on_single_left_click)

        self._click_restore_timer = QTimer(self)
        self._click_restore_timer.setSingleShot(True)
        self._click_restore_timer.timeout.connect(self._on_click_restore_timeout)

        self._autonomous_enabled = False
        self._roam_timer = QTimer(self)
        self._roam_timer.setSingleShot(True)
        self._roam_timer.timeout.connect(self._on_roam_timeout)

        self._probe_timer = QTimer(self)
        self._probe_timer.setSingleShot(True)
        self._probe_timer.timeout.connect(self._on_probe_timeout)

        self._move_animation: QPropertyAnimation | None = None
        self._probe_sequence: QSequentialAnimationGroup | None = None
        self._anim_phase = "hidden"
        self._anim_state_machine: QStateMachine | None = None
        self._summon_sequence: QSequentialAnimationGroup | None = None
        self._summon_peek_animation: QPropertyAnimation | None = None
        self._summon_pause: QPauseAnimation | None = None
        self._summon_enter_animation: QPropertyAnimation | None = None

        self._setup_window_flags()
        self._setup_ui()
        self._setup_animations()
        self._setup_animation_state_machine()
        self.set_state_asset_root(self._state_asset_root)
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
        self.setAttribute(Qt.WidgetAttribute.WA_TransparentForMouseEvents, False)

    def _setup_ui(self) -> None:
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        self._label = QLabel(self)
        self._label.setTextFormat(Qt.TextFormat.RichText)
        self._label.setFont(QFont("Consolas", 8))
        self._label.setStyleSheet(
            "QLabel { background: transparent; color: white; padding: 0px; margin: 0px; }"
        )
        self._sprite_label = QLabel(self)
        self._sprite_label.setStyleSheet(
            "QLabel { background: transparent; padding: 0px; margin: 0px; }"
        )
        self._sprite_label.setAlignment(Qt.AlignmentFlag.AlignCenter)

        self._content_stack = QStackedLayout()
        self._content_stack.setContentsMargins(0, 0, 0, 0)
        self._content_stack.addWidget(self._label)
        self._content_stack.addWidget(self._sprite_label)

        layout.addLayout(self._content_stack)
        self.setMinimumSize(120, 60)

    def _setup_animations(self) -> None:
        self._flee_animation = None
        self._ensure_summon_sequence()

    def _ensure_summon_sequence(self) -> None:
        if self._summon_sequence is not None:
            return
        sequence = QSequentialAnimationGroup(self)
        peek = self._create_slide_animation(
            start_x=0,
            end_x=0,
            y=0,
            duration_ms=1000,
            curve=QEasingCurve.Type.OutCubic,
            parent=sequence,
        )
        pause = QPauseAnimation(600, sequence)
        enter = self._create_slide_animation(
            start_x=0,
            end_x=0,
            y=0,
            duration_ms=650,
            curve=QEasingCurve.Type.OutCubic,
            parent=sequence,
        )
        sequence.addAnimation(peek)
        sequence.addAnimation(pause)
        sequence.addAnimation(enter)
        sequence.finished.connect(self._on_summon_sequence_finished)

        self._summon_sequence = sequence
        self._summon_peek_animation = peek
        self._summon_pause = pause
        self._summon_enter_animation = enter

    def _setup_animation_state_machine(self) -> None:
        if QStateMachine is None or QState is None:
            self._anim_state_machine = None
            self._anim_phase = "hidden"
            return

        machine = QStateMachine(self)
        hidden = QState(machine)
        peeking = QState(machine)
        engaged = QState(machine)
        fleeing = QState(machine)
        roaming = QState(machine)
        probing = QState(machine)
        states = [hidden, peeking, engaged, fleeing, roaming, probing]

        for src in states:
            src.addTransition(self._to_hidden, hidden)
            src.addTransition(self._to_peeking, peeking)
            src.addTransition(self._to_engaged, engaged)
            src.addTransition(self._to_fleeing, fleeing)
            src.addTransition(self._to_roaming, roaming)
            src.addTransition(self._to_probing, probing)

        hidden.entered.connect(partial(self._mark_anim_phase, "hidden"))
        peeking.entered.connect(partial(self._mark_anim_phase, "peeking"))
        engaged.entered.connect(partial(self._mark_anim_phase, "engaged"))
        fleeing.entered.connect(partial(self._mark_anim_phase, "fleeing"))
        roaming.entered.connect(partial(self._mark_anim_phase, "roaming"))
        probing.entered.connect(partial(self._mark_anim_phase, "probing"))

        machine.setInitialState(hidden)
        machine.start()

        self._anim_state_machine = machine
        self._anim_phase = "hidden"

    def _mark_anim_phase(self, phase: str) -> None:
        self._anim_phase = phase

    def _transition_anim_phase(self, phase: str) -> None:
        if phase == self._anim_phase:
            return
        if phase == "hidden":
            self._to_hidden.emit()
        elif phase == "peeking":
            self._to_peeking.emit()
        elif phase == "engaged":
            self._to_engaged.emit()
        elif phase == "fleeing":
            self._to_fleeing.emit()
        elif phase == "roaming":
            self._to_roaming.emit()
        elif phase == "probing":
            self._to_probing.emit()

    def set_state_asset_root(self, root_dir: Path | str) -> None:
        self._state_asset_root = Path(root_dir) if not str(root_dir).startswith(":/") else Path.cwd() / "characters"
        self._state_paths = {}
        root_text = str(root_dir).rstrip("/\\")
        for index in range(1, 8):
            state_name = f"state{index}"
            if root_text.startswith(":/"):
                candidate = f"{root_text}/{state_name}.gif"
            else:
                candidate = str((self._state_asset_root / f"{state_name}.gif").resolve())
            if self._path_exists(candidate):
                self._state_paths[state_name] = candidate
        self._refresh_state_visual()

    def configure_state_assets(self, state_assets: dict[str, Path | str]) -> None:
        normalized: dict[str, str] = {}
        for key, value in state_assets.items():
            state_name = self._normalize_state_name(key)
            if state_name is None:
                continue
            raw = str(value).strip()
            if not raw:
                continue
            candidate = raw if raw.startswith(":/") else str(Path(raw).resolve())
            if self._path_exists(candidate):
                normalized[state_name] = candidate
        if normalized:
            self._state_paths = normalized
            self._refresh_state_visual()

    def preload_state_movies(self) -> None:
        for path in self._state_paths.values():
            self._get_or_create_movie(path)

    def set_state_by_name(self, state_name: str, *, as_base: bool = True) -> bool:
        normalized = self._normalize_state_name(state_name)
        if normalized is None:
            return False
        if as_base:
            self._base_state_name = normalized
        return self._refresh_state_visual()

    def set_behavior_state(self, state_name: str, *, as_base: bool = True) -> bool:
        return self.set_state_by_name(state_name, as_base=as_base)

    def set_autonomous_enabled(self, enabled: bool) -> None:
        self._autonomous_enabled = bool(enabled)
        if not self._autonomous_enabled:
            self._stop_autonomy_timers()
            self._stop_move_animation()
            self._stop_probe_animation()
            self._moving_active = False
            self._probe_state_name = None
            self._refresh_state_visual()
            return
        self._schedule_autonomy_timers()

    def set_interactive(self, enabled: bool) -> None:
        self._interactive_enabled = bool(enabled)
        self.setAttribute(Qt.WidgetAttribute.WA_TransparentForMouseEvents, not self._interactive_enabled)

    def set_ascii_content(self, html: str) -> None:
        self._stop_movie()
        self._content_stack.setCurrentWidget(self._label)
        self._label.setText(html)
        self._label.adjustSize()
        self.adjustSize()

    def set_sprite_content(self, sprite_path: Path | str) -> None:
        raw = str(sprite_path).strip()
        if not raw:
            raise FileNotFoundError("Sprite not found: <empty>")
        candidate = raw if raw.startswith(":/") else str(Path(raw).resolve())
        if not self._path_exists(candidate):
            raise FileNotFoundError(f"Sprite not found: {candidate}")

        suffix = Path(candidate).suffix.lower()
        self._content_stack.setCurrentWidget(self._sprite_label)
        if suffix == ".gif":
            movie = self._get_or_create_movie(candidate)
            if movie is None:
                raise ValueError(f"Invalid GIF: {candidate}")
            self._activate_movie(movie, movie_key=candidate)
            self._sprite_label.adjustSize()
            self.adjustSize()
            return

        pixmap = QPixmap(candidate)
        if pixmap.isNull():
            raise ValueError(f"Invalid image: {candidate}")
        self._stop_movie()
        self._sprite_label.setPixmap(pixmap)
        self._sprite_label.adjustSize()
        self.adjustSize()

    @staticmethod
    def _path_exists(path_text: str) -> bool:
        if path_text.startswith(":/"):
            return QFile.exists(path_text) or QResource(path_text).isValid()
        return Path(path_text).exists()

    def _get_or_create_movie(self, path: Path | str) -> QMovie | None:
        key = str(path).strip()
        if not key:
            return None
        if not key.startswith(":/"):
            key = str(Path(key).resolve())
        cached = self._movie_cache.get(key)
        if cached is not None and cached.isValid():
            return cached
        movie = QMovie(key)
        if not movie.isValid():
            return None
        movie.setCacheMode(QMovie.CacheMode.CacheAll)
        self._movie_cache[key] = movie
        return movie

    def _activate_movie(self, movie: QMovie, *, movie_key: str) -> None:
        if self._movie is movie and self._movie_key == movie_key and self._sprite_label.movie() is movie:
            if movie.state() != QMovie.MovieState.Running:
                movie.start()
            return
        if self._movie is not None and self._movie is not movie:
            self._movie.stop()
        self._sprite_label.setMovie(movie)
        self._movie = movie
        self._movie_key = movie_key
        if movie.state() != QMovie.MovieState.Running:
            movie.start()

    def _stop_movie(self) -> None:
        if self._movie is None:
            return
        self._movie.stop()
        self._sprite_label.setMovie(None)
        self._movie = None
        self._movie_key = None

    def _normalize_state_name(self, state_name: str) -> str | None:
        key = (state_name or "").strip().lower()
        if key in self.STATE_SEMANTICS:
            return key
        return self.STATE_ALIASES.get(key)

    def _compose_target_state(self) -> str:
        if self._moving_active:
            return "state3"
        if self._click_override_state is not None:
            return self._click_override_state
        if self._hover_active:
            return "state5"
        if self._probe_state_name is not None:
            return self._probe_state_name
        return self._base_state_name

    def _refresh_state_visual(self) -> bool:
        state_name = self._normalize_state_name(self._compose_target_state())
        if state_name is None:
            return False
        path = self._state_paths.get(state_name)
        if path is None:
            return False
        movie = self._get_or_create_movie(path)
        if movie is None:
            return False
        self._content_stack.setCurrentWidget(self._sprite_label)
        self._activate_movie(movie, movie_key=str(path))
        self._sprite_label.adjustSize()
        self.adjustSize()
        self._rendered_state_name = state_name
        return True

    def peek(self, edge: str, y_position: int, script=None) -> None:
        screen = QGuiApplication.primaryScreen()
        if screen is None:
            return

        self._transition_anim_phase("peeking")
        self._cancel_animations()
        self._stop_autonomy_timers()
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
            duration_ms=int(1000 * self._current_speed_scale),
            curve=QEasingCurve.Type.OutCubic,
            parent=self,
        )
        self._peek_animation.finished.connect(self._on_peek_finished)
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
            duration_ms=int(650 * self._current_speed_scale),
            curve=QEasingCurve.Type.OutCubic,
            parent=self,
        )
        self._enter_animation.finished.connect(self._on_enter_finished)
        self._enter_animation.start()

    def summon(self, edge: str, y_position: int, script=None) -> None:
        screen = QGuiApplication.primaryScreen()
        if screen is None:
            return

        self._transition_anim_phase("peeking")
        self._cancel_animations()
        self._stop_autonomy_timers()
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

        self._ensure_summon_sequence()
        if (
            self._summon_sequence is None
            or self._summon_peek_animation is None
            or self._summon_pause is None
            or self._summon_enter_animation is None
        ):
            return

        self._summon_sequence.stop()
        self._summon_peek_animation.setStartValue(QPoint(positions.hidden, y_clamped))
        self._summon_peek_animation.setEndValue(QPoint(positions.peeking, y_clamped))
        self._summon_peek_animation.setDuration(max(1, int(1000 * speed_scale)))
        self._summon_pause.setDuration(max(1, int(600 * speed_scale)))
        self._summon_enter_animation.setStartValue(QPoint(positions.peeking, y_clamped))
        self._summon_enter_animation.setEndValue(QPoint(positions.full, y_clamped))
        self._summon_enter_animation.setDuration(max(1, int(650 * speed_scale)))

        self._active_sequence = self._summon_sequence
        self._active_sequence.start()

    def flee(self) -> None:
        self._transition_anim_phase("fleeing")
        if not self.isVisible():
            if self._active_sequence:
                self._active_sequence.stop()
                self._active_sequence = None
            self.flee_completed.emit()
            return

        self._cancel_animations()
        self._stop_autonomy_timers()
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
        self._transition_anim_phase("hidden")
        self._cancel_animations()
        self._stop_autonomy_timers()
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
            if self._active_sequence is not self._summon_sequence:
                self._active_sequence.deleteLater()
            self._active_sequence = None
        if self._peek_animation is not None and self._peek_animation.state() == QPropertyAnimation.State.Running:
            self._peek_animation.stop()
        if self._enter_animation is not None and self._enter_animation.state() == QPropertyAnimation.State.Running:
            self._enter_animation.stop()
        if self._flee_animation is not None and self._flee_animation.state() == QPropertyAnimation.State.Running:
            self._flee_animation.stop()
        self._stop_move_animation()
        self._stop_probe_animation()

    def _on_flee_finished(self) -> None:
        self._transition_anim_phase("hidden")
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

    def _on_peek_finished(self) -> None:
        self._transition_anim_phase("peeking")
        self.peek_completed.emit()
        self._schedule_autonomy_timers()

    def _on_enter_finished(self) -> None:
        self._transition_anim_phase("engaged")
        self.enter_completed.emit()
        self._schedule_autonomy_timers()

    def _on_summon_sequence_finished(self) -> None:
        if self._active_sequence is self._summon_sequence:
            self._active_sequence = None
        self._transition_anim_phase("engaged")
        self._schedule_autonomy_timers()

    def _double_click_interval(self) -> int:
        app = QApplication.instance()
        if app is None:
            return 250
        return max(1, int(app.doubleClickInterval()))

    def _drag_threshold(self) -> int:
        app = QApplication.instance()
        if app is None:
            return 10
        return max(1, int(app.startDragDistance()))

    def _available_geometry(self):
        screen = QGuiApplication.primaryScreen()
        if screen is None:
            return None
        return screen.availableGeometry()

    def _clamp_point_to_screen(self, point: QPoint, *, visible_x_ratio: float = 1.0, visible_y_ratio: float = 1.0) -> QPoint:
        geometry = self._available_geometry()
        if geometry is None:
            return point

        width = max(self.width(), 1)
        height = max(self.height(), 1)
        visible_width = max(1, min(width, int(width * max(0.05, visible_x_ratio))))
        visible_height = max(1, min(height, int(height * max(0.05, visible_y_ratio))))

        min_x = geometry.x() - (width - visible_width)
        max_x = geometry.x() + geometry.width() - visible_width
        min_y = geometry.y() - (height - visible_height)
        max_y = geometry.y() + geometry.height() - visible_height
        if max_x < min_x:
            max_x = min_x
        if max_y < min_y:
            max_y = min_y

        return QPoint(
            max(min_x, min(point.x(), max_x)),
            max(min_y, min(point.y(), max_y)),
        )

    def _is_animation_running(self, animation) -> bool:
        return animation is not None and animation.state() == QAbstractAnimation.State.Running

    def _core_animation_running(self) -> bool:
        return any(
            [
                self._is_animation_running(self._peek_animation),
                self._is_animation_running(self._enter_animation),
                self._is_animation_running(self._flee_animation),
                self._is_animation_running(self._active_sequence),
            ]
        )

    def _schedule_autonomy_timers(self) -> None:
        if not self._autonomous_enabled or not self.isVisible():
            return
        if not self._roam_timer.isActive():
            self._roam_timer.start(random.randint(*self.ROAM_INTERVAL_MS))
        if not self._probe_timer.isActive():
            self._probe_timer.start(random.randint(*self.PROBE_INTERVAL_MS))

    def _stop_autonomy_timers(self) -> None:
        if self._roam_timer.isActive():
            self._roam_timer.stop()
        if self._probe_timer.isActive():
            self._probe_timer.stop()

    def _can_start_autonomous_action(self) -> bool:
        return (
            self._autonomous_enabled
            and self.isVisible()
            and not self._drag_active
            and not self._core_animation_running()
            and not self._is_animation_running(self._move_animation)
            and not self._is_animation_running(self._probe_sequence)
        )

    def _on_roam_timeout(self) -> None:
        if not self._can_start_autonomous_action():
            self._schedule_autonomy_timers()
            return
        geometry = self._available_geometry()
        if geometry is None:
            self._schedule_autonomy_timers()
            return

        start = self._clamp_point_to_screen(self.pos(), visible_x_ratio=1.0, visible_y_ratio=1.0)
        self.move(start)

        width = max(self.width(), 1)
        height = max(self.height(), 1)
        max_x = geometry.x() + geometry.width() - width
        max_y = geometry.y() + geometry.height() - height
        if max_x < geometry.x() or max_y < geometry.y():
            self._schedule_autonomy_timers()
            return

        target = QPoint(random.randint(geometry.x(), max_x), random.randint(geometry.y(), max_y))
        if (target - start).manhattanLength() < 40:
            target = QPoint(random.randint(geometry.x(), max_x), random.randint(geometry.y(), max_y))

        anim = QPropertyAnimation(self, b"pos", self)
        anim.setDuration(random.randint(1_600, 3_200))
        anim.setStartValue(start)
        anim.setEndValue(target)
        anim.setEasingCurve(QEasingCurve.Type.InOutCubic)
        anim.finished.connect(self._on_roam_finished)
        self._move_animation = anim
        self._moving_active = True
        self._transition_anim_phase("roaming")
        self._refresh_state_visual()
        anim.start()

    def _on_roam_finished(self) -> None:
        self._moving_active = False
        self._move_animation = None
        self._transition_anim_phase("engaged")
        self.move(self._clamp_point_to_screen(self.pos(), visible_x_ratio=1.0, visible_y_ratio=1.0))
        self._refresh_state_visual()
        self._schedule_autonomy_timers()

    def _stop_move_animation(self) -> None:
        if self._move_animation is not None and self._is_animation_running(self._move_animation):
            self._move_animation.stop()
        self._move_animation = None
        self._moving_active = False

    def _on_probe_timeout(self) -> None:
        if not self._can_start_autonomous_action():
            self._schedule_autonomy_timers()
            return

        geometry = self._available_geometry()
        if geometry is None:
            self._schedule_autonomy_timers()
            return

        origin = self._clamp_point_to_screen(self.pos(), visible_x_ratio=1.0, visible_y_ratio=1.0)
        self.move(origin)
        width = max(self.width(), 1)
        height = max(self.height(), 1)
        max_x = geometry.x() + geometry.width() - width
        max_y = geometry.y() + geometry.height() - height
        if max_x < geometry.x() or max_y < geometry.y():
            self._schedule_autonomy_timers()
            return

        edge = random.choice(("left", "right", "top"))
        if edge == "left":
            target = QPoint(geometry.x() - width // 2, random.randint(geometry.y(), max_y))
            target = self._clamp_point_to_screen(target, visible_x_ratio=0.5, visible_y_ratio=1.0)
        elif edge == "right":
            target = QPoint(geometry.x() + geometry.width() - width // 2, random.randint(geometry.y(), max_y))
            target = self._clamp_point_to_screen(target, visible_x_ratio=0.5, visible_y_ratio=1.0)
        else:
            target = QPoint(random.randint(geometry.x(), max_x), geometry.y() - height // 2)
            target = self._clamp_point_to_screen(target, visible_x_ratio=1.0, visible_y_ratio=0.5)

        sequence = QSequentialAnimationGroup(self)
        out_anim = QPropertyAnimation(self, b"pos", sequence)
        out_anim.setDuration(random.randint(700, 1_200))
        out_anim.setStartValue(origin)
        out_anim.setEndValue(target)
        out_anim.setEasingCurve(QEasingCurve.Type.OutCubic)

        back_anim = QPropertyAnimation(self, b"pos", sequence)
        back_anim.setDuration(random.randint(800, 1_300))
        back_anim.setStartValue(target)
        back_anim.setEndValue(origin)
        back_anim.setEasingCurve(QEasingCurve.Type.InOutCubic)

        sequence.addAnimation(out_anim)
        sequence.addPause(random.randint(400, 900))
        sequence.addAnimation(back_anim)
        sequence.finished.connect(self._on_probe_finished)

        self._probe_sequence = sequence
        self._probe_state_name = random.choice(("state1", "state7"))
        self._transition_anim_phase("probing")
        self._refresh_state_visual()
        sequence.start()

    def _on_probe_finished(self) -> None:
        self._probe_sequence = None
        self._probe_state_name = None
        self._transition_anim_phase("engaged")
        self.move(self._clamp_point_to_screen(self.pos(), visible_x_ratio=1.0, visible_y_ratio=1.0))
        self._refresh_state_visual()
        self._schedule_autonomy_timers()

    def _stop_probe_animation(self) -> None:
        if self._probe_sequence is not None and self._is_animation_running(self._probe_sequence):
            self._probe_sequence.stop()
        self._probe_sequence = None
        self._probe_state_name = None
        if self.isVisible():
            self.move(self._clamp_point_to_screen(self.pos(), visible_x_ratio=1.0, visible_y_ratio=1.0))

    def _on_single_left_click(self) -> None:
        self._base_state_name = "state1"
        self._click_override_state = random.choice(("state2", "state6"))
        self._refresh_state_visual()
        self._click_restore_timer.start(self.CLICK_RESTORE_MS)

    def _on_click_restore_timeout(self) -> None:
        self._click_override_state = None
        self._refresh_state_visual()

    def enterEvent(self, event: QEnterEvent) -> None:
        if self._interactive_enabled:
            self._hover_active = True
            self._refresh_state_visual()
        super().enterEvent(event)

    def leaveEvent(self, event) -> None:
        self._hover_active = False
        self._refresh_state_visual()
        super().leaveEvent(event)

    def mousePressEvent(self, event: QMouseEvent) -> None:
        if not self._interactive_enabled:
            event.ignore()
            return
        if event.button() == Qt.MouseButton.LeftButton:
            self._drag_active = True
            self._drag_started = False
            self._drag_offset = event.globalPosition().toPoint() - self.frameGeometry().topLeft()
            self._left_press_global = event.globalPosition().toPoint()
            event.accept()
            return
        if event.button() == Qt.MouseButton.RightButton:
            self.context_menu_requested.emit(event.globalPosition().toPoint())
            event.accept()
            return
        super().mousePressEvent(event)

    def mouseMoveEvent(self, event: QMouseEvent) -> None:
        if not self._interactive_enabled:
            event.ignore()
            return
        if self._drag_active and (event.buttons() & Qt.MouseButton.LeftButton):
            global_point = event.globalPosition().toPoint()
            if (global_point - self._left_press_global).manhattanLength() >= self._drag_threshold():
                self._drag_started = True
            if self._drag_started:
                self._stop_autonomy_timers()
                self._stop_move_animation()
                self._stop_probe_animation()
                self.move(
                    self._clamp_point_to_screen(
                        global_point - self._drag_offset,
                        visible_x_ratio=1.0,
                        visible_y_ratio=1.0,
                    )
                )
                event.accept()
                return
        super().mouseMoveEvent(event)

    def mouseReleaseEvent(self, event: QMouseEvent) -> None:
        if event.button() == Qt.MouseButton.LeftButton:
            was_drag = self._drag_started
            self._drag_active = False
            self._drag_started = False
            if self._consume_release_for_double_click:
                self._consume_release_for_double_click = False
                event.accept()
                return
            if not was_drag:
                self._single_click_timer.start(self._double_click_interval())
            else:
                self._schedule_autonomy_timers()
            event.accept()
            return
        super().mouseReleaseEvent(event)

    def mouseDoubleClickEvent(self, event: QMouseEvent) -> None:
        if not self._interactive_enabled:
            event.ignore()
            return
        if event.button() == Qt.MouseButton.LeftButton:
            self._consume_release_for_double_click = True
            if self._single_click_timer.isActive():
                self._single_click_timer.stop()
            self.double_clicked.emit()
            event.accept()
            return
        super().mouseDoubleClickEvent(event)

    def showEvent(self, event) -> None:
        if self._anim_phase == "hidden":
            self._transition_anim_phase("engaged")
        self._refresh_state_visual()
        self._schedule_autonomy_timers()
        super().showEvent(event)

    def hideEvent(self, event) -> None:
        self._transition_anim_phase("hidden")
        self._stop_autonomy_timers()
        self._stop_move_animation()
        self._stop_probe_animation()
        if self._single_click_timer.isActive():
            self._single_click_timer.stop()
        if self._click_restore_timer.isActive():
            self._click_restore_timer.stop()
        self._hover_active = False
        self._click_override_state = None
        self._probe_state_name = None
        self._moving_active = False
        super().hideEvent(event)
