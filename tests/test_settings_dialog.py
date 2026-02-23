from __future__ import annotations

import os
import sys
import unittest
from pathlib import Path

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

from PySide6.QtCore import QCoreApplication
from PySide6.QtWidgets import QApplication

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from core.config_manager import AppConfig
from ui.settings_dialog import SettingsDialog


def _get_or_create_app() -> QApplication | None:
    current = QCoreApplication.instance()
    if current is not None:
        if isinstance(current, QApplication):
            return current
        return None
    return QApplication(sys.argv)


class SettingsDialogConfigTest(unittest.TestCase):
    def setUp(self) -> None:
        self._app = _get_or_create_app()
        if self._app is None:
            self.skipTest("QCoreApplication already exists; settings dialog tests require QApplication.")
        self._dialogs: list[SettingsDialog] = []

    def tearDown(self) -> None:
        for dialog in self._dialogs:
            dialog.close()
            dialog.deleteLater()

    def _create_dialog(self, config: AppConfig) -> SettingsDialog:
        dialog = SettingsDialog(config=config)
        self._dialogs.append(dialog)
        return dialog

    def test_to_config_allows_clearing_screen_preamble(self) -> None:
        config = AppConfig()
        config.screen_commentary.preamble_text = "保留这句将导致回退"
        dialog = self._create_dialog(config)

        dialog.screen_preamble_edit.setText("")
        updated = dialog.to_config()

        self.assertEqual(updated.screen_commentary.preamble_text, "")

    def test_to_config_allows_clearing_asr_model_and_base_url(self) -> None:
        config = AppConfig()
        config.audio.asr_provider = "zhipu_asr"
        config.audio.asr_model = "glm-asr-2512"
        config.audio.asr_base_url = "https://open.bigmodel.cn/api/paas/v4/audio/transcriptions"
        dialog = self._create_dialog(config)

        dialog.asr_provider_combo.setCurrentText("google")
        dialog.asr_model_edit.setText("")
        dialog.asr_base_url_edit.setText("")
        updated = dialog.to_config()

        self.assertEqual(updated.audio.asr_provider, "google")
        self.assertEqual(updated.audio.asr_model, "")
        self.assertEqual(updated.audio.asr_base_url, "")


if __name__ == "__main__":
    unittest.main()
