"""
GIF Particle System.

Manages multiple independent GIF windows ("particles") that can appear
from screen edges and animate towards the center area.

Each particle is a lightweight transparent window displaying a GIF,
with its own entrance/exit animation.
"""

from __future__ import annotations

import random
from dataclasses import dataclass
import json
from pathlib import Path

from PySide6.QtCore import (
    QEasingCurve,
    QPoint,
    QPropertyAnimation,
    QSequentialAnimationGroup,
    QTimer,
    QVariantAnimation,
    Qt,
    Signal,
)
from PySide6.QtGui import QGuiApplication, QMovie
from PySide6.QtWidgets import QLabel, QWidget

try:
    from ui._file_helpers import normalize_asset_path, path_exists
except ModuleNotFoundError:
    from ._file_helpers import normalize_asset_path, path_exists


@dataclass
class ParticleConfig:
    """Configuration for a GIF particle."""
    gif_path: str
    scale: float = 1.0               # Size scale factor
    duration_ms: int = 5000           # How long particle stays visible
    enter_duration_ms: int = 1200     # Entry animation duration
    exit_duration_ms: int = 800       # Exit animation duration
    edge: str = "random"              # "top", "bottom", "left", "right", "random"
    target: str = "center"            # "center", "random_inner"
    loop_gif: bool = True
    opacity: float = 1.0


