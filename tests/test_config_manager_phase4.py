from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from core.config_manager import ConfigManager


class ConfigManagerPhase4Test(unittest.TestCase):
    def test_loads_vision_and_llm_sections(self) -> None:
        config = ConfigManager(ROOT / "config" / "config.json").load()
        self.assertIsNotNone(config.vision)
        self.assertIsNotNone(config.llm)
        self.assertGreaterEqual(config.vision.target_fps, 1)
        self.assertIn(config.llm.provider, {"none", "ollama", "openai"})


if __name__ == "__main__":
    unittest.main()

