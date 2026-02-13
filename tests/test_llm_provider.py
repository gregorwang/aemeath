from __future__ import annotations

import os
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from ai.llm_provider import DummyProvider, LLMRequest, OpenAIProvider


class LLMProviderTest(unittest.IsolatedAsyncioTestCase):
    async def test_dummy_provider_response(self) -> None:
        provider = DummyProvider()
        request = LLMRequest(system_prompt="sys", user_message="hello")
        response = await provider.generate(request)
        self.assertEqual(response.provider, "dummy")
        self.assertTrue(response.text)

    def test_openai_provider_availability_depends_on_key(self) -> None:
        old_key = os.environ.get("OPENAI_API_KEY")
        old_polo_key = os.environ.get("POLOAI_API_KEY")
        old_xai_key = os.environ.get("XAI_API_KEY")
        try:
            os.environ["OPENAI_API_KEY"] = ""
            os.environ["POLOAI_API_KEY"] = ""
            os.environ["XAI_API_KEY"] = ""
            provider = OpenAIProvider(api_key=None)
            self.assertFalse(provider.is_available())
            os.environ["OPENAI_API_KEY"] = "test-key"
            provider2 = OpenAIProvider(api_key=None)
            self.assertTrue(provider2.is_available())
            os.environ["OPENAI_API_KEY"] = ""
            os.environ["POLOAI_API_KEY"] = "polo-key"
            provider3 = OpenAIProvider(api_key=None)
            self.assertTrue(provider3.is_available())
            os.environ["POLOAI_API_KEY"] = ""
            os.environ["XAI_API_KEY"] = "xai-key"
            provider4 = OpenAIProvider(api_key=None)
            self.assertTrue(provider4.is_available())
        finally:
            if old_key is None:
                os.environ.pop("OPENAI_API_KEY", None)
            else:
                os.environ["OPENAI_API_KEY"] = old_key
            if old_polo_key is None:
                os.environ.pop("POLOAI_API_KEY", None)
            else:
                os.environ["POLOAI_API_KEY"] = old_polo_key
            if old_xai_key is None:
                os.environ.pop("XAI_API_KEY", None)
            else:
                os.environ["XAI_API_KEY"] = old_xai_key

    def test_openai_provider_normalizes_base_url(self) -> None:
        provider = OpenAIProvider(api_key="k", base_url="https://poloai.top")
        self.assertEqual(provider._base_url, "https://poloai.top/v1")
        provider2 = OpenAIProvider(api_key="k", base_url="https://api.x.ai/v1/responses")
        self.assertEqual(provider2._base_url, "https://api.x.ai/v1")

    def test_openai_extracts_stream_delta_text(self) -> None:
        payload = {
            "choices": [
                {
                    "delta": {
                        "content": "你好",
                    }
                }
            ]
        }
        self.assertEqual(OpenAIProvider._extract_delta_text(payload), "你好")

        payload_list = {
            "choices": [
                {
                    "delta": {
                        "content": [
                            {"type": "output_text", "text": "你在"},
                            {"type": "output_text", "text": "写代码"},
                        ]
                    }
                }
            ]
        }
        self.assertEqual(OpenAIProvider._extract_delta_text(payload_list), "你在写代码")


if __name__ == "__main__":
    unittest.main()