class GifParticle(QWidget):
    """
    A single animated GIF window that enters from a screen edge.

    Lifecycle: spawn -> enter animation -> linger -> exit animation -> destroy
    """

    finished = Signal(object)  # self

    def __init__(
        self,
        config: ParticleConfig,
        particle_id: int,
        parent: QWidget | None = None,
    ):
        super().__init__(parent)
        self._config = config
        self._particle_id = particle_id
        self._movie: QMovie | None = None
        self._enter_anim: QPropertyAnimation | None = None
        self._exit_anim: QPropertyAnimation | None = None
        self._linger_timer = QTimer(self)
        self._linger_timer.setSingleShot(True)
        self._linger_timer.timeout.connect(self._start_exit)

        self._setup_window()
        self._setup_gif()

    @property
    def particle_id(self) -> int:
        return self._particle_id

    def _setup_window(self) -> None:
        self.setWindowFlags(
            Qt.WindowType.FramelessWindowHint
            | Qt.WindowType.WindowStaysOnTopHint
            | Qt.WindowType.Tool
            | Qt.WindowType.WindowDoesNotAcceptFocus
        )
        self.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground, True)
        self.setAttribute(Qt.WidgetAttribute.WA_ShowWithoutActivating, True)
        self.setAttribute(Qt.WidgetAttribute.WA_TransparentForMouseEvents, True)

        if self._config.opacity < 1.0:
            self.setWindowOpacity(self._config.opacity)

    def _setup_gif(self) -> None:
        source = normalize_asset_path(self._config.gif_path)
        if not path_exists(source):
            print(f"[GifParticle] GIF 文件未找到: {self._config.gif_path}")
            return

        label = QLabel(self)
        label.setStyleSheet("QLabel { background: transparent; }")
        label.setAlignment(Qt.AlignmentFlag.AlignCenter)

        movie = QMovie(source)
        if not movie.isValid():
            print(f"[GifParticle] 无效的 GIF: {source}")
            return

        movie.setCacheMode(QMovie.CacheMode.CacheAll)

        # Apply scale
        if self._config.scale != 1.0:
            movie.setScaledSize(
                movie.currentImage().size() * self._config.scale
            )

        label.setMovie(movie)
        self._movie = movie
        movie.start()
        label.adjustSize()
        self.setFixedSize(label.size())

    def spawn(self) -> None:
        """Start the particle lifecycle."""
        screen = QGuiApplication.primaryScreen()
        if screen is None or self._movie is None:
            self._cleanup()
            return

        geometry = screen.availableGeometry()
        w = self.width()
        h = self.height()

        # Determine edge
        edge = self._config.edge
        if edge == "random":
            edge = random.choice(["top", "bottom", "left", "right"])

        # Calculate start (off-screen) and end (inner area) positions
        start_pos, end_pos = self._calculate_positions(
            geometry, w, h, edge, self._config.target
        )

        self.move(start_pos)
        self.show()
        self.raise_()

        # Enter animation
        self._enter_anim = QPropertyAnimation(self, b"pos", self)
        self._enter_anim.setDuration(self._config.enter_duration_ms)
        self._enter_anim.setStartValue(start_pos)
        self._enter_anim.setEndValue(end_pos)
        self._enter_anim.setEasingCurve(QEasingCurve.Type.OutCubic)
        self._enter_anim.finished.connect(self._on_enter_finished)
        self._enter_anim.start()

    def _calculate_positions(
        self,
        geometry,
        w: int,
        h: int,
        edge: str,
        target: str,
    ) -> tuple[QPoint, QPoint]:
        """Calculate start and end positions for a particle."""
        sx, sy = geometry.x(), geometry.y()
        sw, sh = geometry.width(), geometry.height()

        # Generate a target position in the inner area
        if target == "center":
            # Roughly center area with some randomness
            margin_x = int(sw * 0.15)
            margin_y = int(sh * 0.15)
            tx = sx + random.randint(margin_x, sw - margin_x - w)
            ty = sy + random.randint(margin_y, sh - margin_y - h)
        else:  # random_inner
            margin_x = int(sw * 0.05)
            margin_y = int(sw * 0.05)
            tx = sx + random.randint(margin_x, max(margin_x + 1, sw - margin_x - w))
            ty = sy + random.randint(margin_y, max(margin_y + 1, sh - margin_y - h))

        end_pos = QPoint(tx, ty)

        # Start position – off-screen at the chosen edge
        if edge == "top":
            start_pos = QPoint(tx, sy - h - 20)
        elif edge == "bottom":
            start_pos = QPoint(tx, sy + sh + 20)
        elif edge == "left":
            start_pos = QPoint(sx - w - 20, ty)
        else:  # right
            start_pos = QPoint(sx + sw + 20, ty)

        return start_pos, end_pos

    def _on_enter_finished(self) -> None:
        """Entry animation done – start linger timer."""
        self._linger_timer.start(self._config.duration_ms)

    def _start_exit(self) -> None:
        """Begin exit animation – slide back towards edge."""
        screen = QGuiApplication.primaryScreen()
        if screen is None:
            self._cleanup()
            return

        geometry = screen.availableGeometry()
        current = self.pos()

        # Exit towards the nearest edge
        distances = {
            "left": current.x() - geometry.x(),
            "right": (geometry.x() + geometry.width()) - (current.x() + self.width()),
            "top": current.y() - geometry.y(),
            "bottom": (geometry.y() + geometry.height()) - (current.y() + self.height()),
        }
        nearest_edge = min(distances, key=distances.get)

        if nearest_edge == "left":
            exit_pos = QPoint(geometry.x() - self.width() - 20, current.y())
        elif nearest_edge == "right":
            exit_pos = QPoint(geometry.x() + geometry.width() + 20, current.y())
        elif nearest_edge == "top":
            exit_pos = QPoint(current.x(), geometry.y() - self.height() - 20)
        else:
            exit_pos = QPoint(current.x(), geometry.y() + geometry.height() + 20)

        self._exit_anim = QPropertyAnimation(self, b"pos", self)
        self._exit_anim.setDuration(self._config.exit_duration_ms)
        self._exit_anim.setStartValue(current)
        self._exit_anim.setEndValue(exit_pos)
        self._exit_anim.setEasingCurve(QEasingCurve.Type.InCubic)
        self._exit_anim.finished.connect(self._cleanup)
        self._exit_anim.start()

    def _cleanup(self) -> None:
        """Stop everything and signal completion."""
        if self._movie:
            self._movie.stop()
        self._linger_timer.stop()
        self.hide()
        self.finished.emit(self)
        self.deleteLater()

    def force_dismiss(self) -> None:
        """Immediately dismiss this particle."""
        self._linger_timer.stop()
        if self._enter_anim and self._enter_anim.state() == QPropertyAnimation.State.Running:
            self._enter_anim.stop()
        self._cleanup()


