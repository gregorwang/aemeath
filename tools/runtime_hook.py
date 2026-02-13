"""
PyInstaller runtime hook.
Executed before application imports.
"""

import os
import sys
from pathlib import Path


def _resolve_base_dir() -> Path:
    if getattr(sys, "frozen", False) and hasattr(sys, "_MEIPASS"):
        return Path(sys._MEIPASS)  # type: ignore[attr-defined]
    return Path(__file__).resolve().parents[1]


def _setup_qt_environment(base_dir: Path) -> None:
    """
    Force Qt plugin path to packaged PySide6 directory.
    This avoids plugin lookup conflicts in frozen mode.
    """
    plugins_root = base_dir / "PySide6" / "plugins"
    platforms_dir = plugins_root / "platforms"
    multimedia_dir = plugins_root / "multimedia"

    if plugins_root.exists():
        os.environ.setdefault("QT_PLUGIN_PATH", str(plugins_root))
    if platforms_dir.exists():
        os.environ.setdefault("QT_QPA_PLATFORM_PLUGIN_PATH", str(platforms_dir))
    if multimedia_dir.exists():
        os.environ.setdefault("QT_MULTIMEDIA_PLUGIN_PATH", str(multimedia_dir))


def _setup_comtypes_cache() -> None:
    """
    Ensure COM type cache is writable in frozen app.
    """
    local_app_data = os.environ.get("LOCALAPPDATA", "").strip()
    if local_app_data:
        cache_root = Path(local_app_data)
    else:
        cache_root = Path.home() / "AppData" / "Local"
    cache_dir = cache_root / "CyberCompanion" / "comtypes_cache"
    try:
        cache_dir.mkdir(parents=True, exist_ok=True)
    except OSError:
        return
    os.environ.setdefault("COMTYPES_CACHE", str(cache_dir))


def _setup_environment() -> None:
    base_dir = _resolve_base_dir()
    os.environ.setdefault("APP_BASE_DIR", str(base_dir))
    os.environ.setdefault("PYTHONIOENCODING", "utf-8")
    _setup_qt_environment(base_dir)
    _setup_comtypes_cache()


_setup_environment()
