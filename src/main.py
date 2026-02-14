import ctypes
import ctypes.wintypes
import os
import subprocess
import sys
import threading

# High-DPI setup must happen before creating QApplication.
os.environ.setdefault("QT_ENABLE_HIGHDPI_SCALING", "1")
os.environ.setdefault("QT_SCALE_FACTOR_ROUNDING_POLICY", "PassThrough")

from PySide6.QtWidgets import QApplication, QMenu, QMessageBox, QSystemTrayIcon
from PySide6.QtCore import QAbstractNativeEventFilter, QObject, QTimer, Signal

try:
    from ai.gaze_tracker import GazeTracker
    from ai.llm_provider import DummyProvider, LLMProvider, OpenAIProvider
    from ai.screen_commentator import ScreenCommentator
    from core.asset_manager import AssetManager
    from core.audio_manager import AudioManager
    from core.audio_detector import AudioDetector
    from core.character_loader import CharacterLoader
    from core.command_matcher import VoiceCommandMatcher
    from core.config_manager import ConfigManager
    from core.director import Director, ScriptedEntranceError
    from core.gif_state_mapper import GifStateMapper
    from core.idle_monitor import IdleMonitor
    from core.logger import setup_logger
    from core.mood_system import MoodSystem
    from core.paths import get_base_dir, get_cache_dir, get_log_dir, get_log_file, resolve_config_path
    from core.presence_detector import PresenceDetector
    from core.resource_scheduler import ResourceScheduler
    from core.voice_wakeup import VoiceWakeupListener
    from ui.ascii_renderer import AsciiRenderer
    from ui.entity_window import EntityWindow
    from ui.gif_particle import GifParticleManager
    from ui.settings_dialog import SettingsDialog
    from ui.tray_icon import SystemTrayManager
except ModuleNotFoundError:
    from .ai.gaze_tracker import GazeTracker
    from .ai.llm_provider import DummyProvider, LLMProvider, OpenAIProvider
    from .ai.screen_commentator import ScreenCommentator
    from .core.asset_manager import AssetManager
    from .core.audio_manager import AudioManager
    from .core.audio_detector import AudioDetector
    from .core.character_loader import CharacterLoader
    from .core.command_matcher import VoiceCommandMatcher
    from .core.config_manager import ConfigManager
    from .core.director import Director, ScriptedEntranceError
    from .core.gif_state_mapper import GifStateMapper
    from .core.idle_monitor import IdleMonitor
    from .core.logger import setup_logger
    from .core.mood_system import MoodSystem
    from .core.paths import get_base_dir, get_cache_dir, get_log_dir, get_log_file, resolve_config_path
    from .core.presence_detector import PresenceDetector
    from .core.resource_scheduler import ResourceScheduler
    from .core.voice_wakeup import VoiceWakeupListener
    from .ui.ascii_renderer import AsciiRenderer
    from .ui.entity_window import EntityWindow
    from .ui.gif_particle import GifParticleManager
    from .ui.settings_dialog import SettingsDialog
    from .ui.tray_icon import SystemTrayManager


def _build_llm_provider(config) -> LLMProvider:
    if bool(getattr(config.behavior, "offline_mode", False)):
        return DummyProvider()

    provider = (config.llm.provider or "none").lower()
    if provider in {"openai", "xai", "deepseek"}:
        candidate = OpenAIProvider(
            model=config.llm.model or "grok-4-fast-reasoning",
            api_key=(
                config.llm.api_key
                or os.environ.get("OPENAI_API_KEY", "")
                or os.environ.get("POLOAI_API_KEY", "")
                or os.environ.get("XAI_API_KEY", "")
            ),
            base_url=config.llm.base_url,
        )
        return candidate if candidate.is_available() else DummyProvider()
    return DummyProvider()


