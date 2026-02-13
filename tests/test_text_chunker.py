from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from ai.text_chunker import TextChunker


class TextChunkerTest(unittest.TestCase):
    def test_split_by_sentence_endings(self) -> None:
        chunker = TextChunker(target_chunk_chars=10, max_chunk_chars=20)
        output = chunker.feed("你在写代码。看起来快完成了！")
        self.assertEqual(output, ["你在写代码。", "看起来快完成了！"])

    def test_flush_remaining(self) -> None:
        chunker = TextChunker(target_chunk_chars=12, max_chunk_chars=24)
        output = chunker.feed("这个句子没有结尾标点")
        self.assertEqual(output, [])
        self.assertEqual(chunker.flush(), "这个句子没有结尾标点")


if __name__ == "__main__":
    unittest.main()
