import os
import sys

# High-DPI setup must happen before creating QApplication.
os.environ.setdefault("QT_ENABLE_HIGHDPI_SCALING", "1")
os.environ.setdefault("QT_SCALE_FACTOR_ROUNDING_POLICY", "PassThrough")

from PySide6.QtWidgets import QApplication, QMessageBox, QSystemTrayIcon

try:
    from ai.gaze_tracker import GazeTracker
    from ai.llm_provider import DummyProvider, LLMProvider, OllamaProvider, OpenAIProvider
    from ai.screen_commentator import ScreenCommentator
    from core.asset_manager import AssetManager
    from core.audio_manager import AudioManager
    from core.character_loader import CharacterLoader
    from core.config_manager import ConfigManager
    from core.director import Director
    from core.idle_monitor import IdleMonitor
    from core.logger import setup_logger
    from core.mood_system import MoodSystem
    from core.paths import get_base_dir, get_cache_dir, get_log_dir, resolve_config_path
    from core.presence_detector import PresenceDetector
    from core.resource_scheduler import ResourceScheduler
    from ui.ascii_renderer import AsciiRenderer
    from ui.entity_window import EntityWindow
    from ui.tray_icon import SystemTrayManager
except ModuleNotFoundError:
    from .ai.gaze_tracker import GazeTracker
    from .ai.llm_provider import DummyProvider, LLMProvider, OllamaProvider, OpenAIProvider
    from .ai.screen_commentator import ScreenCommentator
    from .core.asset_manager import AssetManager
    from .core.audio_manager import AudioManager
    from .core.character_loader import CharacterLoader
    from .core.config_manager import ConfigManager
    from .core.director import Director
    from .core.idle_monitor import IdleMonitor
    from .core.logger import setup_logger
    from .core.mood_system import MoodSystem
    from .core.paths import get_base_dir, get_cache_dir, get_log_dir, resolve_config_path
    from .core.presence_detector import PresenceDetector
    from .core.resource_scheduler import ResourceScheduler
    from .ui.ascii_renderer import AsciiRenderer
    from .ui.entity_window import EntityWindow
    from .ui.tray_icon import SystemTrayManager


def _build_llm_provider(config) -> LLMProvider:
    provider = (config.llm.provider or "none").lower()
    if provider == "ollama":
        candidate = OllamaProvider(
            model=config.llm.model or "qwen2.5:7b",
            base_url=config.llm.ollama_base_url,
        )
        return candidate if candidate.is_available() else DummyProvider()
    if provider == "openai":
        candidate = OpenAIProvider(
            model=config.llm.model or "gpt-4o-mini",
            base_url=config.llm.base_url,
        )
        return candidate if candidate.is_available() else DummyProvider()
    return DummyProvider()


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
    config = ConfigManager(config_path).load()
    logger = setup_logger(get_log_dir(), debug=config.behavior.debug_mode)
    logger.info("Application starting. base_dir=%s config=%s", base_dir, config_path)

    if config.vision.camera_enabled and not config.vision.camera_consent_granted:
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
    audio_manager = AudioManager(
        cache_dir=audio_cache,
        voice=character_voice,
        voice_rate=config.audio.tts_rate,
        volume=config.audio.volume,
        cache_enabled=config.audio.cache_enabled,
    )
    gaze_tracker = None
    if config.vision.camera_enabled:
        gaze_tracker = GazeTracker(
            camera_index=config.vision.camera_index,
            target_fps=config.vision.target_fps,
        )
    llm_provider = _build_llm_provider(config)
    screen_commentator = ScreenCommentator(llm_provider=llm_provider, audio_manager=audio_manager)
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
    )
    idle_monitor = IdleMonitor(threshold_ms=config.trigger.idle_threshold_seconds * 1000)
    director.bind_idle_monitor(idle_monitor)
    idle_monitor.start()

    tray_manager: SystemTrayManager | None = None
    if QSystemTrayIcon.isSystemTrayAvailable():
        icon_path = str((base_dir / "assets" / "icon.ico").resolve()) if (base_dir / "assets" / "icon.ico").exists() else None
        tray_manager = SystemTrayManager(app, icon_path=icon_path)
        tray_manager.update_characters(manifests)
        tray_manager.summon_requested.connect(director.summon_now)
        tray_manager.commentary_requested.connect(director.request_screen_commentary)
        tray_manager.toggle_requested.connect(director.toggle_visibility)
        tray_manager.settings_requested.connect(lambda: tray_manager.show_message("设置", "设置窗口尚未实现"))
        tray_manager.status_requested.connect(lambda: tray_manager.show_message("状态", director.get_status_summary()))
        tray_manager.quit_requested.connect(app.quit)

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
            tray_manager.show_message("切换角色", f"已切换到: {pkg.manifest.get('name', pkg.character_id)}")
            logger.info("Character switched to %s", pkg.character_id)

        tray_manager.character_switch_requested.connect(_switch_character)
        tray_manager.show()

    if splash is not None:
        try:
            splash.close()
        except Exception:
            pass

    def _shutdown() -> None:
        logger.info("Application shutting down.")
        director.shutdown()
        idle_monitor.stop()
        if gaze_tracker is not None:
            gaze_tracker.stop_tracking()
        audio_manager.stop()
        if tray_manager is not None:
            tray_manager.hide()

    app.aboutToQuit.connect(_shutdown)
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
