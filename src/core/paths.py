from __future__ import annotations

import os
import sys
from pathlib import Path

from PySide6.QtCore import QStandardPaths


APP_NAME = "CyberCompanion"


def get_base_dir() -> Path:
    """
    Return application read-only base directory.

    Priority:
    1) APP_BASE_DIR from runtime hook
    2) PyInstaller _MEIPASS
    3) project root in development mode
    """
    env_base = os.environ.get("APP_BASE_DIR", "").strip()
    if env_base:
        return Path(env_base)

    if getattr(sys, "frozen", False) and hasattr(sys, "_MEIPASS"):
        return Path(sys._MEIPASS)  # type: ignore[attr-defined]

    return Path(__file__).resolve().parents[2]


def get_user_data_dir() -> Path:
    """
    Return writable user data directory.
    """
    # Prefer Qt-standard writable location for consistency with runtime platform rules.
    location = QStandardPaths.writableLocation(QStandardPaths.StandardLocation.AppDataLocation)
    if location:
        target = Path(location)
    elif sys.platform == "win32":
        target = Path(os.environ.get("LOCALAPPDATA", Path.home() / "AppData" / "Local")) / APP_NAME
    else:
        target = Path.home() / ".local" / "share" / APP_NAME

    # Some environments may return app-agnostic location when app metadata is absent.
    if target.name.lower() != APP_NAME.lower():
        target = target / APP_NAME

    target.mkdir(parents=True, exist_ok=True)
    return target


def get_cache_dir() -> Path:
    location = QStandardPaths.writableLocation(QStandardPaths.StandardLocation.CacheLocation)
    if location:
        base = Path(location)
        if base.name.lower() != APP_NAME.lower():
            base = base / APP_NAME
        target = base / "audio"
    else:
        target = get_user_data_dir() / "cache" / "audio"
    target.mkdir(parents=True, exist_ok=True)
    return target


def get_log_dir() -> Path:
    target = get_user_data_dir() / "logs"
    target.mkdir(parents=True, exist_ok=True)
    return target


def get_log_file() -> Path:
    return get_log_dir() / "app.log"


def resolve_config_path() -> Path:
    """
    Resolve writable config path.

    Priority:
    1) User data config path (`%LOCALAPPDATA%/CyberCompanion/config.json`)
    2) First-run bootstrap copy from bundled config (if present)
    3) Return user data path (even if not created yet)
    """
    user_cfg = get_user_data_dir() / "config.json"
    if user_cfg.exists():
        return user_cfg

    base = get_base_dir()
    root_cfg = base / "config.json"
    folder_cfg = base / "config" / "config.json"
    source_cfg = root_cfg if root_cfg.exists() else folder_cfg if folder_cfg.exists() else None

    if source_cfg is not None:
        try:
            user_cfg.write_text(source_cfg.read_text(encoding="utf-8"), encoding="utf-8")
        except OSError:
            pass

    return user_cfg