def _ensure_camera_consent(config) -> bool:
    if not config.vision.camera_enabled or config.vision.camera_consent_granted:
        return False
    consent = QMessageBox.question(
        None,
        "摄像头授权",
        "是否启用摄像头视线追踪？\n摄像头帧只在本地内存实时处理，不保存、不上传。",
        QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
        QMessageBox.StandardButton.No,
    )
    if consent == QMessageBox.StandardButton.Yes:
        config.vision.camera_consent_granted = True
    else:
        config.vision.camera_enabled = False
    return True


def _migrate_legacy_asr_defaults(config) -> bool:
    provider = str(getattr(config.audio, "asr_provider", "") or "").strip().lower()
    model = str(getattr(config.audio, "asr_model", "") or "").strip().lower()
    base_url = str(getattr(config.audio, "asr_base_url", "") or "").strip().lower().rstrip("/")
    api_key = str(getattr(config.audio, "asr_api_key", "") or "").strip()
    voice_mode = str(getattr(config.audio, "voice_input_mode", "") or "").strip().lower()
    changed = False

    legacy_provider = provider in {"", "xai_realtime"}
    legacy_model = model in {"", "grok-2-mini-transcribe", "whisper-1"}
    legacy_base = base_url in {"", "https://api.x.ai/v1"}
    legacy_mode = voice_mode in {"", "continuous"}

    if legacy_provider and legacy_model and legacy_base and legacy_mode and not api_key:
        config.audio.voice_input_mode = "push_to_talk"
        config.audio.asr_provider = "zhipu_asr"
        config.audio.asr_model = "glm-asr-2512"
        config.audio.asr_base_url = "https://open.bigmodel.cn/api/paas/v4/audio/transcriptions"
        changed = True

    # Handle partially migrated configs where provider is zhipu but model/base are still xAI defaults.
    if provider == "zhipu_asr":
        if model in {"", "grok-2-mini-transcribe", "whisper-1"}:
            config.audio.asr_model = "glm-asr-2512"
            changed = True
        if base_url in {"", "https://api.x.ai/v1"} or "x.ai" in base_url:
            config.audio.asr_base_url = "https://open.bigmodel.cn/api/paas/v4/audio/transcriptions"
            changed = True

    return changed


