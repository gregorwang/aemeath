from __future__ import annotations

import ctypes
import ctypes.wintypes
import html
import json
import logging
import os
import random
import sys
import threading
import time
from datetime import datetime
from enum import Enum, auto
from pathlib import Path

from PySide6.QtCore import QObject, QTimer, Slot
from PySide6.QtGui import QGuiApplication

from .asset_manager import AssetManager, Script
from .audio_manager import AudioManager, AudioPriority
from .audio_output_monitor import AudioOutputMonitor
from .entropy_engine import EntropyEngine
from .gif_state_mapper import GifStateMapper
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
    from ui.gif_particle import TrajectoryPlayer
except ModuleNotFoundError:
    from ..ui.gif_particle import TrajectoryPlayer
try:
    from .paths import get_base_dir, get_user_data_dir
except ModuleNotFoundError:
    from core.paths import get_base_dir, get_user_data_dir

try:
    from .config_manager import AppConfig
except Exception:  # pragma: no cover - typing fallback
    AppConfig = object  # type: ignore[assignment]


class ScreenEdge:
    RIGHT = "right"
    LEFT = "left"


class BehaviorMode(Enum):
    IDLE = auto()
    BUSY = auto()
    MEDIA_PLAYING = auto()
    SUMMONING = auto()


class Director(QObject):
    """FSM-based behavior orchestrator with optional Phase 4 AI modules."""

    VOICE_TRAJECTORY_FILE = "trajectory_1770800738.json"
    LOGGER = logging.getLogger("CyberCompanion")
    EXPRESSION_STATE_MAP = {
        "happy": "state6",
        "neutral": "state1",
        "angry": "state4",
    }

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
        gif_state_mapper: GifStateMapper | None = None,
        audio_output_monitor: AudioOutputMonitor | None = None,
        parent=None,
    ):
        super().__init__(parent)
        self._entity_window = entity_window
        self._audio_manager = audio_manager
        self._asset_manager = asset_manager
        self._ascii_renderer = ascii_renderer
        self._idle_monitor: IdleMonitor | None = None
        self._config = app_config
        self._base_dir = get_base_dir()

        self._base_idle_threshold_ms = (
            max(1, int(app_config.trigger.idle_threshold_seconds)) * 1000 if app_config else IdleMonitor.DEFAULT_THRESHOLD_MS
        )
        self._jitter_range_seconds = app_config.trigger.jitter_range_seconds if app_config else (-30, 60)
        self._auto_dismiss_ms = max(1, int(app_config.trigger.auto_dismiss_seconds)) * 1000 if app_config else 30_000
        self._full_screen_pause = bool(app_config.behavior.full_screen_pause) if app_config else True
        self._audio_output_reactive = bool(app_config.behavior.audio_output_reactive) if app_config else True
        self._preferred_position = (app_config.appearance.position if app_config else "right").lower()

        self._camera_enabled = bool(getattr(getattr(app_config, "vision", None), "camera_enabled", False))
        self._camera_consent = bool(getattr(getattr(app_config, "vision", None), "camera_consent_granted", False))
        self._eye_tracking_enabled = bool(getattr(getattr(app_config, "vision", None), "eye_tracking_enabled", True))
        self._latest_idle_time_ms = 0
        self._latest_gaze_data = GazeData(face_detected=False)
        self._silent_presence_mode = False
        self._current_ascii_template = ""
        self._stable_expression = "neutral"
        self._expression_votes: dict[str, int] = {"happy": 0, "neutral": 0, "angry": 0}
        self._last_expression_visual_at = 0.0

        self._entropy = EntropyEngine()
        self._script_engine = ScriptEngine(asset_manager.idle_scripts, asset_manager.panic_scripts)
        self._presence_detector = presence_detector or PresenceDetector()
        self._mood_system = mood_system or MoodSystem()
        self._resource_scheduler = resource_scheduler or ResourceScheduler()
        self._screen_commentator = screen_commentator
        self._gif_state_mapper = gif_state_mapper
        self._audio_output_monitor = audio_output_monitor

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
        self._voice_trajectory_player: TrajectoryPlayer | None = None
        self._voice_trajectory_playing = False
        self._behavior_mode = BehaviorMode.BUSY
        self._audio_output_active = False
        self._audio_forced_visible = False
        self._self_playback_active = False

        self._auto_dismiss_timer = QTimer(self)
        self._auto_dismiss_timer.setSingleShot(True)
        self._auto_dismiss_timer.timeout.connect(self._on_auto_dismiss_timeout)
        self._voice_trajectory_timeout = QTimer(self)
        self._voice_trajectory_timeout.setSingleShot(True)
        self._voice_trajectory_timeout.timeout.connect(self._on_voice_trajectory_timeout)

        self._mood_decay_timer = QTimer(self)
        self._mood_decay_timer.setInterval(60 * 60 * 1000)
        self._mood_decay_timer.timeout.connect(self._mood_system.natural_decay)
        self._mood_decay_timer.start()

        # Prolonged idle timer: triggers extra particle effects after extended idle
        self._prolonged_idle_timer = QTimer(self)
        self._prolonged_idle_timer.setSingleShot(True)
        self._prolonged_idle_timer.setInterval(10 * 60 * 1000)  # 10 minutes
        self._prolonged_idle_timer.timeout.connect(self._on_prolonged_idle)

        self._state_machine.register_state_handler(EntityState.HIDDEN, on_enter=self._enter_hidden)
        self._state_machine.register_state_handler(
            EntityState.PEEKING,
            on_enter=self._enter_peeking,
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

        # Wire audio output monitor -> behavior mode + optional gif mapper
        if self._audio_output_monitor:
            self._audio_output_monitor.audio_playing_started.connect(self._on_audio_output_started)
            self._audio_output_monitor.audio_playing_stopped.connect(self._on_audio_output_stopped)
            self._audio_output_monitor.start()

        # Ignore our own TTS/play_script output when deciding MEDIA_PLAYING.
        if hasattr(self._audio_manager, "playback_started"):
            self._audio_manager.playback_started.connect(self._on_self_playback_started)
        if hasattr(self._audio_manager, "playback_finished"):
            self._audio_manager.playback_finished.connect(self._on_self_playback_finished)

        # Wire state machine changes to gif mapper
        if self._gif_state_mapper:
            self._state_machine.state_changed.connect(self._on_state_changed_for_particles)

    @property
    def current_state(self) -> EntityState:
        return self._state_machine.current_state

    @property
    def mood(self) -> float:
        return self._mood_system.mood

    @property
    def mood_label(self) -> str:
        return self._mood_system.mood_label

    @property
    def behavior_mode(self) -> BehaviorMode:
        return self._behavior_mode

    def bind_idle_monitor(self, idle_monitor: IdleMonitor) -> None:
        self._idle_monitor = idle_monitor
        idle_monitor.user_idle_confirmed.connect(self.on_user_idle)
        idle_monitor.user_active_detected.connect(self.on_user_active)
        idle_monitor.idle_time_updated.connect(self._on_idle_time_updated)
        self._arm_idle_threshold_with_jitter()
        idle_monitor.reset_to_standby()

    @Slot()
    def on_user_idle(self) -> None:
        if self._voice_trajectory_playing:
            return
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
            self._set_behavior_mode(BehaviorMode.BUSY, apply_visual=False)
            if self._idle_monitor is not None:
                self._idle_monitor.reset_to_standby()
                self._arm_idle_threshold_with_jitter()
            return
        if presence_state == PresenceState.ABSENT:
            # Deep-sleep behavior entry point (Phase 4 expansion hook).
            self._set_behavior_mode(BehaviorMode.BUSY, apply_visual=False)
            if self._idle_monitor is not None:
                self._idle_monitor.reset_to_standby()
                self._arm_idle_threshold_with_jitter()
            return
        self._silent_presence_mode = presence_state == PresenceState.PRESENT_PASSIVE
        self._set_behavior_mode(BehaviorMode.IDLE, apply_visual=False)
        self._state_machine.transition_to(EntityState.ENGAGED)

    @Slot()
    def on_user_active(self) -> None:
        self._set_behavior_mode(BehaviorMode.BUSY)
        state = self._state_machine.current_state
        if state in (EntityState.PEEKING, EntityState.ENGAGED):
            self._mood_system.on_dismissed()
            self._state_machine.transition_to(EntityState.FLEEING)
            return
        if self._idle_monitor is not None:
            self._idle_monitor.reset_to_standby()

    def request_screen_commentary(self) -> None:
        if self._screen_commentator is None:
            self.LOGGER.warning("[ScreenCommentary] Skipped: commentator unavailable")
            return
        self.LOGGER.info("[ScreenCommentary] Requested by user")
        self._mood_system.on_engaged()
        self._set_entity_state("state5", as_base=False)
        try:
            self._screen_commentator.cancel_current_session()
        except Exception:
            pass

        def _worker() -> None:
            try:
                plan = self._resource_scheduler.resolve_plan(is_fullscreen=False, user_dialog_active=True)
                if not plan.llm_running:
                    self.LOGGER.info("[ScreenCommentary] Skipped by resource plan: llm_running=false")
                    self._audio_manager.speak("我现在在省电模式，稍后再看屏幕。", priority=AudioPriority.HIGH)
                    return
                text = self._screen_commentator.comment_on_screen_sync(mood_value=self._mood_system.mood)
                self.LOGGER.info("[ScreenCommentary] Completed: %s", (text or "").strip()[:80])
            except Exception as exc:
                self.LOGGER.exception("[ScreenCommentary] Failed: %s", exc)
                self._audio_manager.speak("我看屏幕失败了，点托盘里的打开日志目录看看。", priority=AudioPriority.HIGH)
                return
            finally:
                self._set_entity_state("state1", as_base=False)

        threading.Thread(target=_worker, daemon=True).start()

    def summon_now(self) -> bool:
        """
        Force summon regardless of idle threshold.
        """
        state = self._state_machine.current_state
        if state == EntityState.FLEEING:
            return False
        behavior_setter = getattr(self, "_set_behavior_mode", None)
        if callable(behavior_setter):
            behavior_setter(BehaviorMode.SUMMONING, apply_visual=False)
        if state == EntityState.HIDDEN:
            if self._try_start_voice_scripted_entrance():
                return True
            self._silent_presence_mode = False
            if self._gif_state_mapper:
                self._gif_state_mapper.on_summoned()
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
                self._set_visual_from_script(script)

    def apply_runtime_config(self, app_config: AppConfig) -> None:
        self._config = app_config
        self._base_idle_threshold_ms = max(1, int(app_config.trigger.idle_threshold_seconds)) * 1000
        self._jitter_range_seconds = app_config.trigger.jitter_range_seconds
        self._auto_dismiss_ms = max(1, int(app_config.trigger.auto_dismiss_seconds)) * 1000
        self._full_screen_pause = bool(app_config.behavior.full_screen_pause)
        self._audio_output_reactive = bool(app_config.behavior.audio_output_reactive)
        self._preferred_position = (app_config.appearance.position or "auto").lower()
        self._eye_tracking_enabled = bool(app_config.vision.eye_tracking_enabled)
        self._camera_consent = bool(app_config.vision.camera_consent_granted)
        self._camera_enabled = bool(app_config.vision.camera_enabled) and self._camera_consent
        if not self._camera_enabled:
            self._stop_camera_tracking()
            self._latest_gaze_data = GazeData(face_detected=False)
        elif self._state_machine.current_state in (EntityState.PEEKING, EntityState.ENGAGED):
            self._start_camera_tracking_if_needed()
        if not self._audio_output_reactive:
            if self._audio_output_active and self._gif_state_mapper is not None:
                self._gif_state_mapper.on_audio_stopped()
            self._audio_output_active = False
            self._audio_forced_visible = False
            if self._behavior_mode == BehaviorMode.MEDIA_PLAYING:
                fallback = BehaviorMode.IDLE if self._state_machine.current_state != EntityState.HIDDEN else BehaviorMode.BUSY
                self._set_behavior_mode(fallback)
        elif (
            self._audio_output_monitor is not None
            and self._audio_output_monitor.is_playing
            and not self._self_playback_active
        ):
            self._on_audio_output_started()
        self._arm_idle_threshold_with_jitter()

    def get_status_summary(self) -> str:
        offline_mode = bool(getattr(getattr(self._config, "behavior", None), "offline_mode", False))
        wakeup_enabled = bool(getattr(getattr(self._config, "wakeup", None), "enabled", False))
        visible = self._state_machine.current_state != EntityState.HIDDEN
        return (
            f"模式: {self._behavior_mode.name} | "
            f"角色可见: {'是' if visible else '否'} | "
            f"系统音频: {'播放中' if self._audio_output_active else '静音/未检测'} | "
            f"心情: {self._mood_system.mood_label} ({self._mood_system.mood:.2f}) | "
            f"摄像头: {'开' if self._camera_enabled else '关'} | "
            f"语音唤醒: {'开' if wakeup_enabled else '关'} | "
            f"网络模式: {'离线' if offline_mode else '在线'}"
        )

    def shutdown(self) -> None:
        self._stop_voice_scripted_entrance()
        self._stop_auto_dismiss_timer()
        if self._mood_decay_timer.isActive():
            self._mood_decay_timer.stop()
        if self._prolonged_idle_timer.isActive():
            self._prolonged_idle_timer.stop()
        self._stop_camera_tracking()
        if self._audio_output_monitor:
            self._audio_output_monitor.stop()
        if self._gif_state_mapper:
            self._gif_state_mapper.shutdown()
        self._set_entity_autonomous(False)

    def _enter_hidden(self) -> None:
        self._stop_auto_dismiss_timer()
        self._stop_camera_tracking()
        self._set_entity_autonomous(False)
        if hasattr(self._entity_window, "hide_now"):
            self._entity_window.hide_now()
        else:
            self._entity_window.hide()
        if self._idle_monitor is not None:
            self._idle_monitor.reset_to_standby()
            self._arm_idle_threshold_with_jitter()
        self._silent_presence_mode = False
        self._audio_forced_visible = False
        # Start prolonged idle timer when going hidden
        self._prolonged_idle_timer.start()
        self._current_ascii_template = ""
        self._set_behavior_mode(BehaviorMode.BUSY, apply_visual=False)

    def _enter_peeking(self) -> None:
        self._enter_engaged()

    def _enter_engaged(self) -> None:
        self._start_camera_tracking_if_needed()
        self._set_entity_state("state1")
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
                    self._set_visual_from_script(script)
                self._entity_window.summon(edge=self._active_edge, y_position=self._active_y, script=script)
        elif hasattr(self._entity_window, "enter"):
            self._entity_window.enter(script=script)

        if (
            script is not None
            and not self._silent_presence_mode
            and self._behavior_mode != BehaviorMode.SUMMONING
        ):
            self._mood_system.on_interacted()
            self._audio_manager.play_script(script, priority=AudioPriority.HIGH)

        if self._behavior_mode != BehaviorMode.SUMMONING:
            self._set_behavior_mode(BehaviorMode.IDLE, apply_visual=False)
        self._apply_behavior_mode_visual()
        self._set_entity_autonomous(True)
        self._auto_dismiss_timer.start(self._auto_dismiss_ms)

    def _enter_fleeing(self) -> None:
        self._stop_auto_dismiss_timer()
        self._stop_camera_tracking()
        self._set_entity_autonomous(False)
        self._set_behavior_mode(BehaviorMode.BUSY, apply_visual=False)
        self._set_entity_state("state4")

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
        self._track_expression_state(gaze_data)
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
        self._stable_expression = "neutral"
        self._expression_votes = {"happy": 0, "neutral": 0, "angry": 0}

    @Slot()
    def _on_audio_output_started(self) -> None:
        if not self._audio_output_reactive:
            return
        if self._self_playback_active:
            self.LOGGER.debug("[AudioOutputMonitor] 忽略本进程语音播放触发的音频输出")
            return
        if self._state_machine.current_state == EntityState.HIDDEN:
            self._audio_forced_visible = bool(self.summon_now())
        self._audio_output_active = True
        if self._gif_state_mapper:
            self._gif_state_mapper.on_audio_started()
        self._set_behavior_mode(BehaviorMode.MEDIA_PLAYING)

    @Slot()
    def _on_audio_output_stopped(self) -> None:
        if not self._audio_output_reactive:
            return
        if self._self_playback_active:
            return
        self._audio_output_active = False
        if self._gif_state_mapper:
            self._gif_state_mapper.on_audio_stopped()
        if (
            self._audio_forced_visible
            and self._state_machine.current_state in (EntityState.PEEKING, EntityState.ENGAGED)
        ):
            self._audio_forced_visible = False
            self._state_machine.transition_to(EntityState.HIDDEN)
            return
        self._audio_forced_visible = False
        if self._behavior_mode == BehaviorMode.SUMMONING:
            return
        self._set_behavior_mode(BehaviorMode.IDLE if self._state_machine.current_state != EntityState.HIDDEN else BehaviorMode.BUSY)

    @Slot(str)
    def _on_self_playback_started(self, _path: str) -> None:
        self._self_playback_active = True
        if self._audio_output_active:
            self._audio_output_active = False
            if self._gif_state_mapper:
                self._gif_state_mapper.on_audio_stopped()
            if self._behavior_mode == BehaviorMode.MEDIA_PLAYING:
                fallback = BehaviorMode.IDLE if self._state_machine.current_state != EntityState.HIDDEN else BehaviorMode.BUSY
                self._set_behavior_mode(fallback)

    @Slot()
    def _on_self_playback_finished(self) -> None:
        self._self_playback_active = False
        # If the user is really playing media in parallel, recover MEDIA_PLAYING immediately.
        if (
            self._audio_output_reactive
            and self._audio_output_monitor is not None
            and self._audio_output_monitor.is_playing
        ):
            self._on_audio_output_started()

    @Slot()
    def _on_auto_dismiss_timeout(self) -> None:
        if self._state_machine.current_state == EntityState.ENGAGED:
            self._mood_system.on_dismissed()
            self._audio_manager.interrupt()
            self._state_machine.transition_to(EntityState.HIDDEN)

    @Slot()
    def _on_voice_trajectory_timeout(self) -> None:
        if not self._voice_trajectory_playing:
            return
        self.LOGGER.warning("[SummonTrajectory] 剧本式登场轨迹播放超时，强制进入 ENGAGED。")
        self._cleanup_voice_trajectory_player()
        self._complete_voice_scripted_entrance()

    @Slot()
    def _on_flee_finished(self) -> None:
        if self._state_machine.current_state == EntityState.FLEEING:
            self._state_machine.transition_to(EntityState.HIDDEN)

    @Slot(object)
    def _on_voice_trajectory_finished(self, player_obj: object) -> None:
        if not self._voice_trajectory_playing:
            return
        if self._voice_trajectory_player is not None and player_obj is not self._voice_trajectory_player:
            return
        self.LOGGER.info("[SummonTrajectory] 剧本式登场轨迹播放完成，进入 ENGAGED。")
        self._cleanup_voice_trajectory_player()
        self._complete_voice_scripted_entrance()

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

    def _set_visual_from_script(self, script: Script) -> None:
        self._set_entity_state("state1")
        if script.sprite_path and hasattr(self._entity_window, "set_sprite_content"):
            try:
                self._entity_window.set_sprite_content(script.sprite_path)
                self._current_ascii_template = ""
                return
            except Exception:
                pass
        self._current_ascii_template = self._resolve_ascii_content(script)
        self._entity_window.set_ascii_content(self._apply_current_gaze(self._current_ascii_template))

    def _set_behavior_mode(self, mode: BehaviorMode, *, apply_visual: bool = True) -> None:
        changed = self._behavior_mode != mode
        self._behavior_mode = mode
        if changed:
            self.LOGGER.info("[BehaviorMode] %s", mode.name)
        if apply_visual:
            self._apply_behavior_mode_visual()

    def _resolve_effective_behavior_mode(self) -> BehaviorMode:
        if self._voice_trajectory_playing:
            return BehaviorMode.SUMMONING
        if self._audio_output_active:
            return BehaviorMode.MEDIA_PLAYING
        return self._behavior_mode

    def _apply_behavior_mode_visual(self) -> None:
        state = self._state_machine.current_state
        if state in (EntityState.HIDDEN, EntityState.FLEEING):
            return
        mode = self._resolve_effective_behavior_mode()
        state_name = {
            BehaviorMode.IDLE: "state1",
            BehaviorMode.BUSY: "state4",
            BehaviorMode.MEDIA_PLAYING: "state3",
            BehaviorMode.SUMMONING: "state6",
        }[mode]
        self._set_entity_state(state_name, as_base=(mode == BehaviorMode.IDLE))

    def _set_entity_state(self, state_name: str, *, as_base: bool = True) -> bool:
        if not hasattr(self._entity_window, "set_state_by_name"):
            return False
        try:
            return bool(self._entity_window.set_state_by_name(state_name, as_base=as_base))
        except Exception:
            return False

    def _set_entity_autonomous(self, enabled: bool) -> None:
        if not hasattr(self._entity_window, "set_autonomous_enabled"):
            return
        try:
            self._entity_window.set_autonomous_enabled(enabled)
        except Exception:
            return

    def _apply_current_gaze(self, ascii_template: str) -> str:
        if self._ascii_renderer is None or not self._eye_tracking_enabled:
            return ascii_template
        return self._ascii_renderer.apply_eye_tracking(ascii_template, self._latest_gaze_data.face_x, eye_width=5)

    def _track_expression_state(self, gaze_data: GazeData) -> None:
        if self._state_machine.current_state not in (EntityState.PEEKING, EntityState.ENGAGED):
            return
        if self._resolve_effective_behavior_mode() != BehaviorMode.IDLE:
            return

        label = (gaze_data.emotion_label or "").strip().lower()
        if not gaze_data.face_detected or label not in self.EXPRESSION_STATE_MAP:
            label = "neutral"
        weight = 2 if float(gaze_data.emotion_score) >= 0.55 else 1

        for key in self._expression_votes:
            self._expression_votes[key] = max(0, self._expression_votes[key] - 1)
        self._expression_votes[label] = min(8, self._expression_votes[label] + weight)

        winner = max(self._expression_votes.items(), key=lambda item: item[1])[0]
        if self._expression_votes[winner] < 3:
            return

        now = time.monotonic()
        if winner == self._stable_expression and now - self._last_expression_visual_at < 0.8:
            return

        self._stable_expression = winner
        self._last_expression_visual_at = now
        state_name = self.EXPRESSION_STATE_MAP.get(winner, "state1")
        self._set_entity_state(state_name, as_base=False)

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

    @Slot(object, object)
    def _on_state_changed_for_particles(self, old_state, new_state) -> None:
        """Forward state changes to GIF particle mapper."""
        if self._gif_state_mapper is None:
            return
        if new_state in (EntityState.PEEKING, EntityState.ENGAGED):
            self._gif_state_mapper.on_engaged()
        elif new_state == EntityState.FLEEING:
            self._gif_state_mapper.on_fleeing()
        elif new_state == EntityState.HIDDEN:
            self._gif_state_mapper.on_hidden()

    @Slot()
    def _on_prolonged_idle(self) -> None:
        """Called when user has been idle for an extended period."""
        if self._gif_state_mapper and self._state_machine.current_state == EntityState.HIDDEN:
            self._gif_state_mapper.on_prolonged_idle()
            # Re-arm for another cycle
            self._prolonged_idle_timer.start()

    def _try_start_voice_scripted_entrance(self) -> bool:
        if self._voice_trajectory_playing:
            return True

        trajectory_path = self._resolve_voice_trajectory_path()
        if trajectory_path is None:
            self.LOGGER.info("[SummonTrajectory] 未找到剧本轨迹文件，回退普通召唤。")
            return False

        trajectory_data = self._load_trajectory_data(trajectory_path)
        if trajectory_data is None:
            self.LOGGER.warning("[SummonTrajectory] 轨迹文件无效，回退普通召唤: %s", trajectory_path)
            return False

        gif_map = self._build_voice_trajectory_gif_map()
        if not gif_map:
            self.LOGGER.warning("[SummonTrajectory] 缺少轨迹状态 GIF 映射，回退普通召唤。")
            return False

        try:
            if hasattr(self._entity_window, "hide_now"):
                self._entity_window.hide_now()
            else:
                self._entity_window.hide()
        except Exception:
            pass

        self._stop_auto_dismiss_timer()
        self._set_entity_autonomous(False)
        self._silent_presence_mode = False
        self._set_behavior_mode(BehaviorMode.SUMMONING, apply_visual=False)

        try:
            player = TrajectoryPlayer(trajectory_data, gif_map)
            player.finished.connect(self._on_voice_trajectory_finished)
            self._voice_trajectory_player = player
            self._voice_trajectory_playing = True
            total_duration = float(trajectory_data.get("total_duration", 0.0))
            timeout_ms = max(3000, int((total_duration + 2.0) * 1000))
            self._voice_trajectory_timeout.start(timeout_ms)
            player.start()
            self.LOGGER.info(
                "[SummonTrajectory] 已启动剧本式登场轨迹: %s (duration=%.2fs)",
                trajectory_path,
                total_duration,
            )
            return True
        except Exception as exc:
            self.LOGGER.exception("[SummonTrajectory] 启动剧本式登场失败: %s", exc)
            self._cleanup_voice_trajectory_player()
            return False

    def _complete_voice_scripted_entrance(self) -> None:
        if self._gif_state_mapper:
            self._gif_state_mapper.on_summoned()
        self._set_behavior_mode(BehaviorMode.SUMMONING, apply_visual=False)
        state = self._state_machine.current_state
        if state == EntityState.FLEEING:
            return
        if state == EntityState.HIDDEN:
            self._state_machine.transition_to(EntityState.ENGAGED)
            return
        if state == EntityState.PEEKING:
            self._state_machine.transition_to(EntityState.ENGAGED)
            return
        if state == EntityState.ENGAGED:
            self._apply_behavior_mode_visual()
            self._set_entity_autonomous(True)
            self._auto_dismiss_timer.start(self._auto_dismiss_ms)

    def _cleanup_voice_trajectory_player(self) -> None:
        if self._voice_trajectory_timeout.isActive():
            self._voice_trajectory_timeout.stop()
        player = self._voice_trajectory_player
        self._voice_trajectory_player = None
        self._voice_trajectory_playing = False
        if player is not None:
            try:
                player.finished.disconnect(self._on_voice_trajectory_finished)
            except Exception:
                pass
            try:
                player.force_dismiss()
            except Exception:
                pass

    def _stop_voice_scripted_entrance(self) -> None:
        if not self._voice_trajectory_playing and self._voice_trajectory_player is None:
            return
        self._cleanup_voice_trajectory_player()

    def _resolve_voice_trajectory_path(self) -> Path | None:
        filename = self.VOICE_TRAJECTORY_FILE
        candidates: list[Path] = []
        seen: set[str] = set()

        def _append(path: Path) -> None:
            try:
                key = str(path.resolve())
            except Exception:
                key = str(path)
            if key in seen:
                return
            seen.add(key)
            candidates.append(path)

        env_path = os.environ.get("CYBERCOMPANION_TRAJECTORY_PATH", "").strip()
        if env_path:
            env_candidate = Path(env_path)
            if env_candidate.suffix.lower() == ".json":
                _append(env_candidate)
            else:
                _append(env_candidate / filename)

        _append(self._base_dir / "recorded_paths" / filename)
        _append(Path.cwd() / "recorded_paths" / filename)
        _append(Path.cwd() / filename)

        for parent in [self._base_dir, *self._base_dir.parents[:5]]:
            _append(parent / "recorded_paths" / filename)
            _append(parent / filename)

        try:
            exe_parent = Path(sys.executable).resolve().parent
            for parent in [exe_parent, *exe_parent.parents[:5]]:
                _append(parent / "recorded_paths" / filename)
                _append(parent / filename)
        except Exception:
            pass

        try:
            _append(get_user_data_dir() / "recorded_paths" / filename)
        except Exception:
            pass

        for candidate in candidates:
            if candidate.exists() and candidate.is_file():
                self.LOGGER.info("[SummonTrajectory] 使用剧本轨迹文件: %s", candidate)
                return candidate
        self.LOGGER.debug(
            "[SummonTrajectory] 剧本轨迹候选路径均不存在: %s",
            " | ".join(str(item) for item in candidates[:12]),
        )
        return None

    def _load_trajectory_data(self, trajectory_path: Path) -> dict | None:
        try:
            payload = json.loads(trajectory_path.read_text(encoding="utf-8"))
        except Exception:
            return None
        if not isinstance(payload, dict):
            return None
        points = payload.get("points")
        if not isinstance(points, list) or not points:
            return None
        return payload

    def _build_voice_trajectory_gif_map(self) -> dict[int, str]:
        characters_root = self._base_dir / "characters"
        mapping: dict[int, str] = {}
        for idx in range(1, 8):
            path = characters_root / f"state{idx}.gif"
            if path.exists():
                mapping[idx] = str(path)
        state8 = characters_root / "aemeath.gif"
        if state8.exists():
            mapping[8] = str(state8)
        return mapping
