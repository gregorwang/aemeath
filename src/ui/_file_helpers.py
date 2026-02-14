from __future__ import annotations

from pathlib import Path

from PySide6.QtCore import QFile, QResource


def normalize_asset_path(path: Path | str) -> str:
    """Normalize a filesystem/resource path into a usable string."""
    raw = str(path).strip()
    if not raw:
        return ""
    if raw.startswith(":/"):
        return raw
    return str(Path(raw).resolve())


def path_exists(path_text: str) -> bool:
    """Return True when a filesystem path or Qt resource path exists."""
    raw = (path_text or "").strip()
    if not raw:
        return False
    if raw.startswith(":/"):
        return QFile.exists(raw) or QResource(raw).isValid()
    return Path(raw).exists()