def main() -> int:
    app = QApplication(sys.argv)
    splash = None
    try:
        import pyi_splash  # type: ignore

        pyi_splash.update_text("正在初始化赛博伴侣...")
        splash = pyi_splash
    except Exception:
        splash = None

    base_dir = get_base_dir()
    characters_root = base_dir / "characters"
    config_path = resolve_config_path()
    config_manager = ConfigManager(config_path)
    config = config_manager.load()
    if _migrate_legacy_asr_defaults(config):
        config_manager.save(config)
    if config.llm.api_key:
        os.environ["OPENAI_API_KEY"] = config.llm.api_key
        os.environ["POLOAI_API_KEY"] = config.llm.api_key
        os.environ["XAI_API_KEY"] = config.llm.api_key
    else:
        os.environ.pop("OPENAI_API_KEY", None)
        os.environ.pop("POLOAI_API_KEY", None)
        os.environ.pop("XAI_API_KEY", None)
    logger = setup_logger(get_log_dir(), debug=config.behavior.debug_mode)
    log_file = get_log_file()
    logger.info("Application starting. base_dir=%s config=%s", base_dir, config_path)

    if _ensure_camera_consent(config):
        config_manager.save(config)

    loader = CharacterLoader(characters_root)
    manifests = loader.scan_characters()
    package = loader.load_character(config.appearance.theme) or loader.load_character("default")
    character_dir = package.root_dir if package else characters_root / "default"
    character_voice = str(package.manifest.get("default_voice", config.audio.tts_voice)) if package else config.audio.tts_voice
    character_width = int(package.manifest.get("ascii_width", config.appearance.ascii_width)) if package else config.appearance.ascii_width
    active_character_id = package.character_id if package else "default"
    audio_cache = get_cache_dir() / active_character_id
    audio_cache.mkdir(parents=True, exist_ok=True)

    asset_manager = AssetManager(character_dir=character_dir)
    ascii_renderer = AsciiRenderer(
        width=character_width,
        font_size_px=config.appearance.font_size_px,
    )
    entity_window = EntityWindow()
    def _resolve_state_asset_root(primary_dir):
        candidates = [
            primary_dir,
            primary_dir / "assets",
            primary_dir / "assets" / "sprites",
            characters_root,
        ]
        for candidate in candidates:
            path = candidate / "state1.gif"
            if path.exists():
                return candidate
        return characters_root

    if hasattr(entity_window, "set_state_asset_root"):
        entity_window.set_state_asset_root(_resolve_state_asset_root(character_dir))
    if hasattr(entity_window, "preload_state_movies"):
        entity_window.preload_state_movies()
    audio_manager = AudioManager(
        cache_dir=audio_cache,
        voice=character_voice,
        voice_rate=config.audio.tts_rate,
        volume=config.audio.volume,
        cache_enabled=config.audio.cache_enabled,
    )
    gaze_tracker = GazeTracker(
        camera_index=config.vision.camera_index,
        target_fps=config.vision.target_fps,
    )
    llm_provider = _build_llm_provider(config)
    screen_commentator = ScreenCommentator(
        llm_provider=llm_provider,
        audio_manager=audio_manager,
        streaming_enabled=config.screen_commentary.streaming_enabled,
        ocr_fallback_enabled=False,
        stream_chunk_chars=config.screen_commentary.stream_chunk_chars,
        max_response_chars=config.screen_commentary.max_response_chars,
        preamble_text=config.screen_commentary.preamble_text,
    )

    # --- GIF Particle System ---
    particle_manager = GifParticleManager()
    gif_state_mapper = GifStateMapper(
        characters_dir=characters_root,  # characters/ dir contains state1-7.gif
        particle_manager=particle_manager,
    )
    audio_output_monitor = AudioDetector(
        poll_interval_ms=200,
        threshold=0.01,
    )

    director = Director(
        entity_window=entity_window,
        audio_manager=audio_manager,
        asset_manager=asset_manager,
        ascii_renderer=ascii_renderer,
        app_config=config,
        gaze_tracker=gaze_tracker,
        presence_detector=PresenceDetector(),
        mood_system=MoodSystem(),
        resource_scheduler=ResourceScheduler(),
        screen_commentator=screen_commentator,
        gif_state_mapper=gif_state_mapper,
        audio_output_monitor=audio_output_monitor,
    )
    idle_monitor = IdleMonitor(threshold_ms=config.trigger.idle_threshold_seconds * 1000)
    director.bind_idle_monitor(idle_monitor)
    idle_monitor.start()

    tray_manager: SystemTrayManager | None = None
    voice_listener: VoiceWakeupListener | None = None
    _ptt_busy = False
    _ptt_lock = threading.Lock()

    class _PushToTalkBridge(QObject):
        result = Signal(str)
        error = Signal(str)

    ptt_bridge = _PushToTalkBridge()

    def _notify(title: str, message: str, timeout_ms: int = 3000) -> None:
        if tray_manager is not None:
            tray_manager.show_message(title, message, timeout_ms=timeout_ms)

    def _summon_now_or_notify(source: str) -> bool:
        try:
            return bool(director.summon_now())
        except ScriptedEntranceError as exc:
            logger.error("[Summon] failed source=%s error=%s", source, exc)
            _notify("召唤失败", str(exc), timeout_ms=4200)
            return False
        except Exception as exc:
            logger.exception("[Summon] unexpected failure source=%s: %s", source, exc)
            _notify("召唤失败", f"{exc}", timeout_ms=4200)
            return False

    def _resolve_asr_runtime() -> tuple[str, str]:
        asr_key = (
            config.audio.asr_api_key
            or config.llm.api_key
            or os.environ.get("ZHIPU_API_KEY", "")
            or os.environ.get("OPENAI_API_KEY", "")
            or os.environ.get("POLOAI_API_KEY", "")
            or os.environ.get("XAI_API_KEY", "")
        )
        if (config.audio.asr_provider or "").lower() == "zhipu_asr":
            default_base = "https://open.bigmodel.cn/api/paas/v4/audio/transcriptions"
        else:
            default_base = config.llm.base_url or "https://api.x.ai/v1"
        asr_base_url = config.audio.asr_base_url or default_base
        return asr_key, asr_base_url

    def _execute_voice_command(text: str, *, source: str) -> bool:
        cleaned = (text or "").strip()
        if not cleaned:
            return False
        match = VoiceCommandMatcher.match(cleaned, min_score=66)
        if match is None:
            logger.info("[VoiceCommand] no-match source=%s text=%s", source, cleaned)
            return False

        logger.info(
            "[VoiceCommand] matched source=%s action=%s score=%d phrase=%s text=%s",
            source,
            match.action,
            match.score,
            match.phrase,
            cleaned,
        )
        if match.action == "summon":
            _summon_now_or_notify(f"voice:{source}")
        elif match.action == "screen_commentary":
            if _summon_now_or_notify(f"voice:{source}"):
                QTimer.singleShot(700, director.request_screen_commentary)
        elif match.action == "hide":
            if getattr(director.current_state, "name", "") != "HIDDEN":
                director.toggle_visibility()
        elif match.action == "toggle_visibility":
            director.toggle_visibility()
        elif match.action == "status":
            _notify("状态", director.get_status_summary(), timeout_ms=4500)
        else:
            return False
        _notify("语音命令", f"{cleaned}\n→ {match.action} ({match.score})", timeout_ms=1800)
        return True

    def _start_push_to_talk_once() -> None:
        nonlocal _ptt_busy
        if not config.audio.microphone_enabled:
            _notify("语音转写", "麦克风未启用，请在设置中打开。", timeout_ms=2400)
            return
        if (config.audio.voice_input_mode or "").lower() != "push_to_talk":
            _notify("语音转写", "当前是连续唤醒模式，B 键单次转写未启用。", timeout_ms=2200)
            return

        with _ptt_lock:
            if _ptt_busy:
                logger.debug("[PushToTalk] 忽略重复触发：当前仍在转写中。")
                return
            _ptt_busy = True

        _notify("语音转写", "开始收音，请说话…", timeout_ms=1200)

        def _worker() -> None:
            nonlocal _ptt_busy
            try:
                asr_key, asr_base_url = _resolve_asr_runtime()
                text = VoiceWakeupListener.transcribe_once(
                    language=config.wakeup.language,
                    recognition_provider=config.audio.asr_provider,
                    openai_api_key=asr_key,
                    openai_base_url=asr_base_url,
                    openai_model=config.audio.asr_model,
                    openai_prompt=config.audio.asr_prompt,
                    openai_temperature=config.audio.asr_temperature,
                    listen_timeout_seconds=6.0,
                    phrase_time_limit_seconds=12.0,
                )
                ptt_bridge.result.emit(text)
            except Exception as exc:
                ptt_bridge.error.emit(str(exc))
            finally:
                with _ptt_lock:
                    _ptt_busy = False

        threading.Thread(target=_worker, daemon=True, name="PushToTalk").start()

    def _on_ptt_result(payload: str) -> None:
        text = (payload or "").strip()
        if not text:
            _notify("语音转写", "未识别到有效语音，请重试。", timeout_ms=2200)
            return
        logger.info("[PushToTalk] transcript=%s", text)
        _notify("语音转写", text, timeout_ms=1800)
        if not _execute_voice_command(text, source="push_to_talk"):
            _notify("语音命令", f"未匹配到动作：{text}", timeout_ms=2600)

    def _on_ptt_error(message: str) -> None:
        _notify("语音转写失败", message, timeout_ms=3500)

    ptt_bridge.result.connect(_on_ptt_result)
    ptt_bridge.error.connect(_on_ptt_error)

    def _open_logs_location() -> None:
        target = log_file.parent
        try:
            if sys.platform == "win32":
                os.startfile(str(target))  # type: ignore[attr-defined]
            elif sys.platform == "darwin":
                subprocess.Popen(["open", str(target)])
            else:
                subprocess.Popen(["xdg-open", str(target)])
        except Exception as exc:
            logger.warning("Failed to open log directory: %s", exc)
            _notify("日志目录", f"打开失败，请手动查看: {target}", timeout_ms=6000)

    def _stop_voice_listener() -> None:
        nonlocal voice_listener
        if voice_listener is None:
            return
        voice_listener.stop_listening()
        voice_listener.deleteLater()
        voice_listener = None

    def _start_voice_listener() -> None:
        nonlocal voice_listener, config
        _stop_voice_listener()
        if (
            not config.audio.microphone_enabled
            or not config.wakeup.enabled
            or (config.audio.voice_input_mode or "").lower() != "continuous"
        ):
            return

        asr_key, asr_base_url = _resolve_asr_runtime()

        listener = VoiceWakeupListener(
            phrases=config.wakeup.phrases,
            language=config.wakeup.language,
            recognition_provider=config.audio.asr_provider,
            openai_api_key=asr_key,
            openai_base_url=asr_base_url,
            openai_model=config.audio.asr_model,
            openai_prompt=config.audio.asr_prompt,
            openai_temperature=config.audio.asr_temperature,
        )

        def _on_wakeup_hit(heard: str) -> None:
            logger.info("Wake phrase detected: %s", heard)
            matched = _execute_voice_command(heard, source="wakeup")
            if not matched:
                _summon_now_or_notify("wakeup")
            lowered = (heard or "").strip().lower()
            if (not matched) and any(keyword in lowered for keyword in ("看屏幕", "看看屏幕", "你在看什么", "屏幕上", "screen")):
                logger.info("[VoiceWakeup] Wake phrase includes screen intent, auto request commentary")
                QTimer.singleShot(1200, director.request_screen_commentary)
            _notify("语音唤醒", f"已识别: {heard}", timeout_ms=2000)

        def _on_wakeup_error(message: str) -> None:
            # Only degrade for the current session — do NOT persist to config file.
            # Users can still re-enable via settings or restart the app to retry.
            logger.warning("Voice wakeup degraded (session only): %s", message)
            _stop_voice_listener()
            _notify("语音降级", f"{message}\n重启应用可重新尝试。", timeout_ms=5000)

        def _on_transcript(text: str) -> None:
            logger.info("[VoiceWakeup] 实时转写: \"%s\"", text)
            if config.behavior.debug_mode:
                _notify("语音转写", text, timeout_ms=1500)

        listener.wake_phrase_detected.connect(_on_wakeup_hit)
        listener.listener_error.connect(_on_wakeup_error)
        listener.transcript_updated.connect(_on_transcript)
        listener.start_listening()
        voice_listener = listener
        logger.info("Voice wakeup listener started successfully.")

    def _apply_runtime_settings(*, notify: bool = True) -> bool:
        nonlocal llm_provider, config
        if config.llm.api_key:
            os.environ["OPENAI_API_KEY"] = config.llm.api_key
            os.environ["POLOAI_API_KEY"] = config.llm.api_key
            os.environ["XAI_API_KEY"] = config.llm.api_key
        else:
            os.environ.pop("OPENAI_API_KEY", None)
            os.environ.pop("POLOAI_API_KEY", None)
            os.environ.pop("XAI_API_KEY", None)
        if _ensure_camera_consent(config):
            logger.info("Camera consent updated via runtime settings.")
        if not config_manager.save(config):
            _notify("设置", "保存配置失败，请检查权限。", timeout_ms=5000)
            return False

        audio_manager.set_voice(config.audio.tts_voice)
        audio_manager.set_rate(config.audio.tts_rate)
        audio_manager.set_volume(config.audio.volume)
        audio_manager.set_cache_enabled(config.audio.cache_enabled)
        gaze_tracker.set_camera_config(config.vision.camera_index, config.vision.target_fps)
        llm_provider = _build_llm_provider(config)
        screen_commentator.set_llm_provider(llm_provider)
        screen_commentator.set_runtime_options(
            streaming_enabled=config.screen_commentary.streaming_enabled,
            ocr_fallback_enabled=False,
            stream_chunk_chars=config.screen_commentary.stream_chunk_chars,
            max_response_chars=config.screen_commentary.max_response_chars,
            preamble_text=config.screen_commentary.preamble_text,
        )
        director.apply_runtime_config(config)
        _start_voice_listener()
        if notify:
            _notify("设置", "已保存并应用。", timeout_ms=2200)
        return True

    def _on_llm_error(message: str) -> None:
        nonlocal config
        lowered = (message or "").lower()
        network_like = any(
            key in lowered
            for key in ("timeout", "timed out", "connection", "network", "dns", "ssl", "refused", "proxy")
        )
        if not network_like or config.behavior.offline_mode:
            return
        logger.warning("LLM network degraded, switching to offline mode: %s", message)
        config.behavior.offline_mode = True
        _apply_runtime_settings(notify=False)
        _notify("离线模式", "网络异常，已切换为本地规则回复。", timeout_ms=4500)

    screen_commentator.set_llm_error_callback(_on_llm_error)

    def _on_screen_capture_error(message: str) -> None:
        logger.warning("Screen capture failed: %s", message)
        _notify("屏幕捕获失败", f"{message}\n日志: {log_file}", timeout_ms=6500)

    screen_commentator.set_capture_error_callback(_on_screen_capture_error)

    def _open_settings() -> None:
        nonlocal config
        dialog = SettingsDialog(config=config)
        if not dialog.exec():
            return
        config = dialog.to_config()
        _apply_runtime_settings(notify=True)

    if QSystemTrayIcon.isSystemTrayAvailable():
        icon_path = str((base_dir / "assets" / "icon.ico").resolve()) if (base_dir / "assets" / "icon.ico").exists() else None
        tray_manager = SystemTrayManager(app, icon_path=icon_path)
        tray_manager.update_characters(manifests)
        tray_manager.summon_requested.connect(lambda: _summon_now_or_notify("tray"))
        tray_manager.commentary_requested.connect(director.request_screen_commentary)
        tray_manager.toggle_requested.connect(director.toggle_visibility)
        tray_manager.status_requested.connect(lambda: tray_manager.show_message("状态", director.get_status_summary()))
        tray_manager.open_logs_requested.connect(_open_logs_location)
        tray_manager.quit_requested.connect(app.quit)

        tray_manager.settings_requested.connect(_open_settings)

        def _switch_character(character_id: str) -> None:
            pkg = loader.load_character(character_id)
            if pkg is None:
                tray_manager.show_message("切换角色", f"角色加载失败: {character_id}")
                return

            local_asset_mgr = AssetManager(pkg.root_dir)
            local_renderer = AsciiRenderer(
                width=int(pkg.manifest.get("ascii_width", config.appearance.ascii_width)),
                font_size_px=config.appearance.font_size_px,
            )
            director.switch_character(
                asset_manager=local_asset_mgr,
                ascii_renderer=local_renderer,
                voice=str(pkg.manifest.get("default_voice", character_voice)),
            )
            if hasattr(entity_window, "set_state_asset_root"):
                entity_window.set_state_asset_root(_resolve_state_asset_root(pkg.root_dir))
            if hasattr(entity_window, "preload_state_movies"):
                entity_window.preload_state_movies()
            tray_manager.show_message("切换角色", f"已切换到: {pkg.manifest.get('name', pkg.character_id)}")
            logger.info("Character switched to %s", pkg.character_id)

        tray_manager.character_switch_requested.connect(_switch_character)
        tray_manager.show()

    def _show_entity_context_menu(global_pos) -> None:
        menu = QMenu()
        toggle_action = menu.addAction("显示/隐藏")
        summon_action = menu.addAction("立即召唤")
        commentary_action = menu.addAction("你在看什么？")
        open_logs_action = menu.addAction("打开日志目录")
        settings_action = menu.addAction("设置")
        menu.addSeparator()
        quit_action = menu.addAction("退出")

        chosen = menu.exec(global_pos)
        if chosen == toggle_action:
            director.toggle_visibility()
        elif chosen == summon_action:
            _summon_now_or_notify("context_menu")
        elif chosen == commentary_action:
            director.request_screen_commentary()
        elif chosen == open_logs_action:
            _open_logs_location()
        elif chosen == settings_action:
            _open_settings()
        elif chosen == quit_action:
            app.quit()

    entity_window.context_menu_requested.connect(_show_entity_context_menu)
    entity_window.double_clicked.connect(director.toggle_visibility)

    def _on_camera_error(message: str) -> None:
        logger.warning("Camera degraded (session only): %s", message)
        _notify("视觉降级", f"{message}\n本次会话已暂停摄像头，可在设置中重试。\n日志: {log_file}", timeout_ms=6500)

    def _on_camera_state_changed(running: bool) -> None:
        logger.info("Camera state changed: %s", "running" if running else "stopped")
        if config.behavior.debug_mode:
            tip = "摄像头已启动（仅在角色出现/互动时启用）" if running else "摄像头已停止"
            _notify("摄像头状态", tip, timeout_ms=2000)

    gaze_tracker.camera_error.connect(_on_camera_error)
    gaze_tracker.camera_state_changed.connect(_on_camera_state_changed)

    _apply_runtime_settings(notify=False)

    # --- Global Hotkeys: summon + push-to-talk ---
    HOTKEY_ID_SUMMON = 1
    HOTKEY_ID_PUSH_TO_TALK = 2
    MOD_CTRL_SHIFT = 0x0002 | 0x0004  # MOD_CONTROL | MOD_SHIFT
    MOD_NONE = 0x0000
    MOD_NOREPEAT = 0x4000
    VK_S = 0x53
    VK_B = 0x42
    _hotkey_registered_ids: list[int] = []
    _hotkey_filter: QAbstractNativeEventFilter | None = None

    def _unregister_hotkeys() -> None:
        nonlocal _hotkey_filter
        if sys.platform != "win32":
            return
        if _hotkey_filter is not None:
            try:
                app.removeNativeEventFilter(_hotkey_filter)
            except Exception:
                pass
            _hotkey_filter = None
        if not _hotkey_registered_ids:
            return
        try:
            user32 = ctypes.windll.user32
            for hotkey_id in list(_hotkey_registered_ids):
                try:
                    user32.UnregisterHotKey(None, hotkey_id)
                except Exception:
                    pass
        finally:
            _hotkey_registered_ids.clear()

    if sys.platform == "win32":
        class _WindowsHotkeyFilter(QAbstractNativeEventFilter):
            WM_HOTKEY = 0x0312

            def nativeEventFilter(self, event_type, message):
                try:
                    if event_type not in ("windows_generic_MSG", "windows_dispatcher_MSG"):
                        return False, 0
                    msg = ctypes.wintypes.MSG.from_address(int(message))
                    if int(msg.message) != self.WM_HOTKEY:
                        return False, 0
                    hotkey_id = int(msg.wParam)
                    if hotkey_id == HOTKEY_ID_SUMMON:
                        logger.info("[Hotkey] Ctrl+Shift+S pressed")
                        QTimer.singleShot(0, lambda: _summon_now_or_notify("hotkey"))
                        return True, 0
                    if hotkey_id == HOTKEY_ID_PUSH_TO_TALK:
                        logger.info("[Hotkey] B pressed")
                        QTimer.singleShot(0, _start_push_to_talk_once)
                        return True, 0
                except Exception as exc:
                    logger.debug("[Hotkey] nativeEventFilter error: %s", exc)
                return False, 0

        _hotkey_filter = _WindowsHotkeyFilter()
        app.installNativeEventFilter(_hotkey_filter)

        try:
            user32 = ctypes.windll.user32
            if user32.RegisterHotKey(None, HOTKEY_ID_SUMMON, MOD_CTRL_SHIFT, VK_S):
                _hotkey_registered_ids.append(HOTKEY_ID_SUMMON)
            else:
                logger.warning("[Hotkey] Failed to register Ctrl+Shift+S (may be in use by another app)")
            if user32.RegisterHotKey(None, HOTKEY_ID_PUSH_TO_TALK, MOD_NONE | MOD_NOREPEAT, VK_B):
                _hotkey_registered_ids.append(HOTKEY_ID_PUSH_TO_TALK)
            elif user32.RegisterHotKey(None, HOTKEY_ID_PUSH_TO_TALK, MOD_NONE, VK_B):
                _hotkey_registered_ids.append(HOTKEY_ID_PUSH_TO_TALK)
                logger.info("[Hotkey] B registered without MOD_NOREPEAT fallback")
            else:
                logger.warning("[Hotkey] Failed to register B for push-to-talk (may be in use by another app)")
            if _hotkey_registered_ids:
                logger.info("[Hotkey] ✅ 全局快捷键已注册: ids=%s", _hotkey_registered_ids)
            else:
                logger.warning("[Hotkey] 未成功注册任何全局快捷键")
        except Exception as exc:
            logger.error("[Hotkey] Failed to initialize native hotkey filter: %s", exc)
            _unregister_hotkeys()
    else:
        logger.info("[Hotkey] Global hotkeys are only enabled on Windows")

    # --- Startup notification about voice wakeup + hotkey ---
    _startup_parts: list[str] = []
    if config.vision.camera_enabled and config.vision.camera_consent_granted:
        _startup_parts.append("摄像头: 开 (仅在角色出现/互动时启用)")
    else:
        _startup_parts.append("摄像头: 关")
    if (config.audio.voice_input_mode or "").lower() == "push_to_talk":
        _startup_parts.append("语音模式: 按键转写 (全局 B 键)")
    elif config.wakeup.enabled and config.audio.microphone_enabled:
        _startup_parts.append(f"语音唤醒: 开 (唤醒词: {', '.join(config.wakeup.phrases)})")
    else:
        reasons = []
        if not config.wakeup.enabled:
            reasons.append("唤醒未启用")
        if not config.audio.microphone_enabled:
            reasons.append("麦克风未启用")
        _startup_parts.append(f"语音唤醒: 关 ({'; '.join(reasons)})")
    registered_shortcuts: list[str] = []
    if HOTKEY_ID_SUMMON in _hotkey_registered_ids:
        registered_shortcuts.append("Ctrl+Shift+S 召唤伴侣")
    if HOTKEY_ID_PUSH_TO_TALK in _hotkey_registered_ids:
        registered_shortcuts.append("B 单次语音转写")
    if registered_shortcuts:
        _startup_parts.append(f"快捷键: {'；'.join(registered_shortcuts)}")
    else:
        _startup_parts.append("快捷键: 未注册（可能被其他程序占用）")
    if config.behavior.debug_mode:
        _startup_parts.append("调试模式: 开 (转写结果将以通知显示)")
        _startup_parts.append(f"日志文件: {log_file}")
    _notify("赛博伴侣已启动", "\n".join(_startup_parts), timeout_ms=4000)

    if splash is not None:
        try:
            splash.close()
        except Exception:
            pass

    def _shutdown() -> None:
        logger.info("Application shutting down.")
        _unregister_hotkeys()
        director.shutdown()
        idle_monitor.stop()
        _stop_voice_listener()
        gaze_tracker.stop_tracking()
        audio_manager.stop()
        if tray_manager is not None:
            tray_manager.hide()

    app.aboutToQuit.connect(_shutdown)
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