class TrajectoryPlayer(QWidget):
    """
    A specialized particle that follows a recorded trajectory
    and switches GIFs based on state triggers.
    """
    finished = Signal(object)  # self

    MIN_STATE_SWITCH_INTERVAL_S = 0.05

    def __init__(self, trajectory_data: dict, gif_map: dict[int, str], parent: QWidget | None = None):
        super().__init__(parent)
        self._points = self._sanitize_points(self._extract_points(trajectory_data))
        self._total_duration = self._resolve_total_duration(trajectory_data, self._points)
        self._gif_map = gif_map
        self._current_index = 0
        self._last_state_switch_elapsed = -999.0
        self._stopped = True

        self._label = QLabel(self)
        self._label.setStyleSheet("background: transparent;")
        self._label.setAlignment(Qt.AlignmentFlag.AlignCenter)

        self._movie_cache: dict[int, QMovie] = {}
        self._base_size = None
        self._current_movie: QMovie | None = None
        self._current_state_id = -1

        self._timeline = QVariantAnimation(self)
        self._timeline.setStartValue(0.0)
        self._timeline.setEndValue(1.0)
        self._timeline.setEasingCurve(QEasingCurve.Type.Linear)
        self._timeline.valueChanged.connect(self._on_timeline_value_changed)
        self._timeline.finished.connect(self._on_timeline_finished)

        self._setup_window()

    def _setup_window(self) -> None:
        self.setWindowFlags(
            Qt.WindowType.FramelessWindowHint
            | Qt.WindowType.WindowStaysOnTopHint
            | Qt.WindowType.Tool
            | Qt.WindowType.WindowDoesNotAcceptFocus
        )
        self.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground, True)
        self.setAttribute(Qt.WidgetAttribute.WA_ShowWithoutActivating, True)
        self.setAttribute(Qt.WidgetAttribute.WA_TransparentForMouseEvents, True)

    @property
    def particle_id(self) -> int:
        return id(self)

    @staticmethod
    def _extract_points(trajectory_data: dict) -> list[dict]:
        raw_points = trajectory_data.get("points")
        if isinstance(raw_points, list):
            return raw_points

        # Also support Qt timeline schema generated by tooling.
        raw_keyframes = trajectory_data.get("keyframes")
        if not isinstance(raw_keyframes, list):
            return []

        converted: list[dict] = []
        for frame in raw_keyframes:
            try:
                x = frame["x"]
                y = frame["y"]
                s = frame.get("state", 1)
                if "time_ms" in frame:
                    t = float(frame["time_ms"]) / 1000.0
                else:
                    t = float(frame["t"])
            except Exception:
                continue
            converted.append({"x": x, "y": y, "t": t, "s": s})
        return converted

    @staticmethod
    def _sanitize_points(raw_points: list[dict]) -> list[dict]:
        points: list[dict] = []
        last_t = -1.0
        for raw in raw_points:
            try:
                x = float(raw["x"])
                y = float(raw["y"])
                t = float(raw["t"])
                s = int(raw.get("s", 1))
            except Exception:
                continue
            if t < 0:
                continue
            if points and t <= last_t:
                t = last_t + 0.0001
            points.append({"x": x, "y": y, "t": t, "s": s})
            last_t = t
        return points

    @staticmethod
    def _resolve_total_duration(trajectory_data: dict, points: list[dict]) -> float:
        duration_ms = trajectory_data.get("duration_ms")
        if duration_ms is not None:
            try:
                candidate = float(duration_ms) / 1000.0
                if candidate > 0:
                    return max(candidate, float(points[-1]["t"]) if points else candidate)
            except Exception:
                pass
        if points:
            return max(float(trajectory_data.get("total_duration", 0.0) or 0.0), float(points[-1]["t"]))
        return float(trajectory_data.get("total_duration", 0.0) or 0.0)

    def _preload_movies(self) -> None:
        if self._movie_cache:
            return
        candidate_sizes: list = []

        for state_id, gif_file in self._gif_map.items():
            source = normalize_asset_path(gif_file)
            if not path_exists(source):
                continue
            movie = QMovie(source, parent=self)
            if not movie.isValid():
                continue
            movie.setCacheMode(QMovie.CacheMode.CacheAll)
            movie.jumpToFrame(0)
            size = movie.currentImage().size()
            if not size.isEmpty():
                candidate_sizes.append((state_id, size))
            self._movie_cache[state_id] = movie

        # Unify render size across all state GIFs to avoid visual jump on switch.
        base_size = None
        for sid, size in candidate_sizes:
            if sid == 1:
                base_size = size
                break
        if base_size is None and candidate_sizes:
            base_size = candidate_sizes[0][1]
        self._base_size = base_size

        if self._base_size is not None:
            for movie in self._movie_cache.values():
                movie.setScaledSize(self._base_size)
            self._label.resize(self._base_size)
            self.resize(self._base_size)

    def start(self) -> None:
        if not self._points:
            self.finished.emit(self)
            self.deleteLater()
            return

        self._preload_movies()
        self._current_index = 0
        self._last_state_switch_elapsed = -999.0
        self._stopped = False

        first_pt = self._points[0]
        self.move(int(first_pt["x"]), int(first_pt["y"]))

        # Force initial state
        initial_state = int(first_pt.get("s", 1))
        self._update_gif_state(initial_state, force=True)

        self.show()
        duration_ms = max(1, int(round(self._total_duration * 1000)))
        self._timeline.setDuration(duration_ms)
        self._timeline.stop()
        self._timeline.start()

    def _on_timeline_value_changed(self, value) -> None:
        if self._stopped:
            return
        try:
            progress = float(value)
        except Exception:
            progress = 0.0
        progress = max(0.0, min(1.0, progress))
        elapsed = progress * self._total_duration
        self._update_frame(elapsed)

    def _on_timeline_finished(self) -> None:
        if self._stopped:
            return
        self._update_frame(self._total_duration)
        self.stop()

    def _update_frame(self, elapsed: float) -> None:
        if self._stopped:
            self.stop()
            return

        # Advance index based on elapsed time
        while (
            self._current_index < len(self._points) - 1
            and self._points[self._current_index + 1]["t"] <= elapsed
        ):
            self._current_index += 1

        pt0 = self._points[self._current_index]
        if self._current_index < len(self._points) - 1:
            pt1 = self._points[self._current_index + 1]
        else:
            pt1 = pt0

        t0 = float(pt0["t"])
        t1 = float(pt1["t"])
        if t1 > t0:
            alpha = max(0.0, min(1.0, (elapsed - t0) / (t1 - t0)))
        else:
            alpha = 0.0

        x = float(pt0["x"]) + (float(pt1["x"]) - float(pt0["x"])) * alpha
        y = float(pt0["y"]) + (float(pt1["y"]) - float(pt0["y"])) * alpha
        self.move(int(round(x)), int(round(y)))

        sid0 = int(pt0.get("s", 1))
        sid1 = int(pt1.get("s", sid0))
        target_sid = sid1 if (sid1 != sid0 and alpha >= 0.35) else sid0
        if target_sid != self._current_state_id:
            self._update_gif_state(target_sid, elapsed=elapsed)

    def _update_gif_state(self, state_id: int, *, elapsed: float | None = None, force: bool = False) -> None:
        if not force and state_id == self._current_state_id:
            return
        if not force and elapsed is not None:
            if elapsed - self._last_state_switch_elapsed < self.MIN_STATE_SWITCH_INTERVAL_S:
                return

        movie = self._movie_cache.get(state_id)
        if movie is None:
            gif_file = self._gif_map.get(state_id)
            if not gif_file:
                return
            source = normalize_asset_path(gif_file)
            if not path_exists(source):
                return
            movie = QMovie(source, parent=self)
            if not movie.isValid():
                return
            movie.setCacheMode(QMovie.CacheMode.CacheAll)
            if self._base_size is not None:
                movie.setScaledSize(self._base_size)
            self._movie_cache[state_id] = movie

        if self._current_movie is movie:
            return

        if self._current_movie:
            self._current_movie.stop()

        movie.stop()
        movie.jumpToFrame(0)
        self._label.setMovie(movie)
        self._current_movie = movie
        movie.start()

        self._current_state_id = state_id
        if elapsed is not None:
            self._last_state_switch_elapsed = elapsed

    def stop(self) -> None:
        if self._stopped:
            return
        self._stopped = True
        self._timeline.stop()
        for movie in self._movie_cache.values():
            movie.stop()
        self.hide()
        self.finished.emit(self)
        self.deleteLater()

    def force_dismiss(self) -> None:
        self.stop()


