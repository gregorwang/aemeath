"""
Idle Invasion Controller.

When the computer has been idle for a configurable duration, GIF characters
begin to appear one-by-one on the screen.  As idle time increases the spawn
rate accelerates until the screen is filled (or a hard cap is reached).

Grid-based placement guarantees no two characters overlap.
"""

from __future__ import annotations

import logging
import random
from enum import Enum, auto
from pathlib import Path
from typing import TYPE_CHECKING

from PySide6.QtCore import QObject, QTimer, Slot
from PySide6.QtGui import QCursor, QGuiApplication, QMovie

if TYPE_CHECKING:
    from .config_manager import IdleInvasionConfig
    from .idle_monitor import IdleMonitor

try:
    from ui.gif_particle import GifParticle, ParticleConfig
except ModuleNotFoundError:
    from ..ui.gif_particle import GifParticle, ParticleConfig
try:
    from core.config_manager import IdleInvasionConfig
except ModuleNotFoundError:
    from .config_manager import IdleInvasionConfig


LOGGER = logging.getLogger("CyberCompanion")


# ---------------------------------------------------------------------------
# State enum
# ---------------------------------------------------------------------------

class InvasionState(Enum):
    INACTIVE = auto()
    SPAWNING = auto()
    SATURATED = auto()
    RETREATING = auto()


# ---------------------------------------------------------------------------
# Main controller
# ---------------------------------------------------------------------------

