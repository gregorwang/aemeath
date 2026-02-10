from __future__ import annotations

import os
import sys
from pathlib import Path


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
    if sys.platform == "win32":
        base = Path(os.environ.get("LOCALAPPDATA", Path.home() / "AppData" / "Local"))
    else:
        base = Path.home() / ".local" / "share"
    target = base / APP_NAME
    target.mkdir(parents=True, exist_ok=True)
    return target


def get_cache_dir() -> Path:
    target = get_user_data_dir() / "cache" / "audio"
    target.mkdir(parents=True, exist_ok=True)
    return target


def get_log_dir() -> Path:
    target = get_user_data_dir() / "logs"
    target.mkdir(parents=True, exist_ok=True)
    return target


def resolve_config_path() -> Path:
    """
    Resolve config path from packaged or development layout.
    """
    base = get_base_dir()
    root_cfg = base / "config.json"
    folder_cfg = base / "config" / "config.json"
    if root_cfg.exists():
        return root_cfg
    return folder_cfg

