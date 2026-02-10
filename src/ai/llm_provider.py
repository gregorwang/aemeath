from __future__ import annotations

import time
from abc import ABC, abstractmethod
from dataclasses import dataclass
from typing import Optional


@dataclass(slots=True)
class LLMRequest:
    system_prompt: str
    user_message: str
    max_tokens: int = 100
    temperature: float = 0.7


@dataclass(slots=True)
class LLMResponse:
    text: str
    tokens_used: int
    latency_ms: int
    provider: str


class LLMProvider(ABC):
    @abstractmethod
    async def generate(self, request: LLMRequest) -> LLMResponse:
        raise NotImplementedError

    @abstractmethod
    def is_available(self) -> bool:
        raise NotImplementedError

    @abstractmethod
    def get_resource_usage(self) -> dict:
        raise NotImplementedError


class OllamaProvider(LLMProvider):
    def __init__(self, model: str = "qwen2.5:7b", base_url: str = "http://localhost:11434"):
        self._model = model
        self._base_url = base_url.rstrip("/")

    async def generate(self, request: LLMRequest) -> LLMResponse:
        import httpx

        start = time.perf_counter()
        async with httpx.AsyncClient(timeout=30.0) as client:
            response = await client.post(
                f"{self._base_url}/api/generate",
                json={
                    "model": self._model,
                    "prompt": f"{request.system_prompt}\n\n{request.user_message}",
                    "stream": False,
                    "options": {
                        "num_predict": request.max_tokens,
                        "temperature": request.temperature,
                    },
                },
            )
            response.raise_for_status()
            result = response.json()
        latency = int((time.perf_counter() - start) * 1000)
        return LLMResponse(
            text=str(result.get("response", "")).strip(),
            tokens_used=int(result.get("eval_count", 0) or 0),
            latency_ms=latency,
            provider="ollama",
        )

    def is_available(self) -> bool:
        try:
            import httpx

            r = httpx.get(f"{self._base_url}/api/tags", timeout=2.0)
            return r.status_code == 200
        except Exception:
            return False

    def get_resource_usage(self) -> dict:
        return {"provider": "ollama", "memory_gb": 4.0}


class OpenAIProvider(LLMProvider):
    def __init__(
        self,
        model: str = "gpt-4o-mini",
        api_key: Optional[str] = None,
        base_url: str = "https://api.openai.com/v1",
    ):
        self._model = model
        self._api_key = api_key or self._load_api_key()
        self._base_url = base_url

    @staticmethod
    def _load_api_key() -> str:
        import os

        return os.environ.get("OPENAI_API_KEY", "")

    async def generate(self, request: LLMRequest) -> LLMResponse:
        from openai import AsyncOpenAI

        start = time.perf_counter()
        client = AsyncOpenAI(api_key=self._api_key, base_url=self._base_url)
        response = await client.chat.completions.create(
            model=self._model,
            messages=[
                {"role": "system", "content": request.system_prompt},
                {"role": "user", "content": request.user_message},
            ],
            max_tokens=request.max_tokens,
            temperature=request.temperature,
        )
        latency = int((time.perf_counter() - start) * 1000)
        text = response.choices[0].message.content or ""
        usage = response.usage.total_tokens if response.usage else 0
        return LLMResponse(
            text=text.strip(),
            tokens_used=int(usage),
            latency_ms=latency,
            provider="openai",
        )

    def is_available(self) -> bool:
        return bool(self._api_key)

    def get_resource_usage(self) -> dict:
        return {"provider": "openai_api", "memory_gb": 0.0}


class DummyProvider(LLMProvider):
    """Offline-safe fallback provider."""

    async def generate(self, request: LLMRequest) -> LLMResponse:
        _ = request
        return LLMResponse(
            text="我先安静陪你，等你需要我再说。",
            tokens_used=0,
            latency_ms=0,
            provider="dummy",
        )

    def is_available(self) -> bool:
        return True

    def get_resource_usage(self) -> dict:
        return {"provider": "dummy", "memory_gb": 0.0}

