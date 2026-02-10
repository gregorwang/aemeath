from __future__ import annotations

import ctypes
import ctypes.wintypes
import html
import random
import threading
from datetime import datetime

from PySide6.QtCore import QObject, QTimer, Slot
from PySide6.QtGui import QGuiApplication

from .asset_manager import AssetManager, Script
from .audio_manager import AudioManager, AudioPriority
from .entropy_engine import EntropyEngine
from .idle_monitor import IdleMonitor
from .mood_system import MoodSystem
from .presence_detector import PresenceDetector, PresenceState
from .resource_scheduler import ResourceScheduler
from .script_engine import ScriptEngine
from .state_machine import EntityState, StateMachine

try:
    from ai.gaze_tracker import GazeData, GazeTracker
    from ai.screen_commentator import ScreenCommentator
except ModuleNotFoundError:
    from ..ai.gaze_tracker import GazeData, GazeTracker
    from ..ai.screen_commentator import ScreenCommentator

try:
    from .config_manager import AppConfig
except Exception:  # pragma: no cover - typing fallback
    AppConfig = object  # type: ignore[assignment]


class ScreenEdge:
    RIGHT = "right"
    LEFT = "left"


class Director(QObject):
    """FSM-based behavior orchestrator with optional Phase 4 AI modules."""

    PEEKING_TIMEOUT_MS = 5000

    def __init__(
        self,
        entity_window,
        audio_manager: AudioManager,
        asset_manager: AssetManager,
        ascii_renderer=None,
        app_config: AppConfig | None = None,
        gaze_tracker: GazeTracker | None = None,
        presence_detector: PresenceDetector | None = None,
        mood_system: MoodSystem | None = None,
        resource_scheduler: ResourceScheduler | None = None,
        screen_commentator: ScreenCommentator | None = None,
        parent=None,
    ):
        super().__init__(parent)
        self._entity_window = entity_window
        self._audio_manager = audio_manager
        self._asset_manager = asset_manager
        self._ascii_renderer = ascii_renderer
        self._idle_monitor: IdleMonitor | None = None
        self._config = app_config

        self._base_idle_threshold_ms = (
            max(1, int(app_config.trigger.idle_threshold_seconds)) * 1000 if app_config else IdleMonitor.DEFAULT_THRESHOLD_MS
        )
        self._jitter_range_seconds = app_config.trigger.jitter_range_seconds if app_config else (-30, 60)
        self._auto_dismiss_ms = max(1, int(app_config.trigger.auto_dismiss_seconds)) * 1000 if app_config else 30_000
        self._full_screen_pause = bool(app_config.behavior.full_screen_pause) if app_config else True
        self._preferred_position = (app_config.appearance.position if app_config else "right").lower()

        self._camera_enabled = bool(getattr(getattr(app_config, "vision", None), "camera_enabled", False))
        self._camera_consent = bool(getattr(getattr(app_config, "vision", None), "camera_consent_granted", False))
        self._eye_tracking_enabled = bool(getattr(getattr(app_config, "vision", None), "eye_tracking_enabled", True))
        self._latest_idle_time_ms = 0
        self._latest_gaze_data = GazeData(face_detected=False)
        self._silent_presence_mode = False
        self._current_ascii_template = ""

        self._entropy = EntropyEngine()
        self._script_engine = ScriptEngine(asset_manager.idle_scripts, asset_manager.panic_scripts)
        self._presence_detector = presence_detector or PresenceDetector()
        self._mood_system = mood_system or MoodSystem()
        self._resource_scheduler = resource_scheduler or ResourceScheduler()
        self._screen_commentator = screen_commentator

        self._gaze_tracker = gaze_tracker
        if self._camera_enabled and not self._camera_consent:
            print("[Vision] 摄像头功能已配置为启用，但未授予授权，已自动禁用。")
            self._camera_enabled = False
        if self._gaze_tracker is not None:
            self._gaze_tracker.gaze_updated.connect(self._on_gaze_updated)
            self._gaze_tracker.camera_error.connect(self._on_camera_error)

        self._state_machine = StateMachine(self)
        self._pending_idle_script: Script | None = None
        self._active_edge = ScreenEdge.RIGHT
        self._active_y = 0

        self._peeking_timer = QTimer(self)
        self._peeking_timer.setSingleShot(True)
        self._peeking_timer.timeout.connect(self._on_peeking_timeout)

        self._auto_dismiss_timer = QTimer(self)
        self._auto_dismiss_timer.setSingleShot(True)
        self._auto_dismiss_timer.timeout.connect(self._on_auto_dismiss_timeout)

        self._mood_decay_timer = QTimer(self)
        self._mood_decay_timer.setInterval(60 * 60 * 1000)
        self._mood_decay_timer.timeout.connect(self._mood_system.natural_decay)
        self._mood_decay_timer.start()

        self._state_machine.register_state_handler(EntityState.HIDDEN, on_enter=self._enter_hidden)
        self._state_machine.register_state_handler(
            EntityState.PEEKING,
            on_enter=self._enter_peeking,
            on_exit=self._stop_peeking_timer,
        )
        self._state_machine.register_state_handler(
            EntityState.ENGAGED,
            on_enter=self._enter_engaged,
            on_exit=self._stop_auto_dismiss_timer,
        )
        self._state_machine.register_state_handler(
            EntityState.FLEEING,
            on_enter=self._enter_fleeing,
            on_exit=self._stop_auto_dismiss_timer,
        )

        if hasattr(self._entity_window, "flee_completed"):
            self._entity_window.flee_completed.connect(self._on_flee_finished)

    @property
    def current_state(self) -> EntityState:
        return self._state_machine.current_state

    @property
    def mood(self) -> float:
        return self._mood_system.mood

    @property
    def mood_label(self) -> str:
        return self._mood_system.mood_label

    def bind_idle_monitor(self, idle_monitor: IdleMonitor) -> None:
        self._idle_monitor = idle_monitor
        idle_monitor.user_idle_confirmed.connect(self.on_user_idle)
        idle_monitor.user_active_detected.connect(self.on_user_active)
        idle_monitor.idle_time_updated.connect(self._on_idle_time_updated)
        self._arm_idle_threshold_with_jitter()
        idle_monitor.reset_to_standby()

    @Slot()
    def on_user_idle(self) -> None:
        if self._state_machine.current_state != EntityState.HIDDEN:
            return
        if self._full_screen_pause and self._is_fullscreen_app_running():
            if self._idle_monitor is not None:
                self._idle_monitor.reset_to_standby()
                self._arm_idle_threshold_with_jitter()
            return

        presence_state = self._presence_detector.determine_presence(
            idle_time_ms=self._latest_idle_time_ms,
            gaze_data=self._latest_gaze_data if self._camera_enabled else None,
        )
        if presence_state == PresenceState.PRESENT_ACTIVE:
            if self._idle_monitor is not None:
                self._idle_monitor.reset_to_standby()
                self._arm_idle_threshold_with_jitter()
            return
        if presence_state == PresenceState.ABSENT:
            # Deep-sleep behavior entry point (Phase 4 expansion hook).
            if self._idle_monitor is not None:
                self._idle_monitor.reset_to_standby()
                self._arm_idle_threshold_with_jitter()
            return
        self._silent_presence_mode = presence_state == PresenceState.PRESENT_PASSIVE
        self._state_machine.transition_to(EntityState.PEEKING)

    @Slot()
    def on_user_active(self) -> None:
        state = self._state_machine.current_state
        if state in (EntityState.PEEKING, EntityState.ENGAGED):
            self._mood_system.on_dismissed()
            self._state_machine.transition_to(EntityState.FLEEING)
            return
        if self._idle_monitor is not None:
            self._idle_monitor.reset_to_standby()

    def request_screen_commentary(self) -> None:
        if self._screen_commentator is None:
            return
        self._mood_system.on_engaged()

        def _worker() -> None:
            try:
                plan = self._resource_scheduler.resolve_plan(is_fullscreen=False, user_dialog_active=True)
                if not plan.llm_running:
                    return
                self._screen_commentator.comment_on_screen_sync(mood_value=self._mood_system.mood)
            except Exception:
                return

        threading.Thread(target=_worker, daemon=True).start()

    def summon_now(self) -> bool:
        """
        Force summon regardless of idle threshold.
        """
        state = self._state_machine.current_state
        if state == EntityState.FLEEING:
            return False
        if state == EntityState.HIDDEN:
            self._silent_presence_mode = False
            return self._state_machine.transition_to(EntityState.ENGAGED)
        if state == EntityState.PEEKING:
            return self._state_machine.transition_to(EntityState.ENGAGED)
        if state == EntityState.ENGAGED:
            self._auto_dismiss_timer.start(self._auto_dismiss_ms)
            return True
        return False

    def toggle_visibility(self) -> None:
        if self._state_machine.current_state == EntityState.HIDDEN:
            self.summon_now()
        elif self._state_machine.current_state in (EntityState.PEEKING, EntityState.ENGAGED):
            self.on_user_active()

    def switch_character(self, asset_manager: AssetManager, *, ascii_renderer=None, voice: str | None = None) -> None:
        self._asset_manager = asset_manager
        self._script_engine.refresh(asset_manager.idle_scripts, asset_manager.panic_scripts)
        if ascii_renderer is not None:
            self._ascii_renderer = ascii_renderer
        if voice:
            self._audio_manager.set_voice(voice)
        self._pending_idle_script = None
        if self._state_machine.current_state == EntityState.ENGAGED:
            now = datetime.now()
            script = self._script_engine.select_idle_script(now=now) or self._asset_manager.get_idle_script_for_time(now)
            if script:
                self._current_ascii_template = self._resolve_ascii_content(script)
                self._entity_window.set_ascii_content(self._apply_current_gaze(self._current_ascii_template))

    def get_status_summary(self) -> str:
        return (
            f"状态: {self._state_machine.current_state.name} | "
            f"心情: {self._mood_system.mood_label} ({self._mood_system.mood:.2f}) | "
            f"摄像头: {'开' if self._camera_enabled else '关'}"
        )

    def shutdown(self) -> None:
        self._stop_peeking_timer()
        self._stop_auto_dismiss_timer()
        if self._mood_decay_timer.isActive():
            self._mood_decay_timer.stop()
        self._stop_camera_tracking()

    def _enter_hidden(self) -> None:
        self._stop_peeking_timer()
        self._stop_auto_dismiss_timer()
        self._stop_camera_tracking()
        if hasattr(self._entity_window, "hide_now"):
            self._entity_window.hide_now()
        else:
            self._entity_window.hide()
        if self._idle_monitor is not None:
            self._idle_monitor.reset_to_standby()
            self._arm_idle_threshold_with_jitter()
        self._silent_presence_mode = False
        self._current_ascii_template = ""

    def _enter_peeking(self) -> None:
        screen = QGuiApplication.primaryScreen()
        if screen is None:
            self._state_machine.transition_to(EntityState.HIDDEN)
            return

        self._start_camera_tracking_if_needed()

        now = datetime.now()
        script = self._script_engine.select_idle_script(now=now) or self._asset_manager.get_idle_script_for_time(now)
        if script is None:
            self._state_machine.transition_to(EntityState.HIDDEN)
            return

        geometry = screen.availableGeometry()
        self._active_edge = self._choose_edge()
        self._active_y = self._entropy.random_y_position(geometry.y(), geometry.height())
        self._pending_idle_script = script

        self._current_ascii_template = self._resolve_ascii_content(script)
        display_html = self._apply_current_gaze(self._current_ascii_template)
        self._entity_window.set_ascii_content(display_html)

        if hasattr(self._entity_window, "peek"):
            self._entity_window.peek(edge=self._active_edge, y_position=self._active_y, script=script)
        else:
            self._entity_window.summon(edge=self._active_edge, y_position=self._active_y, script=script)

        self._peeking_timer.start(self.PEEKING_TIMEOUT_MS)

    def _enter_engaged(self) -> None:
        self._start_camera_tracking_if_needed()
        script = self._pending_idle_script
        if script is None:
            now = datetime.now()
            script = self._script_engine.select_idle_script(now=now) or self._asset_manager.get_idle_script_for_time(now)
            if script:
                self._pending_idle_script = script

        # If ENGAGED is entered directly from HIDDEN (tray summon), ensure the window is shown.
        if not self._entity_window.isVisible():
            screen = QGuiApplication.primaryScreen()
            if screen is not None:
                geometry = screen.availableGeometry()
                self._active_edge = self._choose_edge()
                self._active_y = self._entropy.random_y_position(geometry.y(), geometry.height())
                if script is not None:
                    self._current_ascii_template = self._resolve_ascii_content(script)
                    self._entity_window.set_ascii_content(self._apply_current_gaze(self._current_ascii_template))
                self._entity_window.summon(edge=self._active_edge, y_position=self._active_y, script=script)
        elif hasattr(self._entity_window, "enter"):
            self._entity_window.enter(script=script)

        if script is not None and not self._silent_presence_mode:
            self._mood_system.on_interacted()
            self._audio_manager.play_script(script, priority=AudioPriority.HIGH)

        self._auto_dismiss_timer.start(self._auto_dismiss_ms)

    def _enter_fleeing(self) -> None:
        self._stop_peeking_timer()
        self._stop_auto_dismiss_timer()
        self._stop_camera_tracking()

        now = datetime.now()
        panic_script = self._script_engine.select_panic_script(now=now) or self._asset_manager.get_panic_script(now)
        if panic_script is not None:
            self._audio_manager.play_script(
                panic_script,
                priority=AudioPriority.CRITICAL,
                interrupt=True,
            )
        else:
            self._audio_manager.interrupt()

        self._entity_window.flee()

    @Slot(int)
    def _on_idle_time_updated(self, idle_ms: int) -> None:
        self._latest_idle_time_ms = int(idle_ms)

    @Slot(object)
    def _on_gaze_updated(self, gaze_data: object) -> None:
        if not isinstance(gaze_data, GazeData):
            return
        self._latest_gaze_data = gaze_data
        if not self._eye_tracking_enabled or self._ascii_renderer is None:
            return
        if self._state_machine.current_state not in (EntityState.PEEKING, EntityState.ENGAGED):
            return
        if not self._current_ascii_template:
            return
        html_with_gaze = self._apply_current_gaze(self._current_ascii_template)
        self._entity_window.set_ascii_content(html_with_gaze)

    @Slot(str)
    def _on_camera_error(self, message: str) -> None:
        print(f"[Vision] {message}")
        self._camera_enabled = False
        self._latest_gaze_data = GazeData(face_detected=False)

    @Slot()
    def _on_peeking_timeout(self) -> None:
        if self._state_machine.current_state == EntityState.PEEKING:
            self._state_machine.transition_to(EntityState.ENGAGED)

    @Slot()
    def _on_auto_dismiss_timeout(self) -> None:
        if self._state_machine.current_state == EntityState.ENGAGED:
            self._mood_system.on_dismissed()
            self._audio_manager.interrupt()
            self._state_machine.transition_to(EntityState.HIDDEN)

    @Slot()
    def _on_flee_finished(self) -> None:
        if self._state_machine.current_state == EntityState.FLEEING:
            self._state_machine.transition_to(EntityState.HIDDEN)

    def _stop_peeking_timer(self) -> None:
        if self._peeking_timer.isActive():
            self._peeking_timer.stop()

    def _stop_auto_dismiss_timer(self) -> None:
        if self._auto_dismiss_timer.isActive():
            self._auto_dismiss_timer.stop()

    def _arm_idle_threshold_with_jitter(self) -> None:
        if self._idle_monitor is None:
            return
        jittered = self._entropy.jitter_threshold(
            base_threshold_ms=self._base_idle_threshold_ms,
            jitter_range_seconds=self._jitter_range_seconds,
        )
        self._idle_monitor.set_threshold_ms(jittered)

    def _choose_edge(self) -> str:
        if self._preferred_position == ScreenEdge.LEFT:
            return ScreenEdge.LEFT
        if self._preferred_position == ScreenEdge.RIGHT:
            return ScreenEdge.RIGHT
        return random.choice([ScreenEdge.RIGHT, ScreenEdge.LEFT])

    def _resolve_ascii_content(self, script: Script) -> str:
        if self._ascii_renderer and script.sprite_path:
            try:
                return self._ascii_renderer.render_image(script.sprite_path)
            except Exception:
                pass
        return self._build_fallback_ascii(script.text)

    def _apply_current_gaze(self, ascii_template: str) -> str:
        if self._ascii_renderer is None or not self._eye_tracking_enabled:
            return ascii_template
        return self._ascii_renderer.apply_eye_tracking(ascii_template, self._latest_gaze_data.face_x, eye_width=5)

    @staticmethod
    def _build_fallback_ascii(text: str) -> str:
        safe_text = html.escape(text)
        lines = [
            '<pre style="font-family: Consolas, \'Courier New\', monospace; font-size: 9px; line-height: 1.0; margin: 0;">',
            '<span style="color: rgb(100, 255, 220);">   /\\_/\\</span><br/>',
            '<span style="color: rgb(100, 255, 220);">  ({EYE_L})</span><br/>',
            '<span style="color: rgb(100, 255, 220);">   &gt; ^ &lt;</span><br/>',
            f'<span style="color: rgb(255, 225, 180);">{safe_text}</span>',
            "</pre>",
        ]
        return "".join(lines)

    def _start_camera_tracking_if_needed(self) -> None:
        if not self._camera_enabled or self._gaze_tracker is None:
            return
        plan = self._resource_scheduler.resolve_plan(
            is_fullscreen=self._full_screen_pause and self._is_fullscreen_app_running(),
            user_dialog_active=False,
        )
        if not plan.cv_running:
            return
        self._gaze_tracker.start_tracking()

    def _stop_camera_tracking(self) -> None:
        if self._gaze_tracker is None:
            return
        self._gaze_tracker.stop_tracking()

    @staticmethod
    def _is_fullscreen_app_running() -> bool:
        try:
            user32 = ctypes.windll.user32
            hwnd = user32.GetForegroundWindow()
            if not hwnd:
                return False

            rect = ctypes.wintypes.RECT()
            if not user32.GetWindowRect(hwnd, ctypes.byref(rect)):
                return False

            screen_w = user32.GetSystemMetrics(0)
            screen_h = user32.GetSystemMetrics(1)

            return (
                rect.left <= 0
                and rect.top <= 0
                and rect.right >= screen_w
                and rect.bottom >= screen_h
            )
        except Exception:
            return False
