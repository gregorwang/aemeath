"""
PyInstaller runtime hook.
Executed before application imports.
"""

import os
import sys
from pathlib import Path


def _setup_environment() -> None:
    if getattr(sys, "frozen", False) and hasattr(sys, "_MEIPASS"):
        base_dir = Path(sys._MEIPASS)  # type: ignore[attr-defined]
    else:
        base_dir = Path(__file__).resolve().parents[1]

    os.environ.setdefault("APP_BASE_DIR", str(base_dir))
    os.environ.setdefault("PYTHONIOENCODING", "utf-8")


_setup_environment()

