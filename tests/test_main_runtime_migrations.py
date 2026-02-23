from __future__ import annotations

import importlib
import os
import sys
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from core.config_manager import AppConfig

MAIN_MODULE = importlib.import_module("main")


class MainRuntimeMigrationsTest(unittest.TestCase):
    def test_migrate_legacy_llm_defaults_updates_xai_fast_model_and_v1_base(self) -> None:
        config = AppConfig()
        config.llm.provider = "xai"
        config.llm.model = "grok-4-fast-reasoning"
        config.llm.base_url = "https://api.x.ai/v1"

        changed = MAIN_MODULE._migrate_legacy_llm_defaults(config)

        self.assertTrue(changed)
        self.assertEqual(config.llm.model, "grok-4-latest")
        self.assertEqual(config.llm.base_url, "https://api.x.ai")

    def test_migrate_legacy_llm_defaults_is_noop_when_already_normalized(self) -> None:
        config = AppConfig()
        config.llm.provider = "xai"
        config.llm.model = "grok-4-latest"
        config.llm.base_url = "https://api.x.ai"

        changed = MAIN_MODULE._migrate_legacy_llm_defaults(config)

        self.assertFalse(changed)
        self.assertEqual(config.llm.model, "grok-4-latest")
        self.assertEqual(config.llm.base_url, "https://api.x.ai")

    def test_resolve_asr_runtime_uses_llm_key_for_zhipu_default(self) -> None:
        config = AppConfig()
        config.audio.asr_provider = "zhipu_asr"
        config.audio.asr_api_key = ""
        config.audio.asr_base_url = ""
        config.llm.api_key = "llm-key"

        with patch.dict(
            os.environ,
            {
                "ZHIPU_API_KEY": "",
                "OPENAI_API_KEY": "",
                "POLOAI_API_KEY": "",
                "XAI_API_KEY": "",
            },
            clear=False,
        ):
            asr_key, asr_base_url = MAIN_MODULE._resolve_asr_runtime(config)

        self.assertEqual(asr_key, "llm-key")
        self.assertEqual(asr_base_url, "https://open.bigmodel.cn/api/paas/v4/audio/transcriptions")

    def test_resolve_asr_runtime_keeps_explicit_asr_values(self) -> None:
        config = AppConfig()
        config.audio.asr_provider = "xai_realtime"
        config.audio.asr_api_key = "asr-key"
        config.audio.asr_base_url = " https://proxy.example/v1 "
        config.llm.api_key = "llm-key"
        config.llm.base_url = "https://api.x.ai"

        asr_key, asr_base_url = MAIN_MODULE._resolve_asr_runtime(config)

        self.assertEqual(asr_key, "asr-key")
        self.assertEqual(asr_base_url, "https://proxy.example/v1")

    def test_resolve_asr_runtime_falls_back_to_env_key_and_llm_base(self) -> None:
        config = AppConfig()
        config.audio.asr_provider = "xai_realtime"
        config.audio.asr_api_key = ""
        config.audio.asr_base_url = ""
        config.llm.api_key = ""
        config.llm.base_url = "https://gateway.example/v1"

        with patch.dict(
            os.environ,
            {
                "ZHIPU_API_KEY": "",
                "OPENAI_API_KEY": "",
                "POLOAI_API_KEY": "",
                "XAI_API_KEY": "env-key",
            },
            clear=False,
        ):
            asr_key, asr_base_url = MAIN_MODULE._resolve_asr_runtime(config)

        self.assertEqual(asr_key, "env-key")
        self.assertEqual(asr_base_url, "https://gateway.example/v1")

    def test_apply_dev_fast_idle_profile_enabled(self) -> None:
        config = AppConfig()
        config.trigger.idle_threshold_seconds = 180
        config.idle_invasion.enabled = True
        config.idle_invasion.start_delay_ms = 180_000
        config.idle_invasion.initial_spawn_interval_ms = 10_000
        config.idle_invasion.min_spawn_interval_ms = 2_000

        with patch.dict(os.environ, {"AEMEATH_DEV_FAST_IDLE": "1"}, clear=False):
            changed = MAIN_MODULE._apply_dev_fast_idle_profile(config)

        self.assertTrue(changed)
        self.assertEqual(config.trigger.idle_threshold_seconds, 60)
        self.assertEqual(config.idle_invasion.start_delay_ms, 60_000)
        self.assertEqual(config.idle_invasion.initial_spawn_interval_ms, 2_000)
        self.assertEqual(config.idle_invasion.min_spawn_interval_ms, 500)

    def test_apply_dev_fast_idle_profile_disabled(self) -> None:
        config = AppConfig()
        config.trigger.idle_threshold_seconds = 180
        config.idle_invasion.enabled = True
        config.idle_invasion.start_delay_ms = 180_000
        config.idle_invasion.initial_spawn_interval_ms = 10_000
        config.idle_invasion.min_spawn_interval_ms = 2_000

        with patch.dict(os.environ, {"AEMEATH_DEV_FAST_IDLE": "0"}, clear=False):
            changed = MAIN_MODULE._apply_dev_fast_idle_profile(config)

        self.assertFalse(changed)
        self.assertEqual(config.trigger.idle_threshold_seconds, 180)
        self.assertEqual(config.idle_invasion.start_delay_ms, 180_000)
        self.assertEqual(config.idle_invasion.initial_spawn_interval_ms, 10_000)
        self.assertEqual(config.idle_invasion.min_spawn_interval_ms, 2_000)


if __name__ == "__main__":
    unittest.main()
