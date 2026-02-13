from __future__ import annotations


class TextChunker:
    """Incrementally split streaming text into speakable sentence chunks."""

    _SENTENCE_ENDINGS = set("。！？!?；;")
    _SOFT_BREAKS = set("，,、 \n\t")

    def __init__(self, *, target_chunk_chars: int = 22, max_chunk_chars: int = 36) -> None:
        self._target_chunk_chars = max(8, int(target_chunk_chars))
        self._max_chunk_chars = max(self._target_chunk_chars, int(max_chunk_chars))
        self._buffer = ""

    def feed(self, delta_text: str) -> list[str]:
        if not delta_text:
            return []

        chunks: list[str] = []
        for ch in delta_text:
            self._buffer += ch

            if ch in self._SENTENCE_ENDINGS and len(self._buffer.strip()) >= 4:
                chunk = self._pop_buffer()
                if chunk:
                    chunks.append(chunk)
                continue

            if len(self._buffer) >= self._target_chunk_chars and ch in self._SOFT_BREAKS:
                chunk = self._pop_buffer()
                if chunk:
                    chunks.append(chunk)
                continue

            if len(self._buffer) >= self._max_chunk_chars:
                chunk = self._pop_buffer()
                if chunk:
                    chunks.append(chunk)

        return chunks

    def flush(self) -> str:
        return self._pop_buffer()

    def _pop_buffer(self) -> str:
        chunk = self._buffer.strip()
        self._buffer = ""
        return chunk
