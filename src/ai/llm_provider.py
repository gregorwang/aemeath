from __future__ import annotations

import asyncio
import logging
import time
from abc import ABC, abstractmethod
from dataclasses import dataclass
from typing import AsyncIterator, Optional

logger = logging.getLogger("CyberCompanion")


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

    async def generate_with_image(
        self,
        system_prompt: str,
        image_base64: str,
        user_message: str = "",
        max_tokens: int = 100,
        temperature: float = 0.7,
    ) -> LLMResponse:
        """Generate a response using a multimodal Vision LLM with an image.

        Subclasses that support vision should override this method.
        Default implementation raises NotImplementedError so callers
        should check ``hasattr(provider, 'generate_with_image')`` or catch.
        """
        raise NotImplementedError("This provider does not support vision.")

    async def generate_with_image_stream(
        self,
        system_prompt: str,
        image_base64: str,
        user_message: str = "",
        max_tokens: int = 100,
        temperature: float = 0.7,
    ) -> AsyncIterator[str]:
        """Default streaming fallback: call non-stream endpoint and yield once."""
        response = await self.generate_with_image(
            system_prompt=system_prompt,
            image_base64=image_base64,
            user_message=user_message,
            max_tokens=max_tokens,
            temperature=temperature,
        )
        text = (response.text or "").strip()
        if text:
            yield text