class IdleInvasionController(QObject):
    """
    Orchestrates the gradual spawning and retreat of GIF invaders
    on the desktop while the user is idle.

    Integration points
    ------------------
    * Listens to ``IdleMonitor.idle_time_updated(int)`` for continuous
      idle-time tracking.
    * Listens to ``IdleMonitor.user_active_detected()`` to trigger retreat.
    * Uses ``GifParticleManager`` (or its own particle list) to manage windows.
    """



    def __init__(
        self,
        characters_dir: Path,
        config: IdleInvasionConfig | None = None,
        parent: QObject | None = None,
    ):
        super().__init__(parent)

        self._config = config or IdleInvasionConfig()
        self._characters_dir = characters_dir
        self._state = InvasionState.INACTIVE

        # ---- Grid placement bookkeeping ----
        # Grid is lazily initialised on first spawn.
        self._grid_cols = 0
        self._grid_rows = 0
        self._cell_w = 0
        self._cell_h = 0
        self._screen_x = 0
        self._screen_y = 0
        self._occupied: set[tuple[int, int]] = set()  # (col, row) of occupied cells
        self._particle_cell_map: dict[int, tuple[int, int]] = {}  # pid -> (col, row)
        self._gif_sizes: dict[str, tuple[int, int]] = {}
        self._max_gif_w = 0
        self._max_gif_h = 0

        # ---- Particle tracking ----
        self._particles: dict[int, GifParticle] = {}
        self._next_pid = 0

        # ---- Timers ----
        self._spawn_timer = QTimer(self)
        self._spawn_timer.setSingleShot(True)
        self._spawn_timer.timeout.connect(self._on_spawn_tick)

        self._retreat_timer = QTimer(self)
        self._retreat_timer.setSingleShot(True)
        self._retreat_timer.timeout.connect(self._finish_retreat)

        # ---- Idle tracking ----
        self._idle_time_ms = 0
        self._invasion_started = False  # True once idle exceeds start_delay_ms

        # Resolve absolute GIF paths.
        self._gif_paths: list[str] = self._resolve_gif_paths()

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def bind_idle_monitor(self, idle_monitor: IdleMonitor) -> None:
        """Connect to the shared IdleMonitor signals."""
        idle_monitor.idle_time_updated.connect(self._on_idle_time_updated)
        idle_monitor.user_active_detected.connect(self._on_user_active)

    def apply_config(self, config: IdleInvasionConfig) -> None:
        """Hot-reload configuration at runtime."""
        self._config = config
        self._gif_paths = self._resolve_gif_paths()
        self._refresh_gif_sizes()
        if self._state == InvasionState.INACTIVE:
            self._invasion_started = False
        if not config.enabled and self._state != InvasionState.INACTIVE:
            self._begin_retreat()

    def shutdown(self) -> None:
        """Cleanup on application exit."""
        self._spawn_timer.stop()
        self._retreat_timer.stop()
        self._dismiss_all_immediate()
        self._state = InvasionState.INACTIVE

    @property
    def state(self) -> InvasionState:
        return self._state

    @property
    def active_count(self) -> int:
        return len(self._particles)

    # ------------------------------------------------------------------
    # Idle monitor slots
    # ------------------------------------------------------------------

    @Slot(int)
    def _on_idle_time_updated(self, idle_ms: int) -> None:
        """Called ~every 100 ms with the current idle time."""
        self._idle_time_ms = idle_ms

        if not self._config.enabled:
            return

        if self._state == InvasionState.RETREATING:
            # Don't start new spawns while retreating.
            return

        # Fallback: if user becomes active but IdleMonitor doesn't signal active
        # (e.g. because we started invading BEFORE IdleMonitor's threshold),
        # we detect the drop in idle time here.
        if self._invasion_started and idle_ms < 1000:
            if self._state in (InvasionState.SPAWNING, InvasionState.SATURATED):
                self._begin_retreat()
            else:
                self._invasion_started = False
            return

        # Not yet started? Check if we crossed the start delay.
        if self._state == InvasionState.INACTIVE:
            if self._invasion_started:
                return
            if idle_ms >= self._config.start_delay_ms:
                self._invasion_started = True
                self._begin_spawning()
            return

        # Already in SPAWNING/SATURATED – nothing to do here,
        # the spawn timer drives the cadence.

    @Slot()
    def _on_user_active(self) -> None:
        """User moved mouse / pressed key → retreat all invaders."""
        if self._state in (InvasionState.SPAWNING, InvasionState.SATURATED):
            self._begin_retreat()
        elif self._state == InvasionState.INACTIVE:
            self._invasion_started = False
            self._idle_time_ms = 0

    # ------------------------------------------------------------------
    # Spawning logic
    # ------------------------------------------------------------------

    def _begin_spawning(self) -> None:
        if not self._gif_paths:
            LOGGER.warning("[IdleInvasion] No valid GIF paths, cannot start invasion.")
            return
        LOGGER.info("[IdleInvasion] Invasion started – idle_ms=%d", self._idle_time_ms)
        self._state = InvasionState.SPAWNING
        self._init_grid()
        # Spawn the first invader immediately.
        self._spawn_one()
        if self._state == InvasionState.SPAWNING:
            # Arm the timer for subsequent spawns.
            self._arm_spawn_timer()

    @Slot()
    def _on_spawn_tick(self) -> None:
        """Timer callback – spawn one invader and re-arm."""
        if self._state != InvasionState.SPAWNING:
            return
        self._spawn_one()
        if self._state == InvasionState.SPAWNING:
            self._arm_spawn_timer()

    def _arm_spawn_timer(self) -> None:
        interval = self._current_spawn_interval()
        self._spawn_timer.start(interval)

    def _current_spawn_interval(self) -> int:
        """Determine spawn interval based on current idle time."""
        extra_idle_ms = max(0, self._idle_time_ms - self._config.start_delay_ms)
        initial_ms = max(500, int(self._config.initial_spawn_interval_ms))
        min_ms = max(500, int(self._config.min_spawn_interval_ms))

        # Phase-2 cadence profile (defaults map to:
        # 0-3m: 8-12s, 3-5m: 5-8s, 5-10m: 3-5s, 10m+: 2-3s).
        if extra_idle_ms < 3 * 60_000:
            low, high = int(initial_ms * 0.8), int(initial_ms * 1.2)
        elif extra_idle_ms < 5 * 60_000:
            low, high = int(initial_ms * 0.5), int(initial_ms * 0.8)
        elif extra_idle_ms < 10 * 60_000:
            low, high = int(initial_ms * 0.3), int(initial_ms * 0.5)
        else:
            low, high = min_ms, int(initial_ms * 0.3)

        low = max(min_ms, low)
        high = max(low, high)
        return random.randint(low, high)

    def _spawn_one(self) -> None:
        """Place one invader in a random free grid cell."""
        if len(self._particles) >= self._config.max_invaders:
            LOGGER.info(
                "[IdleInvasion] Reached max invaders (%d), entering SATURATED.",
                self._config.max_invaders,
            )
            self._state = InvasionState.SATURATED
            self._spawn_timer.stop()
            return

        cell = self._pick_free_cell()
        if cell is None:
            LOGGER.info("[IdleInvasion] No free cells remaining, entering SATURATED.")
            self._state = InvasionState.SATURATED
            self._spawn_timer.stop()
            return

        gif_path = random.choice(self._gif_paths)
        gif_w, gif_h = self._gif_sizes.get(gif_path, (self._max_gif_w, self._max_gif_h))

        col, row = cell
        # Keep the particle fully inside its assigned cell to guarantee no overlap.
        cell_x = self._screen_x + col * self._cell_w
        cell_y = self._screen_y + row * self._cell_h
        max_offset_x = max(0, self._cell_w - gif_w)
        max_offset_y = max(0, self._cell_h - gif_h)
        target_x = cell_x + random.randint(0, max_offset_x)
        target_y = cell_y + random.randint(0, max_offset_y)

        config = ParticleConfig(
            gif_path=gif_path,
            scale=self._config.scale,
            duration_ms=999_999_999,  # Never auto-exit; we manage the lifecycle.
            enter_duration_ms=random.randint(800, 1500),
            exit_duration_ms=random.randint(600, 1200),
            edge="random",
            target="fixed",  # We will override the end position below.
            loop_gif=True,
            opacity=random.uniform(0.75, 1.0),
        )

        pid = self._next_pid
        self._next_pid += 1

        particle = _InvasionParticle(config, pid, target_x, target_y)
        particle.finished.connect(self._on_particle_finished)
        self._particles[pid] = particle
        self._occupied.add(cell)
        self._particle_cell_map[pid] = cell

        particle.spawn()
        LOGGER.debug(
            "[IdleInvasion] Spawned invader pid=%d cell=(%d,%d) gif=%s  total=%d",
            pid, col, row, Path(gif_path).name, len(self._particles),
        )

    def _pick_free_cell(self) -> tuple[int, int] | None:
        """Return a random unoccupied grid cell, or None if full."""
        total = self._grid_cols * self._grid_rows
        if len(self._occupied) >= total:
            return None
        # For small grids just build the free list.
        if total <= 200:
            free = [
                (c, r)
                for c in range(self._grid_cols)
                for r in range(self._grid_rows)
                if (c, r) not in self._occupied
            ]
            return random.choice(free) if free else None
        # For larger grids use random probing (faster when mostly empty).
        for _ in range(total * 3):
            c = random.randint(0, self._grid_cols - 1)
            r = random.randint(0, self._grid_rows - 1)
            if (c, r) not in self._occupied:
                return (c, r)
        return None

    # ------------------------------------------------------------------
    # Grid initialisation
    # ------------------------------------------------------------------

    def _init_grid(self) -> None:
        """Compute the virtual grid over the primary screen."""
        screen = QGuiApplication.primaryScreen()
        if screen is None:
            LOGGER.warning("[IdleInvasion] No primary screen found.")
            return
        geo = screen.availableGeometry()
        self._screen_x = geo.x()
        self._screen_y = geo.y()

        # Approximate GIF dimensions after scaling.
        self._refresh_gif_sizes()
        base_w = max(1, self._max_gif_w)
        base_h = max(1, self._max_gif_h)
        self._cell_w = base_w + self._config.cell_padding
        self._cell_h = base_h + self._config.cell_padding

        self._grid_cols = max(1, geo.width() // self._cell_w)
        self._grid_rows = max(1, geo.height() // self._cell_h)

        self._occupied.clear()
        self._particle_cell_map.clear()

        LOGGER.info(
            "[IdleInvasion] Grid initialised: %dx%d cells (%dx%d px each) → max %d slots  screen=%dx%d",
            self._grid_cols,
            self._grid_rows,
            self._cell_w,
            self._cell_h,
            self._grid_cols * self._grid_rows,
            geo.width(),
            geo.height(),
        )

    # ------------------------------------------------------------------
    # Retreat / cleanup
    # ------------------------------------------------------------------

    def _begin_retreat(self) -> None:
        """Trigger all invaders to exit."""
        self._spawn_timer.stop()
        self._state = InvasionState.RETREATING
        LOGGER.info(
            "[IdleInvasion] Retreat triggered – dismissing %d invaders (style=%s).",
            len(self._particles),
            self._config.retreat_style,
        )

        style = str(self._config.retreat_style).lower()
        if style == "instant" or not self._particles:
            self._dismiss_all_immediate()
            self._reset()
            return

        particles = list(self._particles.values())
        if style == "ripple":
            particles = self._sorted_particles_for_ripple(particles)
            self._schedule_ripple_retreat(particles)
        else:
            self._schedule_scatter_retreat(particles)

        # Safety: if after 5s they're not all gone, force-clean.
        self._retreat_timer.start(5000)

    @Slot()
    def _finish_retreat(self) -> None:
        """Fallback cleanup after retreat timeout."""
        if self._state == InvasionState.RETREATING:
            self._dismiss_all_immediate()
            self._reset()

    def _dismiss_all_immediate(self) -> None:
        for particle in list(self._particles.values()):
            try:
                particle.force_dismiss()
            except Exception:
                pass
        self._particles.clear()
        self._occupied.clear()
        self._particle_cell_map.clear()

    def _reset(self) -> None:
        """Return to INACTIVE state, ready for the next idle cycle."""
        self._state = InvasionState.INACTIVE
        self._invasion_started = False
        self._idle_time_ms = 0
        self._occupied.clear()
        self._particle_cell_map.clear()
        LOGGER.info("[IdleInvasion] Reset to INACTIVE.")

    @Slot(object)
    def _on_particle_finished(self, particle: object) -> None:
        """A particle completed its exit animation or was force-dismissed."""
        if not isinstance(particle, GifParticle):
            return
        pid = particle.particle_id
        self._particles.pop(pid, None)
        cell = self._particle_cell_map.pop(pid, None)
        if cell is not None:
            self._occupied.discard(cell)

        # If retreating and all gone → reset.
        if self._state == InvasionState.RETREATING and not self._particles:
            self._retreat_timer.stop()
            self._reset()

    # ------------------------------------------------------------------
    # Helpers
    # ------------------------------------------------------------------

    def _resolve_gif_paths(self) -> list[str]:
        """Build the list of absolute GIF paths from config."""
        paths: list[str] = []
        for filename in self._config.participating_gifs:
            p = self._characters_dir / filename
            if p.exists():
                paths.append(str(p))
            else:
                LOGGER.debug("[IdleInvasion] GIF not found, skipping: %s", p)
        return paths

    def _refresh_gif_sizes(self) -> None:
        default_size = max(1, int(120 * self._config.scale))
        sizes: dict[str, tuple[int, int]] = {}
        max_w = default_size
        max_h = default_size
        for path in self._gif_paths:
            w, h = self._read_scaled_gif_size(path)
            sizes[path] = (w, h)
            max_w = max(max_w, w)
            max_h = max(max_h, h)
        self._gif_sizes = sizes
        self._max_gif_w = max_w
        self._max_gif_h = max_h

    def _read_scaled_gif_size(self, gif_path: str) -> tuple[int, int]:
        default_size = max(1, int(120 * self._config.scale))
        movie = QMovie(gif_path)
        if not movie.isValid():
            return (default_size, default_size)
        size = movie.frameRect().size()
        if size.width() <= 0 or size.height() <= 0:
            movie.jumpToFrame(0)
            size = movie.currentImage().size()
        width = int(max(1, size.width()) * self._config.scale)
        height = int(max(1, size.height()) * self._config.scale)
        return (max(1, width), max(1, height))

    def _schedule_scatter_retreat(self, particles: list[GifParticle]) -> None:
        for particle in particles:
            QTimer.singleShot(random.randint(50, 200), particle.start_retreat)

    def _schedule_ripple_retreat(self, particles: list[GifParticle]) -> None:
        for idx, particle in enumerate(particles):
            delay = min(2200, idx * 60 + random.randint(0, 40))
            QTimer.singleShot(delay, particle.start_retreat)

    def _sorted_particles_for_ripple(self, particles: list[GifParticle]) -> list[GifParticle]:
        if not particles:
            return particles
        cursor = QCursor.pos()
        try:
            return sorted(
                particles,
                key=lambda p: self._distance_sq(p.pos().x(), p.pos().y(), cursor.x(), cursor.y()),
            )
        except Exception:
            return particles

    @staticmethod
    def _distance_sq(x1: int, y1: int, x2: int, y2: int) -> int:
        dx = int(x1) - int(x2)
        dy = int(y1) - int(y2)
        return dx * dx + dy * dy


# ---------------------------------------------------------------------------
# Specialised GifParticle subclass for invasion
# ---------------------------------------------------------------------------

class _InvasionParticle(GifParticle):
    """
    A GifParticle that enters from a screen edge and parks at a fixed
    grid-assigned position.  It does NOT auto-exit; instead the
    ``IdleInvasionController`` tells it when to retreat.
    """

    def __init__(
        self,
        config: ParticleConfig,
        particle_id: int,
        target_x: int,
        target_y: int,
    ):
        super().__init__(config, particle_id)
        self._target_x = target_x
        self._target_y = target_y
        self._retreating = False

    # Override: park at the grid position, not a random inner position.
    def spawn(self) -> None:
        from PySide6.QtCore import QPoint, QPropertyAnimation, QEasingCurve

        screen = QGuiApplication.primaryScreen()
        if screen is None or self._movie is None:
            self._cleanup()
            return

        geometry = screen.availableGeometry()
        w = self.width()
        h = self.height()

        end_pos = QPoint(self._target_x, self._target_y)

        # Choose a random edge for the entrance.
        edge = random.choice(["top", "bottom", "left", "right"])
        sx, sy = geometry.x(), geometry.y()
        sw, sh = geometry.width(), geometry.height()

        if edge == "top":
            start_pos = QPoint(self._target_x, sy - h - 20)
        elif edge == "bottom":
            start_pos = QPoint(self._target_x, sy + sh + 20)
        elif edge == "left":
            start_pos = QPoint(sx - w - 20, self._target_y)
        else:
            start_pos = QPoint(sx + sw + 20, self._target_y)

        self.move(start_pos)
        self.show()
        self.raise_()

        self._enter_anim = QPropertyAnimation(self, b"pos", self)
        self._enter_anim.setDuration(self._config.enter_duration_ms)
        self._enter_anim.setStartValue(start_pos)
        self._enter_anim.setEndValue(end_pos)
        self._enter_anim.setEasingCurve(QEasingCurve.Type.OutCubic)
        # Do NOT connect to _on_enter_finished – we don't want the linger
        # timer since this particle stays indefinitely.
        self._enter_anim.start()

    def start_retreat(self) -> None:
        """Begin exit animation towards the nearest screen edge."""
        if self._retreating:
            return
        self._retreating = True
        self._start_exit()

    def force_dismiss(self) -> None:
        """Skip animation and tear down immediately."""
        self._linger_timer.stop()
        if self._enter_anim and self._enter_anim.state() == self._enter_anim.State.Running:
            self._enter_anim.stop()
        self._cleanup()