class GifParticleManager(QWidget):
    """
    Manages the lifecycle of multiple GIF particle windows.

    Supports spawning waves of particles and limiting max concurrent count.
    """

    MAX_CONCURRENT = 8

    def __init__(self, parent: QWidget | None = None):
        super().__init__(parent)
        self._active_particles: dict[int, GifParticle | TrajectoryPlayer] = {}
        self._next_id = 0
        self.hide()  # The manager widget itself is invisible

    def spawn_from_trajectory(self, json_path: str, gif_map: dict[int, str]) -> None:
        """
        Spawn a particle following a recorded trajectory.
        """
        path = Path(json_path)
        if not path.exists():
            print(f"[GifParticle] Trajectory file missing: {path}")
            return
            
        try:
            with open(path, "r", encoding="utf-8") as f:
                data = json.load(f)
        except Exception as e:
            print(f"[GifParticle] Failed to load trajectory: {e}")
            return

        player = TrajectoryPlayer(data, gif_map)
        pid = player.particle_id

        player.finished.connect(self._on_trajectory_finished)
        self._active_particles[pid] = player
        player.start()

    @property
    def active_count(self) -> int:
        return len(self._active_particles)

    def spawn_particle(self, config: ParticleConfig) -> int | None:
        """
        Spawn a single GIF particle.

        Returns the particle_id or None if at capacity.
        """
        if len(self._active_particles) >= self.MAX_CONCURRENT:
            print(f"[GifParticleManager] 已达最大并发数 {self.MAX_CONCURRENT}，跳过")
            return None

        pid = self._next_id
        self._next_id += 1

        particle = GifParticle(config, pid)
        particle.finished.connect(self._on_particle_finished)
        self._active_particles[pid] = particle
        particle.spawn()
        return pid

    def spawn_wave(
        self,
        gif_paths: list[str],
        *,
        count: int = 3,
        scale: float = 0.8,
        duration_ms: int = 5000,
        stagger_ms: int = 300,
        edges: str = "random",
    ) -> list[int]:
        """
        Spawn a wave of multiple particles with staggered entry.

        Args:
            gif_paths: List of GIF file paths to randomly pick from
            count: Number of particles in this wave
            scale: Size scaling factor
            duration_ms: How long each particle lingers
            stagger_ms: Delay between each particle spawn
            edges: Edge to enter from ("random", "top", "bottom", "left", "right")

        Returns:
            List of spawned particle IDs
        """
        if not gif_paths:
            return []

        spawned: list[int] = []
        for i in range(count):
            gif = random.choice(gif_paths)

            def _spawn_one(g=gif) -> None:
                config = ParticleConfig(
                    gif_path=g,
                    scale=scale,
                    duration_ms=duration_ms,
                    enter_duration_ms=random.randint(800, 1500),
                    exit_duration_ms=random.randint(600, 1000),
                    edge=edges,
                    target="random_inner",
                    opacity=random.uniform(0.7, 1.0),
                )
                pid = self.spawn_particle(config)
                if pid is not None:
                    spawned.append(pid)

            # Stagger the spawns
            if i == 0:
                _spawn_one()
            else:
                QTimer.singleShot(i * stagger_ms, _spawn_one)

        return spawned

    def dismiss_all(self) -> None:
        """Immediately dismiss all active particles."""
        for particle in list(self._active_particles.values()):
            particle.force_dismiss()
        self._active_particles.clear()

    def _on_particle_finished(self, particle: GifParticle) -> None:
        """Handle particle lifecycle completion."""
        self._active_particles.pop(particle.particle_id, None)

    def _on_trajectory_finished(self, player: TrajectoryPlayer) -> None:
        """Handle trajectory player lifecycle completion."""
        self._active_particles.pop(player.particle_id, None)