class OpenAIProvider(LLMProvider):
    OFFICIAL_OPENAI_BASE_URL = "https://api.openai.com/v1"
    MAX_TRANSIENT_RETRIES = 2
    RETRY_BASE_DELAY_SECONDS = 0.8

    def __init__(
        self,
        model: str = "gpt-5-mini",
        api_key: Optional[str] = None,
        base_url: str = OFFICIAL_OPENAI_BASE_URL,
    ):
        self._model = model
        self._api_key = api_key or self._load_api_key()
        self._base_url = self._normalize_base_url(base_url)

    @staticmethod
    def _load_api_key() -> str:
        import os

        return (
            os.environ.get("OPENAI_API_KEY", "")
            or os.environ.get("POLOAI_API_KEY", "")
            or os.environ.get("XAI_API_KEY", "")
        )

    @staticmethod
    def _normalize_base_url(base_url: str) -> str:
        normalized = (base_url or "").strip().rstrip("/")
        if not normalized:
            return OpenAIProvider.OFFICIAL_OPENAI_BASE_URL
        for suffix in ("/chat/completions", "/responses"):
            if normalized.endswith(suffix):
                normalized = normalized[: -len(suffix)]
                break
        if normalized.endswith("/v1"):
            return normalized
        return f"{normalized}/v1"

    def _candidate_base_urls(self) -> list[str]:
        candidates = [self._base_url]
        official = self.OFFICIAL_OPENAI_BASE_URL
        if self._base_url != official:
            candidates.append(official)
        return candidates

    @staticmethod
    def _is_transient_status(status_code: int) -> bool:
        return status_code in {408, 429, 500, 502, 503, 504}

    async def generate(self, request: LLMRequest) -> LLMResponse:
        import httpx

        start = time.perf_counter()
        payload = {
            "model": self._model,
            "messages": [
                {"role": "system", "content": request.system_prompt},
                {"role": "user", "content": request.user_message},
            ],
            "max_tokens": request.max_tokens,
            "temperature": request.temperature,
        }

        if not self._api_key:
            raise RuntimeError("OpenAI-compatible API key is missing.")

        response = await self._send_chat_request(payload)
        latency = int((time.perf_counter() - start) * 1000)
        result = response.json()
        text = self._extract_message_text(result)
        usage = self._extract_total_tokens(result)
        return LLMResponse(
            text=text.strip(),
            tokens_used=int(usage),
            latency_ms=latency,
            provider="openai",
        )

    async def generate_with_image(
        self,
        system_prompt: str,
        image_base64: str,
        user_message: str = "",
        max_tokens: int = 100,
        temperature: float = 0.7,
    ) -> LLMResponse:
        """OpenAI Vision API: send image as base64 in the messages array."""
        start = time.perf_counter()

        if not self._api_key:
            raise RuntimeError("OpenAI-compatible API key is missing.")

        # Build multimodal user message with both text and image.
        user_content: list[dict] = []
        if user_message:
            user_content.append({"type": "text", "text": user_message})
        user_content.append(
            {
                "type": "image_url",
                "image_url": {
                    "url": f"data:image/jpeg;base64,{image_base64}",
                    "detail": "auto",
                },
            }
        )

        payload = {
            "model": self._model,
            "messages": [
                {"role": "system", "content": system_prompt},
                {"role": "user", "content": user_content},
            ],
            "max_tokens": max_tokens,
            "temperature": temperature,
        }

        response = await self._send_chat_request(payload, timeout=60.0)
        latency = int((time.perf_counter() - start) * 1000)
        result = response.json()
        text = self._extract_message_text(result)
        usage = self._extract_total_tokens(result)
        return LLMResponse(
            text=text.strip(),
            tokens_used=int(usage),
            latency_ms=latency,
            provider="openai",
        )

    async def generate_with_image_stream(
        self,
        system_prompt: str,
        image_base64: str,
        user_message: str = "",
        max_tokens: int = 100,
        temperature: float = 0.7,
    ) -> AsyncIterator[str]:
        import httpx

        if not self._api_key:
            raise RuntimeError("OpenAI-compatible API key is missing.")

        user_content: list[dict] = []
        if user_message:
            user_content.append({"type": "text", "text": user_message})
        user_content.append(
            {
                "type": "image_url",
                "image_url": {
                    "url": f"data:image/jpeg;base64,{image_base64}",
                    "detail": "auto",
                },
            }
        )

        payload = {
            "model": self._model,
            "messages": [
                {"role": "system", "content": system_prompt},
                {"role": "user", "content": user_content},
            ],
            "max_tokens": max_tokens,
            "temperature": temperature,
            "stream": True,
        }

        stream_url = f"{self._base_url}/chat/completions"
        bearer_headers = {
            "Content-Type": "application/json",
            "Authorization": f"Bearer {self._api_key}",
        }

        async with httpx.AsyncClient(timeout=60.0) as client:
            async for token in self._stream_sse_chat(client, stream_url, payload, bearer_headers):
                yield token

    async def _stream_sse_chat(
        self,
        client,
        url: str,
        payload: dict,
        bearer_headers: dict[str, str],
    ) -> AsyncIterator[str]:
        plain_headers = {
            "Content-Type": "application/json",
            "Authorization": self._api_key,
        }

        async with client.stream("POST", url, json=payload, headers=bearer_headers) as response:
            if response.status_code in (401, 403):
                await response.aread()
            else:
                response.raise_for_status()
                async for token in self._iter_sse_tokens(response):
                    yield token
                return

        async with client.stream("POST", url, json=payload, headers=plain_headers) as fallback:
            fallback.raise_for_status()
            async for token in self._iter_sse_tokens(fallback):
                yield token

    async def _iter_sse_tokens(self, response) -> AsyncIterator[str]:
        import json

        async for raw_line in response.aiter_lines():
            line = (raw_line or "").strip()
            if not line.startswith("data:"):
                continue
            payload = line[5:].strip()
            if not payload or payload == "[DONE]":
                continue
            try:
                data = json.loads(payload)
            except json.JSONDecodeError:
                continue
            token = self._extract_delta_text(data)
            if token:
                yield token

    @staticmethod
    def _extract_delta_text(payload: dict) -> str:
        choices = payload.get("choices")
        if not isinstance(choices, list) or not choices:
            return ""
        first = choices[0] if isinstance(choices[0], dict) else {}
        delta = first.get("delta") if isinstance(first, dict) else {}

        if isinstance(delta, dict):
            content = delta.get("content")
            if isinstance(content, str):
                return content
            if isinstance(content, list):
                chunks: list[str] = []
                for item in content:
                    if isinstance(item, dict):
                        text_part = item.get("text")
                        if isinstance(text_part, str):
                            chunks.append(text_part)
                return "".join(chunks)

        message = first.get("message") if isinstance(first, dict) else {}
        if isinstance(message, dict):
            content = message.get("content")
            if isinstance(content, str):
                return content
        return ""

    async def _send_chat_request(self, payload: dict, timeout: float = 30.0):
        """Send chat completion with auth retry and transient/base-url fallback."""
        import httpx

        async with httpx.AsyncClient(timeout=timeout) as client:
            last_response = None
            base_urls = self._candidate_base_urls()
            for idx, base_url in enumerate(base_urls):
                url = f"{base_url}/chat/completions"
                logger.debug("[LLM] POST %s model=%s", url, payload.get("model", ""))
                for attempt in range(self.MAX_TRANSIENT_RETRIES + 1):
                    attempt_no = attempt + 1
                    bearer_headers = {
                        "Content-Type": "application/json",
                        "Authorization": f"Bearer {self._api_key}",
                    }
                    response = await client.post(url, json=payload, headers=bearer_headers)
                    if response.status_code in (401, 403):
                        logger.debug("[LLM] Bearer auth rejected (HTTP %s), retrying plain-key auth", response.status_code)
                        plain_key_headers = {
                            "Content-Type": "application/json",
                            "Authorization": self._api_key,
                        }
                        alt = await client.post(url, json=payload, headers=plain_key_headers)
                        if alt.status_code < 400:
                            logger.debug("[LLM] Request success via plain-key auth at %s", base_url)
                            return alt
                        response = alt
                    if response.status_code < 400:
                        logger.debug("[LLM] Request success at %s (attempt %d)", base_url, attempt_no)
                        return response

                    last_response = response
                    logger.warning(
                        "[LLM] Request failed: HTTP %s (attempt %d/%d, base=%s)",
                        response.status_code,
                        attempt_no,
                        self.MAX_TRANSIENT_RETRIES + 1,
                        base_url,
                    )
                    if not self._is_transient_status(response.status_code):
                        break
                    if attempt >= self.MAX_TRANSIENT_RETRIES:
                        break
                    delay = min(3.0, self.RETRY_BASE_DELAY_SECONDS * (2 ** attempt))
                    logger.warning("[LLM] Transient HTTP %s, %.1fs later retry", response.status_code, delay)
                    await asyncio.sleep(delay)

                has_next_base = idx < len(base_urls) - 1
                if not has_next_base:
                    break
                if last_response is not None and not self._is_transient_status(last_response.status_code):
                    break
                logger.warning("[LLM] Switching base URL fallback: %s -> %s", base_url, base_urls[idx + 1])
            if last_response is not None:
                last_response.raise_for_status()
        raise RuntimeError("OpenAI request failed without HTTP response.")

    def is_available(self) -> bool:
        return bool(self._api_key)

    def get_resource_usage(self) -> dict:
        return {"provider": "openai_api", "memory_gb": 0.0}

    @staticmethod
    def _extract_message_text(payload: dict) -> str:
        choices = payload.get("choices")
        if not isinstance(choices, list) or not choices:
            return ""
        first = choices[0] if isinstance(choices[0], dict) else {}
        message = first.get("message") if isinstance(first, dict) else {}
        content = message.get("content") if isinstance(message, dict) else ""
        if isinstance(content, str):
            return content
        if isinstance(content, list):
            chunks: list[str] = []
            for item in content:
                if isinstance(item, dict):
                    text_part = item.get("text")
                    if isinstance(text_part, str) and text_part.strip():
                        chunks.append(text_part.strip())
            return "\n".join(chunks)
        return ""

    @staticmethod
    def _extract_total_tokens(payload: dict) -> int:
        usage = payload.get("usage")
        if not isinstance(usage, dict):
            return 0
        total = usage.get("total_tokens", 0)
        try:
            return int(total)
        except (TypeError, ValueError):
            return 0


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
