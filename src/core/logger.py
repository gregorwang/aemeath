from __future__ import annotations

import logging
import threading
from logging.handlers import RotatingFileHandler
from pathlib import Path

from PySide6.QtCore import QtMsgType, qInstallMessageHandler

_QT_BRIDGE_LOCK = threading.Lock()
_QT_BRIDGE_INSTALLED = False
_QT_PREV_HANDLER = None
_QT_BRIDGE_LOGGER: logging.Logger | None = None


def _qt_message_handler(mode, context, message: str) -> None:
    global _QT_BRIDGE_LOGGER
    logger = _QT_BRIDGE_LOGGER
    if logger is None:
        return

    category = ""
    try:
        category = str(getattr(context, "category", "") or "").strip()
    except Exception:
        category = ""
    prefix = f"[Qt:{category}] " if category else "[Qt] "

    if mode == QtMsgType.QtDebugMsg:
        logger.debug("%s%s", prefix, message)
    elif mode == QtMsgType.QtInfoMsg:
        logger.info("%s%s", prefix, message)
    elif mode == QtMsgType.QtWarningMsg:
        logger.warning("%s%s", prefix, message)
    elif mode == QtMsgType.QtCriticalMsg:
        logger.error("%s%s", prefix, message)
    elif mode == QtMsgType.QtFatalMsg:
        logger.critical("%s%s", prefix, message)
    else:
        logger.info("%s%s", prefix, message)

    if _QT_PREV_HANDLER is not None:
        try:
            _QT_PREV_HANDLER(mode, context, message)
        except Exception:
            pass


def _install_qt_message_bridge(logger: logging.Logger) -> None:
    global _QT_BRIDGE_INSTALLED, _QT_PREV_HANDLER, _QT_BRIDGE_LOGGER
    with _QT_BRIDGE_LOCK:
        _QT_BRIDGE_LOGGER = logger
        if _QT_BRIDGE_INSTALLED:
            return
        _QT_PREV_HANDLER = qInstallMessageHandler(_qt_message_handler)
        _QT_BRIDGE_INSTALLED = True


def setup_logger(log_dir: Path, debug: bool = False) -> logging.Logger:
    """
    Configure rotating file logger.
    """
    log_dir.mkdir(parents=True, exist_ok=True)
    logger = logging.getLogger("CyberCompanion")
    logger.setLevel(logging.DEBUG if debug else logging.INFO)
    logger.propagate = False

    # prevent duplicate handlers if setup is called multiple times
    if logger.handlers:
        _install_qt_message_bridge(logger)
        return logger

    file_handler = RotatingFileHandler(
        log_dir / "app.log",
        maxBytes=5 * 1024 * 1024,
        backupCount=3,
        encoding="utf-8",
    )
    file_handler.setLevel(logging.DEBUG)
    file_handler.setFormatter(
        logging.Formatter(
            "[%(asctime)s] [%(levelname)s] [%(name)s] %(message)s",
            datefmt="%Y-%m-%d %H:%M:%S",
        )
    )
    logger.addHandler(file_handler)

    if debug:
        console_handler = logging.StreamHandler()
        console_handler.setLevel(logging.DEBUG)
        console_handler.setFormatter(logging.Formatter("[%(levelname)s] %(message)s"))
        logger.addHandler(console_handler)

    _install_qt_message_bridge(logger)
    return logger
