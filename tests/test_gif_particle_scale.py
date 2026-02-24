from __future__ import annotations

import os
import sys
import unittest
from pathlib import Path

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

from PySide6.QtCore import QCoreApplication
from PySide6.QtGui import QMovie
from PySide6.QtWidgets import QApplication

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from ui.gif_particle import GifParticle, ParticleConfig


def _get_or_create_app() -> QApplication | None:
    current = QCoreApplication.instance()
    if current is not None:
        if isinstance(current, QApplication):
            return current
        return None
    return QApplication(sys.argv)


class GifParticleScaleTest(unittest.TestCase):
    def setUp(self) -> None:
        self._app = _get_or_create_app()
        if self._app is None:
            self.skipTest("QCoreApplication already exists; this test requires QApplication.")
        self._particles: list[GifParticle] = []

    def tearDown(self) -> None:
        for particle in self._particles:
            particle.close()
            particle.deleteLater()

    def test_scale_uses_first_frame_size_when_current_image_is_empty(self) -> None:
        gif_path = ROOT / "characters" / "state1.gif"
        if not gif_path.exists():
            self.skipTest(f"GIF missing: {gif_path}")

        probe = QMovie(str(gif_path))
        if not probe.isValid():
            self.skipTest(f"Invalid GIF: {gif_path}")
        probe.jumpToFrame(0)
        base_size = probe.currentImage().size()
        if base_size.isEmpty():
            self.skipTest(f"Unable to read GIF size: {gif_path}")

        scale = 0.7
        expected_w = max(1, int(round(base_size.width() * scale)))
        expected_h = max(1, int(round(base_size.height() * scale)))

        particle = GifParticle(
            ParticleConfig(gif_path=str(gif_path), scale=scale),
            particle_id=1,
        )
        self._particles.append(particle)
        self._app.processEvents()

        self.assertGreater(particle.width(), 10)
        self.assertGreater(particle.height(), 10)
        self.assertEqual(particle.width(), expected_w)
        self.assertEqual(particle.height(), expected_h)


if __name__ == "__main__":
    unittest.main()
