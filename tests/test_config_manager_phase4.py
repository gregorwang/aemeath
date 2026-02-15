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
        self.assertIsNotNone(config.wakeup)
        self.assertIsNotNone(config.screen_commentary)
        self.assertIsNotNone(config.idle_invasion)
        self.assertGreaterEqual(config.vision.target_fps, 1)
        self.assertIn(config.llm.provider, {"none", "openai", "xai", "deepseek"})
        self.assertIsInstance(config.wakeup.phrases, tuple)
        self.assertEqual(config.audio.tts_provider, "edge")
        self.assertIn(config.audio.asr_provider, {"openai_whisper", "google", "xai_realtime", "zhipu_asr"})
        self.assertIn(config.audio.voice_input_mode, {"continuous", "push_to_talk"})

    def test_save_and_reload_new_fields(self) -> None:
        from tempfile import TemporaryDirectory

        with TemporaryDirectory() as td:
            cfg_path = Path(td) / "config.json"
            manager = ConfigManager(cfg_path)
            config = manager.load()
            config.audio.microphone_enabled = True
            config.behavior.offline_mode = True
            config.behavior.audio_output_reactive = False
            config.wakeup.enabled = True
            config.wakeup.phrases = ("小爱同学请你出来",)
            config.llm.provider = "xai"
            config.llm.api_key = "sk-test"
            config.audio.tts_provider = "edge"
            config.audio.voice_input_mode = "push_to_talk"
            config.audio.asr_provider = "zhipu_asr"
            config.audio.asr_api_key = "whisper-key"
            config.audio.asr_model = "glm-asr-2512"
            config.audio.asr_base_url = "https://open.bigmodel.cn/api/paas/v4/audio/transcriptions"
            config.audio.asr_temperature = 0.2
            config.audio.asr_prompt = "测试语音上下文"
            config.screen_commentary.streaming_enabled = True
            config.screen_commentary.ocr_fallback_enabled = False
            config.screen_commentary.stream_chunk_chars = 18
            config.screen_commentary.max_response_chars = 66
            config.screen_commentary.preamble_text = "我先看一眼你的屏幕。"
            config.screen_commentary.auto_enabled = True
            config.screen_commentary.auto_interval_minutes = 15
            config.idle_invasion.enabled = True
            config.idle_invasion.start_delay_ms = 120_000
            config.idle_invasion.initial_spawn_interval_ms = 9_000
            config.idle_invasion.min_spawn_interval_ms = 1_500
            config.idle_invasion.max_invaders = 33
            config.idle_invasion.scale = 0.65
            config.idle_invasion.cell_padding = 12
            config.idle_invasion.participating_gifs = ("state1.gif", "state7.gif")
            config.idle_invasion.retreat_style = "ripple"
            self.assertTrue(manager.save(config))

            loaded = manager.load()
            self.assertTrue(loaded.audio.microphone_enabled)
            self.assertTrue(loaded.behavior.offline_mode)
            self.assertFalse(loaded.behavior.audio_output_reactive)
            self.assertTrue(loaded.wakeup.enabled)
            self.assertEqual(loaded.wakeup.phrases, ("小爱同学请你出来",))
            self.assertEqual(loaded.llm.provider, "xai")
            self.assertEqual(loaded.llm.api_key, "sk-test")
            self.assertEqual(loaded.audio.tts_provider, "edge")
            self.assertEqual(loaded.audio.voice_input_mode, "push_to_talk")
            self.assertEqual(loaded.audio.asr_provider, "zhipu_asr")
            self.assertEqual(loaded.audio.asr_api_key, "whisper-key")
            self.assertEqual(loaded.audio.asr_model, "glm-asr-2512")
            self.assertEqual(loaded.audio.asr_base_url, "https://open.bigmodel.cn/api/paas/v4/audio/transcriptions")
            self.assertEqual(loaded.audio.asr_temperature, 0.2)
            self.assertEqual(loaded.audio.asr_prompt, "测试语音上下文")
            self.assertTrue(loaded.screen_commentary.streaming_enabled)
            self.assertFalse(loaded.screen_commentary.ocr_fallback_enabled)
            self.assertEqual(loaded.screen_commentary.stream_chunk_chars, 18)
            self.assertEqual(loaded.screen_commentary.max_response_chars, 66)
            self.assertEqual(loaded.screen_commentary.preamble_text, "我先看一眼你的屏幕。")
            self.assertTrue(loaded.screen_commentary.auto_enabled)
            self.assertEqual(loaded.screen_commentary.auto_interval_minutes, 15)
            self.assertTrue(loaded.idle_invasion.enabled)
            self.assertEqual(loaded.idle_invasion.start_delay_ms, 120_000)
            self.assertEqual(loaded.idle_invasion.initial_spawn_interval_ms, 9_000)
            self.assertEqual(loaded.idle_invasion.min_spawn_interval_ms, 1_500)
            self.assertEqual(loaded.idle_invasion.max_invaders, 33)
            self.assertAlmostEqual(loaded.idle_invasion.scale, 0.65)
            self.assertEqual(loaded.idle_invasion.cell_padding, 12)
            self.assertEqual(loaded.idle_invasion.participating_gifs, ("state1.gif", "state7.gif"))
            self.assertEqual(loaded.idle_invasion.retreat_style, "ripple")

    def test_idle_invasion_retreat_style_fallback(self) -> None:
        from tempfile import TemporaryDirectory

        with TemporaryDirectory() as td:
            cfg_path = Path(td) / "config.json"
            cfg_path.write_text(
                """
{
  "idle_invasion": {
    "enabled": true,
    "retreat_style": "unknown_style"
  }
}
""".strip(),
                encoding="utf-8",
            )
            manager = ConfigManager(cfg_path)
            loaded = manager.load()
            self.assertEqual(loaded.idle_invasion.retreat_style, "scatter")


if __name__ == "__main__":
    unittest.main()
