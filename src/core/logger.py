from __future__ import annotations

import logging
from logging.handlers import RotatingFileHandler
from pathlib import Path


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

    return logger

