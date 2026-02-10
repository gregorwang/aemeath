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
        try:
            os.environ["OPENAI_API_KEY"] = ""
            provider = OpenAIProvider(api_key=None)
            self.assertFalse(provider.is_available())
            os.environ["OPENAI_API_KEY"] = "test-key"
            provider2 = OpenAIProvider(api_key=None)
            self.assertTrue(provider2.is_available())
        finally:
            if old_key is None:
                os.environ.pop("OPENAI_API_KEY", None)
            else:
                os.environ["OPENAI_API_KEY"] = old_key


if __name__ == "__main__":
    unittest.main()

